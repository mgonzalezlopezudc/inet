//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211MacSapServiceTag_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211MldMac.h"

#include "inet/common/INETUtils.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/packet/Message.h"
#include "inet/common/packet/Packet.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/linklayer/common/UserPriorityTag_m.h"
#include "inet/linklayer/ieee80211/llc/IIeee80211Llc.h"
#include "inet/linklayer/ieee80211/llc/LlcProtocolTag_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211SubtypeTag_m.h"
#include "inet/linklayer/ieee80211/mac/Rx.h"
#include "inet/linklayer/ieee80211/mac/contract/IContention.h"
#include "inet/linklayer/ieee80211/mac/contract/IFrameSequence.h"
#include "inet/linklayer/ieee80211/mac/contract/IRx.h"
#include "inet/linklayer/ieee80211/mac/contract/ITx.h"
#include "inet/linklayer/ieee80211/mac/aggregation/MpduDeaggregation.h"
#include "inet/networklayer/contract/IInterfaceTable.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211ControlInfo_m.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeTxVector.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"
#include "inet/linklayer/ieee80211/twt/ITwtManager.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

Define_Module(Ieee80211Mac);

Ieee80211Mac::Ieee80211Mac()
{
}

Ieee80211Mac::~Ieee80211Mac()
{
    if (pendingRadioConfigMsg)
        delete pendingRadioConfigMsg;
}

void Ieee80211Mac::initialize(int stage)
{
    MacProtocolBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        modeSet = Ieee80211ModeSet::getModeSet(par("modeSet"));
        fcsMode = parseFcsMode(par("fcsMode"));
        mib.reference(this, "mibModule", true);
        mib->qos = par("qosStation");
    }
    else if (stage == INITSTAGE_LINK_LAYER) {
        cModule *llcModule = gate("upperLayerOut")->getNextGate()->getOwnerModule();
        llc = check_and_cast<IIeee80211Llc *>(llcModule);
        cModule *radioModule = gate("lowerLayerOut")->getNextGate()->getOwnerModule();
        radioModule->subscribe(IRadio::radioModeChangedSignal, this);
        radioModule->subscribe(IRadio::receptionStateChangedSignal, this);
        radioModule->subscribe(IRadio::transmissionStateChangedSignal, this);
        radioModule->subscribe(IRadio::receivedSignalPartChangedSignal, this);
        radioModule->subscribe(modesetChangedSignal, this);
        radio = check_and_cast<IRadio *>(radioModule);
        radioModePolicy = check_and_cast<IRadioModePolicy *>(getSubmodule("radioModePolicy"));
        auto modeSetProvider = dynamic_cast<IIeee80211ModeSetProvider *>(radio.get());
        if (modeSetProvider != nullptr && modeSetProvider->getModeSet() != nullptr)
            modeSet = modeSetProvider->getModeSet();
        ds = check_and_cast<IDs *>(getSubmodule("ds"));
        rx = check_and_cast<IRx *>(getSubmodule("rx"));
        tx = check_and_cast<ITx *>(getSubmodule("tx"));
        auto ccaProvider = dynamic_cast<IIeee80211CcaProvider *>(radio.get());
        if (ccaProvider != nullptr) {
            radioModule->subscribe(IIeee80211CcaProvider::ccaStateChangedSignal, this);
            rx->ccaStateChanged(ccaProvider->getCcaSnapshot());
        }
        emit(modesetChangedSignal, modeSet);
        initializeRadioMode();
        rx = check_and_cast<IRx *>(getSubmodule("rx"));
        tx = check_and_cast<ITx *>(getSubmodule("tx"));
        dcf = check_and_cast<Dcf *>(getSubmodule("dcf"));
        auto hcfModule = getSubmodule("hcf");
        hcf = dynamic_cast<IQosCoordinationFunction *>(hcfModule);
        if (hcfModule != nullptr && hcf == nullptr)
            throw cRuntimeError("hcf module '%s' does not implement IQosCoordinationFunction", hcfModule->getFullPath().c_str());
        dcf->setMgmtExchangeResultHandler(mgmtExchangeResultHandler);
        if (hcf != nullptr)
            hcf->setMgmtExchangeResultHandler(mgmtExchangeResultHandler);
        if (hasPar("twtModule")) {
            const char *twtModulePath = par("twtModule");
            if (*twtModulePath) {
                auto twtModule = getModuleByPath(twtModulePath);
                twtManager = dynamic_cast<ITwtManager *>(twtModule);
                if (twtModule != nullptr && twtManager == nullptr)
                    throw cRuntimeError("twtModule does not implement ITwtManager");
            }
        }
        if (mib->qos && !hcf)
            throw cRuntimeError("Missing hcf module, required for QoS");

        if (hasPar("mldMacModule")) {
            const char *mldMacPath = par("mldMacModule");
            if (mldMacPath && *mldMacPath) {
                mldMac = check_and_cast<Ieee80211MldMac *>(getModuleByPath(mldMacPath));
                mldMac->registerLinkMac(this);
            }
        }
    }
}

