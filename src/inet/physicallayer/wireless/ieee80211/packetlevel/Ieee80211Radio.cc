//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"

#include "inet/common/packet/chunk/BitCountChunk.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211DsssMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211DsssOfdmMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ErpOfdmMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211FhssMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HrDsssMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211IrMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211OfdmMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211ControlInfo_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211DataEncodingPlanTag.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211VhtSigA.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211VhtSigB.h"

namespace inet {

namespace physicallayer {

namespace {

uint8_t getVhtBandwidthCode(Hz bandwidth)
{
    if (bandwidth == MHz(20))
        return 0;
    if (bandwidth == MHz(40))
        return 1;
    if (bandwidth == MHz(80))
        return 2;
    if (bandwidth == MHz(160))
        return 3;
    throw cRuntimeError("Unsupported VHT-SIG-A bandwidth %s", bandwidth.str().c_str());
}

void populateHtOrVhtHeader(const Ptr<Ieee80211PhyHeader>& phyHeader,
        const IIeee80211Mode *mode, b psduLength, const Packet *packet,
        const Ieee80211DataEncodingPlan *plan)
{
    const auto *dataMode = mode->getDataMode();
    if (auto htHeader = dynamicPtrCast<Ieee80211HtPhyHeader>(phyHeader)) {
        auto htDataMode = check_and_cast<const Ieee80211HtDataMode *>(dataMode);
        if (plan == nullptr || plan->getPhyFormat() != Ieee80211PhyFormat::HT ||
            plan->getFecType() != htDataMode->getFecType())
            throw cRuntimeError("HT PHY header requires its authoritative data encoding plan");
        htHeader->setMcs(htDataMode->getModulationAndCodingScheme()->getMcsIndex());
        htHeader->setChannelWidth40(htDataMode->getBandwidth() == MHz(40));
        htHeader->setFecCoding(htDataMode->getFecType() == Ieee80211FecType::LDPC);
        htHeader->setShortGi(htDataMode->getGuardIntervalType() == Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT);
    }
    else if (auto vhtHeader = dynamicPtrCast<Ieee80211VhtPhyHeader>(phyHeader)) {
        auto vhtDataMode = check_and_cast<const Ieee80211VhtDataMode *>(dataMode);
        if (plan == nullptr || plan->getPhyFormat() != Ieee80211PhyFormat::VHT_SU ||
            plan->getFecType() != vhtDataMode->getFecType())
            throw cRuntimeError("VHT PHY header requires its authoritative data encoding plan");
        bool shortGi = vhtDataMode->getGuardIntervalType() == Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT;
        vhtHeader->setBandwidth(getVhtBandwidthCode(vhtDataMode->getBandwidth()));
        auto sigBLayout = getVhtSuSigBLayout(vhtHeader->getBandwidth());
        // Keep the aggregate chunk metadata authoritative even when this
        // helper is used independently of encapsulate()'s mode-sized header.
        vhtHeader->setChunkLength(b(48 + sigBLayout.getBitLength()));
        vhtHeader->setVhtSigBLength(encodeVhtSuSigBLength(psduLength));
        vhtHeader->setVhtSigBReserved(sigBLayout.getReservedValue());
        vhtHeader->setVhtSigBTail(0);
        if (auto request = packet->findTag<Ieee80211VhtSigAReq>()) {
            validateVhtSuGroupIdAndPartialAid(request->getGroupId(), request->getPartialAid());
            vhtHeader->setGroupId(request->getGroupId());
            vhtHeader->setPartialAid(request->getPartialAid());
        }
        else {
            auto macHeader = packet->peekAtFront<ieee80211::Ieee80211MacHeader>();
            if (macHeader->getReceiverAddress().isMulticast()) {
                vhtHeader->setGroupId(63);
                vhtHeader->setPartialAid(0);
            }
            else if (auto dataOrMgmtHeader = dynamicPtrCast<const ieee80211::Ieee80211DataOrMgmtHeader>(macHeader);
                     dataOrMgmtHeader != nullptr && dataOrMgmtHeader->getToDS() && !dataOrMgmtHeader->getFromDS()) {
                vhtHeader->setGroupId(0);
                vhtHeader->setPartialAid(computeVhtPartialAidForBssid(dataOrMgmtHeader->getReceiverAddress()));
            }
            else
                throw cRuntimeError("VHT transmission requires MAC-supplied GROUP_ID/PARTIAL_AID context for this unicast frame");
        }
        vhtHeader->setNumberOfSpaceTimeStreams(vhtDataMode->getNumberOfSpatialStreams() - 1);
        vhtHeader->setShortGi(shortGi);
        vhtHeader->setShortGiNsymDisambiguation(shortGi && plan->getNumberOfSymbols() % 10 == 9);
        vhtHeader->setCoding(vhtDataMode->getFecType() == Ieee80211FecType::LDPC);
        vhtHeader->setLdpcExtraOfdmSymbol(vhtDataMode->getFecType() == Ieee80211FecType::LDPC &&
                                          plan->getAdditionalCapacityApplied());
        vhtHeader->setMcs(vhtDataMode->getModulationAndCodingScheme()->getMcsIndex());
    }
}

bool validateHtOrVhtHeader(const Ptr<const Ieee80211PhyHeader>& phyHeader,
        const IIeee80211Mode *mode, const Ieee80211DataEncodingPlan *plan)
{
    const auto *dataMode = mode->getDataMode();
    if (auto htHeader = dynamicPtrCast<const Ieee80211HtPhyHeader>(phyHeader)) {
        auto htDataMode = dynamic_cast<const Ieee80211HtDataMode *>(dataMode);
        return htDataMode != nullptr && plan != nullptr &&
               plan->getPhyFormat() == Ieee80211PhyFormat::HT &&
               plan->getFecType() == htDataMode->getFecType() &&
               htHeader->getMcs() == htDataMode->getModulationAndCodingScheme()->getMcsIndex() &&
               htHeader->getChannelWidth40() == (htDataMode->getBandwidth() == MHz(40)) &&
               htHeader->getFecCoding() == (htDataMode->getFecType() == Ieee80211FecType::LDPC) &&
               htHeader->getShortGi() == (htDataMode->getGuardIntervalType() == Ieee80211HtModeBase::HT_GUARD_INTERVAL_SHORT);
    }
    if (auto vhtHeader = dynamicPtrCast<const Ieee80211VhtPhyHeader>(phyHeader)) {
        auto vhtDataMode = dynamic_cast<const Ieee80211VhtDataMode *>(dataMode);
        if (vhtDataMode == nullptr)
            return false;
        if (plan == nullptr || plan->getPhyFormat() != Ieee80211PhyFormat::VHT_SU ||
            plan->getFecType() != vhtDataMode->getFecType())
            return false;
        bool isLdpc = vhtDataMode->getFecType() == Ieee80211FecType::LDPC;
        bool shortGi = vhtDataMode->getGuardIntervalType() == Ieee80211VhtModeBase::HT_GUARD_INTERVAL_SHORT;
        auto sigBLayout = getVhtSuSigBLayout(vhtHeader->getBandwidth());
        bool valid = vhtHeader->getBandwidth() == getVhtBandwidthCode(vhtDataMode->getBandwidth()) &&
               (vhtHeader->getGroupId() == 0 || vhtHeader->getGroupId() == 63) &&
               vhtHeader->getPartialAid() <= 511 &&
               vhtHeader->getNumberOfSpaceTimeStreams() == vhtDataMode->getNumberOfSpatialStreams() - 1 &&
               vhtHeader->getShortGi() == shortGi &&
               vhtHeader->getShortGiNsymDisambiguation() == (shortGi && plan->getNumberOfSymbols() % 10 == 9) &&
               vhtHeader->getCoding() == isLdpc &&
               vhtHeader->getLdpcExtraOfdmSymbol() == (isLdpc && plan->getAdditionalCapacityApplied()) &&
               vhtHeader->getMcs() == vhtDataMode->getModulationAndCodingScheme()->getMcsIndex() &&
               vhtHeader->getVhtSigBLength() == encodeVhtSuSigBLength(vhtHeader->getLengthField()) &&
               vhtHeader->getVhtSigBReserved() == sigBLayout.getReservedValue() &&
               vhtHeader->getVhtSigBTail() == 0;
        if (!valid)
            EV_DEBUG << "Received VHT-SU PHY header disagrees with receiver-derived mode/plan: "
                     << "bw=" << vhtHeader->getBandwidth()
                     << " expectedBw=" << unsigned(getVhtBandwidthCode(vhtDataMode->getBandwidth()))
                     << " gid=" << vhtHeader->getGroupId()
                     << " nsts=" << vhtHeader->getNumberOfSpaceTimeStreams()
                     << " expectedNsts=" << vhtDataMode->getNumberOfSpatialStreams() - 1
                     << " shortGi=" << vhtHeader->getShortGi()
                     << " shortGiDisambiguation=" << vhtHeader->getShortGiNsymDisambiguation()
                     << " expectedShortGiDisambiguation=" << (shortGi && plan->getNumberOfSymbols() % 10 == 9)
                     << " coding=" << vhtHeader->getCoding()
                     << " extra=" << vhtHeader->getLdpcExtraOfdmSymbol()
                     << " expectedExtra=" << (isLdpc && plan->getAdditionalCapacityApplied())
                     << " mcs=" << vhtHeader->getMcs()
                     << " expectedMcs=" << vhtDataMode->getModulationAndCodingScheme()->getMcsIndex()
                     << " sigBLength=" << vhtHeader->getVhtSigBLength()
                     << " baseLength=" << vhtHeader->getLengthField()
                     << " reserved=" << vhtHeader->getVhtSigBReserved()
                     << " tail=" << vhtHeader->getVhtSigBTail() << endl;
        return valid;
    }
    return true;
}

} // namespace

Define_Module(Ieee80211Radio);

simsignal_t Ieee80211Radio::radioChannelChangedSignal = cComponent::registerSignal("radioChannelChanged");

Ieee80211Radio::Ieee80211Radio() :
    FlatRadioBase()
{
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
    auto ieee80211Transmitter = check_and_cast<const Ieee80211Transmitter *>(transmitter);
    auto mode = ieee80211Transmitter->computeTransmissionMode(packet);
    std::unique_ptr<Ieee80211DataEncodingPlan> plan;
    auto phyFormat = mode->getDataMode()->getPhyFormat();
    b apepLength = B(packet->getDataLength());
    if (phyFormat == Ieee80211PhyFormat::HT || phyFormat == Ieee80211PhyFormat::VHT_SU) {
        // VHT-SU LDPC uses the exact, possibly unaligned APEP_LENGTH only
        // while constructing the local TXVECTOR. The packet data itself is
        // the complete PSDU after §10.12.6 MAC padding, so the local request
        // must not be replaced by packet->getDataLength().
        if (phyFormat == Ieee80211PhyFormat::VHT_SU &&
            mode->getDataMode()->getFecType() == Ieee80211FecType::LDPC) {
            auto request = packet->findTag<Ieee80211VhtApepReq>();
            if (request == nullptr || request->getApepLength() < 0)
                throw cRuntimeError("IEEE 802.11 VHT-SU LDPC requires a valid local APEP_LENGTH TXVECTOR request");
            apepLength = B(request->getApepLength());
        }
        plan = std::make_unique<Ieee80211DataEncodingPlan>(
                mode->getDataMode()->computeEncodingPlan(apepLength));
        if (phyFormat == Ieee80211PhyFormat::VHT_SU &&
            mode->getDataMode()->getFecType() == Ieee80211FecType::LDPC) {
            b expectedPsduLength = B((plan->getUncodedDataBits() - 16) / 8);
            if (packet->getDataLength() != expectedPsduLength)
                throw cRuntimeError("IEEE 802.11 VHT-SU LDPC packet data length %s does not match the planned complete PSDU length %s",
                                    packet->getDataLength().str().c_str(), expectedPsduLength.str().c_str());
            // This request is MAC-to-PHY context only. Removing it before
            // header insertion prevents exact APEP_LENGTH from entering the
            // immutable transmission packet or crossing the medium.
            packet->removeTagIfPresent<Ieee80211VhtApepReq>();
        }
        packet->addTagIfAbsent<Ieee80211DataEncodingPlanTag>()->setPlan(*plan);
    }
    auto phyHeader = mode->getHeaderMode()->createHeader();
    phyHeader->setChunkLength(b(mode->getHeaderMode()->getLength()));
    phyHeader->setLengthField(apepLength);
    populateHtOrVhtHeader(phyHeader, mode, apepLength, packet, plan.get());
    if (phyFormat == Ieee80211PhyFormat::VHT_SU)
        // VHT-SIG-A now contains the request's wire representation; the
        // MAC-to-PHY request itself must not cross the radio boundary.
        packet->removeTagIfPresent<Ieee80211VhtSigAReq>();
    if (phyFormat == Ieee80211PhyFormat::VHT_SU &&
        mode->getDataMode()->getFecType() == Ieee80211FecType::LDPC) {
        // The generic lengthField is not serialized for VHT. Do not let it
        // carry the exact sender APEP_LENGTH across the radio boundary: keep
        // only the rounded RXVECTOR indication represented by VHT-SIG-B.
        auto vhtHeader = dynamicPtrCast<Ieee80211VhtPhyHeader>(phyHeader);
        phyHeader->setLengthField(decodeVhtSuSigBLength(vhtHeader->getVhtSigBLength()));
    }
    insertFcs(phyHeader);
    packet->insertAtFront(phyHeader);

    auto tailLength = dynamic_cast<const Ieee80211OfdmMode *>(mode) ? b(6) : b(0);
    // VHT-SU LDPC PHY padding is already represented by the residual bits
    // between SERVICE+complete PSDU octets and Npld in the LDPC data field;
    // do not derive or append structured byte padding from the local APEP
    // lengthField. Other PHYs retain their existing mode-based padding.
    auto paddingLength = (phyFormat == Ieee80211PhyFormat::VHT_SU &&
                          mode->getDataMode()->getFecType() == Ieee80211FecType::LDPC) ?
            b(0) : mode->getDataMode()->getPaddingLength(B(phyHeader->getLengthField()));
    if (tailLength + paddingLength != b(0)) {
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
    else
        throw cRuntimeError("Invalid IEEE 802.11 PHY header type.");
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(protocol);
}

void Ieee80211Radio::decapsulate(Packet *packet) const
{
    auto mode = packet->getTag<Ieee80211ModeInd>()->getMode();
    auto planTag = packet->findTag<Ieee80211DataEncodingPlanTag>();
    auto plan = planTag != nullptr && planTag->hasPlan() ? &planTag->getPlan() : nullptr;
    const auto& phyHeader = popIeee80211PhyHeaderAtFront(packet, b(-1), Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED);
    bool headerValid = !phyHeader->isIncorrect() && !phyHeader->isIncomplete() &&
            !phyHeader->isImproperlyRepresented() && verifyFcs(phyHeader) &&
            validateHtOrVhtHeader(phyHeader, mode, plan);
    if (!headerValid) {
        EV_DEBUG << "Received IEEE 802.11 PHY header validation failed: incorrect="
                 << phyHeader->isIncorrect() << " incomplete=" << phyHeader->isIncomplete()
                 << " improper=" << phyHeader->isImproperlyRepresented()
                 << " plan=" << (plan != nullptr) << endl;
        packet->setBitError(true);
    }
    // The reconstructed plan is private PHY receive context used only for
    // header/data validation. Do not leak it beyond decapsulation.
    packet->removeTagIfPresent<Ieee80211DataEncodingPlanTag>();
    auto tailLength = dynamic_cast<const Ieee80211OfdmMode *>(mode) ? b(6) : b(0);
    auto paddingLength = (mode->getDataMode()->getPhyFormat() == Ieee80211PhyFormat::VHT_SU &&
                          mode->getDataMode()->getFecType() == Ieee80211FecType::LDPC) ?
            b(0) : mode->getDataMode()->getPaddingLength(B(phyHeader->getLengthField()));
    if (tailLength + paddingLength != b(0))
        packet->popAtBack(tailLength + paddingLength, Chunk::PF_ALLOW_INCORRECT);
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::ieee80211Mac);
}

const Ptr<const Ieee80211PhyHeader> Ieee80211Radio::popIeee80211PhyHeaderAtFront(Packet *packet, b length, int flags)
{
    int id = packet->getTag<PacketProtocolTag>()->getProtocol()->getId();
    if (id == Protocol::ieee80211FhssPhy.getId())
        return packet->popAtFront<Ieee80211FhssPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211IrPhy.getId())
        return packet->popAtFront<Ieee80211IrPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211DsssPhy.getId())
        return packet->popAtFront<Ieee80211DsssPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211HrDsssPhy.getId())
        return packet->popAtFront<Ieee80211HrDsssPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211OfdmPhy.getId())
        return packet->popAtFront<Ieee80211OfdmPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211ErpOfdmPhy.getId())
        return packet->popAtFront<Ieee80211ErpOfdmPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211HtPhy.getId())
        return packet->popAtFront<Ieee80211HtPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211VhtPhy.getId())
        return packet->popAtFront<Ieee80211VhtPhyHeader>(length, flags);
    else
        throw cRuntimeError("Invalid IEEE 802.11 PHY protocol.");
}

const Ptr<const Ieee80211PhyHeader> Ieee80211Radio::peekIeee80211PhyHeaderAtFront(const Packet *packet, b length, int flags)
{
    int id = packet->getTag<PacketProtocolTag>()->getProtocol()->getId();
    if (id == Protocol::ieee80211FhssPhy.getId())
        return packet->peekAtFront<Ieee80211FhssPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211IrPhy.getId())
        return packet->peekAtFront<Ieee80211IrPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211DsssPhy.getId())
        return packet->peekAtFront<Ieee80211DsssPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211HrDsssPhy.getId())
        return packet->peekAtFront<Ieee80211HrDsssPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211OfdmPhy.getId())
        return packet->peekAtFront<Ieee80211OfdmPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211ErpOfdmPhy.getId())
        return packet->peekAtFront<Ieee80211ErpOfdmPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211HtPhy.getId())
        return packet->peekAtFront<Ieee80211HtPhyHeader>(length, flags);
    else if (id == Protocol::ieee80211VhtPhy.getId())
        return packet->peekAtFront<Ieee80211VhtPhyHeader>(length, flags);
    else
        throw cRuntimeError("Invalid IEEE 802.11 PHY protocol.");
}

} // namespace physicallayer

} // namespace inet
