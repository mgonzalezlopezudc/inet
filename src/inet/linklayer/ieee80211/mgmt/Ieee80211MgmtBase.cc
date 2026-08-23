//
// Copyright (C) 2006 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtBase.h"

#include <algorithm>

#include "inet/common/INETUtils.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/lifecycle/LifecycleOperation.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/lifecycle/NodeStatus.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211SubtypeTag_m.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/networklayer/common/NetworkInterface.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211CapabilityElements.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"

namespace inet {

namespace ieee80211 {

using namespace inet::physicallayer;

namespace {

bool applyType0OperatingMode(uint8_t operatingMode, Ieee80211PeerLdpcStatus& status)
{
    if ((operatingMode & 0x80) != 0)
        return false;

    int maximumBandwidthMhz = -1;
    switch (operatingMode & 0x07) {
        case 0: maximumBandwidthMhz = 20; break;
        case 1: maximumBandwidthMhz = 40; break;
        case 2: maximumBandwidthMhz = 80; break;
        case 6:
            if (status.maximumVhtRxBandwidthMhz >= 160)
                maximumBandwidthMhz = 160;
            break;
    }
    if (maximumBandwidthMhz == -1)
        return false;
    status.operatingModeType0Valid = true;
    status.operatingModeMaximumBandwidthMhz = maximumBandwidthMhz;
    status.operatingModeMaximumSpatialStreams = ((operatingMode >> 4) & 0x07) + 1;
    return true;
}

void applyOperatingMode(uint8_t operatingMode, Ieee80211PeerLdpcStatus& status)
{
    // IEEE Std 802.11-2024, 9.4.1.51, Tables 9-110 and 9-111. No-LDPC is
    // the latest-any-type preference. Type-1 and reserved tuples do not erase
    // the independently retained latest valid Type-0 width/NSS constraint.
    status.hasOperatingMode = true;
    status.noLdpcPreferred = (operatingMode & 0x08) != 0;
    applyType0OperatingMode(operatingMode, status);
}

int getMaximumSupportedSpatialStreams(uint16_t mcsMap)
{
    int result = 0;
    for (int spatialStream = 1; spatialStream <= 8; spatialStream++)
        if (((mcsMap >> (2 * (spatialStream - 1))) & 0x03) != 0x03)
            result = spatialStream;
    return result;
}

} // namespace

void Ieee80211MgmtBase::initialize(int stage)
{
    OperationalBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        htLdpcRxSupported = hasPar("htLdpcRxSupported") && par("htLdpcRxSupported").boolValue();
        vhtLdpcRxSupported = hasPar("vhtLdpcRxSupported") && par("vhtLdpcRxSupported").boolValue();
        mib.reference(this, "mibModule", true);
        interfaceTable.reference(this, "interfaceTableModule", true);
        myIface = getContainingNicModule(this);
        numMgmtFramesReceived = 0;
        numMgmtFramesDropped = 0;
        getContainingNicModule(this)->subscribe(modesetChangedSignal, this);
        WATCH(numMgmtFramesReceived);
        WATCH(numMgmtFramesDropped);
    }
    else if (stage == INITSTAGE_LINK_LAYER) {
        auto radioModule = getModuleFromPar<cModule>(par("radioModule"), this);
        auto radio = check_and_cast<physicallayer::IRadio *>(radioModule);
        maximumSpatialStreams = radio->getAntenna()->getNumAntennas();
        if (maximumSpatialStreams < 1 || maximumSpatialStreams > 8)
            throw cRuntimeError("IEEE 802.11 HT/VHT capability generation supports 1 to 8 radio antennas");
    }
}

void Ieee80211MgmtBase::setLocalLdpcCapabilities(const Ptr<Ieee80211MgmtFrame>& frame) const
{
    populateIeee80211CapabilityElements(frame, modeSet, maximumSpatialStreams,
            htLdpcRxSupported, vhtLdpcRxSupported);
}

Ieee80211PeerLdpcStatus Ieee80211MgmtBase::mergeLdpcCapabilities(
        const Ieee80211PeerLdpcStatus& previous, const Ieee80211MgmtFrame& frame)
{
    Ieee80211PeerLdpcStatus result = previous;
    if (frame.getHtCapabilitiesPresent()) {
        result.htRxLdpc = (frame.getHtCapabilities(0) & 0x01) != 0 ? Ieee80211CapabilityStatus::SUPPORTED : Ieee80211CapabilityStatus::UNSUPPORTED;
        result.htRxMcsSetKnown = true;
        for (size_t i = 0; i < result.htRxMcsSet.size(); i++)
            result.htRxMcsSet[i] = frame.getHtCapabilities(3 + i);
        result.maximumHtRxBandwidthMhz = (frame.getHtCapabilities(0) & 0x02) != 0 ? 40 : 20;
    }
    if (frame.getVhtCapabilitiesPresent())
        result.vhtRxLdpc = (frame.getVhtCapabilities(0) & 0x10) != 0 ? Ieee80211CapabilityStatus::SUPPORTED : Ieee80211CapabilityStatus::UNSUPPORTED;
    if (frame.getVhtCapabilitiesPresent()) {
        result.vhtRxMcsMapKnown = true;
        result.vhtRxMcsMap = frame.getVhtCapabilities(4) | (frame.getVhtCapabilities(5) << 8);
        result.vhtTxMcsMapKnown = true;
        result.vhtTxMcsMap = frame.getVhtCapabilities(8) | (frame.getVhtCapabilities(9) << 8);
        int supportedChannelWidthSet = (frame.getVhtCapabilities(0) >> 2) & 0x03;
        result.maximumVhtRxBandwidthMhz = supportedChannelWidthSet == 0 ? 80 :
                supportedChannelWidthSet == 1 || supportedChannelWidthSet == 2 ? 160 : -1;
    }
    if (frame.getExtendedCapabilitiesPresent())
        result.operatingModeNotification = frame.getExtendedCapabilitiesArraySize() >= 8 &&
                (frame.getExtendedCapabilities(7) & 0x40) != 0 ? Ieee80211CapabilityStatus::SUPPORTED : Ieee80211CapabilityStatus::UNSUPPORTED;
    // IEEE Std 802.11-2024 Clauses 10.15 and 11.40 use the most recently
    // received Operating Mode field. A later frame without the optional
    // element does not revoke the stored No LDPC preference.
    if (frame.getOperatingModePresent()) {
        applyOperatingMode(frame.getOperatingMode(), result);
    }
    return result;
}

Ieee80211PeerLdpcStatus Ieee80211MgmtBase::mergePeerLdpcCapabilities(const MacAddress& peer,
        const Ieee80211PeerLdpcStatus& previous, const Ieee80211MgmtFrame& frame)
{
    auto result = mergeLdpcCapabilities(previous, frame);
    if (frame.getOperatingModePresent()) {
        latestPeerOperatingModes[peer] = frame.getOperatingMode();
        Ieee80211PeerLdpcStatus parsed = result;
        parsed.operatingModeType0Valid = false;
        if (applyType0OperatingMode(frame.getOperatingMode(), parsed))
            latestPeerType0OperatingModes[peer] = frame.getOperatingMode();
    }
    return result;
}

Ieee80211PeerLdpcStatus Ieee80211MgmtBase::applyLatestPeerOperatingMode(const MacAddress& peer,
        const Ieee80211PeerLdpcStatus& status) const
{
    auto result = status;
    auto type0It = latestPeerType0OperatingModes.find(peer);
    if (type0It != latestPeerType0OperatingModes.end())
        applyType0OperatingMode(type0It->second, result);
    auto it = latestPeerOperatingModes.find(peer);
    if (it != latestPeerOperatingModes.end())
        applyOperatingMode(it->second, result);
    return result;
}

void Ieee80211MgmtBase::updateLatestPeerOperatingMode(const MacAddress& peer, uint8_t operatingMode)
{
    latestPeerOperatingModes[peer] = operatingMode;
    auto parsed = getPeerLdpcStatus(peer);
    parsed.operatingModeType0Valid = false;
    if (applyType0OperatingMode(operatingMode, parsed))
        latestPeerType0OperatingModes[peer] = operatingMode;
}

Ieee80211PeerLdpcStatus Ieee80211MgmtBase::getPeerLdpcStatus(const MacAddress& peer) const
{
    return applyLatestPeerOperatingMode(peer, {});
}

Ieee80211IntendedReceiverSet Ieee80211MgmtBase::resolveIntendedReceivers(const MacAddress& receiverAddress) const
{
    if (receiverAddress.isMulticast())
        return {false, {}};
    else
        return {true, {receiverAddress}};
}

Ieee80211VhtSigAParameters Ieee80211MgmtBase::getVhtSigAParameters(const MacAddress& receiverAddress) const
{
    return {};
}

void Ieee80211MgmtBase::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details)
{
    Enter_Method("%s", cComponent::getSignalName(signalID));

    if (signalID == modesetChangedSignal) {
        modeSet = check_and_cast<physicallayer::Ieee80211ModeSet *>(obj);
        supportedRates.numRates = std::min(8, modeSet->getNumModes());
        int rateIndex = 0;
        for (int i = 0; i < supportedRates.numRates; i++)
            if (modeSet->isMandatory(i))
                supportedRates.rate[rateIndex++] = modeSet->getMode(i)->getDataMode()->getNetBitrate().get<Mbps>();
    }
}

