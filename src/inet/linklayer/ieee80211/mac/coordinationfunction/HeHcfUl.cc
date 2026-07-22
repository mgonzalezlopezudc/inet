//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h"

#include <algorithm>
#include <optional>
#include <sstream>

#include "inet/common/INETMath.h"
#include "inet/common/ModuleAccess.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"
#include "inet/linklayer/ethernet/common/Ethernet.h"
#include "inet/linklayer/ieee80211/mac/blockack/BlockAckAgreementUtils.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edca.h"
#include "inet/linklayer/ieee80211/mac/channelaccess/Edcaf.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeUlMuTxOpFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeSoundingFs.h"
#include "inet/common/packet/chunk/SequenceChunk.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HcfFs.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeFrameSequenceHandler.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/originator/QosAckHandler.h"
#include "inet/linklayer/ieee80211/mac/contract/IRecoveryProcedure.h"
#include "inet/linklayer/ieee80211/mac/contract/IRateControl.h"
#include "inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreement.h"
#include "inet/linklayer/ieee80211/mac/blockack/RecipientBlockAckAgreement.h"
#include "inet/linklayer/ieee80211/mac/contract/IOriginatorBlockAckAgreementHandler.h"
#include "inet/linklayer/ieee80211/mac/originator/OriginatorQosMacDataService.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/FlatReceiverBase.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/FlatTransmitterBase.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HePreamblePuncturing.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeTwtGating.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeSoundingCoordinator.h"

// HE HCF uplink MU support.

