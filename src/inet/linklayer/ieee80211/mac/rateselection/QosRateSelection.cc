//
// Copyright (C) 2016 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/linklayer/ieee80211/mac/rateselection/QosRateSelection.h"

#include "inet/common/ModuleAccess.h"
#include "inet/common/Simsignals.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211Mib.h"
#include "inet/linklayer/ieee80211/mac/rateselection/Ieee80211RateSelectionPolicy.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211FecCodingReq.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/networklayer/common/NetworkInterface.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

Define_Module(QosRateSelection);

static bool isHeOrEhtMode(const Ieee80211ModeSet *modeSet, const IIeee80211Mode *mode)
{
    auto family = modeSet->getPhyFamily(mode);
    return family == Ieee80211PhyFamily::HE || family == Ieee80211PhyFamily::EHT;
}

static const IIeee80211Mode *selectHeOrEhtResponseMode(const Ieee80211ModeSet *modeSet,
        const IIeee80211Mode *precedingMode, const IIeee80211Mode *configuredMode)
{
    if (!isHeOrEhtMode(modeSet, precedingMode))
        return nullptr;
    if (configuredMode != nullptr)
        return configuredMode;
    if (modeSet->getIsMandatory(precedingMode))
        return precedingMode;
    if (auto slowerMode = modeSet->getSlowerMandatoryMode(precedingMode))
        return slowerMode;
    throw cRuntimeError("Mandatory HE/EHT response mode not found");
}

void QosRateSelection::initialize(int stage)
{
    ModeSetListener::initialize(stage);
    if (stage == INITSTAGE_LINK_LAYER) {
        mib = check_and_cast<Ieee80211Mib *>(getContainingNicModule(this)->getSubmodule("mib"));
        dataOrMgmtRateControl = dynamic_cast<IRateControl *>(findModuleByPath(par("rateControlModule")));
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
        double responseBlockAckFrameBitrate = par("responseBlockAckFrameBitrate");
        responseBlockAckFrameMode = (responseBlockAckFrameBitrate == -1) ? nullptr : modeSet->getMode(bps(responseBlockAckFrameBitrate));
        double responseCtsFrameBitrate = par("responseCtsFrameBitrate");
        responseCtsFrameMode = (responseCtsFrameBitrate == -1) ? nullptr : modeSet->getMode(bps(responseCtsFrameBitrate));
        WATCH_EXPR("dataFrameModeName", dataFrameMode ? dataFrameMode->getName() : "<null>");
        WATCH_EXPR("dataFrameModeNetBitrate", dataFrameMode ? dataFrameMode->getDataMode()->getNetBitrate().get() : -1);
        WATCH_EXPR("dataFrameModeBandwidth", dataFrameMode ? dataFrameMode->getDataMode()->getBandwidth().get() : -1);
        WATCH_EXPR("dataFrameModeNumSpatialStreams", dataFrameMode ? dataFrameMode->getDataMode()->getNumberOfSpatialStreams() : -1);
        WATCH_EXPR("fastestMandatoryModeName", fastestMandatoryMode ? fastestMandatoryMode->getName() : "<null>");
        WATCH_EXPR("fastestMandatoryModeNetBitrate", fastestMandatoryMode ? fastestMandatoryMode->getDataMode()->getNetBitrate().get() : -1);
        WATCH_EXPR("fastestMandatoryModeBandwidth", fastestMandatoryMode ? fastestMandatoryMode->getDataMode()->getBandwidth().get() : -1);
        WATCH_EXPR("fastestMandatoryModeNumSpatialStreams", fastestMandatoryMode ? fastestMandatoryMode->getDataMode()->getNumberOfSpatialStreams() : -1);
    }
}

const IIeee80211Mode *QosRateSelection::getMode(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    const auto& modeReqTag = packet->findTag<Ieee80211ModeReq>();
    if (modeReqTag)
        return modeReqTag->getMode();
    const auto& modeIndTag = packet->findTag<Ieee80211ModeInd>();
    if (modeIndTag)
        return modeIndTag->getMode();
    throw cRuntimeError("Missing mode");
}

bool QosRateSelection::isControlResponseFrame(const Ptr<const Ieee80211MacHeader>& header, TxopProcedure *txopProcedure)
{
    bool nonSelfCts = dynamicPtrCast<const Ieee80211CtsFrame>(header) && !txopProcedure->isTxopInitiator(header);
    bool blockAck = dynamicPtrCast<const Ieee80211BlockAck>(header) != nullptr;
    bool ack = dynamicPtrCast<const Ieee80211AckFrame>(header) != nullptr;
    return ack || blockAck || nonSelfCts;
}