void Ieee80211Mac::setMgmtExchangeResultHandler(
        IIeee80211MgmtExchangeResultHandler *handler)
{
    mgmtExchangeResultHandler = handler;
    if (dcf != nullptr)
        dcf->setMgmtExchangeResultHandler(handler);
    if (hcf != nullptr)
        hcf->setMgmtExchangeResultHandler(handler);
}

void Ieee80211Mac::initializeRadioMode()
{
    radioModePolicy->initializeState(parseRadioMode(par("initialRadioMode")),
            radio->getRadioMode(), isUp());
    lastRequestedRadioMode = -1;
    auto snapshot = radioModePolicy->getSnapshot();
    // Bootstrap synchronously so initialization-stage users observe the
    // configured radio state; runtime arbitration remains command-based.
    if (snapshot.isLifecycleUp() &&
            snapshot.getDesiredRadioMode() != snapshot.getObservedRadioMode())
        radio->setRadioMode(snapshot.getDesiredRadioMode());
}

IRadio::RadioMode Ieee80211Mac::parseRadioMode(const char *radioMode) const
{
    if (!strcmp(radioMode, "off"))
        return IRadio::RADIO_MODE_OFF;
    else if (!strcmp(radioMode, "sleep"))
        return IRadio::RADIO_MODE_SLEEP;
    else if (!strcmp(radioMode, "receiver"))
        return IRadio::RADIO_MODE_RECEIVER;
    else if (!strcmp(radioMode, "transmitter"))
        return IRadio::RADIO_MODE_TRANSMITTER;
    else if (!strcmp(radioMode, "transceiver"))
        return IRadio::RADIO_MODE_TRANSCEIVER;
    else
        throw cRuntimeError("Unknown radio mode: %s", radioMode);
}

const MacAddress& Ieee80211Mac::isInterfaceRegistered()
{
//    if (!par("multiMac"))
//        return MacAddress::UNSPECIFIED_ADDRESS;
    IInterfaceTable *ift = findModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this);
    if (!ift)
        return MacAddress::UNSPECIFIED_ADDRESS;
    cModule *interfaceModule = findModuleUnderContainingNode(this);
    if (!interfaceModule)
        throw cRuntimeError("NIC module not found in the host");
    std::string interfaceName = utils::stripnonalnum(interfaceModule->getFullName());
    NetworkInterface *e = ift->findInterfaceByName(interfaceName.c_str());
    if (e)
        return e->getMacAddress();
    return MacAddress::UNSPECIFIED_ADDRESS;
}

void Ieee80211Mac::configureNetworkInterface()
{
    // TODO the mib module should use the mac address from NetworkInterface
    mib->address = networkInterface->getMacAddress();
    networkInterface->setMtu(par("mtu"));
    // capabilities
    networkInterface->setBroadcast(true);
    networkInterface->setMulticast(true);
    networkInterface->setPointToPoint(false);
}

void Ieee80211Mac::handleMessageWhenUp(cMessage *message)
{
    if (message->arrivedOn("mgmtIn")) {
        if (!message->isPacket())
            handleUpperCommand(message);
        else
            handleMgmtPacket(check_and_cast<Packet *>(message));
    }
    else
        LayeredProtocolBase::handleMessageWhenUp(message);
}

void Ieee80211Mac::handleSelfMessage(cMessage *msg)
{
    ASSERT(false);
}