void Ieee80211MgmtBase::handleMessageWhenUp(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        // process timers
        EV << "Timer expired: " << msg << "\n";
        handleTimer(msg);
    }
    else if (msg->arrivedOn("macIn")) {
        // process incoming frame
        EV << "Frame arrived from MAC: " << msg << "\n";
        auto packet = check_and_cast<Packet *>(msg);
        processFrameFromMac(packet);
    }
    else if (msg->arrivedOn("agentIn")) {
        // process command from agent
        EV << "Command arrived from agent: " << msg << "\n";
        int msgkind = msg->getKind();
        cObject *ctrl = msg->removeControlInfo();
        delete msg;

        handleCommand(msgkind, ctrl);
    }
    else
        throw cRuntimeError("Unknown message");
}

void Ieee80211MgmtBase::processFrameFromMac(Packet *packet)
{
    const auto& macProtocolHeader = packet->getTag<MacProtocolInd>()->getMacProtocolHeader();
    const auto& header = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(macProtocolHeader);
    if (header == nullptr)
        throw cRuntimeError("Missing IEEE 802.11 MAC header indication on packet '%s'", packet->getName());
    processFrame(packet, header);
}

void Ieee80211MgmtBase::sendDown(Packet *frame)
{
    ASSERT(isUp());
    frame->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::ieee80211Mgmt);
    send(frame, "macOut");
}