//
// If a CTS or ACK control response frame is carried in a non-HT PPDU, the primary rate is defined to
// be the highest rate in the BSSBasicRateSet parameter that is less than or equal to the rate (or non-HT
// reference rate; see 9.7.9) of the previous frame. If no rate in the BSSBasicRateSet parameter meets
// these conditions, the primary rate is defined to be the highest mandatory rate of the attached PHY
// that is less than or equal to the rate (or non-HT reference rate; see 9.7.9) of the previous frame. The
// STA may select an alternate rate according to the rules in 9.7.6.5.4. The STA shall transmit the
// non-HT PPDU CTS or ACK control response frame at either the primary rate or the alternate rate, if
// one exists.
//
const IIeee80211Mode *QosRateSelection::computeResponseAckFrameMode(Packet *packet, const Ptr<const Ieee80211DataOrMgmtHeader>& dataOrMgmtHeader)
{
    auto mode = getMode(packet, dataOrMgmtHeader);
    ASSERT(modeSet->containsMode(mode));
    if (auto selected = selectHeOrEhtResponseMode(modeSet, mode, responseAckFrameMode))
        return selected; // HE/EHT-specific response rules are outside this policy's scope.
    auto basicRates = mib->getBssBasicLegacyRates();
    Ieee80211RateSelectionPolicy::Context context {modeSet, &mib->localOperationalRates, &basicRates, nullptr};
    return Ieee80211RateSelectionPolicy::selectResponse(context, mode, responseAckFrameMode);
}

const IIeee80211Mode *QosRateSelection::computeResponseCtsFrameMode(Packet *packet, const Ptr<const Ieee80211RtsFrame>& rtsFrame)
{
    auto mode = getMode(packet, rtsFrame);
    ASSERT(modeSet->containsMode(mode));
    if (auto selected = selectHeOrEhtResponseMode(modeSet, mode, responseCtsFrameMode))
        return selected; // HE/EHT-specific response rules are outside this policy's scope.
    auto basicRates = mib->getBssBasicLegacyRates();
    Ieee80211RateSelectionPolicy::Context context {modeSet, &mib->localOperationalRates, &basicRates, nullptr};
    return Ieee80211RateSelectionPolicy::selectResponse(context, mode, responseCtsFrameMode);
}

//
// If a Basic BlockAck frame is sent as an immediate response to a BlockAckReq frame that was
// carried in a non-HT PPDU and the Basic BlockAck frame is carried in a non-HT PPDU, the primary
// rate is defined to be the same rate and modulation class as the BlockAckReq frame, and the STA
// shall transmit the Basic BlockAck frame at the primary rate.
//
const IIeee80211Mode *QosRateSelection::computeResponseBlockAckFrameMode(Packet *packet, const Ptr<const Ieee80211BlockAckReq>& blockAckReq)
{
    auto precedingMode = getMode(packet, blockAckReq);
    if (isHeOrEhtMode(modeSet, precedingMode))
        return responseBlockAckFrameMode != nullptr ? responseBlockAckFrameMode : precedingMode;
    auto precedingFamily = modeSet->getPhyFamily(precedingMode);
    bool precedingIsLegacy = precedingFamily == Ieee80211PhyFamily::DSSS ||
            precedingFamily == Ieee80211PhyFamily::ERP_OFDM || precedingFamily == Ieee80211PhyFamily::OFDM;
    if (dynamicPtrCast<const Ieee80211BasicBlockAckReq>(blockAckReq) && precedingIsLegacy) {
        if (responseBlockAckFrameMode != nullptr && responseBlockAckFrameMode != precedingMode)
            throw cRuntimeError("Configured Basic Block Ack response mode conflicts with the required Block Ack Request rate and modulation class");
        return precedingMode;
    }
    auto basicRates = mib->getBssBasicLegacyRates();
    Ieee80211RateSelectionPolicy::Context context {modeSet, &mib->localOperationalRates, &basicRates,
            mib->findPeerLegacyRates(blockAckReq->getTransmitterAddress())};
    return responseBlockAckFrameMode != nullptr ?
            Ieee80211RateSelectionPolicy::selectResponse(context, precedingMode, responseBlockAckFrameMode) :
            Ieee80211RateSelectionPolicy::selectResponse(context, precedingMode);
}