void Ieee80211Mac::handleMgmtPacket(Packet *packet)
{
    const auto& first = packet->peekAtFront();
    if (auto header = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(first)) {
        packet->insertAtBack(makeShared<Ieee80211MacTrailer>());
        processUpperFrame(packet, header);
        return;
    }
    if (auto header = dynamicPtrCast<const Ieee80211MacHeader>(first)) {
        packet->insertAtBack(makeShared<Ieee80211MacTrailer>());
        sendDownFrame(packet);
        return;
    }
    const auto& header = makeShared<Ieee80211MgmtHeader>();
    header->setType((Ieee80211FrameType)packet->getTag<Ieee80211SubtypeReq>()->getSubtype());
    header->setReceiverAddress(packet->getTag<MacAddressReq>()->getDestAddress());
    if (mib->mode == Ieee80211Mib::INFRASTRUCTURE && mib->getStationType() == Ieee80211Mib::ACCESS_POINT)
        header->setAddress3(mib->getBssid());
    packet->insertAtFront(header);
    packet->insertAtBack(makeShared<Ieee80211MacTrailer>());
    processUpperFrame(packet, header);
}

void Ieee80211Mac::handleUpperPacket(Packet *packet)
{
    if (mib->mode == Ieee80211Mib::INFRASTRUCTURE && mib->getStationType() == Ieee80211Mib::STATION && !mib->isAssociated()) {
        EV << "STA is not associated with an access point, discarding packet " << packet << "\n";
        PacketDropDetails details;
        details.setReason(OTHER_PACKET_DROP);
        emit(packetDroppedSignal, packet, &details);
        delete packet;
        return;
    }
    encapsulate(packet);
    const auto& header = packet->peekAtFront<Ieee80211DataOrMgmtHeader>();
    if (mib->mode == Ieee80211Mib::INFRASTRUCTURE && mib->getStationType() == Ieee80211Mib::ACCESS_POINT) {
        auto receiverAddress = header->getReceiverAddress();
        if (!receiverAddress.isMulticast()) {
            if (!mib->isPeerAssociated(receiverAddress)) {
                EV << "STA with MAC address " << receiverAddress << " not associated with this AP, dropping frame\n";
                PacketDropDetails details;
                details.setReason(OTHER_PACKET_DROP);
                emit(packetDroppedSignal, packet, &details);
                delete packet;
                return;
            }
        }
    }
    processUpperFrame(packet, header);
}

