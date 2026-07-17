//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"

#include "inet/common/packet/chunk/BitCountChunk.h"

// IEEE 802.11 radio with HE MU/HE TB header handling.
//
// Encapsulates upper-layer packets with the appropriate IEEE 802.11 PHY header
// and decapsulates received packets.  For HE MU it collects per-user RU info,
// fills the HE MU PHY header (BSS color, PPDU format, Trigger ID, GI/coding,
// spatial reuse), computes the common PPDU duration, and exposes allocation
// info on reception via Ieee80211HeMuRxTag.
// Relevant clauses:
//   - Clause 27.3.4: HE PPDU formats.
//   - Clause 27.3.11.7: HE-SIG-A fields.
//   - Clause 27.3.11.8: HE-SIG-B fields for HE MU PPDUs.
//
// Approximations / simplifications:
//   - DL MU user durations are first estimated with estimateHeMuUserDuration()
//     and later recomputed with computeHeUserPhyParameters(); the two paths may
//     differ slightly.
//   - HE MU PHY header length is estimated rather than bit-exact (see
//     Ieee80211Transmitter for the same approximation).
//   - MU-MIMO grouping uses the same simplified same-RU-index heuristic as the
//     transmitter; full validation is delegated to Ieee80211HePhyCalculator.
//   - FCS handling for HE MU relies on higher-layer bit-error flags rather than
//     a per-MPDU FCS field in the PHY header.
#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/networklayer/common/NetworkInterface.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211DsssMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211DsssOfdmMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ErpOfdmMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211FhssMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HrDsssMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211IrMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211OfdmMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211EhtMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211ControlInfo_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeSigCodec.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.h"