const IIeee80211Mode *QosRateSelection::computeDataOrMgmtFrameMode(const Ptr<const Ieee80211DataOrMgmtHeader>& dataOrMgmtHeader)
{
    auto basicRates = mib->getBssBasicLegacyRates();
    auto peerRates = mib->findPeerLegacyRates(dataOrMgmtHeader->getReceiverAddress());
    Ieee80211RateSelectionPolicy::Context context {modeSet, &mib->localOperationalRates, &basicRates, peerRates,
            &mib->htOperation, &mib->vhtOperation};
    // This subclause describes the rate selection rules for group addressed data and management frames, excluding
    // the following:
    //   — Non-STBC Beacon and non-STBC PSMP frames
    //   — STBC group addressed data and management frames
    //   — Data frames located in an FMS stream (see 10.23.7)
    if (dataOrMgmtHeader->getReceiverAddress().isMulticast()) {
        // If the BSSBasicRateSet parameter is not empty, a data or management frame (excluding the frames listed
        // above) with a group address in the Address 1 field shall be transmitted in a non-HT PPDU using one of the
        // rates included in the BSSBasicRateSet parameter or the rate chosen by the AP, described in 10.23.7, if the data
        // frames are part of an FMS stream.
        // TODO BSSBasicRateSet
        // If the BSSBasicRateSet parameter is empty and the BSSBasicMCSSet parameter is not empty, the frame shall
        // be transmitted in an HT PPDU using one of the MCSs included in the BSSBasicMCSSet parameter.

        // If both the BSSBasicRateSet parameter and the BSSBasicMCSSet parameter are empty (e.g., a scanning STA
        // that is not yet associated with a BSS), the frame shall be transmitted in a non-HT PPDU using one of the
        // mandatory PHY rates.
        auto groupMode = multicastFrameMode;
        if (groupMode == nullptr && dataFrameMode != nullptr &&
                !Ieee80211ModeSet::isHtOrVhtMode(dataFrameMode)) {
            auto family = modeSet->getPhyFamily(dataFrameMode);
            if (family != Ieee80211PhyFamily::DSSS && family != Ieee80211PhyFamily::ERP_OFDM &&
                    family != Ieee80211PhyFamily::OFDM)
                groupMode = dataFrameMode; // Preserve HE/EHT behavior, which is outside this policy's scope.
        }
        if (groupMode == nullptr && dataOrMgmtRateControl != nullptr &&
                isHeOrEhtMode(modeSet, dataOrMgmtRateControl->getRate()))
            groupMode = dataOrMgmtRateControl->getRate(); // Preserve the prior HE/EHT rate-control dispatch.
        return Ieee80211RateSelectionPolicy::selectGroupOrControl(context, groupMode,
                multicastFrameMode == nullptr ? Ieee80211RateSelectionPolicy::DEFAULT_SELECTION :
                Ieee80211RateSelectionPolicy::EXPLICIT_CONFIGURATION);
    }
    // A data or management frame not identified in 9.7.5.1 through 9.7.5.5 shall be sent using any data rate or MCS
    // subject to the following constraints:
    //    — A STA shall not transmit a frame using a rate or MCS that is not supported by the receiver STA or
    //      STAs, as reported in any Supported Rates element, Extended Supported Rates element, or
    //      Supported MCS field in management frames transmitted by the receiver STA.
    //    — A STA shall not transmit a frame using a value for the CH_BANDWIDTH parameter of the
    //      TXVECTOR that is not supported by the receiver STA.
    //    — A STA shall not initiate transmission of a frame at a data rate higher than the greatest rate in the
    //      OperationalRateSet or the HTOperationalMCSset, which are parameters of the MLME-
    //      JOIN.request primitive.
    else {
        const IIeee80211Mode *configured = dynamicPtrCast<const Ieee80211DataHeader>(dataOrMgmtHeader) ? dataFrameMode : mgmtFrameMode;
        if (configured != nullptr)
            return Ieee80211RateSelectionPolicy::selectUnicast(context, configured,
                    Ieee80211RateSelectionPolicy::EXPLICIT_CONFIGURATION);
        if (dataOrMgmtRateControl)
            return Ieee80211RateSelectionPolicy::selectUnicast(context, dataOrMgmtRateControl->getRate(),
                    Ieee80211RateSelectionPolicy::RATE_CONTROL);
        return Ieee80211RateSelectionPolicy::selectUnicast(context, fastestMandatoryMode,
                Ieee80211RateSelectionPolicy::DEFAULT_SELECTION);
    }
}