void Ieee80211Mac::handleLowerPacket(Packet *packet)
{
    const bool keepHtAmpduIntact =
            packet->getDataLength() > b(0) &&
            dynamicPtrCast<const Ieee80211MpduSubframeHeader>(
                    packet->peekAtFront()) != nullptr &&
            hcf != nullptr && hcf->isAllowedToProcessIntactHtAmpdu();
    auto heRxVector = packet->findTag<Ieee80211HeRxVectorInd>();
    auto heRecipientContext =
            packet->findTag<Ieee80211HeTbRecipientContextInd>();
    const bool keepHeTbAmpduIntact =
            packet->getDataLength() > b(0) &&
            dynamicPtrCast<const Ieee80211MpduSubframeHeader>(
                    packet->peekAtFront()) != nullptr &&
            ((heRxVector != nullptr && heRxVector->getRxVector() != nullptr &&
              heRxVector->getRxVector()->getCommon().getPpduFormat() ==
                      HE_TRIGGER_BASED_UPLINK &&
              heRecipientContext != nullptr &&
              heRecipientContext->getRecipientParameters() != nullptr) ||
             packet->findTag<Ieee80211HeTriggerCorrelationTag>() != nullptr ||
             (hcf != nullptr && hcf->isExpectingIntactAmpduResponse()));
    const bool keepAmpduIntact = keepHtAmpduIntact || keepHeTbAmpduIntact;
    if (packet->getDataLength() > b(0)) {
        const auto& frontChunk = packet->peekAtFront();
        if (dynamicPtrCast<const Ieee80211MpduSubframeHeader>(frontChunk) != nullptr &&
                packet->findTag<Ieee80211MpduReceiveInd>() == nullptr &&
                !keepAmpduIntact) {
            MpduDeaggregation deaggregation;
            auto frames = deaggregation.deaggregateFrame(packet);
            for (auto frame : *frames)
                handleLowerPacket(frame);
            delete frames;
            return;
        }
    }
    if (auto legacyPreambleInd = packet->findTag<Ieee80211LegacyPreambleInd>()) {
        rx->legacySignalReceived(legacyPreambleInd->getDurationField());
        if (hcf != nullptr)
            hcf->legacyPreambleReceived(packet);
        delete packet;
        return;
    }
    auto aggregateContext = keepAmpduIntact ?
            AggregateReceptionContext::INTACT_AMPDU :
            AggregateReceptionContext::ORDINARY_FRAME;
    if (rx->lowerFrameReceived(packet, aggregateContext)) {
        if (packet->getDataLength() > b(0) &&
                dynamicPtrCast<const Ieee80211MpduSubframeHeader>(packet->peekAtFront()) != nullptr &&
                (packet->findTag<Ieee80211MpduReceiveInd>() != nullptr ||
                        keepAmpduIntact)) {
            // Packet-level reception may have produced ordered delimiter/MPDU
            // outcomes. The explicitly enabled HT implicit-BlockAck path also
            // needs the intact A-MPDU so Ack Policy 00 is interpreted once for
            // the aggregate instead of once per deaggregated MPDU.
            processLowerFrame(packet, nullptr);
            return;
        }
        if (packet->getDataLength() == b(0)) {
            auto ndpIndication = packet->findTag<Ieee80211NdpInd>();
            auto rxVectorInd = packet->findTag<Ieee80211HeRxVectorInd>();
            auto recipientContext = packet->findTag<Ieee80211HeTbRecipientContextInd>();
            const bool nfrpFeedbackNdp = rxVectorInd != nullptr &&
                    rxVectorInd->getRxVector() != nullptr &&
                    rxVectorInd->getRxVector()->getCommon().getPpduFormat() ==
                            HE_TRIGGER_BASED_UPLINK &&
                    recipientContext != nullptr &&
                    recipientContext->getRecipientParameters() != nullptr &&
                    recipientContext->getRecipientParameters()->ndpFeedbackReport;
            if ((ndpIndication != nullptr || nfrpFeedbackNdp) && mib->qos) {
                // Sounding and NFRP feedback NDPs are intentionally headerless.
                // Route their format-neutral indication (and HE NFRP context)
                // to HCF before the generic empty-packet discard path.
                processLowerFrame(packet, nullptr);
                return;
            }
            take(packet);
            delete packet;
            return;
        }
        auto header = packet->peekAtFront<Ieee80211MacHeader>();
        if (mib->getStationType() == Ieee80211Mib::ACCESS_POINT) {
            if (auto twoAddressHeader = dynamicPtrCast<const Ieee80211TwoAddressHeader>(header)) {
                if (auto signalPower = packet->findTag<SignalPowerInd>())
                    mib->updateStationReceivedPower(twoAddressHeader->getTransmitterAddress(), signalPower->getPower());
            }
        }
        processLowerFrame(packet, header);
    }
    else { // corrupted frame received
        if (mib->qos)
            hcf->corruptedFrameReceived();
        else
            dcf->corruptedFrameReceived();
    }
}

void Ieee80211Mac::handleUpperCommand(cMessage *msg)
{
    if (msg->getKind() == RADIO_C_CONFIGURE) {
        EV_DEBUG << "Passing on command " << msg->getName() << " to physical layer\n";
        auto configureCommand = check_and_cast<ConfigureRadioCommand *>(msg->getControlInfo());
        int externalRadioMode = configureCommand->getRadioMode();
        if (externalRadioMode != -1)
            configureCommand->setRadioMode(-1);
        if (pendingRadioConfigMsg != nullptr) {
            // merge contents of the old command into the new one, then delete it
            Ieee80211ConfigureRadioCommand *oldConfigureCommand = check_and_cast<Ieee80211ConfigureRadioCommand *>(pendingRadioConfigMsg->getControlInfo());
            Ieee80211ConfigureRadioCommand *newConfigureCommand = check_and_cast<Ieee80211ConfigureRadioCommand *>(msg->getControlInfo());
            if (newConfigureCommand->getChannelNumber() == -1 && oldConfigureCommand->getChannelNumber() != -1)
                newConfigureCommand->setChannelNumber(oldConfigureCommand->getChannelNumber());
            if (std::isnan(newConfigureCommand->getBitrate().get<bps>()) && !std::isnan(oldConfigureCommand->getBitrate().get<bps>()))
                newConfigureCommand->setBitrate(oldConfigureCommand->getBitrate());
            delete pendingRadioConfigMsg;
            pendingRadioConfigMsg = nullptr;
        }
        if (externalRadioMode != -1)
            pendingExternalRadioMode = externalRadioMode;
        pendingRadioConfigMsg = msg;

        if (rx->isMediumFree()) { // TODO this should be just the physical channel sense!!!!
            EV_DEBUG << "Sending it down immediately\n";
//            PhyControlInfo *phyControlInfo = dynamic_cast<PhyControlInfo *>(msg->getControlInfo());
//            if (phyControlInfo)
//                phyControlInfo->setAdaptiveSensitivity(true);
            // end dynamic power
            sendDownPendingRadioConfigMsg();
        }
        else {
            // TODO waiting potentially indefinitely?! wtf?!
            EV_DEBUG << "Delaying " << msg->getName() << " until next IDLE or DEFER state\n";
            pendingRadioConfigMediumReleased = false;
        }
    }
    else {
        throw cRuntimeError("Unrecognized command from mgmt layer: (%s)%s msgkind=%d", msg->getClassName(), msg->getName(), msg->getKind());
    }
}

