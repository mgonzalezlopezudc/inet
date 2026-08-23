//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/rateselection/RateSelection.h"

#include "inet/common/ModuleAccess.h"
#include "inet/common/Simsignals.h"
#include "inet/linklayer/ieee80211/mac/contract/IRateControl.h"
#include "inet/networklayer/common/NetworkInterface.h"
#include "inet/physicallayer/wireless/ieee80211/mode/IIeee80211Mode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211VhtSigA.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

Define_Module(RateSelection);

void RateSelection::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL) {
        getContainingNicModule(this)->subscribe(modesetChangedSignal, this);
    }
    else if (stage == INITSTAGE_LINK_LAYER) {
        dataOrMgmtRateControl = dynamic_cast<IRateControl *>(findModuleByPath(par("rateControlModule")));
        peerCapabilities = dynamic_cast<IIeee80211PeerCapabilities *>(findModuleByPath(par("peerCapabilitiesModule")));
        ldpcSelectionConfig.htTxActivated = par("htLdpcTxActivated");
        ldpcSelectionConfig.vhtTxActivated = par("vhtLdpcTxActivated");
        double multicastFrameBitrate = par("multicastFrameBitrate");
        multicastFrameMode = (multicastFrameBitrate == -1) ? nullptr : modeSet->getMode(bps(multicastFrameBitrate));
        double dataFrameBitrate = par("dataFrameBitrate");
        dataFrameMode = (dataFrameBitrate == -1) ? nullptr : modeSet->getMode(bps(dataFrameBitrate), Hz(par("dataFrameBandwidth")), par("dataFrameNumSpatialStreams"));
        double mgmtFrameBitrate = par("mgmtFrameBitrate");
        mgmtFrameMode = (mgmtFrameBitrate == -1) ? nullptr : modeSet->getMode(bps(mgmtFrameBitrate));
        double controlFrameBitrate = par("controlFrameBitrate");
        controlFrameMode = (controlFrameBitrate == -1) ? nullptr : modeSet->getMode(bps(controlFrameBitrate));
        double responseAckFrameBitrate = par("responseAckFrameBitrate");
        responseAckFrameMode = (responseAckFrameBitrate == -1) ? nullptr : modeSet->getMode(bps(responseAckFrameBitrate));
        double responseCtsFrameBitrate = par("responseCtsFrameBitrate");
        responseCtsFrameMode = (responseCtsFrameBitrate == -1) ? nullptr : modeSet->getMode(bps(responseCtsFrameBitrate));
        fastestMandatoryMode = modeSet->getFastestMandatoryMode();
//        WATCH(dataOrMgmtRateControl);

//        WATCH(*((cObject**)&fastestMandatoryMode));
//        WATCH(*((cObject**)&modeSet));
        WATCH(lastTransmittedFrameMode);
//        WATCH(*((cObject**)&multicastFrameMode));
//        WATCH(*((cObject**)&dataFrameMode));
//        WATCH(*((cObject**)&mgmtFrameMode));
//        WATCH(*((cObject**)&controlFrameMode));
//        WATCH(*((cObject**)&responseAckFrameMode));
//        WATCH(*((cObject**)&responseCtsFrameMode));
//        WATCH();
//        WATCH();
//        WATCH();
//        WATCH();
    }
}

const IIeee80211Mode *RateSelection::getMode(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    const auto& modeReqTag = packet->findTag<Ieee80211ModeReq>();
    if (modeReqTag)
        return modeReqTag->getMode();
    const auto& modeIndTag = packet->findTag<Ieee80211ModeInd>();
    if (modeIndTag)
        return modeIndTag->getMode();
    throw cRuntimeError("Missing mode");
}