const IIeee80211Mode *QosRateSelection::computeControlFrameMode(const Ptr<const Ieee80211MacHeader>& header, TxopProcedure *txopProcedure)
{
    if (dynamicPtrCast<const Ieee80211MultiStaBlockAck>(header))
        return controlFrameMode ? controlFrameMode : fastestMandatoryMode;
    ASSERT(!isControlResponseFrame(header, txopProcedure));
    auto basicRates = mib->getBssBasicLegacyRates();
    Ieee80211RateSelectionPolicy::Context context {modeSet, &mib->localOperationalRates, &basicRates,
            mib->findPeerLegacyRates(header->getReceiverAddress()), &mib->htOperation, &mib->vhtOperation};
    if (txopProcedure->isInitialProtectionPending() && dynamicPtrCast<const Ieee80211RtsFrame>(header)) {
        // IEEE Std 802.11-2024, 10.27.3: non-HT mixed-mode protection
        // starts with an RTS carried in a legacy PPDU. CTS derives its mode
        // from this received RTS in computeResponseCtsFrameMode().
        auto mode = Ieee80211RateSelectionPolicy::selectGroupOrControl(context);
        if (Ieee80211ModeSet::isHtOrVhtMode(mode))
            throw cRuntimeError("HT legacy RTS/CTS protection requires a legacy BSS Basic or mandatory PHY mode");
        return mode;
    }
    // This subclause describes the rate selection rules for control frames that initiate a TXOP and that are not carried
    // in an A-MPDU.
    if (txopProcedure->isTxopInitiator(header)) {
        // If a control frame other than a Basic BlockAckReq or Basic BlockAck is carried in a non-HT PPDU, the
        // transmitting STA shall transmit the frame using one of the rates in the BSSBasicRateSet parameter or a rate
        // from the mandatory rate set of the attached PHY if the BSSBasicRateSet is empty.
        if (!dynamicPtrCast<const Ieee80211BlockAck>(header) && !dynamicPtrCast<const Ieee80211BlockAckReq>(header)) {
            return Ieee80211RateSelectionPolicy::selectGroupOrControl(context, controlFrameMode,
                    controlFrameMode == nullptr ? Ieee80211RateSelectionPolicy::DEFAULT_SELECTION :
                    Ieee80211RateSelectionPolicy::EXPLICIT_CONFIGURATION);
        }
        // If a Basic BlockAckReq or Basic BlockAck frame is carried in a non-HT PPDU, the transmitting STA shall
        // transmit the frame using a rate supported by the receiver STA, if known (as reported in the Supported Rates
        // element and/or Extended Supported Rates element in frames transmitted by that STA). If the supported rate set
        // of the receiving STA or STAs is not known, the transmitting STA shall transmit using a rate from the
        // BSSBasicRateSet parameter or using a rate from the mandatory rate set of the attached PHY if the
        // BSSBasicRateSet is empty.
        else {
            if (controlFrameMode != nullptr)
                return Ieee80211RateSelectionPolicy::selectUnicast(context, controlFrameMode,
                        Ieee80211RateSelectionPolicy::EXPLICIT_CONFIGURATION);
            return Ieee80211RateSelectionPolicy::selectBlockAck(context);
        }
    }
    // This subclause describes the rate selection rules for control frames that are not control response frames, are not
    // the frame that initiates a TXOP, are not the frame that terminates a TXOP, and are not carried in an A-MPDU.
    else if (!txopProcedure->isTxopTerminator(header)) {
        // A frame other than a BlockAckReq or BlockAck that is carried in a non-HT PPDU shall be transmitted by the
        // STA using a rate no higher than the highest rate in the BSSBasicRateSet parameter that is less than or equal to
        // the rate or non-HT reference rate (see 9.7.9) of the previously transmitted frame that was directed to the same
        // receiving STA. If no rate in the BSSBasicRateSet parameter meets these conditions, the control frame shall be
        // transmitted at a rate no higher than the highest mandatory rate of the attached PHY that is less than or equal to
        // the rate or non-HT reference rate (see 9.7.9) of the previously transmitted frame that was directed to the same
        // receiving STA.
        // TODO BSSBasicRateSet
        if (!dynamicPtrCast<const Ieee80211BlockAck>(header) && !dynamicPtrCast<const Ieee80211BlockAckReq>(header)) {
            auto it = lastTransmittedFrameMode.find(header->getReceiverAddress());
            if (it != lastTransmittedFrameMode.end()) {
                if (isHeOrEhtMode(modeSet, it->second))
                    return controlFrameMode != nullptr ? controlFrameMode : it->second;
                return Ieee80211RateSelectionPolicy::selectResponse(context, it->second, controlFrameMode);
            }
            return Ieee80211RateSelectionPolicy::selectGroupOrControl(context, controlFrameMode,
                    controlFrameMode == nullptr ? Ieee80211RateSelectionPolicy::DEFAULT_SELECTION :
                    Ieee80211RateSelectionPolicy::EXPLICIT_CONFIGURATION);
        }
        // A BlockAckReq or BlockAck that is carried in a non-HT PPDU shall be transmitted by the STA using a rate
        // supported by the receiver STA, as reported in the Supported Rates element and/or Extended Supported Rates
        // element in frames transmitted by that STA. When the supported rate set of the receiving STA or STAs is not
        // known, the transmitting STA shall transmit using a rate from the BSSBasicRateSet parameter or from the
        // mandatory rate set of the attached PHY if the BSSBasicRateSet is empty.
        else {
            if (controlFrameMode != nullptr)
                return Ieee80211RateSelectionPolicy::selectUnicast(context, controlFrameMode,
                        Ieee80211RateSelectionPolicy::EXPLICIT_CONFIGURATION);
            return Ieee80211RateSelectionPolicy::selectBlockAck(context);
        }
    }
    else
        throw cRuntimeError("Control frames cannot terminate TXOPs");
}