void Ieee80211Mac::encapsulate(Packet *packet)
{
    packet->addTagIfAbsent<LlcProtocolTag>()->setProtocol(packet->getTag<PacketProtocolTag>()->getProtocol());
    auto macAddressReq = packet->getTag<MacAddressReq>();
    auto destAddress = macAddressReq->getDestAddress();
    const auto& header = makeShared<Ieee80211DataHeader>();
    header->setTransmitterAddress(mib->address);
    if (mib->mode == Ieee80211Mib::INDEPENDENT)
        header->setReceiverAddress(destAddress);
    else if (mib->mode == Ieee80211Mib::INFRASTRUCTURE) {
        if (mib->getStationType() == Ieee80211Mib::ACCESS_POINT) {
            header->setFromDS(true);
            header->setAddress3(mib->address);
            header->setReceiverAddress(destAddress);
        }
        else if (mib->getStationType() == Ieee80211Mib::STATION) {
            header->setToDS(true);
            header->setReceiverAddress(mib->getBssid());
            header->setAddress3(destAddress);
        }
        else
            throw cRuntimeError("Unknown station type");
    }
    else
        throw cRuntimeError("Unknown mode");
    if (auto userPriorityReq = packet->findTag<UserPriorityReq>()) {
        // make it a QoS frame, and set TID
        header->setType(ST_DATA_WITH_QOS);
        header->addChunkLength(QOSCONTROL_PART_LENGTH);
        header->setTid(userPriorityReq->getUserPriority());
    }
    packet->insertAtFront(header);
    packet->insertAtBack(makeShared<Ieee80211MacTrailer>());
    auto packetProtocolTag = packet->addTagIfAbsent<PacketProtocolTag>();
    packetProtocolTag->setProtocol(&Protocol::ieee80211Mac);
}

void Ieee80211Mac::decapsulate(Packet *packet)
{
    const auto& header = packet->popAtFront<Ieee80211DataOrMgmtHeader>();
    auto packetProtocolTag = packet->addTagIfAbsent<PacketProtocolTag>();
    if (dynamicPtrCast<const Ieee80211DataHeader>(header))
        packetProtocolTag->setProtocol(llc->getProtocol());
    else if (dynamicPtrCast<const Ieee80211MgmtHeader>(header))
        packetProtocolTag->setProtocol(&Protocol::ieee80211Mgmt);
    auto macAddressInd = packet->addTagIfAbsent<MacAddressInd>();
    if (mib->mode == Ieee80211Mib::INDEPENDENT) {
        macAddressInd->setSrcAddress(header->getTransmitterAddress());
        macAddressInd->setDestAddress(header->getReceiverAddress());
    }
    else if (mib->mode == Ieee80211Mib::INFRASTRUCTURE) {
        if (mib->getStationType() == Ieee80211Mib::ACCESS_POINT) {
            macAddressInd->setSrcAddress(header->getTransmitterAddress());
            macAddressInd->setDestAddress(header->getAddress3());
        }
        else if (mib->getStationType() == Ieee80211Mib::STATION) {
            macAddressInd->setSrcAddress(header->getAddress3());
            macAddressInd->setDestAddress(header->getReceiverAddress());
        }
        else
            throw cRuntimeError("Unknown station type");
    }
    else
        throw cRuntimeError("Unknown mode");
    if (header->getType() == ST_DATA_WITH_QOS) {
        auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header);
        int tid = dataHeader->getTid();
        if (tid < 8)
            packet->addTagIfAbsent<UserPriorityInd>()->setUserPriority(tid);
    }
    packet->addTagIfAbsent<InterfaceInd>()->setInterfaceId(networkInterface->getInterfaceId());
    packet->popAtBack<Ieee80211MacTrailer>(B(4));
}