void Ieee80211MgmtBase::sendOperatingModeNotification(const MacAddress& receiverAddress, uint8_t operatingMode)
{
    if (receiverAddress.isMulticast())
        throw cRuntimeError("Cannot validate an IEEE 802.11 Operating Mode Notification for an unresolved group receiver set");
    auto peerStatus = getPeerLdpcStatus(receiverAddress);
    if (peerStatus.operatingModeNotification != Ieee80211CapabilityStatus::SUPPORTED)
        throw cRuntimeError("Cannot send an individually addressed IEEE 802.11 Operating Mode field to a peer that has not advertised Operating Mode Notification capability");
    if ((operatingMode & 0x80) != 0)
        throw cRuntimeError("IEEE 802.11 Rx NSS Type 1 Operating Mode is outside the supported VHT-SU scope");
    int encodedWidth = operatingMode & 0x07;
    int bandwidthMhz = encodedWidth == 0 ? 20 : encodedWidth == 1 ? 40 :
                       encodedWidth == 2 ? 80 : encodedWidth == 6 ? 160 : -1;
    int numberOfSpatialStreams = ((operatingMode >> 4) & 0x07) + 1;
    if (!peerStatus.vhtTxMcsMapKnown ||
        numberOfSpatialStreams > getMaximumSupportedSpatialStreams(peerStatus.vhtTxMcsMap))
        throw cRuntimeError("IEEE 802.11 Operating Mode Rx NSS exceeds or cannot be validated against the recipient's advertised VHT transmit NSS");
    bool modeSupported = false;
    if (modeSet != nullptr && bandwidthMhz != -1 && numberOfSpatialStreams <= maximumSpatialStreams) {
        for (int i = 0; i < modeSet->getNumModes(); i++) {
            const auto *dataMode = modeSet->getMode(i)->getDataMode();
            if (dataMode->getPhyFormat() == Ieee80211PhyFormat::VHT_SU &&
                dataMode->getBandwidth() == MHz(bandwidthMhz) &&
                dataMode->getNumberOfSpatialStreams() == numberOfSpatialStreams) {
                modeSupported = true;
                break;
            }
        }
    }
    if (!modeSupported)
        throw cRuntimeError("IEEE 802.11 Operating Mode byte 0x%02x does not describe a supported local VHT width/NSS combination", operatingMode);
    auto packet = new Packet("OperatingModeNotification");
    packet->addTag<MacAddressReq>()->setDestAddress(receiverAddress);
    packet->addTag<Ieee80211SubtypeReq>()->setSubtype(ST_ACTION);
    auto action = makeShared<Ieee80211OperatingModeNotification>();
    action->setOperatingMode(operatingMode);
    packet->insertAtBack(action);
    sendDown(packet);
}