const IIeee80211Mode *QosRateSelection::computeMode(Packet *packet, const Ptr<const Ieee80211MacHeader>& header, TxopProcedure *txopProcedure)
{
    const IIeee80211Mode *selectedMode;
    if (auto dataOrMgmtHeader = dynamicPtrCast<const Ieee80211DataOrMgmtHeader>(header))
        selectedMode = computeDataOrMgmtFrameMode(dataOrMgmtHeader);
    else
        selectedMode = computeControlFrameMode(header, txopProcedure);
    if (!mib->isEhtModeAllowedForPeer(selectedMode, header->getReceiverAddress()))
        throw cRuntimeError("Explicitly selected EHT mode is prohibited by the effective peer capability or operation state");
    if (!mib->isHtModeAllowedForPeer(selectedMode, header->getReceiverAddress()))
        throw cRuntimeError("Selected HT mode is prohibited by the effective peer capability or operation state");
    if (!mib->isVhtModeAllowedForPeer(selectedMode, header->getReceiverAddress()))
        throw cRuntimeError("Selected VHT mode is prohibited by the effective peer capability or operation state");
    if (Ieee80211ModeSet::isPeerNegotiatedFecMode(selectedMode))
        packet->addTagIfAbsent<Ieee80211FecCodingReq>()->setLdpcAllowed(
                mib->isLdpcAllowedForPeer(selectedMode, header->getReceiverAddress()));
    return selectedMode;
}

void QosRateSelection::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details)
{
    Enter_Method("%s", cComponent::getSignalName(signalID));

    if (signalID == modesetChangedSignal) {
        modeSet = check_and_cast<Ieee80211ModeSet *>(obj);
        cModule *nic = getContainingNicModule(this);
        cModule *radio = nic ? nic->getSubmodule("radio") : nullptr;
        auto modeSetProvider = dynamic_cast<IIeee80211ModeSetProvider *>(radio);
        if (modeSetProvider == nullptr || modeSetProvider->getModeSet() != modeSet)
            throw cRuntimeError("QoS rate selection received an inconsistent 802.11 mode profile");
        fastestMandatoryMode = modeSet->getFastestBasicMode(modeSetProvider->getModeBandwidth());
    }
}

void QosRateSelection::frameTransmitted(Packet *packet, const Ptr<const Ieee80211MacHeader>& header)
{
    auto receiverAddr = header->getReceiverAddress();
    lastTransmittedFrameMode[receiverAddr] = getMode(packet, header);
}

} /* namespace ieee80211 */
} /* namespace inet */