void Ieee80211Mac::receiveSignal(cComponent *source, simsignal_t signalID, intval_t value, cObject *details)
{
    Enter_Method("%s", cComponent::getSignalName(signalID));

    if (signalID == IRadio::receptionStateChangedSignal) {
        rx->receptionStateChanged(static_cast<IRadio::ReceptionState>(value));
    }
    else if (signalID == IRadio::radioModeChangedSignal) {
        auto radioMode = static_cast<IRadio::RadioMode>(value);
        radioModePolicy->setObservedRadioMode(radioMode);
        if (radioMode == IRadio::RADIO_MODE_SWITCHING)
            return;
        lastRequestedRadioMode = -1;
        bool applyDeferredRadioMode = radioModeIntentDeferred;
        radioModeIntentDeferred = false;
        deferredRadioModeGeneration = 0;
        if (applyDeferredRadioMode)
            applyDesiredRadioMode();
        releasePendingRadioConfigMsg();
    }
    else if (signalID == IRadio::transmissionStateChangedSignal) {
        auto oldTransmissionState = transmissionState;
        transmissionState = static_cast<IRadio::TransmissionState>(value);
        if (transmissionState == IRadio::TRANSMISSION_STATE_TRANSMITTING) {
            lastTxStart = simTime();
            lastTxEnd = -1;
        }
        else if (oldTransmissionState == IRadio::TRANSMISSION_STATE_TRANSMITTING) {
            lastTxEnd = simTime();
        }

        bool transmissionFinished = (oldTransmissionState == IRadio::TRANSMISSION_STATE_TRANSMITTING && transmissionState == IRadio::TRANSMISSION_STATE_IDLE);
        if (transmissionFinished) {
            radioModePolicy->setTransmissionActive(false);
            tx->radioTransmissionFinished();
            applyDesiredRadioMode();
        }
        rx->transmissionStateChanged(transmissionState);

        if (transmissionFinished) {
            bool completedTwtPsPoll = twtPsPollTransmissionActive;
            if (completedTwtPsPoll)
                twtPsPollTransmissionActive = false;
            if (!pendingTwtPsPollPeers.empty())
                sendNextTwtPsPoll();
            else if (completedTwtPsPoll && twtManager != nullptr && twtManager->isStationAwake())
                twtServicePeriodChanged();
        }

        if (mldMac != nullptr) {
            mldMac->linkTransmissionStateChanged(this, transmissionState);
        }
    }
    else if (signalID == IRadio::receivedSignalPartChangedSignal) {
        rx->receivedSignalPartChanged(static_cast<IRadioSignal::SignalPart>(value));
    }
}

void Ieee80211Mac::receiveSignal(cComponent *source, simsignal_t signalID, cObject *value, cObject *details)
{
    Enter_Method("%s", cComponent::getSignalName(signalID));

    if (signalID == modesetChangedSignal) {
        auto updatedModeSet = check_and_cast<Ieee80211ModeSet *>(value);
        auto modeSetProvider = dynamic_cast<IIeee80211ModeSetProvider *>(radio.get());
        if (modeSetProvider == nullptr || updatedModeSet != modeSetProvider->getModeSet())
            throw cRuntimeError("Radio published an inconsistent 802.11 mode profile");
        modeSet = updatedModeSet;
    }
    else if (signalID == IIeee80211CcaProvider::ccaStateChangedSignal)
        rx->ccaStateChanged(*check_and_cast<Ieee80211CcaSnapshot *>(value));
}

void Ieee80211Mac::sendRadioModeCommand(IRadio::RadioMode radioMode)
{
    auto configureCommand = new ConfigureRadioCommand();
    configureCommand->setRadioMode(radioMode);
    auto request = new Request("configureRadioMode", RADIO_C_CONFIGURE);
    request->setControlInfo(configureCommand);
    sendDown(request);
}