//
// In order to allow the transmitting STA to calculate the contents of the Duration/ID field, the responding STA
// shall transmit its Control Response frame (either CTS or ACK) at the same rate as the immediately previous
// frame in the frame exchange sequence (as defined in 9.7), if this rate belongs to the PHY mandatory rates, or
// else at the highest possible rate belonging to the PHY rates in the BSSBasicRateSet.
//
const IIeee80211Mode *RateSelection::computeResponseAckFrameMode(Packet *solicitingPacket, const Ptr<const Ieee80211DataOrMgmtHeader>& dataOrMgmtHeader, Packet *responsePacket)
{
    const IIeee80211Mode *candidate;
    const IIeee80211Mode *solicitingMode = getMode(solicitingPacket, dataOrMgmtHeader);
    if (responseAckFrameMode)
        candidate = responseAckFrameMode;
    else {
        ASSERT(modeSet->containsMode(solicitingMode));
        candidate = modeSet->getIsMandatory(solicitingMode) ? solicitingMode : modeSet->getSlowerMandatoryMode(solicitingMode); // TODO BSSBasicRateSet
    }
    auto selectedMode = applyFecSelection(candidate, dataOrMgmtHeader->getTransmitterAddress(),
            Ieee80211LdpcFrameContext::CONTROL_RESPONSE, solicitingMode);
    if (responsePacket != nullptr)
        attachVhtSigAParameters(responsePacket, dataOrMgmtHeader->getTransmitterAddress(), selectedMode);
    return selectedMode;
}

const IIeee80211Mode *RateSelection::computeResponseCtsFrameMode(Packet *solicitingPacket, const Ptr<const Ieee80211RtsFrame>& rtsFrame, Packet *responsePacket)
{
    const IIeee80211Mode *candidate;
    const IIeee80211Mode *solicitingMode = getMode(solicitingPacket, rtsFrame);
    if (responseCtsFrameMode)
        candidate = responseCtsFrameMode;
    else {
        ASSERT(modeSet->containsMode(solicitingMode));
        candidate = modeSet->getIsMandatory(solicitingMode) ? solicitingMode : modeSet->getSlowerMandatoryMode(solicitingMode); // TODO BSSBasicRateSet
    }
    auto selectedMode = applyFecSelection(candidate, rtsFrame->getTransmitterAddress(),
            Ieee80211LdpcFrameContext::CONTROL_RESPONSE, solicitingMode);
    if (responsePacket != nullptr)
        attachVhtSigAParameters(responsePacket, rtsFrame->getTransmitterAddress(), selectedMode);
    return selectedMode;
}

// 802.11-1999 Std.
//
// All frames with multicast and broadcast RA shall be transmitted at one of the rates included in the
// BSSBasicRateSet, regardless of their type.
//
// TODO Data and/or management MPDUs with a unicast immediate address shall be sent on any supported data rate
// selected by the rate switching mechanism (whose output is an internal MAC variable called MACCurrentRate,
// defined in units of 500 kbit/s, which is used for calculating the Duration/ID field of each frame). A STA shall
// not transmit at a rate that is known not to be supported by the destination STA, as reported in the supported
// rates element in the management frames. For frames of type Data+CF-ACK, Data+CF-Poll+CF-ACK, and CF-
// Poll+CF-ACK, the rate chosen to transmit the frame must be supported by both the addressed recipient STA
// and the STA to which the ACK is intended.
//
const IIeee80211Mode *RateSelection::computeDataOrMgmtFrameMode(const Ptr<const Ieee80211DataOrMgmtHeader>& dataOrMgmtHeader)
{
    if (dataOrMgmtHeader->getReceiverAddress().isMulticast() && multicastFrameMode)
        return multicastFrameMode;
    if (dynamicPtrCast<const Ieee80211DataHeader>(dataOrMgmtHeader) && dataFrameMode)
        return dataFrameMode;
    if (dynamicPtrCast<const Ieee80211MgmtHeader>(dataOrMgmtHeader) && mgmtFrameMode)
        return mgmtFrameMode;
    if (dataOrMgmtRateControl)
        return dataOrMgmtRateControl->getRate();
    else
        return fastestMandatoryMode;
}