void Ieee80211MgmtBase::dropManagementFrame(Packet *frame)
{
    EV << "ignoring management frame: " << (cMessage *)frame << "\n";
    delete frame;
    numMgmtFramesDropped++;
}

void Ieee80211MgmtBase::processFrame(Packet *packet, const Ptr<const Ieee80211DataOrMgmtHeader>& header)
{
    switch (header->getType()) {
        case ST_AUTHENTICATION:
            numMgmtFramesReceived++;
            handleAuthenticationFrame(packet, dynamicPtrCast<const Ieee80211MgmtHeader>(header));
            break;

        case ST_DEAUTHENTICATION:
            numMgmtFramesReceived++;
            handleDeauthenticationFrame(packet, dynamicPtrCast<const Ieee80211MgmtHeader>(header));
            break;

        case ST_ASSOCIATIONREQUEST:
            numMgmtFramesReceived++;
            handleAssociationRequestFrame(packet, dynamicPtrCast<const Ieee80211MgmtHeader>(header));
            break;

        case ST_ASSOCIATIONRESPONSE:
            numMgmtFramesReceived++;
            handleAssociationResponseFrame(packet, dynamicPtrCast<const Ieee80211MgmtHeader>(header));
            break;

        case ST_REASSOCIATIONREQUEST:
            numMgmtFramesReceived++;
            handleReassociationRequestFrame(packet, dynamicPtrCast<const Ieee80211MgmtHeader>(header));
            break;

        case ST_REASSOCIATIONRESPONSE:
            numMgmtFramesReceived++;
            handleReassociationResponseFrame(packet, dynamicPtrCast<const Ieee80211MgmtHeader>(header));
            break;

        case ST_DISASSOCIATION:
            numMgmtFramesReceived++;
            handleDisassociationFrame(packet, dynamicPtrCast<const Ieee80211MgmtHeader>(header));
            break;

        case ST_BEACON:
            numMgmtFramesReceived++;
            handleBeaconFrame(packet, dynamicPtrCast<const Ieee80211MgmtHeader>(header));
            break;

        case ST_PROBEREQUEST:
            numMgmtFramesReceived++;
            handleProbeRequestFrame(packet, dynamicPtrCast<const Ieee80211MgmtHeader>(header));
            break;

        case ST_PROBERESPONSE:
            numMgmtFramesReceived++;
            handleProbeResponseFrame(packet, dynamicPtrCast<const Ieee80211MgmtHeader>(header));
            break;

        case ST_ACTION: {
            numMgmtFramesReceived++;
            auto operatingModeNotification = dynamicPtrCast<const Ieee80211OperatingModeNotification>(header);
            if (operatingModeNotification != nullptr)
                handleOperatingModeNotificationFrame(packet, operatingModeNotification);
            else
                dropManagementFrame(packet);
            break;
        }

        default:
            throw cRuntimeError("Unexpected frame type (%s)%s", packet->getClassName(), packet->getName());
    }
}

void Ieee80211MgmtBase::handleOperatingModeNotificationFrame(Packet *packet,
        const Ptr<const Ieee80211OperatingModeNotification>& header)
{
    // IEEE Std 802.11-2024, 9.6.22.4 and 11.40: retain the most recently
    // received Operating Mode field from each peer.
    updateLatestPeerOperatingMode(header->getTransmitterAddress(), header->getOperatingMode());
    delete packet;
}

void Ieee80211MgmtBase::start()
{
}

void Ieee80211MgmtBase::stop()
{
    clearPeerOperatingModes();
}

} // namespace ieee80211

} // namespace inet