void Ieee80211Mac::applyDesiredRadioMode()
{
    if (applyingRadioMode) {
        radioModeApplyPending = true;
        return;
    }
    applyingRadioMode = true;
    try {
        do {
            radioModeApplyPending = false;
            auto snapshot = radioModePolicy->getSnapshot();
            auto desiredRadioMode = snapshot.getDesiredRadioMode();
            if (snapshot.getObservedRadioMode() == IRadio::RADIO_MODE_SWITCHING) {
                radioModeIntentDeferred = true;
                deferredRadioModeGeneration = snapshot.getGeneration();
            }
            else if (snapshot.getObservedRadioMode() == desiredRadioMode) {
                lastRequestedRadioMode = -1;
                radioModeIntentDeferred = false;
                deferredRadioModeGeneration = 0;
            }
            else if (lastRequestedRadioMode != desiredRadioMode) {
                lastRequestedRadioMode = desiredRadioMode;
                radioModeIntentDeferred = false;
                deferredRadioModeGeneration = 0;
                sendRadioModeCommand(desiredRadioMode);
            }
        } while (radioModeApplyPending);
        applyingRadioMode = false;
    }
    catch (...) {
        applyingRadioMode = false;
        throw;
    }
}

void Ieee80211Mac::sendUp(cMessage *msg)
{
    Enter_Method("sendUp(\"%s\")", msg->getName());
    take(msg);
    MacProtocolBase::sendUp(msg);
}

void Ieee80211Mac::sendUpFrame(Packet *frame)
{
    Enter_Method("sendUpFrame(\"%s\")", frame->getName());
    take(frame);
    const auto& header = frame->peekAtFront<Ieee80211DataOrMgmtHeader>();
    decapsulate(frame);
    if (!(header->getType() & 0x30))
        send(frame, "mgmtOut");
    else
        ds->processDataFrame(frame, dynamicPtrCast<const Ieee80211DataHeader>(header));
}

void Ieee80211Mac::sendDownFrame(Packet *frame)
{
    Enter_Method("sendDownFrame(\"%s\")", frame->getName());
    take(frame);
    radioModePolicy->setTransmissionActive(true);
    applyDesiredRadioMode();
    // MAC-SAP provenance is node-local metadata. Tx passes a duplicate here,
    // so stripping it at the MAC/PHY boundary preserves the HCF-owned queued,
    // in-progress, and completion copies.
    auto serviceTags = frame->getAllRegionTags<Ieee80211MacSapServiceTag>(
            frame->getFrontOffset(), frame->getDataLength());
    for (const auto& region : serviceTags)
        frame->removeRegionTagIfPresent<Ieee80211MacSapServiceTag>(
                region.getOffset(), region.getLength());
    frame->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::ieee80211Mac);
    sendDown(frame);
}

void Ieee80211Mac::setTwtRadioAwake(bool awake)
{
    Enter_Method("setTwtRadioAwake");
    radioModePolicy->setTwtAwake(awake);
    applyDesiredRadioMode();
}

void Ieee80211Mac::twtServicePeriodChanged()
{
    Enter_Method("twtServicePeriodChanged");
    if (hcf != nullptr)
        hcf->twtServicePeriodChanged();
}

bool Ieee80211Mac::isTwtPeerEligible(const MacAddress& peer) const
{
    return twtManager == nullptr || twtManager->isPeerEligible(peer);
}

void Ieee80211Mac::sendTwtPsPoll(const MacAddress& peer)
{
    Enter_Method("sendTwtPsPoll");
    pendingTwtPsPollPeers.push_back(peer);
    sendNextTwtPsPoll();
}

void Ieee80211Mac::sendNextTwtPsPoll()
{
    Enter_Method("sendNextTwtPsPoll");
    if (twtPsPollTransmissionActive || pendingTwtPsPollPeers.empty() || transmissionState == IRadio::TRANSMISSION_STATE_TRANSMITTING)
        return;

    auto peer = pendingTwtPsPollPeers.front();
    pendingTwtPsPollPeers.pop_front();
    auto header = makeShared<Ieee80211PsPollFrame>();
    header->setAID(mib->getLocalAssociationId());
    header->setReceiverAddress(peer);
    header->setTransmitterAddress(mib->address);
    auto packet = new Packet("TwtPsPoll", header);
    auto trailer = makeShared<Ieee80211MacTrailer>();
    trailer->setFcsMode(getFcsMode());
    packet->insertAtBack(trailer);

    // Set a basic/mandatory rate for the control frame transmission
    if (modeSet != nullptr) {
        if (auto mode = modeSet->getSlowestMandatoryMode())
            packet->addTagIfAbsent<Ieee80211ModeReq>()->setMode(mode);
    }

    twtPsPollTransmissionActive = true;
    sendDownFrame(packet);
}

void Ieee80211Mac::sendDownPendingRadioConfigMsg()
{
    pendingRadioConfigMediumReleased = true;
    releasePendingRadioConfigMsg();
}