namespace inet {
namespace ieee80211 {

void populateHeTbRequestFromTrigger(physicallayer::Ieee80211HeMuReq *request,
        const Ieee80211TriggerFrame& trigger, const Ieee80211HeTriggerUserInfo& user,
        uint16_t staId)
{
    if (request == nullptr)
        throw cRuntimeError("Cannot populate an empty HE-TB request");
    request->setPpduFormat(physicallayer::HE_TRIGGER_BASED_UPLINK);
    request->setTriggerId(trigger.getTriggerId());
    request->setLSigLength(trigger.getUlLength());
    request->setChannelBandwidthMhz(trigger.getChannelBandwidthMhz());
    request->setNoSignalExtension(trigger.getNoSignalExtension());
    request->setRuIndex(user.ruIndex);
    request->setRuToneSize(user.ruToneSize);
    request->setRuToneOffset(user.ruToneOffset);
    request->setStaId(staId);
    request->setMcs(user.mcs);
    request->setNumberOfSpatialStreams(user.numberOfSpatialStreams);
    request->setStreamStartIndex(user.streamStartIndex);
    request->setTotalNsts(user.numberOfSpatialStreams);
    request->setMuMimo(user.muMimo);
    request->setGuardInterval(trigger.getGuardInterval());
    request->setLtfType(trigger.getLtfType());
    request->setCoding(user.coding);
    request->setTriggerMethod(static_cast<uint8_t>(physicallayer::Ieee80211HeTriggerMethod::TRIGGER_FRAME));
    request->setLdpcExtraSymbolSegment(trigger.getLdpcExtraSymbolSegment());
    request->setPreFecPaddingFactor(trigger.getPreFecPaddingFactor());
    request->setPeDisambiguity(trigger.getPeDisambiguity());
    request->setNumberOfHeLtfSymbols(trigger.getNumberOfHeLtfSymbols());
    request->setPacketExtensionDurationUs(trigger.getPacketExtensionDurationUs());
    request->setPuncturedSubchannelMask(trigger.getPuncturedSubchannelMask());
    request->setCommonDuration(trigger.getCommonDuration());
    request->setCommonDurationExact(trigger.getCommonDurationExact());
}

double computeIeee80211HeTriggerPathLossDb(int apTxPowerDbm20Mhz,
        W receivedPower, Hz receivedBandwidth)
{
    if (receivedPower <= W(0) || receivedBandwidth < MHz(20))
        throw cRuntimeError("Cannot compute HE Trigger path loss from nonpositive power or bandwidth below 20 MHz");
    const double receivedPowerDbm = math::mW2dBmW(receivedPower.get<mW>());
    const double receivedPowerDbm20Mhz = receivedPowerDbm -
            10 * std::log10(receivedBandwidth.get() / 20e6);
    return apTxPowerDbm20Mhz - receivedPowerDbm20Mhz;
}

W computeIeee80211HeTbTransmitPower(W maximumPower, int targetReceivePowerDbm,
        double pathLossDb, bool useMaximumTransmitPower)
{
    if (useMaximumTransmitPower)
        return maximumPower;
    if (!std::isfinite(pathLossDb))
        return maximumPower;
    W requestedPower = mW(math::dBmW2mW(targetReceivePowerDbm + pathLossDb));
    return std::min(requestedPower, maximumPower);
}

static AccessCategory aciToAccessCategory(uint8_t aci)
{
    switch (aci) {
        case 0: return AC_BE;
        case 1: return AC_BK;
        case 2: return AC_VI;
        case 3: return AC_VO;
        default: throw cRuntimeError("Invalid Preferred AC in Basic Trigger");
    }
}

bool HeHcf::allAssociatedStationsSupportPreamblePuncturing() const
{
    return std::all_of(mac->getMib()->bssAccessPointData.stations.begin(),
            mac->getMib()->bssAccessPointData.stations.end(), [&] (const auto& station) {
                auto capabilities = mac->getMib()->findNegotiatedHeCapabilities(station.first);
                return station.second != Ieee80211Mib::ASSOCIATED ||
                        (capabilities != nullptr && capabilities->valid &&
                         capabilities->intersection.preamblePuncturing);
            });
}

bool HeHcf::supportsPreamblePuncturing(const IIeee80211HeUlScheduler::RuAllocation& allocation) const
{
    if (allocation.randomAccess)
        return allAssociatedStationsSupportPreamblePuncturing();
    auto capabilities = mac->getMib()->findNegotiatedHeCapabilities(allocation.staAddress);
    return capabilities != nullptr && capabilities->valid && capabilities->intersection.preamblePuncturing;
}

HeUlScheduleFinalizationResult HeHcf::finalizeUlSchedule(
        const IIeee80211HeUlScheduler::Schedule& proposedSchedule,
        Hz centerFrequency, Hz channelBandwidth,
        IIeee80211HeUlTriggerPolicy::TriggerType triggerType)
{
    HeUlScheduleFinalizationResult result;
    result.schedule = proposedSchedule;
    result.schedule.channelBandwidth = channelBandwidth;
    result.schedule.ulLength = 0;
    result.schedule.commonDuration = SIMTIME_ZERO;
    result.schedule.commonDurationExact = false;

    const bool feedbackNdp = triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER;
    if (proposedSchedule.allocations.empty() && !feedbackNdp) {
        result.error = "HE UL schedule has no RU allocations";
        return result;
    }
    if (proposedSchedule.commonDuration <= SIMTIME_ZERO) {
        result.error = "HE UL schedule has no positive duration budget";
        return result;
    }
    if (triggerType != IIeee80211HeUlTriggerPolicy::BASIC_TRIGGER &&
            triggerType != IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER &&
            triggerType != IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
        result.error = "HE UL schedule has an invalid Trigger type";
        return result;
    }

    std::vector<physicallayer::Ieee80211HeUserPhyParameters> users;
    users.reserve(feedbackNdp ? 1 : proposedSchedule.allocations.size());
    if (feedbackNdp) {
        physicallayer::Ieee80211HeUserPhyParameters user;
        user.ru = physicallayer::getHeEqualRuLayout(centerFrequency, channelBandwidth, 1).front();
        user.mcs = 0;
        user.numberOfSpatialStreams = 1;
        user.coding = physicallayer::HE_CODING_BCC;
        user.psduLength = B(0);
        user.ndpFeedbackReport = true;
        user.ndpRuToneSetIndex = 1;
        users.push_back(user);
    }
    for (const auto& allocation : proposedSchedule.allocations) {
        physicallayer::Ieee80211HeUserPhyParameters user;
        user.ru = allocation.ru;
        user.mcs = allocation.mcs;
        user.numberOfSpatialStreams = allocation.numberOfSpatialStreams;
        user.streamStartIndex = allocation.streamStartIndex;
        user.staId = allocation.associationId;
        user.coding = proposedSchedule.coding;
        // A feedback NDP has no Data field. Other Trigger types need at least
        // one data symbol; the actual PSDU is padded to the selected duration.
        user.psduLength = B(1);
        users.push_back(user);
    }

    physicallayer::Ieee80211HeTriggerResponseFinalizationRequest request;
    request.users = users;
    request.centerFrequency = centerFrequency;
    request.channelBandwidth = channelBandwidth;
    request.guardInterval = proposedSchedule.guardInterval;
    request.ltfType = proposedSchedule.ltfType;
    request.packetExtensionDurationUs = proposedSchedule.packetExtensionDurationUs;
    request.noSignalExtension = proposedSchedule.noSignalExtension;
    request.durationBudget = proposedSchedule.commonDuration;
    auto finalization = physicallayer::finalizeHeTriggerResponse(request);
    if (!finalization) {
        result.error = finalization.error;
        return result;
    }

    result.schedule.ulLength = finalization.ulLength;
    result.schedule.commonDuration = finalization.commonDuration;
    result.schedule.commonDurationExact = finalization.commonDurationExact;
    result.schedule.numberOfHeLtfSymbols =
            finalization.parameters.common.numberOfHeLtfSymbols;
    if (!feedbackNdp) {
        result.schedule.preFecPaddingFactor =
                finalization.parameters.common.preFecPaddingFactor;
        result.schedule.ldpcExtraSymbolSegment =
                finalization.parameters.common.ldpcExtraSymbol;
        result.schedule.peDisambiguity = finalization.peDisambiguity;
        result.schedule.packetExtensionDurationUs =
                finalization.parameters.common.packetExtensionDurationUs;
    }
    result.resolvedTxTime = finalization.resolvedTxTime;
    result.valid = true;
    return result;
}

bool HeHcf::tryStartUlMuFrameSequence(AccessCategory ac)
{
    // IEEE 802.11-2024 26.5.2.2: an AP that wins channel access may solicit
    // HE TB PPDUs from one or more non-AP HE STAs by transmitting a Trigger
    // frame.  EDCA/TXOP ownership is still inherited from HCF/EDCA (10.23).
    if (pendingUlTrigger == IIeee80211HeUlTriggerPolicy::NO_TRIGGER ||
            !mac->isApInAxMode() || !ulCoordinator->isEnabled())
        return false;

    ulTriggerAccessRequested = false;
    auto radio = check_and_cast<physicallayer::IRadio *>(getContainingNicModule(this)->getSubmodule("radio"));
    auto transmitter = check_and_cast<const physicallayer::NarrowbandTransmitterBase *>(radio->getTransmitter());
    auto receiver = check_and_cast<const physicallayer::FlatReceiverBase *>(radio->getReceiver());
    auto centerFrequency = transmitter->getCenterFrequency();
    Hz channelBandwidth = transmitter->getBandwidth();
    if (std::isnan(channelBandwidth.get()) || modeSet->findHeMode(0, 1, channelBandwidth, channelBandwidth > MHz(20)) == nullptr)
        channelBandwidth = MHz(20);
    auto edcaf = edca->getEdcaf(ac);
    simtime_t txopLimit = SIMTIME_ZERO;
    if (edcaf->getTxopProcedure()->getLimit() > SIMTIME_ZERO)
        txopLimit = std::max(SIMTIME_ZERO,
                edcaf->getTxopProcedure()->getLimit() - edcaf->getTxopProcedure()->getDuration());
    auto sensitivityDbm = math::mW2dBmW(receiver->getSensitivity().get<mW>());
    IIeee80211HeUlScheduler::Schedule ulSchedule;
    // 9.3.1.22 encodes the triggering AP's combined transmit power normalized
    // to 20 MHz in one-dB steps. Keep the projected value in the schedule so
    // the frame sequence and serializer cannot silently substitute a default.
    auto apTxPowerDbm20Mhz = math::mW2dBmW(transmitter->getMaxPower().get<mW>()) -
            10 * std::log10(channelBandwidth.get() / 20e6);
    if (pendingUlTrigger == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER ||
            pendingUlTrigger == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
        // IEEE 802.11-2024 9.3.1.22 Table 9-47 defines BSRP as Trigger type 4.
        // 26.5.2 permits the AP to solicit HE TB responses via addressed User
        // Info fields and RA-RUs.  The standard leaves scheduling policy open;
        // this model gives each polled STA one RU and exposes the remainder as
        // associated-STA RA-RUs, an implementation policy rather than a rule.
        auto maxRus = physicallayer::getHeMaxRuCount(channelBandwidth);
        auto layout = physicallayer::getHeEqualRuLayout(centerFrequency, channelBandwidth, maxRus);
        int index = 0;
        auto ulScheduler = getSubmodule("ulScheduler");
        int maxMuStations = ulScheduler ? ulScheduler->par("maxMuStations").intValue() : maxRus;
        std::vector<uint16_t> nfrpEligibleAids;
        for (const auto& station : mac->getMib()->bssAccessPointData.stations) {
            if (station.second != Ieee80211Mib::ASSOCIATED)
                continue;
            if (pendingUlTrigger != IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER &&
                    (index >= maxRus || index >= maxMuStations))
                break;
            if (isTwtSleeping(mac, station.first)) {
                EV_DEBUG << "HE UL BSRP: skipping sleeping TWT STA " << station.first << "\n";
                continue;
            }
            auto negotiated = mac->getMib()->findNegotiatedHeCapabilities(station.first);
            if (pendingUlTrigger == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER &&
                    (negotiated == nullptr || !negotiated->valid || !negotiated->intersection.ndpFeedbackReport))
                continue;
            if (pendingUlTrigger == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
                nfrpEligibleAids.push_back(mac->getMib()->getAssociationId(station.first));
                continue;
            }
            IIeee80211HeUlScheduler::RuAllocation allocation;
            allocation.staAddress = station.first;
            allocation.associationId = mac->getMib()->getAssociationId(station.first);
            allocation.ru = layout[index++];
            allocation.targetRssiDbm = (int)std::round(sensitivityDbm + (double)par("ulTargetRssiMargin"));
            ulSchedule.allocations.push_back(allocation);
        }
        if (pendingUlTrigger == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
            if (nfrpEligibleAids.empty()) {
                EV_WARN << "HE UL skipping NFRP Trigger because no awake associated STA negotiated NDP feedback\n";
                pendingUlTrigger = IIeee80211HeUlTriggerPolicy::NO_TRIGGER;
                return false;
            }
            std::sort(nfrpEligibleAids.begin(), nfrpEligibleAids.end());
            const int scheduledStaCount = IIeee80211HeUlScheduler::getNfrpScheduledStaCount(
                    channelBandwidth, false); // deterministic one-spatial-stream policy
            uint16_t selectedStart = nfrpEligibleAids.front();
            int selectedCount = -1;
            for (auto aid : nfrpEligibleAids) {
                auto candidateStart = std::min<int>(aid, 4096 - scheduledStaCount);
                auto end = std::lower_bound(nfrpEligibleAids.begin(), nfrpEligibleAids.end(),
                        candidateStart + scheduledStaCount);
                auto begin = std::lower_bound(nfrpEligibleAids.begin(), nfrpEligibleAids.end(), candidateStart);
                int count = end - begin;
                if (count > selectedCount || (count == selectedCount && candidateStart < selectedStart)) {
                    selectedStart = candidateStart;
                    selectedCount = count;
                }
            }
            ulSchedule.nfrpStartingAid = selectedStart;
            ulSchedule.nfrpFeedbackType = 0; // 26.5.7.4 resource request
            ulSchedule.nfrpMultiplexingFlag = false;
            ulSchedule.nfrpTargetRssiDbm = (int)std::round(sensitivityDbm + (double)par("ulTargetRssiMargin"));
        }
        while (index < maxRus && pendingUlTrigger != IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
            IIeee80211HeUlScheduler::RuAllocation allocation;
            allocation.randomAccess = true;
            allocation.associationId = 0;
            allocation.ru = layout[index++];
            allocation.targetRssiDbm = (int)std::round(sensitivityDbm + (double)par("ulTargetRssiMargin"));
            ulSchedule.allocations.push_back(allocation);
        }
        ulSchedule.commonDuration = std::min(SimTime(par("maxHeTbPpduDuration")), txopLimit > SIMTIME_ZERO ?
                txopLimit : SimTime(par("maxHeTbPpduDuration")));
    }
    else {
        int staleOrUnknown = 0;
        for (const auto& station : mac->getMib()->bssAccessPointData.stations) {
            if (station.second != Ieee80211Mib::ASSOCIATED)
                continue;
            auto aid = mac->getMib()->getAssociationId(station.first);
            auto status = ulCoordinator->getBufferStatus().find(aid);
            if (status == ulCoordinator->getBufferStatus().end() ||
                    simTime() - status->second.updateTime > ulCoordinator->getReportMaxAge())
                staleOrUnknown++;
        }
        ulSchedule = ulCoordinator->createSchedule(mac->getMib(), centerFrequency, channelBandwidth,
                txopLimit, par("maxHeTbPpduDuration"), sensitivityDbm,
                par("ulTargetRssiMargin"), staleOrUnknown, 0, 0);
    }
    ulSchedule.allocations.erase(std::remove_if(ulSchedule.allocations.begin(), ulSchedule.allocations.end(),
            [this] (const auto& allocation) {
                return !allocation.randomAccess && isTwtSleeping(mac, allocation.staAddress);
            }), ulSchedule.allocations.end());
    auto puncturedSubchannels = pendingUlTrigger == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER ?
            std::vector<bool>() : resolveHePreamblePuncturing(this, channelBandwidth);
    if (!puncturedSubchannels.empty()) {
        for (size_t i = 0; i < puncturedSubchannels.size(); ++i)
            if (puncturedSubchannels[i])
                ulSchedule.puncturedSubchannelMask |= 1U << i;
        ulSchedule.allocations.erase(std::remove_if(ulSchedule.allocations.begin(), ulSchedule.allocations.end(),
                [&] (const auto& allocation) {
                    return overlapsHePuncturedSubchannel(allocation.ru, puncturedSubchannels, channelBandwidth) ||
                            !supportsPreamblePuncturing(allocation);
                }), ulSchedule.allocations.end());
    }
    ulSchedule.packetExtensionDurationUs = mac->getMib()->heOperation.defaultPeDurationUs;
    if (par("enableUlMuMimo").boolValue() && pendingUlTrigger == IIeee80211HeUlTriggerPolicy::BASIC_TRIGGER) {
        std::vector<IIeee80211HeUlScheduler::RuAllocation *> eligible;
        for (auto& allocation : ulSchedule.allocations) {
            if (allocation.randomAccess)
                continue;
            auto negotiated = mac->getMib()->findNegotiatedHeCapabilities(allocation.staAddress);
            if (negotiated != nullptr && negotiated->valid && negotiated->intersection.fullBandwidthUlMuMimo)
                eligible.push_back(&allocation);
        }
        if (eligible.size() >= 2) {
            auto fullRu = physicallayer::getHeEqualRuLayout(centerFrequency, channelBandwidth, 1).front();
            int stream = 0;
            for (auto allocation : eligible) {
                allocation->ru = fullRu;
                allocation->streamStartIndex = stream++;
                allocation->muMimo = true;
            }
            ulSchedule.allocations.erase(std::remove_if(ulSchedule.allocations.begin(), ulSchedule.allocations.end(),
                    [] (const auto& allocation) { return allocation.randomAccess || !allocation.muMimo; }), ulSchedule.allocations.end());
        }
    }
    // Select one complete Table 9-49 GI/HE-LTF pair. Full-bandwidth UL
    // MU-MIMO may use the raw-0 1x/1.6 us pair; other medium-GI schedules use
    // raw 1 (2x/1.6 us), and long GI uses raw 2 (4x/3.2 us).
    if (auto heMode = dynamic_cast<const physicallayer::Ieee80211HeMode *>(modeSet->findHeMode(0, 1, channelBandwidth, channelBandwidth > MHz(20)))) {
        switch (heMode->getDataMode()->getGuardIntervalType()) {
            case physicallayer::Ieee80211HeModeBase::HE_GUARD_INTERVAL_SHORT:
            case physicallayer::Ieee80211HeModeBase::HE_GUARD_INTERVAL_MEDIUM:
                ulSchedule.guardInterval = physicallayer::HE_GI_1_6_US;
                ulSchedule.ltfType = std::any_of(ulSchedule.allocations.begin(), ulSchedule.allocations.end(),
                        [] (const auto& allocation) { return allocation.muMimo; }) ?
                        physicallayer::HE_LTF_1X : physicallayer::HE_LTF_2X;
                break;
            case physicallayer::Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG:
                ulSchedule.guardInterval = physicallayer::HE_GI_3_2_US;
                ulSchedule.ltfType = physicallayer::HE_LTF_4X;
                break;
        }
    }
    // Table 27-32 permits only 4x HE-LTF with 3.2 us GI for feedback NDP.
    if (pendingUlTrigger == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
        ulSchedule.guardInterval = physicallayer::HE_GI_3_2_US;
        ulSchedule.ltfType = physicallayer::HE_LTF_4X;
    }
    bool ldpcSupportedByAll = mac->getMib()->localHeCapabilities.ldpc;
    for (const auto& allocation : ulSchedule.allocations) {
        if (allocation.randomAccess)
            continue;
        auto capabilities = mac->getMib()->findNegotiatedHeCapabilities(allocation.staAddress);
        ldpcSupportedByAll = ldpcSupportedByAll && capabilities != nullptr && capabilities->valid &&
                capabilities->intersection.ldpc;
    }
    ulSchedule.coding = ldpcSupportedByAll ? physicallayer::HE_CODING_LDPC : physicallayer::HE_CODING_BCC;
    auto triggerType = pendingUlTrigger;
    if (triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
        ulSchedule.coding = physicallayer::HE_CODING_BCC;
        ulSchedule.packetExtensionDurationUs = 0;
    }
    ulSchedule.apTxPowerDbm = std::clamp((int)std::lround(apTxPowerDbm20Mhz), -20, 40);
    pendingUlTrigger = IIeee80211HeUlTriggerPolicy::NO_TRIGGER;
    if (ulSchedule.allocations.empty() && triggerType != IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
        EV_WARN << "HE UL skipping Trigger because no usable RU allocations remain"
                << " after scheduling and puncturing checks\n";
        return false;
    }

    auto finalization = finalizeUlSchedule(ulSchedule, centerFrequency,
            channelBandwidth, triggerType);
    if (!finalization) {
        EV_WARN << "HE UL skipping Trigger because schedule timing cannot be finalized: "
                << finalization.error << "\n";
        return false;
    }
    ulSchedule = std::move(finalization.schedule);

    ASSERT(ulSchedule.commonDuration > SIMTIME_ZERO);
    EV_INFO << "HE UL starting"
             << (triggerType == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER ? " BSRP" :
                     triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER ? " NFRP" : " Basic")
             << " exchange with " << ulSchedule.allocations.size()
             << " RU allocations for " << ulSchedule.commonDuration << "\n";
    frameSequenceHandler->startFrameSequence(
            new HeUlMuTxOpFs(ulCoordinator, this, ulSchedule, triggerType,
                    modeSet, mac->getAddress()),
            buildContext(ac), this);
    emit(IFrameSequenceHandler::frameSequenceStartedSignal, frameSequenceHandler->getContext());
    return true;
}

void HeHcf::processTriggeredUlFrame(Packet *packet, const Ptr<const Ieee80211DataHeader>& header, uint16_t aid)
{
    emit(packetReceivedFromPeerSignal, packet);
    if (header->getBufferStatusPresent())
        ulCoordinator->updateBufferStatus(aid,
                static_cast<AccessCategory>(header->getBufferStatusAc()),
                header->getBufferStatusTid(), header->getBufferStatusQueueSize(), header->getRetry());
    if (header->getType() == ST_QOS_NULL) {
        delete packet;
        return;
    }
    if (recipientBlockAckAgreementHandler != nullptr) {
        auto agreement = recipientBlockAckAgreementHandler->getAgreement(header->getTid(), header->getTransmitterAddress());
        if (agreement != nullptr)
            recipientBlockAckAgreementHandler->qosFrameReceived(header, this);
    }
    // The Trigger exchange acknowledges all collected responses with one Multi-STA
    // Block Ack. Deliver the data through the normal QoS receive service without
    // invoking Hcf::recipientProcessReceivedFrame(), which would schedule a
    // legacy per-frame Ack while the collection sequence is still running.
    // This exchange carries its own per-user acknowledgment record. Do not
    // hold the decoded MPDU in the legacy single-user Block Ack reorder
    // buffer, whose sequence window may be advancing independently through
    // ordinary EDCA transmissions.
    sendUp(recipientDataService->dataFrameReceived(packet, header, nullptr));
}
Ptr<Ieee80211CompressedBlockAck> buildHeMuBarCompressedBlockAck(
        const Ieee80211HeTriggerUserInfo& user, RecipientBlockAckAgreement *agreement,
        const MacAddress& receiverAddress, const MacAddress& transmitterAddress)
{
    ASSERT(agreement != nullptr);
    ASSERT(user.muBarCompressedBitmap && !user.muBarMultiTid);
    // The MU-BAR User Info field embeds a Compressed BlockAckReq whose
    // Starting Sequence Number selects the 64-MPDU response bitmap window
    // (9.3.1.22.4 and 9.3.1.9).  Using the agreement's initial window here
    // makes every later response repeat the first bitmap and eventually
    // exhausts the originator's BA window.
    auto startingSequenceNumber = SequenceNumberCyclic(user.muBarStartingSequenceNumber);
    std::vector<uint8_t> bytes(8, 0);
    BitVector bitmap(bytes);
    for (int i = 0; i < 64; ++i) {
        bool ackState = agreement->getBlockAckRecord()->getAckState(startingSequenceNumber + i, 0);
        bitmap.setBit(i, ackState);
    }
    auto blockAck = makeShared<Ieee80211CompressedBlockAck>();
    blockAck->setReceiverAddress(receiverAddress);
    blockAck->setTransmitterAddress(transmitterAddress);
    blockAck->setCompressedBitmap(true);
    blockAck->setStartingSequenceNumber(startingSequenceNumber);
    blockAck->setTidInfo(user.tid);
    blockAck->setBlockAckBitmap(bitmap);
    blockAck->setDurationField(SIMTIME_ZERO);
    return blockAck;
}

void HeHcf::sendTriggeredBlockAckResponse(Packet *packet, const Ptr<const Ieee80211TriggerFrame>& trigger)
{
    // 9.3.1.22.4 defines MU-BAR Trigger User Info as BAR Control plus BAR
    // Information.  26.4.5 requires a Compressed BlockAck response when the
    // addressed User Info field contains a Compressed BlockAckReq variant.
    auto myAid = mac->getMib()->bssStationData.associationId;
    const Ieee80211HeTriggerUserInfo *selected = nullptr;
    for (unsigned int i = 0; i < trigger->getUsersArraySize(); ++i)
        if (trigger->getUsers(i).aid == myAid) {
            selected = &trigger->getUsers(i);
            break;
        }
    auto agreement = selected == nullptr || recipientBlockAckAgreementHandler == nullptr ?
            nullptr : recipientBlockAckAgreementHandler->getAgreement(
                    selected->tid, trigger->getTransmitterAddress());
    if (selected == nullptr)
        EV_WARN << "Ignoring MU-BAR Trigger because it has no User Info for local AID " << myAid << endl;
    else if (recipientBlockAckAgreementHandler == nullptr)
        EV_WARN << "Ignoring MU-BAR Trigger for AID " << myAid << " because no recipient Block Ack handler is installed" << endl;
    else if (agreement == nullptr)
        EV_WARN << "Ignoring MU-BAR Trigger for AID " << myAid << " because no recipient Block Ack agreement exists for TID "
                << (int)selected->tid << " and originator " << trigger->getTransmitterAddress() << endl;
    if (agreement != nullptr) {
        if (!selected->muBarCompressedBitmap || selected->muBarMultiTid)
            throw cRuntimeError("Unsupported MU-BAR BlockAckReq variant");
        auto blockAck = buildHeMuBarCompressedBlockAck(*selected, agreement,
                trigger->getTransmitterAddress(), mac->getAddress());
        auto response = new Packet("HE-TB-BlockAck", blockAck);
        response->insertAtBack(makeShared<Ieee80211MacTrailer>());
        auto request = response->addTagIfAbsent<physicallayer::Ieee80211HeMuReq>();
        populateHeTbRequestFromTrigger(request.get(), *trigger, *selected, myAid);
        tx->transmitFrame(response, blockAck, modeSet->getSifsTime(), this);
        delete response;
    }
    delete packet;
    return;
}

void HeHcf::retryPendingTriggeredUlExchanges()
{
    for (auto& entry : triggeredUlExchanges) {
        if (entry.second.randomAccess)
            ulCoordinator->reportRandomAccessResult(false);
        for (auto pkt : entry.second.packets) {
            auto writableHeader = pkt->removeAtFront<Ieee80211DataHeader>();
            writableHeader->setRetry(true);
            pkt->insertAtFront(writableHeader);
            entry.second.sourceQueue->pushPacket(pkt, nullptr);
        }
    }
    triggeredUlExchanges.clear();
}

Packet *HeHcf::buildHeTbAmpdu(const std::vector<Packet *>& mpdus)
{
    ASSERT(!mpdus.empty());
    auto ampdu = new Packet("HE-TB-A-MPDU");
    // 9.7.1 A-MPDU subframes are carried behind MPDU delimiters and padded
    // to 4-octet boundaries. 26.5.2.4 applies this to both QoS Data and QoS
    // Null MPDUs carried in HE TB responses, including a single subframe.
    for (size_t i = 0; i < mpdus.size(); ++i) {
        auto delimiter = makeShared<Ieee80211MpduSubframeHeader>();
        delimiter->setLength(mpdus[i]->getByteLength());
        delimiter->setEof(i + 1 == mpdus.size());
        ampdu->insertAtBack(delimiter);
        ampdu->insertAtBack(mpdus[i]->peekAll());
        int padding = (4 - (B(4) + B(mpdus[i]->getByteLength())).get<B>() % 4) % 4;
        if (i + 1 != mpdus.size() && padding != 0)
            ampdu->insertAtBack(makeShared<ByteCountChunk>(B(padding)));
    }
    return ampdu;
}

Packet *HeHcf::buildTriggeredUlResponsePacket(Packet *sourcePacket, queueing::IPacketQueue *sourceQueue,
        AccessCategory selectedAc, uint8_t selectedTid, int64_t queueBytes, int availableSlots,
        const Ieee80211HeTriggerUserInfo *selected, const Ptr<const Ieee80211TriggerFrame>& trigger,
        TriggeredUlExchange& exchange)
{
    Packet *responsePacket = nullptr;
    if (sourcePacket != nullptr) {
        // 26.5.2.4 requires a QoS Null response when the allocation cannot
        // contain pending data. Check the first MPDU too; the aggregation loop
        // below performs the same check for every additional MPDU.
        auto sourceHeader = sourcePacket->peekAtFront<Ieee80211DataHeader>();
        B psduLength = B(4 + sourcePacket->getByteLength()) +
                (sourceHeader->getBufferStatusPresent() ? B(0) : B(4));
        auto ru = exchange.ru;
        ru.dataSubcarriers = physicallayer::getHeRuDataSubcarrierCount(ru.toneSize);
        ru.pilotSubcarriers = physicallayer::getHeRuPilotSubcarrierCount(ru.toneSize);
        ru.bandwidth = Hz(ru.toneSize * 78125.0);
        auto duration = physicallayer::computeHeUserPhyParameters(psduLength, ru, selected->mcs,
                selected->numberOfSpatialStreams, false,
                static_cast<physicallayer::Ieee80211HeGuardInterval>(trigger->getGuardInterval()),
                static_cast<physicallayer::Ieee80211HeCoding>(selected->coding)).duration;
        if (duration > trigger->getCommonDuration())
            sourcePacket = nullptr;
    }
    if (sourcePacket != nullptr) {
        // 26.5.2.4: a Basic Trigger response can carry QoS Data in an A-MPDU.
        // The Ack Policy is Block Ack/Implicit BAR style so the AP can return a
        // Multi-STA BA context after collecting simultaneous HE TB responses.
        auto writableHeader = sourcePacket->removeAtFront<Ieee80211DataHeader>();
        if (!writableHeader->getRetry()) {
            auto qosDataService = check_and_cast<OriginatorQosMacDataService *>(originatorDataService);
            qosDataService->assignSequenceNumber(writableHeader);
        }
        if (!writableHeader->getBufferStatusPresent())
            writableHeader->setChunkLength(writableHeader->getChunkLength() + B(4));
        writableHeader->setOrder(true);
        writableHeader->setAckPolicy(BLOCK_ACK);
        writableHeader->setBufferStatusPresent(true);
        writableHeader->setBufferStatusTid(selectedTid);
        writableHeader->setBufferStatusAc(selectedAc);
        writableHeader->setBufferStatusQueueSize(queueBytes);
        sourcePacket->insertAtFront(writableHeader);
        responsePacket = sourcePacket->dup();
    }
    else {
        // 26.5.2.4 allows a triggered STA with no data fitting the allocation
        // to carry a QoS Null-style response; we still include BSR so the AP's
        // scheduler state is refreshed by the HE TB exchange.
        auto nullHeader = makeShared<Ieee80211DataHeader>();
        nullHeader->setType(ST_QOS_NULL);
        nullHeader->setReceiverAddress(mac->getMib()->bssData.bssid);
        nullHeader->setTransmitterAddress(mac->getAddress());
        nullHeader->setAddress3(mac->getMib()->bssData.bssid);
        nullHeader->setToDS(true);
        nullHeader->setTid(selectedTid);
        nullHeader->setAckPolicy(BLOCK_ACK);
        nullHeader->setOrder(true);
        nullHeader->setBufferStatusPresent(true);
        nullHeader->setBufferStatusTid(selectedTid);
        nullHeader->setBufferStatusAc(selectedAc);
        nullHeader->setBufferStatusQueueSize(queueBytes);
        nullHeader->setChunkLength(B(30));
        responsePacket = new Packet("HE-TB-QoS-Null", nullHeader);
        responsePacket->insertAtBack(makeShared<Ieee80211MacTrailer>());
    }

    if (sourcePacket != nullptr) {
        exchange.packets.push_back(sourcePacket);
        exchange.sequenceNumbers.push_back(sourcePacket->peekAtFront<Ieee80211DataHeader>()->getSequenceNumber().get());

        // 26.6.3 permits multi-TID HE TB A-MPDUs only within the negotiated
        // Trigger TID Aggregation Limit.  This model deliberately restricts
        // Basic Trigger UL aggregation to one TID; retained packets are removed
        // from the EDCA queue only after the HE TB PSDU is built and are retried
        // individually from the returned Multi-STA BA bitmap.
        int maximumMpduCount = std::min(64, availableSlots);
        for (int i = 0; availableSlots > 0 && (int)exchange.packets.size() < maximumMpduCount &&
                i < sourceQueue->getNumPackets(); ++i) {
            auto candidate = sourceQueue->getPacket(i);
            if (candidate == sourcePacket)
                continue;
            auto candidateHeader = dynamicPtrCast<const Ieee80211DataHeader>(candidate->peekAtFront<Ieee80211MacHeader>());
            if (candidateHeader == nullptr || candidateHeader->getType() != ST_DATA_WITH_QOS ||
                    candidateHeader->getTid() != selectedTid ||
                    candidateHeader->getReceiverAddress() != mac->getMib()->bssData.bssid)
                continue;
            B psduLength(0);
            for (auto packet : exchange.packets)
                psduLength += B(4 + packet->getByteLength());
            psduLength += B(4 + candidate->getByteLength());
            physicallayer::Ieee80211HeRu ru = exchange.ru;
            ru.dataSubcarriers = physicallayer::getHeRuDataSubcarrierCount(ru.toneSize);
            ru.pilotSubcarriers = physicallayer::getHeRuPilotSubcarrierCount(ru.toneSize);
            ru.bandwidth = Hz(ru.toneSize * 78125.0);
            if (physicallayer::computeHeUserPhyParameters(psduLength, ru, selected->mcs,
                    selected->numberOfSpatialStreams, false,
                    static_cast<physicallayer::Ieee80211HeGuardInterval>(trigger->getGuardInterval()),
                    static_cast<physicallayer::Ieee80211HeCoding>(selected->coding)).duration > trigger->getCommonDuration())
                break;
            auto writableCandidateHeader = candidate->removeAtFront<Ieee80211DataHeader>();
            if (!writableCandidateHeader->getRetry()) {
                auto qosDataService = check_and_cast<OriginatorQosMacDataService *>(originatorDataService);
                qosDataService->assignSequenceNumber(writableCandidateHeader);
            }
            writableCandidateHeader->setOrder(true);
            writableCandidateHeader->setAckPolicy(BLOCK_ACK);
            candidate->insertAtFront(writableCandidateHeader);
            exchange.packets.push_back(candidate);
            exchange.sequenceNumbers.push_back(writableCandidateHeader->getSequenceNumber().get());
        }
        int64_t reportedQueueBytes = queueBytes;
        for (auto pkt : exchange.packets)
            reportedQueueBytes = std::max<int64_t>(0, reportedQueueBytes - pkt->getByteLength());
        auto firstHeader = exchange.packets.front()->removeAtFront<Ieee80211DataHeader>();
        firstHeader->setBufferStatusQueueSize(reportedQueueBytes);
        exchange.packets.front()->insertAtFront(firstHeader);
        for (auto mpdu : exchange.packets) {
            auto trailer = mpdu->removeAtBack<Ieee80211MacTrailer>(B(4));
            auto fcsMode = mac->getFcsMode();
            trailer->setFcsMode(fcsMode);
            if (fcsMode == FCS_COMPUTED)
                trailer->setFcs(computeEthernetFcs(mpdu, fcsMode));
            mpdu->insertAtBack(trailer);
        }
        delete responsePacket;
        responsePacket = buildHeTbAmpdu(exchange.packets);
        for (auto pkt : exchange.packets) {
            exchange.sourceQueue->removePacket(pkt);
            take(pkt);
        }
    }
    else {
        auto nullMpdu = responsePacket;
        auto trailer = nullMpdu->removeAtBack<Ieee80211MacTrailer>(B(4));
        auto fcsMode = mac->getFcsMode();
        trailer->setFcsMode(fcsMode);
        if (fcsMode == FCS_COMPUTED)
            trailer->setFcs(computeEthernetFcs(nullMpdu, fcsMode));
        nullMpdu->insertAtBack(trailer);
        responsePacket = buildHeTbAmpdu({nullMpdu});
        delete nullMpdu;
    }
    return responsePacket;
}

void HeHcf::processReceivedTriggerFrame(Packet *packet, const Ptr<const Ieee80211TriggerFrame>& trigger)
{
    // IEEE 802.11-2024 9.3.1.22 Table 9-47: Trigger type 2 is MU-BAR.  A
    // MU-BAR Trigger carries BAR control/information in each User Info field
    // and solicits BlockAck responses in HE TB PPDUs.
    if (trigger->getTriggerType() == 2) {
        sendTriggeredBlockAckResponse(packet, trigger);
        return;
    }
    if (trigger->getTriggerType() == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER &&
            !mac->getMib()->localHeCapabilities.ndpFeedbackReport) {
        delete packet;
        return;
    }
    if (!ulCoordinator->isEnabled() || mac->isApInAxMode() ||
            mac->getMib()->bssStationData.associationId <= 0) {
        delete packet;
        return;
    }
    std::optional<double> triggerPathLossDb;
    if (trigger->getTransmitterAddress() == mac->getMib()->bssData.bssid) {
        auto signalPower = packet->findTag<SignalPowerInd>();
        auto modeInd = packet->findTag<physicallayer::Ieee80211ModeInd>();
        if (signalPower != nullptr && modeInd != nullptr && modeInd->getMode() != nullptr) {
            const auto receivedBandwidth = modeInd->getMode()->getDataMode()->getBandwidth();
            triggerPathLossDb = computeIeee80211HeTriggerPathLossDb(
                    trigger->getApTxPowerDbm(), signalPower->getPower(), receivedBandwidth);
        }
    }
    retryPendingTriggeredUlExchanges();
    auto myAid = mac->getMib()->bssStationData.associationId;
    const Ieee80211HeTriggerUserInfo *selected = nullptr;
    Ieee80211HeTriggerUserInfo nfrpUser;
    uint8_t nfrpToneSetIndex = 0;
    uint8_t nfrpStartingStsNumber = 0;
    if (trigger->getTriggerType() == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
        if (trigger->getTransmitterAddress() != mac->getMib()->bssData.bssid ||
                isTwtSleeping(mac, mac->getMib()->bssData.bssid) ||
                trigger->getNfrpFeedbackType() != 0) {
            delete packet;
            return;
        }
        auto resource = IIeee80211HeUlScheduler::getNfrpResponseResource(
                trigger->getNfrpStartingAid(), myAid,
                Hz(trigger->getChannelBandwidthMhz() * 1e6),
                trigger->getNfrpMultiplexingFlag());
        if (!resource.scheduled) {
            EV_INFO << "Ignoring NFRP Trigger " << trigger->getTriggerId()
                    << ": AID " << myAid << " is outside the scheduled range\n";
            delete packet;
            return;
        }
        auto maximumRu = physicallayer::getHeEqualRuLayout(Hz(0),
                Hz(trigger->getChannelBandwidthMhz() * 1e6), 1).front();
        nfrpUser.aid = myAid;
        nfrpUser.ruIndex = maximumRu.index;
        nfrpUser.ruToneSize = maximumRu.toneSize;
        nfrpUser.ruToneOffset = maximumRu.toneOffset;
        nfrpUser.mcs = 0;
        nfrpUser.coding = physicallayer::HE_CODING_BCC;
        nfrpUser.numberOfSpatialStreams = 1;
        nfrpUser.streamStartIndex = 0;
        nfrpUser.targetRssiDbm = trigger->getNfrpTargetRssiDbm();
        nfrpUser.useMaximumTransmitPower = trigger->getNfrpUseMaximumTransmitPower();
        nfrpToneSetIndex = resource.toneSetIndex;
        nfrpStartingStsNumber = resource.startingStsNumber;
        selected = &nfrpUser;
    }
    std::vector<const Ieee80211HeTriggerUserInfo *> randomAccessUsers;
    for (unsigned int i = 0; selected == nullptr && i < trigger->getUsersArraySize(); i++) {
        const auto& user = trigger->getUsers(i);
        if (user.randomAccess)
            randomAccessUsers.push_back(&user);
        else if (user.aid == myAid)
            selected = &user;
    }

    AccessCategory selectedAc = AC_BE;
    queueing::IPacketQueue *sourceQueue = nullptr;
    Packet *sourcePacket = nullptr;
    bool randomAccess = false;
    int bsrpTid = -1;
    if (selected != nullptr && trigger->getTriggerType() != IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER &&
            trigger->getTriggerType() != IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
        // 26.5.2.4: an associated non-AP STA responding to a Basic Trigger
        // addressed to its AID constructs an HE TB A-MPDU using the Trigger's
        // TID aggregation limit and the addressed User Info field.  INET keeps
        // this path single-TID. Preferred AC is a lower-bound recommendation,
        // not a selected TID (9.3.1.22.2), so choose pending data from that AC
        // or a higher-priority AC and derive the actual TID from the MPDU.
        auto preferredAc = aciToAccessCategory(selected->preferredAc);
        for (int ac = AC_VO; ac >= preferredAc && sourcePacket == nullptr; ac--) {
            auto queue = edca->getEdcaf(static_cast<AccessCategory>(ac))->getPendingQueue();
            for (int i = 0; i < queue->getNumPackets(); i++) {
                auto candidate = queue->getPacket(i);
                auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(
                        candidate->peekAtFront<Ieee80211MacHeader>());
                if (dataHeader != nullptr && dataHeader->getType() == ST_DATA_WITH_QOS) {
                    sourceQueue = queue;
                    selectedAc = static_cast<AccessCategory>(ac);
                    sourcePacket = candidate;
                    break;
                }
            }
        }
        if (sourceQueue == nullptr) {
            selectedAc = preferredAc;
            sourceQueue = edca->getEdcaf(selectedAc)->getPendingQueue();
        }
        EV_DEBUG << "HE TB Basic Trigger queue selection: preferredAc=" << preferredAc
                 << ", AC=" << selectedAc
                 << ", queued=" << sourceQueue->getNumPackets()
                 << ", data=" << (sourcePacket != nullptr) << "\n";
    }
    else if (selected != nullptr && trigger->getTriggerType() == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER) {
        // 9.3.1.22.6: BSRP Trigger has no trigger-dependent User Info; the
        // HE TB response is used to report buffer status.  We choose the
        // highest-priority queued TID to report when a directed BSRP RU exists.
        for (int ac = AC_VO; ac >= AC_BK && sourceQueue == nullptr; ac--) {
            auto queue = edca->getEdcaf(static_cast<AccessCategory>(ac))->getPendingQueue();
            for (int i = 0; i < queue->getNumPackets(); i++) {
                auto candidate = queue->getPacket(i);
                auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(
                        candidate->peekAtFront<Ieee80211MacHeader>());
                if (dataHeader != nullptr && dataHeader->getType() == ST_DATA_WITH_QOS) {
                    sourceQueue = queue;
                    selectedAc = static_cast<AccessCategory>(ac);
                    bsrpTid = dataHeader->getTid();
                    break;
                }
            }
        }
    }
    else if (selected == nullptr && !randomAccessUsers.empty()) {
        // 9.3.1.22 Table 9-52 encodes AID12=0 as RA-RUs for associated STAs.
        // 26.5.4 supplies the UORA access procedure; the coordinator chooses
        // whether this STA wins one of the advertised RA-RUs.
        queueing::IPacketQueue *pendingQueue = nullptr;
        Packet *pendingPacket = nullptr;
        AccessCategory pendingAc = AC_BE;
        int pendingTid = -1;
        for (int ac = AC_VO; ac >= AC_BK && pendingPacket == nullptr; ac--) {
            auto queue = edca->getEdcaf(static_cast<AccessCategory>(ac))->getPendingQueue();
            for (int i = 0; i < queue->getNumPackets(); i++) {
                auto candidate = queue->getPacket(i);
                auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(
                        candidate->peekAtFront<Ieee80211MacHeader>());
                if (dataHeader != nullptr && dataHeader->getType() == ST_DATA_WITH_QOS) {
                    pendingPacket = candidate;
                    pendingQueue = queue;
                    pendingAc = static_cast<AccessCategory>(ac);
                    pendingTid = dataHeader->getTid();
                    break;
                }
            }
        }
        if (pendingQueue != nullptr) {
            int raIndex = ulCoordinator->selectRandomAccessRu(randomAccessUsers.size());
            if (raIndex >= 0) {
                selected = randomAccessUsers[raIndex];
                randomAccess = true;
                selectedAc = pendingAc;
                sourceQueue = pendingQueue;
                if (trigger->getTriggerType() == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER) {
                    bsrpTid = pendingTid;
                    sourcePacket = nullptr;
                }
                else {
                    sourcePacket = pendingPacket;
                }
            }
        }
    }
    if (selected == nullptr) {
        EV_INFO << "Ignoring HE UL Trigger " << trigger->getTriggerId()
                 << ": this STA has no scheduled or selected random-access RU\n";
        delete packet;
        return;
    }

    ASSERT(selected->ruToneSize > 0);
    ASSERT(trigger->getCommonDuration() > SIMTIME_ZERO);

    uint8_t selectedTid = bsrpTid >= 0 ? bsrpTid : 0;
    if (sourcePacket != nullptr) {
        auto sourceHeader = dynamicPtrCast<const Ieee80211DataHeader>(
                sourcePacket->peekAtFront<Ieee80211MacHeader>());
        selectedTid = sourceHeader->getTid();
    }
    if (sourceQueue == nullptr)
        sourceQueue = edca->getEdcaf(selectedAc)->getPendingQueue();
    auto ulBaAgreement = originatorBlockAckAgreementHandler == nullptr ? nullptr :
            originatorBlockAckAgreementHandler->getAgreement(mac->getMib()->bssData.bssid, selectedTid);
    int occupiedSlots = edca->getEdcaf(selectedAc)->getAckHandler()->getOccupiedBlockAckSequenceNumbers(
            mac->getMib()->bssData.bssid, selectedTid).size();
    int availableSlots = ulBaAgreement == nullptr ? 0 :
            std::max(0, ulBaAgreement->getBufferSize() - occupiedSlots);
    EV_DEBUG << "HE TB Block Ack window: agreement=" << (ulBaAgreement != nullptr)
             << ", size=" << (ulBaAgreement != nullptr ? ulBaAgreement->getBufferSize() : 0)
             << ", occupied=" << occupiedSlots
             << ", available=" << availableSlots << "\n";
    // IEEE 802.11-2024 10.3.2.13.3 and 26.4.4.5 allow a tagged QoS Data MPDU
    // in an HE TB response to be acknowledged immediately by a Multi-STA BA;
    // an existing block ack agreement is required only for the untagged
    // multi-MPDU aggregation case. Reserve one slot so both scheduled and UORA
    // users can send their first non-aggregated MPDU before an agreement exists.
    if (sourcePacket != nullptr && ulBaAgreement == nullptr)
        availableSlots = 1;
    if (sourcePacket != nullptr && availableSlots == 0)
        sourcePacket = nullptr;
    int64_t queueBytes = 0;
    if (trigger->getTriggerType() == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
        for (int ac = AC_BK; ac <= AC_VO; ac++) {
            auto queue = edca->getEdcaf(static_cast<AccessCategory>(ac))->getPendingQueue();
            for (int i = 0; i < queue->getNumPackets(); i++)
                queueBytes += queue->getPacket(i)->getByteLength();
        }
    }
    else {
        for (int i = 0; i < sourceQueue->getNumPackets(); i++) {
            auto queuedPacket = sourceQueue->getPacket(i);
            auto queuedHeader = dynamicPtrCast<const Ieee80211DataHeader>(
                    queuedPacket->peekAtFront<Ieee80211MacHeader>());
            if (queuedHeader != nullptr && queuedHeader->getTid() == selectedTid)
                queueBytes += queuedPacket->getByteLength();
        }
    }
    TriggeredUlExchange exchange;
    exchange.tid = selectedTid;
    exchange.sourceQueue = sourceQueue;
    exchange.randomAccess = randomAccess;
    exchange.ru.index = selected->ruIndex;
    exchange.ru.toneSize = selected->ruToneSize;
    exchange.ru.toneOffset = selected->ruToneOffset;
    exchange.expectedResponseTime = simTime() + modeSet->getSifsTime();
    Packet *responsePacket;
    Ptr<const Ieee80211MacHeader> responseHeader;
    size_t responsePacketCount = 0;
    if (trigger->getTriggerType() == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
        // IEEE 802.11ax 27.3.11.11: the NFRP response is a preamble-only NDP with no PSDU.
        // Create a truly empty packet so the PCAP recorder writes only a Radiotap header
        // followed by a zero-byte 802.11 body — matching real hardware monitor-mode captures.
        responsePacket = new Packet("HE-TB-NDP-Feedback-Report");
        // Synthesise a detached header solely for address-stamping in Tx::transmitFrame().
        // This header is NOT serialised into the PPDU.
        auto ndpHeader = makeShared<Ieee80211DataHeader>();
        ndpHeader->setType(ST_QOS_NULL);
        ndpHeader->setReceiverAddress(mac->getMib()->bssData.bssid);
        ndpHeader->setTransmitterAddress(mac->getAddress());
        ndpHeader->setAddress3(mac->getMib()->bssData.bssid);
        ndpHeader->setToDS(true);
        ndpHeader->setChunkLength(B(30));
        responseHeader = ndpHeader;
        // NDP carries no MPDU; no triggered exchange state to track.
    }
    else {
        responsePacket = buildTriggeredUlResponsePacket(sourcePacket, sourceQueue, selectedAc,
                selectedTid, queueBytes, availableSlots, selected, trigger, exchange);
        if (exchange.packets.empty())
            responseHeader = responsePacket->peekAt<Ieee80211DataHeader>(B(4));
        else
            responseHeader = exchange.packets.front()->peekAtFront<Ieee80211MacHeader>();
        responsePacketCount = exchange.packets.empty() ? 1 : exchange.packets.size();
        if (!exchange.packets.empty() || exchange.randomAccess)
            triggeredUlExchanges.emplace(trigger->getTriggerId(), std::move(exchange));
    }

    auto radio = check_and_cast<physicallayer::IRadio *>(getContainingNicModule(this)->getSubmodule("radio"));
    auto transmitter = check_and_cast<const physicallayer::FlatTransmitterBase *>(radio->getTransmitter());
    W transmitPower = transmitter->getMaxPower();
    if (triggerPathLossDb.has_value())
        transmitPower = computeIeee80211HeTbTransmitPower(transmitter->getMaxPower(),
                selected->targetRssiDbm, *triggerPathLossDb, selected->useMaximumTransmitPower);
    // 26.5.2.3.3 and 27.3.11.12: the HE TB TXVECTOR is derived from the
    // selected Trigger User Info and Common Info fields.  These request tags
    // carry that standard information to INET's packet-level PHY.
    auto request = responsePacket->addTagIfAbsent<physicallayer::Ieee80211HeMuReq>();
    populateHeTbRequestFromTrigger(request.get(), *trigger, *selected, myAid);
    if (trigger->getTriggerType() == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
        request->setPsduLength(B(0));
        request->setDcm(false);
        request->setNdpFeedbackReport(true);
        request->setNdpFeedbackStatus(queueBytes > 256 ? 1 : 0);
        request->setNdpRuToneSetIndex(nfrpToneSetIndex);
        request->setNdpStartingStsNumber(nfrpStartingStsNumber);
        request->setPsrDisallowed(true);
        responsePacket->addTagIfAbsent<physicallayer::Ieee80211HeMuTxTag>()->setNdp(true);
    }
    request->setTransmitPower(transmitPower);
    EV_INFO << "Sending HE-TB response: trigger=" << trigger->getTriggerId()
             << ", AID=" << myAid
             << ", " << (randomAccess ? "random-access" : "scheduled")
             << " RU=" << selected->ruIndex
             << ", packets=" << responsePacketCount << "\n";
    tx->transmitFrame(responsePacket, responseHeader,
            modeSet->getSifsTime(), this);
    delete responsePacket;
    delete packet;
    return;
}

void HeHcf::processReceivedMultiStaBlockAck(Packet *packet, const Ptr<const Ieee80211MultiStaBlockAck>& multiStaBlockAck)
{
    // 26.4.2: a non-AP STA originator processes only the Per AID TID Info
    // record matching its AID and TID, then applies the Block Ack starting
    // sequence number and bitmap to the outstanding triggered MPDUs.
    auto myAid = mac->getMib()->bssStationData.associationId;
    bool success = false;
    for (unsigned int i = 0; i < multiStaBlockAck->getRecordsArraySize(); i++) {
        const auto& record = multiStaBlockAck->getRecords(i);
        if (record.aid == myAid) {
            success = record.responseReceived && (record.bitmap & 1);
            break;
        }
    }
    for (auto& entry : triggeredUlExchanges) {
        auto& exchange = entry.second;
        const Ieee80211MultiStaBlockAckRecord *record = nullptr;
        for (unsigned int i = 0; i < multiStaBlockAck->getRecordsArraySize(); ++i)
            if (multiStaBlockAck->getRecords(i).aid == myAid && multiStaBlockAck->getRecords(i).tid == exchange.tid) {
                record = &multiStaBlockAck->getRecords(i);
                break;
            }
        // A UORA response to a BSRP Trigger is a QoS Null carrying buffer
        // status, so there is no queued data MPDU or bitmap bit to retire. A
        // positive per-AID response record is nevertheless a successful UORA
        // attempt and must reset OCW and be counted.
        bool exchangeSuccess = exchange.randomAccess && exchange.packets.empty() &&
                record != nullptr && record->responseReceived;
        if (record != nullptr && record->responseReceived) {
            AccessCategory ac = edca->mapTidToAc(exchange.tid);
            if (ac >= 0 && ac < 4) {
                edca->getEdcaf(ac)->startMuEdcaTimer();
            }
        }
        for (size_t i = 0; i < exchange.packets.size(); ++i) {
            bool acknowledged = false;
            if (record != nullptr && record->responseReceived) {
                int offset = (exchange.sequenceNumbers[i] - record->startingSequenceNumber + 4096) % 4096;
                acknowledged = offset < 64 && (record->bitmap & (UINT64_C(1) << offset));
            }
            if (acknowledged) {
                delete exchange.packets[i];
                exchangeSuccess = true;
            }
            else {
                auto writableHeader = exchange.packets[i]->removeAtFront<Ieee80211DataHeader>();
                writableHeader->setRetry(true);
                exchange.packets[i]->insertAtFront(writableHeader);
                exchange.sourceQueue->pushPacket(exchange.packets[i], nullptr);
            }
        }
        if (exchange.randomAccess)
            ulCoordinator->reportRandomAccessResult(exchangeSuccess);
        success = success || exchangeSuccess;
    }
    triggeredUlExchanges.clear();
    delete packet;
    return;
}
} // namespace ieee80211
} // namespace inet