namespace inet {

namespace physicallayer {

Define_Module(Ieee80211Radio);

simsignal_t Ieee80211Radio::radioChannelChangedSignal = cComponent::registerSignal("radioChannelChanged");
simsignal_t Ieee80211Radio::heRuIndexSignal = cComponent::registerSignal("heRuIndex");
simsignal_t Ieee80211Radio::heRuToneSizeSignal = cComponent::registerSignal("heRuToneSize");
simsignal_t Ieee80211Radio::heRuToneOffsetSignal = cComponent::registerSignal("heRuToneOffset");
simsignal_t Ieee80211Radio::heStaIdSignal = cComponent::registerSignal("heStaId");
simsignal_t Ieee80211Radio::heSpatialStreamsSignal = cComponent::registerSignal("heSpatialStreams");
simsignal_t Ieee80211Radio::heStreamStartIndexSignal = cComponent::registerSignal("heStreamStartIndex");
simsignal_t Ieee80211Radio::hePuncturedSubchannelMaskSignal = cComponent::registerSignal("hePuncturedSubchannelMask");
simsignal_t Ieee80211Radio::acknowledgmentFrameTypeSignal = cComponent::registerSignal("acknowledgmentFrameType");
simsignal_t Ieee80211Radio::acknowledgmentAirtimeSignal = cComponent::registerSignal("acknowledgmentAirtime");

static std::vector<Ieee80211HeMuUserInfo> collectHeMuUsers(const Packet *packet)
{
    std::vector<Ieee80211HeMuUserInfo> users;
    if (auto request = packet->findTag<Ieee80211HeMuReq>()) {
        // A direct HE MU/TB request already contains the TXVECTOR-like user
        // parameters from Clause 27.3.11.7/27.3.11.8, so it is converted into
        // one HE-SIG-B User field model entry.
        Ieee80211HeMuUserInfo user;
        user.ruIndex = request->getRuIndex();
        user.ruToneSize = request->getRuToneSize();
        user.ruToneOffset = request->getRuToneOffset();
        user.staId = request->getStaId();
        user.mcs = request->getMcs();
        user.numberOfSpatialStreams = request->getNumberOfSpatialStreams();
        user.streamStartIndex = request->getStreamStartIndex();
        user.dcm = request->getDcm();
        user.psduLength = request->getPsduLength() != B(-1) ? request->getPsduLength() : B(packet->getDataLength());
        Ieee80211HeRu ru;
        ru.index = user.ruIndex;
        ru.toneSize = std::max<int>(user.ruToneSize, 26);
        ru.toneOffset = user.ruToneOffset;
        ru.dataSubcarriers = getHeRuDataSubcarrierCount(ru.toneSize);
        ru.pilotSubcarriers = getHeRuPilotSubcarrierCount(ru.toneSize);
        ru.bandwidth = Hz(ru.toneSize * 78125.0);
        user.duration = computeHeUserPhyParameters(user.psduLength, ru, user.mcs,
                user.numberOfSpatialStreams, user.dcm,
                static_cast<Ieee80211HeGuardInterval>(request->getGuardInterval()),
                static_cast<Ieee80211HeCoding>(request->getCoding())).duration;
        users.push_back(user);
        return users;
    }
    if (dynamicPtrCast<const ieee80211::Ieee80211MacHeader>(packet->peekAtFront()) == nullptr)
        return users;
    auto packetCopy = packet->dup();
    packetCopy->popAtFront<ieee80211::Ieee80211MacHeader>();
    while (packetCopy->getDataLength() > b(0)) {
        auto payloadHeader = dynamicPtrCast<const Ieee80211HeMuRuPayloadHeader>(packetCopy->peekAtFront());
        if (payloadHeader == nullptr)
            break;
        // DL HE MU aggregate payloads are represented as per-RU payload
        // descriptors; each descriptor becomes one HE-SIG-B User field for
        // receiver-side RU filtering (Clause 27.3.11.8.4).
        packetCopy->popAtFront<Ieee80211HeMuRuPayloadHeader>();
        Ieee80211HeMuUserInfo user;
        user.ruIndex = payloadHeader->getRuIndex();
        user.ruToneSize = payloadHeader->getRuToneSize();
        user.ruToneOffset = payloadHeader->getRuToneOffset();
        user.staId = payloadHeader->getStaId();
        user.mcs = payloadHeader->getMcs();
        user.numberOfSpatialStreams = payloadHeader->getNumberOfSpatialStreams();
        user.dcm = payloadHeader->getDcm();
        user.psduLength = payloadHeader->getMpduLength();
        user.streamStartIndex = payloadHeader->getStreamStartIndex();
        user.duration = estimateHeMuUserDuration(user.psduLength, user.ruToneSize, user.mcs);
        users.push_back(user);
        if (payloadHeader->getMpduLength() > B(0))
            packetCopy->popAtFront(payloadHeader->getMpduLength());
    }
    delete packetCopy;
    return users;
}

Ieee80211Radio::Ieee80211Radio() :
    FlatRadioBase()
{
}

bool Ieee80211Radio::supportsParallelReception(const ITransmission *transmission) const
{
    auto packet = transmission == nullptr ? nullptr : transmission->getPacket();
    if (packet == nullptr || transmission->getPacketProtocol() != &Protocol::ieee80211HePhy ||
            !packet->hasAtFront<Ieee80211HeMuPhyHeader>())
        return false;
    // HE TB PPDUs are trigger responses that may arrive concurrently on
    // distinct RUs; non-TB formats use the normal single-reception path.
    return packet->peekAtFront<Ieee80211HeMuPhyHeader>()->getPpduFormat() == HE_TRIGGER_BASED_UPLINK;
}

void Ieee80211Radio::initialize(int stage)
{
    FlatRadioBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        const char *fcsModeString = par("fcsMode");
        fcsMode = parseFcsMode(fcsModeString, true);
    }
    if (stage == INITSTAGE_PHYSICAL_LAYER) {
        int channelNumber = par("channelNumber");
        if (channelNumber != -1)
            setChannelNumber(channelNumber);
    }
}

void Ieee80211Radio::handleUpperCommand(cMessage *message)
{
    if (message->getKind() == RADIO_C_CONFIGURE) {
        Ieee80211ConfigureRadioCommand *configureCommand = dynamic_cast<Ieee80211ConfigureRadioCommand *>(message->getControlInfo());
        if (configureCommand != nullptr) {
            const char *opMode = configureCommand->getOpMode();
            if (*opMode)
                setModeSet(Ieee80211ModeSet::getModeSet(opMode));
            const Ieee80211ModeSet *modeSet = configureCommand->getModeSet();
            if (modeSet != nullptr)
                setModeSet(modeSet);
            const IIeee80211Mode *mode = configureCommand->getMode();
            if (mode != nullptr)
                setMode(mode);
            const IIeee80211Band *band = configureCommand->getBand();
            if (band != nullptr)
                setBand(band);
            const Ieee80211Channel *channel = configureCommand->getChannel();
            if (channel != nullptr)
                setChannel(channel);
            int newChannelNumber = configureCommand->getChannelNumber();
            if (newChannelNumber != -1)
                setChannelNumber(newChannelNumber);
        }
    }
    FlatRadioBase::handleUpperCommand(message);
}

void Ieee80211Radio::setModeSet(const Ieee80211ModeSet *modeSet)
{
    Ieee80211Transmitter *ieee80211Transmitter = const_cast<Ieee80211Transmitter *>(check_and_cast<const Ieee80211Transmitter *>(transmitter));
    Ieee80211Receiver *ieee80211Receiver = const_cast<Ieee80211Receiver *>(check_and_cast<const Ieee80211Receiver *>(receiver));
    ieee80211Transmitter->setModeSet(modeSet);
    ieee80211Receiver->setModeSet(modeSet);
    EV << "Changing radio mode set to " << modeSet << endl;
    receptionTimer = nullptr;
    emit(listeningChangedSignal, 0);
}

void Ieee80211Radio::setMode(const IIeee80211Mode *mode)
{
    Ieee80211Transmitter *ieee80211Transmitter = const_cast<Ieee80211Transmitter *>(check_and_cast<const Ieee80211Transmitter *>(transmitter));
    ieee80211Transmitter->setMode(mode);
    EV << "Changing radio mode to " << mode << endl;
    receptionTimer = nullptr;
    emit(listeningChangedSignal, 0);
}

void Ieee80211Radio::setBand(const IIeee80211Band *band)
{
    Ieee80211Transmitter *ieee80211Transmitter = const_cast<Ieee80211Transmitter *>(check_and_cast<const Ieee80211Transmitter *>(transmitter));
    Ieee80211Receiver *ieee80211Receiver = const_cast<Ieee80211Receiver *>(check_and_cast<const Ieee80211Receiver *>(receiver));
    ieee80211Transmitter->setBand(band);
    ieee80211Receiver->setBand(band);
    EV << "Changing radio band to " << band << endl;
    receptionTimer = nullptr;
    emit(listeningChangedSignal, 0);
}

void Ieee80211Radio::setChannel(const Ieee80211Channel *channel)
{
    Ieee80211Transmitter *ieee80211Transmitter = const_cast<Ieee80211Transmitter *>(check_and_cast<const Ieee80211Transmitter *>(transmitter));
    Ieee80211Receiver *ieee80211Receiver = const_cast<Ieee80211Receiver *>(check_and_cast<const Ieee80211Receiver *>(receiver));
    ieee80211Transmitter->setChannel(channel);
    ieee80211Receiver->setChannel(channel);
    EV << "Changing radio channel to " << channel->getChannelNumber() << endl;
    receptionTimer = nullptr;
    emit(radioChannelChangedSignal, channel->getChannelNumber());
    emit(listeningChangedSignal, 0);
}

void Ieee80211Radio::setChannelNumber(int newChannelNumber)
{
    Ieee80211Transmitter *ieee80211Transmitter = const_cast<Ieee80211Transmitter *>(check_and_cast<const Ieee80211Transmitter *>(transmitter));
    Ieee80211Receiver *ieee80211Receiver = const_cast<Ieee80211Receiver *>(check_and_cast<const Ieee80211Receiver *>(receiver));
    ieee80211Transmitter->setChannelNumber(newChannelNumber);
    ieee80211Receiver->setChannelNumber(newChannelNumber);
    EV << "Changing radio channel to " << newChannelNumber << ".\n";
    receptionTimer = nullptr;
    emit(radioChannelChangedSignal, newChannelNumber);
    emit(listeningChangedSignal, 0);
}

void Ieee80211Radio::insertFcs(const Ptr<Ieee80211PhyHeader>& phyHeader) const
{
    if (auto header = dynamic_cast<Ieee80211FhssPhyHeader *>(phyHeader.get())) {
        header->setFcsMode(fcsMode);
        switch (fcsMode) {
            case FCS_COMPUTED:
                header->setFcs(0); // TODO calculate FCS
                break;
            case FCS_DECLARED_CORRECT:
                header->setFcs(0xC00D);
                break;
            case FCS_DECLARED_INCORRECT:
                header->setFcs(0xBAAD);
                break;
            default:
                throw cRuntimeError("Invalid FCS mode: %i", (int)fcsMode);
        }
    }
    else if (auto header = dynamic_cast<Ieee80211IrPhyHeader *>(phyHeader.get())) {
        header->setFcsMode(fcsMode);
        switch (fcsMode) {
            case FCS_COMPUTED:
                header->setFcs(0); // TODO calculate FCS
                break;
            case FCS_DECLARED_CORRECT:
                header->setFcs(0xC00D);
                break;
            case FCS_DECLARED_INCORRECT:
                header->setFcs(0xBAAD);
                break;
            default:
                throw cRuntimeError("Invalid FCS mode: %i", (int)fcsMode);
        }
    }
    else if (auto header = dynamic_cast<Ieee80211DsssPhyHeader *>(phyHeader.get())) {
        // IEEE Std 802.11-2024 15.3.3.7 and 16.2.3.7 specify CRC-16 over the
        // SIGNAL/SERVICE/LENGTH fields for DSSS and HR/DSSS. The current
        // packet-level model records declared correctness instead of computing
        // the bit-level CRC.
        header->setFcsMode(fcsMode);
        switch (fcsMode) {
            case FCS_COMPUTED:
                header->setFcs(0); // TODO calculate FCS
                break;
            case FCS_DECLARED_CORRECT:
                header->setFcs(0xC00D);
                break;
            case FCS_DECLARED_INCORRECT:
                header->setFcs(0xBAAD);
                break;
            default:
                throw cRuntimeError("Invalid FCS mode: %i", (int)fcsMode);
        }
    }
}

bool Ieee80211Radio::verifyFcs(const Ptr<const Ieee80211PhyHeader>& phyHeader) const
{
    if (auto header = dynamicPtrCast<const Ieee80211FhssPhyHeader>(phyHeader)) {
        switch (header->getFcsMode()) {
            case FCS_COMPUTED:
                return true; // TODO calculate and check FCS
            case FCS_DECLARED_CORRECT:
                return true;
            case FCS_DECLARED_INCORRECT:
                return false;
            default:
                throw cRuntimeError("Invalid FCS mode: %i", (int)fcsMode);
        }
    }
    else if (auto header = dynamicPtrCast<const Ieee80211IrPhyHeader>(phyHeader)) {
        switch (header->getFcsMode()) {
            case FCS_COMPUTED:
                return true; // TODO calculate and check FCS
            case FCS_DECLARED_CORRECT:
                return true;
            case FCS_DECLARED_INCORRECT:
                return false;
            default:
                throw cRuntimeError("Invalid FCS mode: %i", (int)fcsMode);
        }
    }
    else if (auto header = dynamicPtrCast<const Ieee80211DsssPhyHeader>(phyHeader)) {
        switch (header->getFcsMode()) {
            case FCS_COMPUTED:
                return true; // TODO calculate and check FCS
            case FCS_DECLARED_CORRECT:
                return true;
            case FCS_DECLARED_INCORRECT:
                return false;
            default:
                throw cRuntimeError("Invalid FCS mode: %i", (int)fcsMode);
        }
    }
    else
        return true;
}

void Ieee80211Radio::encapsulate(Packet *packet) const
{
    auto self = const_cast<Ieee80211Radio *>(this);
    auto ieee80211Transmitter = check_and_cast<const Ieee80211Transmitter *>(transmitter);
    auto mode = ieee80211Transmitter->computeTransmissionMode(packet);
    auto heMuUsers = collectHeMuUsers(packet);
    if (!heMuUsers.empty()) {
        auto request = packet->findTag<Ieee80211HeMuReq>();
        auto commonRequest = packet->findTag<Ieee80211HeMuCommonReq>();
        self->emit(hePuncturedSubchannelMaskSignal, (long)(request != nullptr ? request->getPuncturedSubchannelMask() :
                commonRequest != nullptr ? commonRequest->getPuncturedSubchannelMask() : 0));
        for (const auto& user : heMuUsers) {
            self->emit(heRuIndexSignal, (long)user.ruIndex);
            self->emit(heRuToneSizeSignal, (long)user.ruToneSize);
            self->emit(heRuToneOffsetSignal, (long)user.ruToneOffset);
            self->emit(heStaIdSignal, (long)user.staId);
            self->emit(heSpatialStreamsSignal, (long)user.numberOfSpatialStreams);
            self->emit(heStreamStartIndexSignal, (long)user.streamStartIndex);
        }
    }
    // An HE TB NDP (preamble-only) has no PSDU and therefore no MAC header chunk.
    // Guard the peek so it is not attempted on an empty packet.
    if (packet->getDataLength() > b(0)) {
        auto frontChunk = packet->peekAtFront<Chunk>();
        if (auto macHeader = dynamicPtrCast<const ieee80211::Ieee80211MacHeader>(frontChunk)) {
            bool acknowledgment = dynamicPtrCast<const ieee80211::Ieee80211AckFrame>(macHeader) != nullptr ||
                    dynamicPtrCast<const ieee80211::Ieee80211BlockAckReq>(macHeader) != nullptr ||
                    dynamicPtrCast<const ieee80211::Ieee80211BlockAck>(macHeader) != nullptr;
            if (acknowledgment) {
                self->emit(acknowledgmentFrameTypeSignal, (long)macHeader->getType());
                self->emit(acknowledgmentAirtimeSignal,
                        SimTime(mode->getDuration(packet->getDataLength()).dbl()));
            }
        }
    }
    auto phyHeader = !heMuUsers.empty() ? staticPtrCast<Ieee80211PhyHeader>(makeShared<Ieee80211HeMuPhyHeader>()) : mode->getHeaderMode()->createHeader();
    if (auto heMuPhyHeader = dynamicPtrCast<Ieee80211HeMuPhyHeader>(phyHeader)) {
        auto networkInterface = getContainingNicModule(this);
        auto mib = networkInterface ? dynamic_cast<const ieee80211::Ieee80211Mib *>(networkInterface->getSubmodule("mib")) : nullptr;
        if (mib != nullptr)
            heMuPhyHeader->setBssColor(mib->heOperation.bssColor);
        auto request = packet->findTag<Ieee80211HeMuReq>();
        heMuPhyHeader->setPpduFormat(request == nullptr ? HE_MU_DOWNLINK : request->getPpduFormat());
        heMuPhyHeader->setTriggerId(request == nullptr ? 0 : request->getTriggerId());
        auto commonRequest = packet->findTag<Ieee80211HeMuCommonReq>();
        heMuPhyHeader->setGuardInterval(request != nullptr ? request->getGuardInterval() :
                commonRequest != nullptr ? commonRequest->getGuardInterval() : HE_GI_3_2_US);
        heMuPhyHeader->setCoding(request != nullptr ? request->getCoding() :
                commonRequest != nullptr ? commonRequest->getCoding() : HE_CODING_BCC);
        heMuPhyHeader->setPacketExtensionDurationUs(request != nullptr ? request->getPacketExtensionDurationUs() :
                commonRequest != nullptr ? commonRequest->getPacketExtensionDurationUs() : 0);
        heMuPhyHeader->setPuncturedSubchannelMask(request != nullptr ? request->getPuncturedSubchannelMask() :
                commonRequest != nullptr ? commonRequest->getPuncturedSubchannelMask() : 0);
        heMuPhyHeader->setSpatialReuse(request != nullptr ? request->getSpatialReuse() :
                commonRequest != nullptr ? commonRequest->getSpatialReuse() : 0);
        heMuPhyHeader->setNonSrgObssPdDisallowed(request != nullptr ? request->getNonSrgObssPdDisallowed() :
                commonRequest != nullptr ? commonRequest->getNonSrgObssPdDisallowed() : false);
        heMuPhyHeader->setSrgObssPdDisallowed(request != nullptr ? request->getSrgObssPdDisallowed() :
                commonRequest != nullptr ? commonRequest->getSrgObssPdDisallowed() : false);
        heMuPhyHeader->setPsrDisallowed(request != nullptr ? request->getPsrDisallowed() :
                commonRequest != nullptr ? commonRequest->getPsrDisallowed() : false);
        std::vector<Ieee80211HeUserPhyParameters> requestedUsers;
        for (auto& user : heMuUsers) {
            Ieee80211HeRu ru;
            ru.index = user.ruIndex;
            ru.toneSize = std::max<int>(user.ruToneSize, 26);
            ru.toneOffset = user.ruToneOffset;
            ru.dataSubcarriers = getHeRuDataSubcarrierCount(ru.toneSize);
            ru.pilotSubcarriers = getHeRuPilotSubcarrierCount(ru.toneSize);
            ru.bandwidth = Hz(ru.toneSize * 78125.0);
            Ieee80211HeUserPhyParameters requested;
            requested.ru = ru;
            requested.mcs = user.mcs;
            requested.numberOfSpatialStreams = user.numberOfSpatialStreams;
            requested.dcm = user.dcm;
            requested.coding = static_cast<Ieee80211HeCoding>(heMuPhyHeader->getCoding());
            requested.psduLength = user.psduLength;
            requested.streamStartIndex = user.streamStartIndex;
            requested.staId = user.staId;
            requestedUsers.push_back(requested);
        }
        auto calculation = computeHePpduParameters(requestedUsers,
                mode->getDataMode()->getBandwidth(),
                static_cast<Ieee80211HePpduFormat>(heMuPhyHeader->getPpduFormat()),
                static_cast<Ieee80211HeGuardInterval>(heMuPhyHeader->getGuardInterval()), HE_LTF_4X,
                heMuPhyHeader->getPacketExtensionDurationUs());
        if (!calculation)
            throw cRuntimeError("Cannot construct HE MU PPDU: %s", calculation.error.c_str());
        simtime_t commonDuration = request != nullptr && request->getCommonDuration() > SIMTIME_ZERO ?
                request->getCommonDuration() : calculation.parameters.duration;
        if (request != nullptr && request->getPpduFormat() == HE_MU_DOWNLINK &&
                request->getCommonDuration() > SIMTIME_ZERO &&
                request->getCommonDuration() != calculation.parameters.duration)
            throw cRuntimeError("Planned HE MU PPDU duration does not match the resolved PHY parameters");
        if (request != nullptr && request->getPpduFormat() == HE_TRIGGER_BASED_UPLINK)
            commonDuration = std::max(commonDuration, calculation.parameters.duration);
        for (size_t i = 0; i < heMuUsers.size(); ++i) {
            auto& user = heMuUsers[i];
            user.duration = commonDuration;
            heMuPhyHeader->appendUsers(user);
        }
        // Detect MU-MIMO
        std::map<int, std::vector<size_t>> ruUserIndices;
        for (size_t i = 0; i < heMuUsers.size(); ++i) {
            ruUserIndices[heMuUsers[i].ruIndex].push_back(i);
        }
        bool isMuMimo = false;
        int maxTotalNsts = 0;
        for (const auto& pair : ruUserIndices) {
            if (pair.second.size() > 1) {
                isMuMimo = true;
                int groupNsts = 0;
                for (size_t idx : pair.second) {
                    groupNsts += heMuUsers[idx].numberOfSpatialStreams;
                }
                if (groupNsts > maxTotalNsts)
                    maxTotalNsts = groupNsts;
            }
        }
        heMuPhyHeader->setMuMimo(isMuMimo);
        if (request != nullptr && request->getMuMimo()) {
            isMuMimo = true;
            heMuPhyHeader->setMuMimo(true);
            maxTotalNsts = request->getTotalNsts();
        }
        if (isMuMimo) {
            heMuPhyHeader->setTotalNsts(maxTotalNsts);
        }
        heMuPhyHeader->setCommonDuration(commonDuration);
        int64_t totalBits = 64; // HE-SIG-A
        if (heMuPhyHeader->getPpduFormat() == 0) {
            // HE-SIG-B
            uint32_t maxToneIndex = 0;
            unsigned int numUsers = heMuPhyHeader->getUsersArraySize();
            for (unsigned int i = 0; i < numUsers; ++i) {
                const auto& u = heMuPhyHeader->getUsers(i);
                maxToneIndex = std::max(maxToneIndex, (uint32_t)(u.ruToneOffset + u.ruToneSize));
            }
            uint8_t bwField = 0;
            if (maxToneIndex > 996) bwField = 3;
            else if (maxToneIndex > 484) bwField = 2;
            else if (maxToneIndex > 242) bwField = 1;

            Hz channelBw = (bwField == 3) ? Hz(160e6) : ((bwField == 2) ? Hz(80e6) : ((bwField == 1) ? Hz(40e6) : Hz(20e6)));
            std::vector<Ieee80211HeRu> rus;
            for (unsigned int i = 0; i < numUsers; ++i) {
                const auto& user = heMuPhyHeader->getUsers(i);
                Ieee80211HeRu ru;
                ru.index = user.ruIndex;
                ru.toneSize = user.ruToneSize;
                ru.toneOffset = user.ruToneOffset;
                rus.push_back(ru);
            }
            auto codecResult = encodeHeSigBCommonField(rus, channelBw);
            int commonBits = 0;
            if (codecResult) {
                for (const auto& cc : codecResult.commonField.contentChannels) {
                    commonBits += cc.ruAllocationSubfields.size() * 8;
                }
                if (channelBw > Hz(40e6)) {
                    commonBits += 2; // 1 bit per content channel
                }
            }
            totalBits += commonBits + numUsers * 20;
        }
        phyHeader->setChunkLength(b(totalBits));
    }
    else
        phyHeader->setChunkLength(b(mode->getHeaderMode()->getLength()));

    if (auto htPhyHeader = dynamicPtrCast<Ieee80211HtPhyHeader>(phyHeader)) {
        if (auto htMode = dynamic_cast<const Ieee80211HtMode *>(mode)) {
            if (htMode->getDataMode()->getCode()) {
                htPhyHeader->setCoding(htMode->getDataMode()->getCode()->isLdpc() ? 1 : 0);
            }
        }
        else if (auto heMode = dynamic_cast<const Ieee80211HeMode *>(mode)) {
            if (heMode->getDataMode()->getCode()) {
                htPhyHeader->setCoding(heMode->getDataMode()->getCode()->isLdpc() ? 1 : 0);
            }
        }
        else if (auto ehtMode = dynamic_cast<const Ieee80211EhtMode *>(mode)) {
            if (ehtMode->getDataMode()->getCode()) {
                htPhyHeader->setCoding(ehtMode->getDataMode()->getCode()->isLdpc() ? 1 : 0);
            }
        }
    }
    else if (auto vhtPhyHeader = dynamicPtrCast<Ieee80211VhtPhyHeader>(phyHeader)) {
        if (auto vhtMode = dynamic_cast<const Ieee80211VhtMode *>(mode)) {
            if (vhtMode->getDataMode()->getCode()) {
                vhtPhyHeader->setCoding(vhtMode->getDataMode()->getCode()->isLdpc() ? 1 : 0);
            }
        }
    }

    phyHeader->setLengthField(B((packet->getDataLength().get<b>() + 7) / 8));
    // The inherited lengthField stores TXVECTOR LENGTH in octets for duration
    // calculations. IEEE Std 802.11-2024 17.3.4.3 uses the same unit for
    // OFDM/ERP L-SIG, while 15.3.3.6 and 16.2.3.6 convert DSSS/HR-DSSS PLCP
    // LENGTH to microseconds; that conversion is not represented here.
    insertFcs(phyHeader);
    packet->insertAtFront(phyHeader);

    auto tailLength = dynamic_cast<const Ieee80211OfdmMode *>(mode) ? b(6) : b(0);
    auto paddingLength = mode->getDataMode()->getPaddingLength(B(phyHeader->getLengthField()));
    if (tailLength + paddingLength != b(0)) {
        // IEEE Std 802.11-2024 17.3.5.3 and 17.3.5.4: non-HT OFDM DATA
        // materializes six tail bits plus pad bits. HT/VHT padding is accounted
        // for in airtime calculations, not as explicit packet trailer bits.
        const auto& phyTrailer = makeShared<BitCountChunk>(tailLength + paddingLength);
        packet->insertAtBack(phyTrailer);
    }
    const Protocol *protocol = nullptr;
    if (dynamic_cast<Ieee80211FhssPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211FhssPhy;
    else if (dynamic_cast<Ieee80211IrPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211IrPhy;
    else if (dynamic_cast<Ieee80211DsssPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211DsssPhy;
    else if (dynamic_cast<Ieee80211HrDsssPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211HrDsssPhy;
    else if (dynamic_cast<Ieee80211OfdmPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211OfdmPhy;
    else if (dynamic_cast<Ieee80211ErpOfdmPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211ErpOfdmPhy;
    else if (dynamic_cast<Ieee80211HtPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211HtPhy;
    else if (dynamic_cast<Ieee80211VhtPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211VhtPhy;
    else if (dynamic_cast<Ieee80211HeMuPhyHeader *>(phyHeader.get()))
        protocol = &Protocol::ieee80211HePhy;
    else
        throw cRuntimeError("Invalid IEEE 802.11 PHY header type.");
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(protocol);
}

void Ieee80211Radio::decapsulate(Packet *packet) const
{
    auto mode = packet->getTag<Ieee80211ModeInd>()->getMode();
    const auto& phyHeader = popIeee80211PhyHeaderAtFront(packet, b(-1), Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED);
    if (phyHeader->isIncorrect() || phyHeader->isIncomplete() || phyHeader->isImproperlyRepresented() || !verifyFcs(phyHeader))
        packet->setBitError(true);

    if (auto heMuPhyHeader = dynamicPtrCast<const Ieee80211HeMuPhyHeader>(phyHeader)) {
        auto tag = packet->addTagIfAbsent<Ieee80211HeMuRxTag>();
        tag->setPpduFormat(heMuPhyHeader->getPpduFormat());
        tag->setTriggerId(heMuPhyHeader->getTriggerId());
        tag->setGuardInterval(heMuPhyHeader->getGuardInterval());
        tag->setCoding(heMuPhyHeader->getCoding());
        tag->setPacketExtensionDurationUs(heMuPhyHeader->getPacketExtensionDurationUs());
        tag->setPuncturedSubchannelMask(heMuPhyHeader->getPuncturedSubchannelMask());
        tag->setRuIndex(-1);
        auto networkInterface = getContainingNicModule(this);
        std::optional<uint16_t> myStaId;
        if (heMuPhyHeader->getPpduFormat() == HE_TRIGGER_BASED_UPLINK) {
            MacAddress transmitterAddress;
            if (packet->getDataLength() > b(0))
                if (auto macHeader = dynamicPtrCast<const ieee80211::Ieee80211TwoAddressHeader>(packet->peekAtFront()))
                    transmitterAddress = macHeader->getTransmitterAddress();
            if (!transmitterAddress.isUnspecified())
                myStaId = resolveHeMuStaIdForReception(networkInterface, transmitterAddress);
        }
        else {
            myStaId = resolveHeMuStaIdForReception(networkInterface, networkInterface->getMacAddress());
        }
        for (unsigned int i = 0; i < heMuPhyHeader->getUsersArraySize(); ++i) {
            const auto& user = heMuPhyHeader->getUsers(i);
            Ieee80211HeMuRxAllocationInfo info;
            info.ruIndex = user.ruIndex;
            info.staId = user.staId;
            info.ruToneSize = user.ruToneSize;
            info.ruToneOffset = user.ruToneOffset;
            info.mcs = user.mcs;
            info.numberOfSpatialStreams = user.numberOfSpatialStreams;
            info.dcm = user.dcm;
            info.duration = user.duration;
            tag->appendAllocations(info);
            if (myStaId.has_value() && user.staId == *myStaId)
                tag->setRuIndex(user.ruIndex);
        }
    }
    if (auto indication = packet->findTagForUpdate<Ieee80211MpduReceiveInd>()) {
        bool failed = packet->hasBitError();
        for (unsigned int i = 0; i < indication->getResultsArraySize(); ++i) {
            auto result = indication->getResults(i);
            if (result.status == MPDU_NOT_EVALUATED)
                result.status = failed ? MPDU_FCS_ERROR : MPDU_SUCCESS;
            indication->setResults(i, result);
        }
    }

    auto tailLength = dynamic_cast<const Ieee80211OfdmMode *>(mode) ? b(6) : b(0);
    auto paddingLength = mode->getDataMode()->getPaddingLength(B(phyHeader->getLengthField()));
    if (tailLength + paddingLength != b(0))
        packet->popAtBack(tailLength + paddingLength, Chunk::PF_ALLOW_INCORRECT);
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::ieee80211Mac);
}

const Ptr<const Ieee80211PhyHeader> Ieee80211Radio::popIeee80211PhyHeaderAtFront(Packet *packet, b length, int flags)
{
    auto protocolTag = packet->findTag<PacketProtocolTag>();
    int id = protocolTag != nullptr && protocolTag->getProtocol() != nullptr ? protocolTag->getProtocol()->getId() : -1;
    if (id == Protocol::ieee80211FhssPhy.getId() || dynamicPtrCast<const Ieee80211FhssPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->popAtFront<Ieee80211FhssPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211IrPhy.getId() || dynamicPtrCast<const Ieee80211IrPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->popAtFront<Ieee80211IrPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211DsssPhy.getId() || dynamicPtrCast<const Ieee80211DsssPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->popAtFront<Ieee80211DsssPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211HrDsssPhy.getId() || dynamicPtrCast<const Ieee80211HrDsssPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->popAtFront<Ieee80211HrDsssPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211OfdmPhy.getId() || dynamicPtrCast<const Ieee80211OfdmPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->popAtFront<Ieee80211OfdmPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211ErpOfdmPhy.getId() || dynamicPtrCast<const Ieee80211ErpOfdmPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->popAtFront<Ieee80211ErpOfdmPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211HtPhy.getId() || dynamicPtrCast<const Ieee80211HtPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->popAtFront<Ieee80211HtPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211VhtPhy.getId() || dynamicPtrCast<const Ieee80211VhtPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->popAtFront<Ieee80211VhtPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211HePhy.getId() || dynamicPtrCast<const Ieee80211HeMuPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->popAtFront<Ieee80211HeMuPhyHeader>(length, flags);
    else
        throw cRuntimeError("Invalid IEEE 802.11 PHY protocol.");
}

const Ptr<const Ieee80211PhyHeader> Ieee80211Radio::peekIeee80211PhyHeaderAtFront(const Packet *packet, b length, int flags)
{
    auto protocolTag = const_cast<Packet *>(packet)->findTag<PacketProtocolTag>();
    int id = protocolTag != nullptr && protocolTag->getProtocol() != nullptr ? protocolTag->getProtocol()->getId() : -1;
    if (id == Protocol::ieee80211FhssPhy.getId() || dynamicPtrCast<const Ieee80211FhssPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->peekAtFront<Ieee80211FhssPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211IrPhy.getId() || dynamicPtrCast<const Ieee80211IrPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->peekAtFront<Ieee80211IrPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211DsssPhy.getId() || dynamicPtrCast<const Ieee80211DsssPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->peekAtFront<Ieee80211DsssPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211HrDsssPhy.getId() || dynamicPtrCast<const Ieee80211HrDsssPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->peekAtFront<Ieee80211HrDsssPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211OfdmPhy.getId() || dynamicPtrCast<const Ieee80211OfdmPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->peekAtFront<Ieee80211OfdmPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211ErpOfdmPhy.getId() || dynamicPtrCast<const Ieee80211ErpOfdmPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->peekAtFront<Ieee80211ErpOfdmPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211HtPhy.getId() || dynamicPtrCast<const Ieee80211HtPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->peekAtFront<Ieee80211HtPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211VhtPhy.getId() || dynamicPtrCast<const Ieee80211VhtPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->peekAtFront<Ieee80211VhtPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211HePhy.getId() || dynamicPtrCast<const Ieee80211HeMuPhyHeader>(packet->peekAtFront()) != nullptr)
        return packet->peekAtFront<Ieee80211HeMuPhyHeader>(length, flags);
    else
        throw cRuntimeError("Invalid IEEE 802.11 PHY protocol.");
}

} // namespace physicallayer

} // namespace inet