void Ieee80211Mac::discardPendingRadioConfigMsg()
{
    delete pendingRadioConfigMsg;
    pendingRadioConfigMsg = nullptr;
    pendingExternalRadioMode = -1;
    pendingRadioConfigMediumReleased = false;
}

void Ieee80211Mac::releasePendingRadioConfigMsg()
{
    if (pendingRadioConfigMsg == nullptr || !pendingRadioConfigMediumReleased)
        return;
    if (!radioModePolicy->getSnapshot().isLifecycleUp()) {
        // Configuration transactions are observations of current upper-layer
        // intent. Replaying a channel or bitrate command after restart would
        // apply stale state to a new lifecycle, so discard it on shutdown.
        discardPendingRadioConfigMsg();
        return;
    }
    if (pendingExternalRadioMode != -1) {
        int externalRadioMode = pendingExternalRadioMode;
        pendingExternalRadioMode = -1;
        radioModePolicy->setExternalRadioMode(static_cast<IRadio::RadioMode>(externalRadioMode));
        applyDesiredRadioMode();
    }
    // A synchronous terminal radio-mode signal may have reentered this method
    // and released the ordinary configuration while applyDesiredRadioMode()
    // was sending its command.
    if (pendingRadioConfigMsg == nullptr)
        return;
    if (radioModePolicy->getSnapshot().getObservedRadioMode() == IRadio::RADIO_MODE_SWITCHING)
        return;
    auto message = pendingRadioConfigMsg;
    pendingRadioConfigMsg = nullptr;
    pendingRadioConfigMediumReleased = false;
    sendDown(message);
}

void Ieee80211Mac::processUpperFrame(Packet *packet, const Ptr<const Ieee80211DataOrMgmtHeader>& header)
{
    Enter_Method("processUpperFrame(\"%s\")", packet->getName());
    take(packet);
    EV_INFO << "Frame " << packet << " received from higher layer, receiver = " << header->getReceiverAddress() << "\n";
    ASSERT(!header->getReceiverAddress().isUnspecified());
    if (mib->qos)
        hcf->processUpperFrame(packet, header);
    else
        dcf->processUpperFrame(packet, header);
}

void Ieee80211Mac::processLowerFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    Enter_Method("processLowerFrame(\"%s\")", packet->getName());
    take(packet);
    if (mib->qos)
        hcf->processLowerFrame(packet, header);
    else
        // TODO what if the received frame is ST_DATA_WITH_QOS? drop?
        dcf->processLowerFrame(packet, header);
}

// FIXME
void Ieee80211Mac::handleStartOperation(LifecycleOperation *operation)
{
    if (!operation)
        return; // do nothing when called from initialize()

    radioModePolicy->setLifecycleUp(true);
    lastRequestedRadioMode = -1;
    applyDesiredRadioMode();
}

// FIXME
void Ieee80211Mac::handleStopOperation(LifecycleOperation *operation)
{
    radioModePolicy->setLifecycleUp(false);
    discardPendingRadioConfigMsg();
    lastRequestedRadioMode = -1;
    applyDesiredRadioMode();
}

// FIXME
void Ieee80211Mac::handleCrashOperation(LifecycleOperation *operation)
{
    radioModePolicy->setLifecycleUp(false);
    discardPendingRadioConfigMsg();
    lastRequestedRadioMode = -1;
    applyDesiredRadioMode();
}

void Ieee80211Mac::invalidatePeerDerivedState(const MacAddress& peer)
{
    if (hcf != nullptr)
        hcf->invalidatePeerDerivedState(peer);
}

bool Ieee80211Mac::isTransmittingDuring(simtime_t rxStart, simtime_t rxEnd) const
{
    if (transmissionState == physicallayer::IRadio::TRANSMISSION_STATE_TRANSMITTING) {
        simtime_t txStart = lastTxStart;
        simtime_t txEnd = simTime();
        if (txStart < rxEnd && txEnd > rxStart)
            return true;
    }
    if (lastTxStart != -1 && lastTxEnd != -1) {
        simtime_t txStart = lastTxStart;
        simtime_t txEnd = lastTxEnd;
        if (txStart < rxEnd && txEnd > rxStart)
            return true;
    }
    return false;
}

void Ieee80211Mac::otherLinkTransmissionStateChanged()
{
    if (auto concreteRx = dynamic_cast<Rx *>(rx.get())) {
        concreteRx->recomputeMediumFree();
    }
}

} // namespace ieee80211
} // namespace inet