// 802.11-1999 Std.
//
// All Control frames shall be transmitted at one of the rates in the BSSBasicRateSet
// (see 10.3.10.1), or at one of the rates in the PHY mandatory rate set so they will
// be understood by all STAs.
//
const IIeee80211Mode *RateSelection::computeControlFrameMode(const Ptr<const Ieee80211MacHeader>& header)
{
    // TODO BSSBasicRateSet
    return fastestMandatoryMode;
}

const IIeee80211Mode *RateSelection::computeMode(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    auto requestedModeTag = packet->findTag<Ieee80211ModeReq>();
    if (requestedModeTag != nullptr && requestedModeTag->getMode()->getDataMode()->getFecType() == Ieee80211FecType::BCC) {
        auto selectedMode = constrainIeee80211ModeByPeerOperatingMode(modeSet,
                requestedModeTag->getMode(), peerCapabilities, header->getReceiverAddress(), true);
        attachVhtSigAParameters(packet, header->getReceiverAddress(), selectedMode);
        return selectedMode;
    }
    bool forcedLdpc = requestedModeTag != nullptr && requestedModeTag->getMode()->getDataMode()->getFecType() == Ieee80211FecType::LDPC;
    const IIeee80211Mode *candidate;
    Ieee80211LdpcFrameContext context;
    if (auto dataOrMgmtHeader = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(header)) {
        candidate = forcedLdpc ? requestedModeTag->getMode() : computeDataOrMgmtFrameMode(dataOrMgmtHeader);
        context = Ieee80211LdpcFrameContext::DATA_OR_MANAGEMENT;
    }
    else {
        candidate = forcedLdpc ? requestedModeTag->getMode() : computeControlFrameMode(header);
        context = Ieee80211LdpcFrameContext::OTHER_CONTROL;
    }
    auto selectedMode = applyFecSelection(candidate, header->getReceiverAddress(), context, nullptr, forcedLdpc);
    attachVhtSigAParameters(packet, header->getReceiverAddress(), selectedMode);
    return selectedMode;
}

const IIeee80211Mode *RateSelection::applyFecSelection(const IIeee80211Mode *candidate,
        const MacAddress& receiverAddress, Ieee80211LdpcFrameContext context,
        const IIeee80211Mode *solicitingMode, bool forcedLdpc) const
{
    return selectIeee80211FecMode(modeSet, candidate, forcedLdpc, ldpcSelectionConfig,
            peerCapabilities, receiverAddress, context, solicitingMode).mode;
}

void RateSelection::attachVhtSigAParameters(Packet *packet, const MacAddress& receiverAddress,
        const IIeee80211Mode *mode) const
{
    if (mode->getDataMode()->getPhyFormat() != Ieee80211PhyFormat::VHT_SU)
        return;
    Ieee80211VhtSigAParameters parameters;
    if (receiverAddress.isMulticast())
        parameters = {true, 63, 0};
    else if (peerCapabilities != nullptr)
        parameters = peerCapabilities->getVhtSigAParameters(receiverAddress);
    if (parameters.known) {
        validateVhtSuGroupIdAndPartialAid(parameters.groupId, parameters.partialAid);
        auto request = packet->addTagIfAbsent<Ieee80211VhtSigAReq>();
        request->setGroupId(parameters.groupId);
        request->setPartialAid(parameters.partialAid);
    }
}

void RateSelection::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details)
{
    Enter_Method("%s", cComponent::getSignalName(signalID));

    if (signalID == modesetChangedSignal) {
        modeSet = check_and_cast<Ieee80211ModeSet *>(obj);
        fastestMandatoryMode = modeSet->getFastestMandatoryMode();
    }
}

void RateSelection::frameTransmitted(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    auto receiverAddr = header->getReceiverAddress();
    lastTransmittedFrameMode[receiverAddr] = getMode(packet, header);
}

void RateSelection::setFrameMode(Packet *packet, const Ptr<const Ieee80211MacHeader>& header, const IIeee80211Mode *mode)
{
    ASSERT(mode != nullptr);
    packet->addTagIfAbsent<Ieee80211ModeReq>()->setMode(mode);
}

} // namespace ieee80211
} // namespace inet
