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
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeTxVector.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"
#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame_m.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HePreamblePuncturing.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeTwtGating.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeSoundingCoordinator.h"

// HE HCF uplink MU support.

namespace inet {
namespace ieee80211 {

static physicallayer::Ieee80211HeTxVectorValidationResult createHeTbTxVector(
        const Ieee80211TriggerFrame& trigger, const Ieee80211HeTriggerUserInfo& selected,
        Hz centerFrequency, uint16_t staId, B psduLength,
        uint8_t bssColor = 0,
        bool ndpFeedbackReport = false, uint8_t ndpFeedbackStatus = 0,
        uint8_t ndpRuToneSetIndex = 0, uint8_t ndpStartingStsNumber = 0,
        physicallayer::Ieee80211HeTxopDuration txopDuration = {})
{
    physicallayer::Ieee80211HeTxVectorRequest request;
    request.centerFrequency = centerFrequency;
    request.channelBandwidth = Hz(trigger.getChannelBandwidthMhz() * 1e6);
    request.ppduFormat = physicallayer::HE_TRIGGER_BASED_UPLINK;
    // Puncturing is not carried by the 802.11ax Trigger Common Info field.
    // The supported HE-TB response therefore uses the unpunctured bandwidth
    // described by UL BW and the selected wire RU allocation.
    request.puncturedSubchannelMask = 0;
    request.lSigLength = trigger.getUlLength();
    request.noSignalExtension = false;
    request.requestedTxTime = trigger.getCommonDuration();
    // UL Length reconstructs a 4 us response-time envelope, not the original
    // transmitter-local exact TXTIME.
    request.requestedTxTimeExact = false;
    request.triggerMethod = physicallayer::Ieee80211HeTriggerMethod::TRIGGER_FRAME;
    request.bssColor = bssColor;
    request.txopDuration = txopDuration;
    request.preFecPaddingFactor = trigger.getPreFecPaddingFactor();
    request.ldpcExtraSymbolSegment = trigger.getLdpcExtraSymbolSegment();
    request.peDisambiguity = trigger.getPeDisambiguity();
    for (size_t i = 0; i < request.spatialReuse.size(); ++i)
        request.spatialReuse[i] = (trigger.getUlSpatialReuse() >> (4 * i)) & 0xF;
    request.doppler = trigger.getDoppler();
    request.numberOfHeLtfSymbols = trigger.getNumberOfHeLtfSymbols();
    request.guardInterval =
            static_cast<physicallayer::Ieee80211HeGuardInterval>(trigger.getGuardInterval());
    request.ltfType =
            static_cast<physicallayer::Ieee80211HeLtfType>(trigger.getLtfType());
    // The nominal PE is reconstructed from the wire pre-FEC padding factor
    // and PE disambiguity fields by the HE-TB calculator.
    request.packetExtensionDurationUs = 0;
    request.ndp = ndpFeedbackReport;

    auto appendUser = [&] (const Ieee80211HeTriggerUserInfo& triggerUser,
            uint16_t userStaId, B userPsduLength) {
        physicallayer::Ieee80211HeUserTxVectorRequest user;
        user.ru.index = triggerUser.ruIndex;
        user.ru.toneSize = triggerUser.ruToneSize;
        user.ru.toneOffset = triggerUser.ruToneOffset;
        user.staId = userStaId;
        user.mcs = triggerUser.mcs;
        user.numberOfSpatialStreams = triggerUser.numberOfSpatialStreams;
        user.streamStartIndex = triggerUser.streamStartIndex;
        user.coding =
                static_cast<physicallayer::Ieee80211HeCoding>(triggerUser.coding);
        user.psduLength = userPsduLength;
        if (&triggerUser == &selected) {
            user.ndpFeedbackReport = ndpFeedbackReport;
            user.ndpFeedbackStatus = ndpFeedbackStatus;
            user.ndpRuToneSetIndex = ndpRuToneSetIndex;
            user.ndpStartingStsNumber = ndpStartingStsNumber;
        }
        request.users.push_back(user);
    };

    appendUser(selected, staId, psduLength);
    if (selected.muMimo) {
        for (unsigned int i = 0; i < trigger.getUsersArraySize(); ++i) {
            const auto& peer = trigger.getUsers(i);
            if (&peer == &selected || !peer.muMimo ||
                    peer.ruToneSize != selected.ruToneSize ||
                    peer.ruToneOffset != selected.ruToneOffset)
                continue;
            appendUser(peer, peer.aid, B(0));
        }
    }
    return physicallayer::Ieee80211HeTxVectorFactory::create(request);
}

std::optional<physicallayer::Ieee80211HeTxopDuration>
getIeee80211HeSolicitingTxopDuration(const Packet *packet)
{
    auto indication = packet == nullptr ? nullptr :
            packet->findTag<physicallayer::Ieee80211HeRxVectorInd>();
    if (indication == nullptr || indication->getRxVector() == nullptr)
        return std::nullopt;
    return indication->getRxVector()->getCommon().getTxopDuration();
}

HeTbResponseProtection deriveIeee80211HeTbResponseProtection(
        const std::optional<physicallayer::Ieee80211HeTxopDuration>& solicitingTxopDuration,
        simtime_t triggerDuration, simtime_t sifsTime, simtime_t responseTxTime)
{
    if (triggerDuration < SIMTIME_ZERO || sifsTime < SIMTIME_ZERO ||
            responseTxTime < SIMTIME_ZERO)
        throw cRuntimeError("Cannot derive HE-TB response protection from negative timing");
    auto remaining = std::max(SIMTIME_ZERO,
            triggerDuration - sifsTime - responseTxTime);
    int64_t remainingUs = remaining.inUnit(SIMTIME_US);
    if (SimTime(remainingUs, SIMTIME_US) < remaining)
        remainingUs++;
    HeTbResponseProtection result;
    result.macDurationField = SimTime(remainingUs, SIMTIME_US);
    if (solicitingTxopDuration.has_value() && solicitingTxopDuration->unspecified)
        result.txopDuration = {};
    else
        result.txopDuration = {false, static_cast<uint16_t>(
                std::min<int64_t>(8448, remainingUs))};
    return result;
}

HeTbResponseProtection attachHeTbTxVectorFromTrigger(Packet *packet,
        const Ieee80211TriggerFrame& trigger, const Ieee80211HeTriggerUserInfo& user,
        uint16_t staId, Hz centerFrequency, W transmitPower, B psduLength,
        uint8_t bssColor, uint32_t triggerId,
        bool ndpFeedbackReport, uint8_t ndpFeedbackStatus,
        uint8_t ndpRuToneSetIndex, uint8_t ndpStartingStsNumber,
        const std::optional<physicallayer::Ieee80211HeTxopDuration>& solicitingTxopDuration,
        simtime_t sifsTime)
{
    if (packet == nullptr)
        throw cRuntimeError("Cannot attach an HE-TB TXVECTOR to an empty packet");
    auto preliminary = createHeTbTxVector(trigger, user, centerFrequency, staId, psduLength,
            bssColor,
            ndpFeedbackReport, ndpFeedbackStatus, ndpRuToneSetIndex,
            ndpStartingStsNumber);
    if (!preliminary)
        throw cRuntimeError("Cannot construct preliminary Trigger-derived HE-TB TXVECTOR: %s (%s)",
                preliminary.getContext().fieldName.c_str(),
                preliminary.getContext().detail.c_str());
    auto protection = deriveIeee80211HeTbResponseProtection(
            solicitingTxopDuration, trigger.getDurationField(), sifsTime,
            preliminary.getPpduLayout()->getDuration());
    auto result = createHeTbTxVector(trigger, user, centerFrequency, staId, psduLength,
            bssColor,
            ndpFeedbackReport, ndpFeedbackStatus, ndpRuToneSetIndex,
            ndpStartingStsNumber, protection.txopDuration);
    if (!result)
        throw cRuntimeError("Cannot construct Trigger-derived HE-TB TXVECTOR: %s (%s)",
                result.getContext().fieldName.c_str(), result.getContext().detail.c_str());
    packet->addTag<physicallayer::Ieee80211HeTxVectorReq>()->setCanonicalPair(
            result.getTxVector(), result.getPpduLayout());
    packet->addTagIfAbsent<physicallayer::Ieee80211HeTriggerCorrelationTag>()->
            setTriggerId(triggerId);
    if (!std::isnan(transmitPower.get()))
        packet->addTagIfAbsent<SignalPowerReq>()->setPower(transmitPower);
    return protection;
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

std::optional<std::string> validateIeee80211HeUlTrigger(
        const Ieee80211TriggerFrame& trigger, Hz centerFrequency)
{
    using namespace physicallayer;
    const auto triggerType = trigger.getTriggerType();
    if (triggerType != IIeee80211HeUlTriggerPolicy::BASIC_TRIGGER &&
            triggerType != IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER &&
            triggerType != IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER)
        return "unsupported Trigger type";
    const auto bandwidth = Hz(trigger.getChannelBandwidthMhz() * 1e6);
    if (bandwidth != MHz(20) && bandwidth != MHz(40) &&
            bandwidth != MHz(80) && bandwidth != MHz(160))
        return "unsupported Trigger bandwidth";
    if (trigger.getUlLength() > 4095 || trigger.getUlLength() % 3 != 1 ||
            trigger.getCommonDuration() <= SIMTIME_ZERO ||
            trigger.getCommonDuration() > SimTime(5.484, SIMTIME_MS))
        return "invalid UL Length or common duration";
    const bool validGiLtf =
            (trigger.getGuardInterval() == HE_GI_1_6_US &&
             (trigger.getLtfType() == HE_LTF_1X || trigger.getLtfType() == HE_LTF_2X)) ||
            (trigger.getGuardInterval() == HE_GI_3_2_US &&
             trigger.getLtfType() == HE_LTF_4X);
    if (!validGiLtf || (trigger.getNumberOfHeLtfSymbols() != 1 &&
            trigger.getNumberOfHeLtfSymbols() != 2 &&
            trigger.getNumberOfHeLtfSymbols() != 4 &&
            trigger.getNumberOfHeLtfSymbols() != 6 &&
            trigger.getNumberOfHeLtfSymbols() != 8) ||
            trigger.getPreFecPaddingFactor() < 1 ||
            trigger.getPreFecPaddingFactor() > 4 ||
            trigger.getApTxPowerDbm() < -20 || trigger.getApTxPowerDbm() > 40)
        return "invalid Trigger common signaling";
    if (triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
        if (trigger.getUsersArraySize() != 0 || trigger.getNfrpFeedbackType() != 0 ||
                trigger.getNfrpStartingAid() > 4095 ||
                trigger.getGuardInterval() != HE_GI_3_2_US ||
                trigger.getLtfType() != HE_LTF_4X ||
                trigger.getNumberOfHeLtfSymbols() != 2)
            return "invalid NFRP Trigger fields";
        try {
            if (trigger.getNfrpStartingAid() +
                    IIeee80211HeUlScheduler::getNfrpScheduledStaCount(
                            bandwidth, trigger.getNfrpMultiplexingFlag()) > 4096)
                return "NFRP AID range exceeds 12 bits";
        }
        catch (const std::exception&) {
            return "invalid NFRP bandwidth";
        }
        return std::nullopt;
    }
    if (trigger.getUsersArraySize() == 0)
        return "Basic/BSRP Trigger contains no User Info records";

    auto catalog = getHeRuAllocationCatalog(centerFrequency, bandwidth);
    std::set<uint16_t> scheduledAids;
    std::map<std::pair<int, int>, std::vector<const Ieee80211HeTriggerUserInfo *>> usersPerRu;
    std::vector<Ieee80211HeRu> physicalRus;
    for (unsigned int i = 0; i < trigger.getUsersArraySize(); ++i) {
        const auto& user = trigger.getUsers(i);
        auto canonical = std::find_if(catalog.begin(), catalog.end(), [&] (const auto& ru) {
            return ru.index == user.ruIndex && ru.toneSize == user.ruToneSize &&
                    ru.toneOffset == user.ruToneOffset;
        });
        if (canonical == catalog.end())
            return "User Info RU is not canonical";
        if (user.mcs > 11 || user.numberOfSpatialStreams < 1 ||
                user.numberOfSpatialStreams > 8 || user.streamStartIndex > 7 ||
                user.streamStartIndex + user.numberOfSpatialStreams > 8)
            return "invalid User Info MCS or spatial streams";
        if (!user.useMaximumTransmitPower &&
                (user.targetRssiDbm < -110 || user.targetRssiDbm > -20))
            return "invalid User Info target RSSI";
        if (user.randomAccess) {
            if (user.aid != 0 || user.muMimo ||
                    user.numberOfSpatialStreams != 1 || user.streamStartIndex != 0)
                return "invalid associated-STA random-access User Info";
        }
        else if (user.aid == 0 || user.aid > 2007 ||
                !scheduledAids.insert(user.aid).second)
            return "invalid or duplicate scheduled AID";
        if (user.coding == HE_CODING_BCC &&
                (user.mcs > 9 || user.numberOfSpatialStreams > 4 ||
                 user.ruToneSize >= 484))
            return "invalid BCC User Info";
        auto geometry = std::make_pair(user.ruToneSize, user.ruToneOffset);
        if (usersPerRu[geometry].empty())
            physicalRus.push_back(*canonical);
        usersPerRu[geometry].push_back(&user);
    }
    if (!validateHeRuLayout(physicalRus, bandwidth))
        return "overlapping or out-of-band Trigger RU layout";
    const auto fullRu = getHeEqualRuLayout(centerFrequency, bandwidth, 1).front();
    bool fullBandwidthUlMuMimo = physicalRus.size() == 1;
    for (const auto& entry : usersPerRu) {
        const auto& users = entry.second;
        if (users.size() == 1) {
            if (users.front()->muMimo || users.front()->streamStartIndex != 0)
                return "single-user RU cannot use MU-MIMO or a nonzero starting stream";
            fullBandwidthUlMuMimo = false;
            continue;
        }
        if (users.size() > 8 || entry.first.first != fullRu.toneSize ||
                entry.first.second != fullRu.toneOffset)
            return "UL MU-MIMO requires at most eight users on the full-bandwidth RU";
        std::set<int> streams;
        for (const auto user : users) {
            if (!user->muMimo || user->randomAccess ||
                    user->numberOfSpatialStreams > 4)
                return "shared RU is not scheduled UL MU-MIMO";
            for (int stream = user->streamStartIndex;
                    stream < user->streamStartIndex + user->numberOfSpatialStreams; ++stream)
                if (!streams.insert(stream).second)
                    return "UL MU-MIMO spatial streams overlap";
        }
        if (streams.empty() || streams.size() > 8 || *streams.begin() != 0 ||
                *streams.rbegin() + 1 != static_cast<int>(streams.size()))
            return "UL MU-MIMO spatial streams are gapped or exceed eight streams";
    }
    if (trigger.getLtfType() == HE_LTF_1X && !fullBandwidthUlMuMimo)
        return "1x HE-LTF requires full-bandwidth UL MU-MIMO";
    return std::nullopt;
}

bool HeHcf::allAssociatedStationsSupportPreamblePuncturing() const
{
    return std::all_of(mac->getMib()->bssAccessPointData.stations.begin(),
            mac->getMib()->bssAccessPointData.stations.end(), [&] (const auto& station) {
                auto capabilities = mac->getMib()->findNegotiatedHeCapabilities(station.first);
                return station.second != Ieee80211Mib::ASSOCIATED ||
                        (capabilities != nullptr && capabilities->localRxPeerTx.valid &&
                         capabilities->localRxPeerTx.preamblePuncturing);
            });
}

bool HeHcf::supportsPreamblePuncturing(const IIeee80211HeUlScheduler::RuAllocation& allocation) const
{
    if (allocation.randomAccess)
        return allAssociatedStationsSupportPreamblePuncturing();
    auto capabilities = mac->getMib()->findNegotiatedHeCapabilities(allocation.staAddress);
    return capabilities != nullptr && capabilities->localRxPeerTx.valid && capabilities->localRxPeerTx.preamblePuncturing;
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
        user.coding = allocation.coding;
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
    if (!feedbackNdp && proposedSchedule.ulLength != 0) {
        physicallayer::Ieee80211HeTbCapacityBoundary boundary;
        boundary.channelBandwidth = channelBandwidth;
        boundary.ulLength = proposedSchedule.ulLength;
        boundary.guardInterval = proposedSchedule.guardInterval;
        boundary.ltfType = proposedSchedule.ltfType;
        boundary.preFecPaddingFactor = proposedSchedule.preFecPaddingFactor;
        boundary.ldpcExtraSymbolSegment = proposedSchedule.ldpcExtraSymbolSegment;
        boundary.peDisambiguity = proposedSchedule.peDisambiguity;
        boundary.numberOfHeLtfSymbols = proposedSchedule.numberOfHeLtfSymbols;
        boundary.packetExtensionDurationUs = proposedSchedule.packetExtensionDurationUs;
        request.fixedBoundary = boundary;
    }
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
    const auto triggerType = pendingUlTrigger;
    const auto phy = getLinkPhyContext().getSnapshot();
    auto centerFrequency = phy.getChannelCenterFrequency();
    auto channelBandwidth = phy.getChannelBandwidth();
    auto edcaf = edca->getEdcaf(ac);
    simtime_t txopLimit = SIMTIME_ZERO;
    if (edcaf->getTxopProcedure()->getLimit() > SIMTIME_ZERO)
        txopLimit = std::max(SIMTIME_ZERO,
                edcaf->getTxopProcedure()->getLimit() - edcaf->getTxopProcedure()->getDuration());
    auto sensitivityDbm = math::mW2dBmW(phy.getReceiveSensitivity().get<mW>());
    const auto puncturedSubchannels = triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER ?
            std::vector<bool>() : phy.getPuncturedSubchannels();
    if (std::any_of(puncturedSubchannels.begin(), puncturedSubchannels.end(),
            [] (bool punctured) { return punctured; })) {
        EV_WARN << "HE UL skipping Trigger because the modeled HE-TB Trigger fields "
                << "do not carry a punctured response bandwidth\n";
        return false;
    }
    IIeee80211HeUlScheduler::Schedule ulSchedule;
    IIeee80211HeUlScheduler::ScheduleContext schedulerContext;
    bool schedulerPrepared = false;
    bool useUlMuMimoPolicy = false;
    // 9.3.1.22 encodes the triggering AP's combined transmit power normalized
    // to 20 MHz in one-dB steps. Keep the projected value in the schedule so
    // the frame sequence and serializer cannot silently substitute a default.
    auto apTxPowerDbm20Mhz = math::mW2dBmW(phy.getMaximumTransmitPower().get<mW>()) -
            10 * std::log10(channelBandwidth.get() / 20e6);
    if (triggerType == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER ||
            triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
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
            if (triggerType != IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER &&
                    (index >= maxRus || index >= maxMuStations))
                break;
            if (isTwtSleeping(mac, station.first)) {
                EV_DEBUG << "HE UL BSRP: skipping sleeping TWT STA " << station.first << "\n";
                continue;
            }
            auto negotiated = mac->getMib()->findNegotiatedHeCapabilities(station.first);
            if (triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER &&
                    (negotiated == nullptr || !negotiated->localRxPeerTx.valid ||
                     !negotiated->localRxPeerTx.transmitterCanTransmitNdpFeedbackReport))
                continue;
            if (triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
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
        if (triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
            if (nfrpEligibleAids.empty()) {
                EV_WARN << "HE UL skipping NFRP Trigger because no awake associated STA negotiated NDP feedback\n";
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
        while (index < maxRus && triggerType != IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
            IIeee80211HeUlScheduler::RuAllocation allocation;
            allocation.randomAccess = true;
            allocation.associationId = 0;
            allocation.ru = layout[index++];
            allocation.targetRssiDbm = (int)std::round(sensitivityDbm + (double)par("ulTargetRssiMargin"));
            ulSchedule.allocations.push_back(allocation);
        }
        ulSchedule.commonDuration = std::min(SimTime(par("maxHeTbPpduDuration")), txopLimit > SIMTIME_ZERO ?
                txopLimit : SimTime(par("maxHeTbPpduDuration")));
        for (auto& allocation : ulSchedule.allocations)
            allocation.estimatedDuration = ulSchedule.commonDuration;
    }
    else {
        int staleOrUnknown = 0;
        int fullBandwidthMuMimoCandidates = 0;
        const auto fullBandwidthRu = physicallayer::getHeEqualRuLayout(
                centerFrequency, channelBandwidth, 1).front();
        for (const auto& station : mac->getMib()->bssAccessPointData.stations) {
            if (station.second != Ieee80211Mib::ASSOCIATED)
                continue;
            auto aid = mac->getMib()->getAssociationId(station.first);
            auto status = ulCoordinator->getBufferStatus().find(aid);
            if (status == ulCoordinator->getBufferStatus().end() ||
                    simTime() - status->second.updateTime > ulCoordinator->getReportMaxAge()) {
                staleOrUnknown++;
                continue;
            }
            bool hasServiceRequest = false;
            for (const auto& estimate : status->second.backlogEstimates)
                hasServiceRequest |= estimate.getConservativeBytes() > 0 ||
                        estimate.kind == Ieee80211HeQueueSizeKind::UNKNOWN;
            auto negotiated = mac->getMib()->findNegotiatedHeCapabilities(station.first);
            Ieee80211HeOperatingMode mode;
            const bool disabled = isTwtSleeping(mac, station.first) ||
                    (getPeerOperatingMode(station.first, mode) && mode.ulMuDisable);
            if (hasServiceRequest && !disabled && negotiated != nullptr &&
                    negotiated->localRxPeerTx.valid &&
                    negotiated->localRxPeerTx.transmitterCanTransmitFullBandwidthUlMuMimo &&
                    negotiated->localRxPeerTx.supportedRuToneSizes.count(
                            fullBandwidthRu.toneSize) != 0 &&
                    negotiated->localRxPeerTx.mcsNss.maxMcsPerNss[0] >= 0)
                fullBandwidthMuMimoCandidates++;
        }
        useUlMuMimoPolicy = par("enableUlMuMimo").boolValue() &&
                fullBandwidthMuMimoCandidates >= 2;
        physicallayer::Ieee80211HeTbCapacityBoundary capacityBoundary;
        const auto boundaryLayout = physicallayer::getHeEqualRuLayout(centerFrequency,
                channelBandwidth, physicallayer::getHeMaxRuCount(channelBandwidth));
        physicallayer::Ieee80211HeUserPhyParameters boundaryUser;
        boundaryUser.ru = boundaryLayout.front();
        boundaryUser.mcs = 0;
        boundaryUser.coding = physicallayer::HE_CODING_BCC;
        boundaryUser.psduLength = B(1);
        auto boundaryLdpcUser = boundaryUser;
        boundaryLdpcUser.ru = boundaryLayout[1];
        boundaryLdpcUser.coding = physicallayer::HE_CODING_LDPC;
        physicallayer::Ieee80211HeTriggerResponseFinalizationRequest boundaryRequest;
        // Seed both coding families so the immutable common boundary is not
        // derived from a one-user BCC-only approximation.
        boundaryRequest.users = {boundaryUser, boundaryLdpcUser};
        boundaryRequest.centerFrequency = centerFrequency;
        boundaryRequest.channelBandwidth = channelBandwidth;
        boundaryRequest.guardInterval = phy.getGuardInterval() == physicallayer::HE_GI_3_2_US ?
                physicallayer::HE_GI_3_2_US : physicallayer::HE_GI_1_6_US;
        boundaryRequest.ltfType = boundaryRequest.guardInterval == physicallayer::HE_GI_3_2_US ?
                physicallayer::HE_LTF_4X : useUlMuMimoPolicy ?
                physicallayer::HE_LTF_1X : physicallayer::HE_LTF_2X;
        boundaryRequest.packetExtensionDurationUs = phy.getPacketExtensionDurationUs();
        boundaryRequest.durationBudget = std::min(SimTime(par("maxHeTbPpduDuration")),
                txopLimit > SIMTIME_ZERO ? txopLimit : SimTime(par("maxHeTbPpduDuration")));
        auto boundaryFinalization = physicallayer::finalizeHeTriggerResponse(boundaryRequest);
        if (!boundaryFinalization) {
            EV_WARN << "HE UL skipping OFDMA optimization because the common timing boundary "
                    << "cannot be finalized: " << boundaryFinalization.error << "\n";
            return false;
        }
        capacityBoundary.channelBandwidth = channelBandwidth;
        capacityBoundary.ulLength = boundaryFinalization.ulLength;
        capacityBoundary.guardInterval = boundaryRequest.guardInterval;
        capacityBoundary.ltfType = boundaryRequest.ltfType;
        capacityBoundary.preFecPaddingFactor =
                boundaryFinalization.parameters.common.preFecPaddingFactor;
        capacityBoundary.ldpcExtraSymbolSegment =
                boundaryFinalization.parameters.common.ldpcExtraSymbol;
        capacityBoundary.peDisambiguity = boundaryFinalization.peDisambiguity;
        capacityBoundary.numberOfHeLtfSymbols =
                boundaryFinalization.parameters.common.numberOfHeLtfSymbols;
        capacityBoundary.packetExtensionDurationUs =
                boundaryFinalization.parameters.common.packetExtensionDurationUs;
        ulSchedule = ulCoordinator->prepareSchedule(mac->getMib(), getLinkPhyContext(),
                SimTime(par("linkEstimateMaxAge")), centerFrequency, channelBandwidth,
                txopLimit, par("maxHeTbPpduDuration"), sensitivityDbm,
                par("ulTargetRssiMargin"), staleOrUnknown, 0, 0,
                [this] (const MacAddress& address) {
                    Ieee80211HeOperatingMode mode;
                    return isTwtSleeping(mac, address) ||
                            (getPeerOperatingMode(address, mode) && mode.ulMuDisable);
                }, &schedulerContext, &capacityBoundary,
                useUlMuMimoPolicy);
        ulSchedule.ulLength = capacityBoundary.ulLength;
        ulSchedule.guardInterval = capacityBoundary.guardInterval;
        ulSchedule.ltfType = capacityBoundary.ltfType;
        ulSchedule.preFecPaddingFactor = capacityBoundary.preFecPaddingFactor;
        ulSchedule.ldpcExtraSymbolSegment = capacityBoundary.ldpcExtraSymbolSegment;
        ulSchedule.peDisambiguity = capacityBoundary.peDisambiguity;
        ulSchedule.numberOfHeLtfSymbols = capacityBoundary.numberOfHeLtfSymbols;
        ulSchedule.packetExtensionDurationUs = capacityBoundary.packetExtensionDurationUs;
        schedulerPrepared = true;
    }
    if (!schedulerPrepared)
        ulSchedule.packetExtensionDurationUs = phy.getPacketExtensionDurationUs();
    // Select one complete Table 9-49 GI/HE-LTF pair. Full-bandwidth UL
    // MU-MIMO may use the raw-0 1x/1.6 us pair; other medium-GI schedules use
    // raw 1 (2x/1.6 us), and long GI uses raw 2 (4x/3.2 us).
    switch (phy.getGuardInterval()) {
        case physicallayer::HE_GI_0_8_US:
        case physicallayer::HE_GI_1_6_US:
            ulSchedule.guardInterval = physicallayer::HE_GI_1_6_US;
            ulSchedule.ltfType = std::any_of(ulSchedule.allocations.begin(), ulSchedule.allocations.end(),
                    [] (const auto& allocation) { return allocation.muMimo; }) ?
                    physicallayer::HE_LTF_1X : physicallayer::HE_LTF_2X;
            break;
        case physicallayer::HE_GI_3_2_US:
            ulSchedule.guardInterval = physicallayer::HE_GI_3_2_US;
            ulSchedule.ltfType = physicallayer::HE_LTF_4X;
            break;
    }
    // Table 27-32 permits only 4x HE-LTF with 3.2 us GI for feedback NDP.
    if (triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
        ulSchedule.guardInterval = physicallayer::HE_GI_3_2_US;
        ulSchedule.ltfType = physicallayer::HE_LTF_4X;
    }
    // UL FEC Coding Type is a per-User Info field. Scheduled users retain the
    // scheduler's capability-aware choice; UORA uses the universally legal
    // 26-tone/MCS-0 BCC combination because the responder is not yet known.
    for (auto& allocation : ulSchedule.allocations)
        if (allocation.randomAccess)
            allocation.coding = physicallayer::HE_CODING_BCC;
    if (triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
        ulSchedule.coding = physicallayer::HE_CODING_BCC;
        ulSchedule.packetExtensionDurationUs = 0;
    }
    ulSchedule.apTxPowerDbm = std::clamp((int)std::lround(apTxPowerDbm20Mhz), -20, 40);
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

    HeUlMuPlan::ValidationContext validationContext;
    validationContext.centerFrequency = centerFrequency;
    validationContext.requireSchedulerCandidate = schedulerPrepared;
    for (const auto& station : mac->getMib()->bssAccessPointData.stations) {
        if (station.second != Ieee80211Mib::ASSOCIATED)
            continue;
        auto capabilities = mac->getMib()->findNegotiatedHeCapabilities(station.first);
        HeUlMuPlan::StationContract contract;
        contract.station = station.first;
        contract.associationId = mac->getMib()->getAssociationId(station.first);
        if (capabilities != nullptr)
            contract.capabilities = *capabilities;
        contract.schedulerCandidate = !schedulerPrepared ||
                std::any_of(schedulerContext.candidates.begin(), schedulerContext.candidates.end(),
                        [&] (const auto& candidate) { return candidate.staAddress == station.first; });
        Ieee80211HeOperatingMode operatingMode;
        contract.ulMuDisabled = getPeerOperatingMode(station.first, operatingMode) && operatingMode.ulMuDisable;
        validationContext.stations.push_back(contract);
    }
    HeUlMuPlanDiagnostic diagnostic;
    auto ulPlan = HeUlMuPlan::create(validationContext, ulSchedule, triggerType, diagnostic);
    if (!ulPlan) {
        EV_WARN << "HE UL plan rejected: code=" << (int)diagnostic.code
                << ", allocation=" << diagnostic.allocationIndex
                << ", station=" << diagnostic.station
                << ", detail=" << diagnostic.detail
                << "; no Trigger transmitted and coordinator state preserved\n";
        return false;
    }

    EV_INFO << "HE UL starting"
             << (triggerType == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER ? " BSRP" :
                     triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER ? " NFRP" : " Basic")
             << " exchange with " << ulPlan->getSchedule().allocations.size()
             << " RU allocations for " << ulPlan->getSchedule().commonDuration << "\n";
    frameSequenceHandler->startFrameSequence(
            new HeUlMuTxOpFs(ulCoordinator, this, *ulPlan, modeSet, mac->getAddress()),
            buildContext(ac), this);
    pendingUlTrigger = IIeee80211HeUlTriggerPolicy::NO_TRIGGER;
    emit(IFrameSequenceHandler::frameSequenceStartedSignal, frameSequenceHandler->getContext());
    return true;
}

void HeHcf::processTriggeredUlFrame(Packet *packet, const Ptr<const Ieee80211DataHeader>& header, uint16_t aid)
{
    emit(packetReceivedFromPeerSignal, packet);
    if (header->getBufferStatusPresent())
    {
        Ieee80211HeQueueSizeEstimate estimate;
        estimate.kind = static_cast<Ieee80211HeQueueSizeKind>(
                header->getBufferStatusQueueSizeKind());
        estimate.lowerBoundBytes = header->getBufferStatusQueueSizeLowerBound();
        estimate.upperBoundBytes = header->getBufferStatusQueueSizeUpperBound();
        estimate.hasUpperBound = header->getBufferStatusQueueSizeHasUpperBound();
        if (estimate.lowerBoundBytes == 0 &&
                header->getBufferStatusQueueSize() != 0) {
            estimate.lowerBoundBytes = header->getBufferStatusQueueSize();
            estimate.upperBoundBytes = estimate.lowerBoundBytes;
            estimate.hasUpperBound = true;
        }
        ulCoordinator->updateBufferStatus(aid, header->getTransmitterAddress(),
                static_cast<AccessCategory>(header->getBufferStatusAc()),
                header->getBufferStatusTid(), estimate);
    }
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
    blockAck->setTidInfo(user.muBarTidInfo);
    blockAck->setBlockAckBitmap(bitmap);
    blockAck->setDurationField(SIMTIME_ZERO);
    return blockAck;
}

void HeHcf::sendTriggeredBlockAckResponse(Packet *packet, const Ptr<const Ieee80211TriggerFrame>& trigger,
        uint32_t triggerId)
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
                    selected->muBarTidInfo, trigger->getTransmitterAddress());
    if (selected == nullptr)
        EV_WARN << "Ignoring MU-BAR Trigger because it has no User Info for local AID " << myAid << endl;
    else if (recipientBlockAckAgreementHandler == nullptr)
        EV_WARN << "Ignoring MU-BAR Trigger for AID " << myAid << " because no recipient Block Ack handler is installed" << endl;
    else if (agreement == nullptr)
        EV_WARN << "Ignoring MU-BAR Trigger for AID " << myAid << " because no recipient Block Ack agreement exists for TID "
                << (int)selected->muBarTidInfo << " and originator " << trigger->getTransmitterAddress() << endl;
    if (agreement != nullptr) {
        if (!selected->muBarCompressedBitmap || selected->muBarMultiTid)
            throw cRuntimeError("Unsupported MU-BAR BlockAckReq variant");
        auto blockAck = buildHeMuBarCompressedBlockAck(*selected, agreement,
                trigger->getTransmitterAddress(), mac->getAddress());
        auto response = new Packet("HE-TB-BlockAck", blockAck);
        response->insertAtBack(makeShared<Ieee80211MacTrailer>());
        const auto phy = getLinkPhyContext().getSnapshot();
        auto protection = attachHeTbTxVectorFromTrigger(response, *trigger, *selected, myAid,
                phy.getChannelCenterFrequency(), phy.getMaximumTransmitPower(),
                B((response->getDataLength().get<b>() + 7) / 8),
                mac->getMib()->heOperation.bssColor, triggerId, false, 0, 0, 0,
                getIeee80211HeSolicitingTxopDuration(packet),
                modeSet->getSifsTime());
        auto writableBlockAck = response->removeAtFront<Ieee80211CompressedBlockAck>();
        writableBlockAck->setDurationField(protection.macDurationField);
        response->insertAtFront(writableBlockAck);
        auto trailer = response->removeAtBack<Ieee80211MacTrailer>(B(4));
        auto fcsMode = mac->getFcsMode();
        trailer->setFcsMode(fcsMode);
        if (fcsMode == FCS_COMPUTED)
            trailer->setFcs(computeEthernetFcs(response, fcsMode));
        response->insertAtBack(trailer);
        tx->transmitFrame(response,
                response->peekAtFront<Ieee80211CompressedBlockAck>(),
                modeSet->getSifsTime(), this);
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
            auto header = pkt->peekAtFront<Ieee80211DataHeader>();
            edca->getEdcaf(edca->mapTidToAc(header->getTid()))->
                    getRecoveryProcedure()->dataFrameTransmissionFailed(pkt, header);
            auto writableHeader = pkt->removeAtFront<Ieee80211DataHeader>();
            writableHeader->setRetry(true);
            pkt->insertAtFront(writableHeader);
            entry.second.sourceQueue->pushPacket(pkt, nullptr);
        }
    }
    triggeredUlExchanges.clear();
    cancelEvent(triggeredUlResponseTimer);
}

void HeHcf::scheduleTriggeredUlResponseTimeout()
{
    cancelEvent(triggeredUlResponseTimer);
    if (triggeredUlExchanges.empty())
        return;
    auto deadline = std::min_element(triggeredUlExchanges.begin(), triggeredUlExchanges.end(),
            [] (const auto& left, const auto& right) {
                return left.second.expectedResponseTime < right.second.expectedResponseTime;
            })->second.expectedResponseTime;
    scheduleAt(std::max(simTime(), deadline), triggeredUlResponseTimer);
}

void HeHcf::handleTriggeredUlResponseTimeout()
{
    for (auto it = triggeredUlExchanges.begin(); it != triggeredUlExchanges.end(); ) {
        if (it->second.expectedResponseTime > simTime()) {
            ++it;
            continue;
        }
        auto& exchange = it->second;
        EV_WARN << "HE-TB response timeout: trigger=" << it->first
                << ", packets=" << exchange.packets.size() << "\n";
        if (exchange.randomAccess)
            ulCoordinator->reportRandomAccessResult(false);
        for (auto packet : exchange.packets) {
            auto header = packet->peekAtFront<Ieee80211DataHeader>();
            edca->getEdcaf(edca->mapTidToAc(header->getTid()))->
                    getRecoveryProcedure()->dataFrameTransmissionFailed(packet, header);
            auto writableHeader = packet->removeAtFront<Ieee80211DataHeader>();
            writableHeader->setRetry(true);
            packet->insertAtFront(writableHeader);
            exchange.sourceQueue->pushPacket(packet, nullptr);
        }
        it = triggeredUlExchanges.erase(it);
    }
    scheduleTriggeredUlResponseTimeout();
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
        ampdu->insertAtBack(mpdus[i]->peekData());
        int padding = (4 - (B(4) + B(mpdus[i]->getByteLength())).get<B>() % 4) % 4;
        if (i + 1 != mpdus.size() && padding != 0)
            ampdu->insertAtBack(makeShared<ByteCountChunk>(B(padding)));
    }
    return ampdu;
}

Packet *HeHcf::buildTriggeredUlResponsePacket(Packet *sourcePacket, queueing::IPacketQueue *sourceQueue,
        AccessCategory selectedAc, uint8_t selectedTid, int64_t queueBytes, int availableSlots,
        const Ieee80211HeTriggerUserInfo *selected, const Ptr<const Ieee80211TriggerFrame>& trigger,
        uint32_t triggerId, W transmitPower,
        const std::optional<physicallayer::Ieee80211HeTxopDuration>& solicitingTxopDuration,
        TriggeredUlExchange& exchange, Ptr<const Ieee80211MacHeader>& responseHeader,
        bool& committed)
{
    committed = false;
    responseHeader = nullptr;
    if (sourceQueue == nullptr || selected == nullptr || trigger == nullptr)
        throw cRuntimeError("Cannot prepare an HE-TB response without queue and Trigger context");
    const auto phy = getLinkPhyContext().getSnapshot();
    auto qosDataService = check_and_cast<OriginatorQosMacDataService *>(originatorDataService);
    auto preparedSequenceNumberState = qosDataService->cloneSequenceNumberState();
    std::vector<Packet *> originalPackets;
    std::vector<std::unique_ptr<Packet>> preparedPacketOwners;
    std::unique_ptr<Packet> nullMpdu;
    std::unique_ptr<Packet> responsePacket;
    const bool hadPendingPayload = sourcePacket != nullptr;
    int64_t reportedQueueBytes = queueBytes;
    if (sourcePacket != nullptr) {
        // 26.5.2.4 requires a QoS Null response when the allocation cannot
        // contain pending data. Check the first MPDU too; the aggregation loop
        // below performs the same check for every additional MPDU.
        auto sourceHeader = sourcePacket->peekAtFront<Ieee80211DataHeader>();
        B psduLength = B(4 + sourcePacket->getByteLength()) +
                (sourceHeader->getBufferStatusPresent() ? B(0) : B(4));
        auto prospective = createHeTbTxVector(*trigger, *selected,
                phy.getChannelCenterFrequency(),
                mac->getMib()->bssStationData.associationId, psduLength);
        if (!prospective)
            sourcePacket = nullptr;
    }
    if (sourcePacket != nullptr) {
        auto originalSourcePacket = sourcePacket;
        preparedPacketOwners.emplace_back(sourcePacket->dup());
        sourcePacket = preparedPacketOwners.back().get();
        originalPackets.push_back(originalSourcePacket);
        // IEEE Std 802.11-2024 Table 9-13, 10.3.2.13.3, and 26.4.4.5:
        // Ack Policy wire bits 00 are context-dependent in an A-MPDU. They
        // denote Implicit BAR on preceding untagged MPDUs and Normal Ack on
        // the tagged final MPDU that solicits the immediate Multi-STA Block Ack.
        auto writableHeader = sourcePacket->removeAtFront<Ieee80211DataHeader>();
        if (!writableHeader->getRetry())
            preparedSequenceNumberState->assignSequenceNumber(writableHeader);
        if (!writableHeader->getBufferStatusPresent())
            writableHeader->setChunkLength(writableHeader->getChunkLength() + B(4));
        writableHeader->setOrder(true);
        writableHeader->setAckPolicy(NORMAL_ACK);
        writableHeader->setBufferStatusPresent(true);
        writableHeader->setBufferStatusTid(selectedTid);
        writableHeader->setBufferStatusAc(selectedAc);
        writableHeader->setBufferStatusQueueSize(queueBytes);
        sourcePacket->insertAtFront(writableHeader);
        responsePacket.reset(sourcePacket->dup());
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
        // IEEE Std 802.11-2024 Table 9-13, 10.3.2.13.3, and 26.4.4.5:
        // this single, tagged final QoS Null MPDU uses wire bits 00 as Normal
        // Ack to solicit the immediate Multi-STA Block Ack.
        nullHeader->setAckPolicy(NORMAL_ACK);
        nullHeader->setOrder(true);
        nullHeader->setBufferStatusPresent(true);
        nullHeader->setBufferStatusTid(selectedTid);
        nullHeader->setBufferStatusAc(selectedAc);
        nullHeader->setBufferStatusQueueSize(queueBytes);
        nullHeader->setChunkLength(B(30));
        preparedSequenceNumberState->assignSequenceNumber(nullHeader);
        nullMpdu = std::make_unique<Packet>("HE-TB-QoS-Null", nullHeader);
        nullMpdu->insertAtBack(makeShared<Ieee80211MacTrailer>());
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
            if (candidate == originalPackets.front())
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
            auto prospective = createHeTbTxVector(*trigger, *selected,
                    phy.getChannelCenterFrequency(),
                    mac->getMib()->bssStationData.associationId, psduLength);
            if (!prospective)
                break;
            auto originalCandidate = candidate;
            preparedPacketOwners.emplace_back(candidate->dup());
            candidate = preparedPacketOwners.back().get();
            auto writableCandidateHeader = candidate->removeAtFront<Ieee80211DataHeader>();
            if (!writableCandidateHeader->getRetry())
                preparedSequenceNumberState->assignSequenceNumber(writableCandidateHeader);
            writableCandidateHeader->setOrder(true);
            writableCandidateHeader->setAckPolicy(NORMAL_ACK);
            candidate->insertAtFront(writableCandidateHeader);
            originalPackets.push_back(originalCandidate);
            exchange.packets.push_back(candidate);
            exchange.sequenceNumbers.push_back(writableCandidateHeader->getSequenceNumber().get());
        }
        for (auto pkt : exchange.packets)
            reportedQueueBytes = std::max<int64_t>(0, reportedQueueBytes - pkt->getByteLength());
        auto firstHeader = exchange.packets.front()->removeAtFront<Ieee80211DataHeader>();
        firstHeader->setBufferStatusQueueSize(reportedQueueBytes);
        exchange.packets.front()->insertAtFront(firstHeader);
    }

    auto finalizeMpdu = [&] (Packet *mpdu, simtime_t durationField) {
        auto header = mpdu->removeAtFront<Ieee80211DataHeader>();
        header->setDurationField(durationField);
        mpdu->insertAtFront(header);
        auto trailer = mpdu->removeAtBack<Ieee80211MacTrailer>(B(4));
        auto fcsMode = mac->getFcsMode();
        trailer->setFcsMode(fcsMode);
        if (fcsMode == FCS_COMPUTED)
            trailer->setFcs(computeEthernetFcs(mpdu, fcsMode));
        mpdu->insertAtBack(trailer);
    };
    auto buildResponseAmpdu = [&] {
        if (sourcePacket != nullptr)
            responsePacket.reset(buildHeTbAmpdu(exchange.packets));
        else
            responsePacket.reset(buildHeTbAmpdu({nullMpdu.get()}));
    };
    if (sourcePacket != nullptr)
        for (auto mpdu : exchange.packets)
            finalizeMpdu(mpdu, SIMTIME_ZERO);
    else
        finalizeMpdu(nullMpdu.get(), SIMTIME_ZERO);
    buildResponseAmpdu();

    // Complete and validate the Trigger-derived request while all selected
    // queue packets and live sequence counters are still untouched.
    auto txVector = createHeTbTxVector(*trigger, *selected,
            phy.getChannelCenterFrequency(),
            mac->getMib()->bssStationData.associationId,
            B((responsePacket->getDataLength().get<b>() + 7) / 8));
    if (!txVector)
        throw cRuntimeError("Prepared HE-TB response is invalid: %s (%s)",
                txVector.getContext().fieldName.c_str(),
                txVector.getContext().detail.c_str());
    auto protection = deriveIeee80211HeTbResponseProtection(
            solicitingTxopDuration, trigger->getDurationField(),
            modeSet->getSifsTime(), txVector.getPpduLayout()->getDuration());
    if (sourcePacket != nullptr)
        for (auto mpdu : exchange.packets)
            finalizeMpdu(mpdu, protection.macDurationField);
    else
        finalizeMpdu(nullMpdu.get(), protection.macDurationField);
    buildResponseAmpdu();
    auto attachedProtection = attachHeTbTxVectorFromTrigger(
            responsePacket.get(), *trigger, *selected,
            mac->getMib()->bssStationData.associationId,
            phy.getChannelCenterFrequency(), transmitPower,
            B((responsePacket->getDataLength().get<b>() + 7) / 8),
            mac->getMib()->heOperation.bssColor, triggerId, false, 0, 0, 0,
            solicitingTxopDuration, modeSet->getSifsTime());
    if (attachedProtection.macDurationField != protection.macDurationField ||
            !(attachedProtection.txopDuration == protection.txopDuration))
        throw cRuntimeError("HE-TB response protection changed while rebuilding the unchanged-length PSDU");

    if (!originalPackets.empty()) {
        for (auto original : originalPackets) {
            bool found = false;
            for (int i = 0; i < sourceQueue->getNumPackets(); ++i)
                found |= sourceQueue->getPacket(i) == original;
            if (!found)
                throw cRuntimeError("HE-TB selected packet changed queue membership before commit");
        }
        for (size_t i = 0; i < originalPackets.size(); ++i)
            beforeTriggeredUlPacketCommit(i);
    }

    // Explicit commit boundary. QoS Null responses also consume a sequence
    // number even though they retain no queued MPDU. Post-boundary failures
    // are invariant violations and deliberately fail loudly instead of
    // faking rollback after observer-visible state changes.
    committed = true;
    qosDataService->commitSequenceNumberState(*preparedSequenceNumberState);
    if (!originalPackets.empty()) {
        for (size_t i = 0; i < originalPackets.size(); ++i) {
            auto original = originalPackets[i];
            auto prepared = preparedPacketOwners[i].get();
            sourceQueue->removePacket(original);
            original->removeAll();
            original->insertAtBack(prepared->peekAll());
            original->setFrontOffset(prepared->getFrontOffset());
            original->setBackOffset(prepared->getBackOffset());
            original->clearTags();
            original->copyTags(*prepared);
            original->getRegionTags() = prepared->getRegionTags();
            take(original);
            exchange.packets[i] = original;
        }
    }
    // The PSDU starts with an A-MPDU delimiter, so its first MAC header is
    // deliberately retained from the inner MPDU instead of being re-peeked
    // through the aggregate representation by the Tx handoff.
    responseHeader = sourcePacket != nullptr ?
            exchange.packets.front()->peekAtFront<Ieee80211MacHeader>() :
            nullMpdu->peekAtFront<Ieee80211MacHeader>();
    HeTbResponseEvent event;
    event.triggerId = triggerId;
    event.triggerType = static_cast<IIeee80211HeUlTriggerPolicy::TriggerType>(
            trigger->getTriggerType());
    event.reason = sourcePacket != nullptr ? HeTbResponseEvent::DATA_SELECTED :
            trigger->getTriggerType() == IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER ?
                    HeTbResponseEvent::BUFFER_STATUS_REPORTED :
            hadPendingPayload || queueBytes > 0 ? HeTbResponseEvent::NO_FITTING_PAYLOAD :
                    HeTbResponseEvent::NO_PENDING_DATA;
    event.associationId = mac->getMib()->bssStationData.associationId;
    event.tid = selectedTid;
    event.accessCategory = selectedAc;
    event.ruIndex = selected->ruIndex;
    event.ruToneSize = selected->ruToneSize;
    event.ruToneOffset = selected->ruToneOffset;
    for (auto selectedPacket : exchange.packets)
        event.selectedBytes += selectedPacket->getByteLength();
    event.reportedBytes = reportedQueueBytes;
    auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(responseHeader);
    event.ackPolicy = dataHeader == nullptr ? -1 : dataHeader->getAckPolicy();
    emit(heTbResponseCommittedSignal, &event);
    return responsePacket.release();
}

void HeHcf::processReceivedTriggerFrame(Packet *packet, const Ptr<const Ieee80211TriggerFrame>& trigger)
{
    // IEEE 802.11-2024 9.3.1.22 Table 9-47: Trigger type 2 is MU-BAR.  A
    // MU-BAR Trigger carries BAR control/information in each User Info field
    // and solicits BlockAck responses in HE TB PPDUs.
    if (trigger->getTransmitterAddress() != mac->getMib()->bssData.bssid) {
        EV_WARN << "Ignoring Trigger from non-associated AP "
                << trigger->getTransmitterAddress() << "\n";
        delete packet;
        return;
    }
    auto correlation = packet->findTag<physicallayer::Ieee80211HeTriggerCorrelationTag>();
    if (correlation == nullptr || correlation->getTriggerId() == 0) {
        EV_WARN << "Ignoring Trigger without model-only correlation context\n";
        delete packet;
        return;
    }
    const uint32_t triggerId = correlation->getTriggerId();
    if (trigger->getTriggerType() == 2) {
        sendTriggeredBlockAckResponse(packet, trigger, triggerId);
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
    const auto phy = getLinkPhyContext().getSnapshot();
    if (Hz(trigger->getChannelBandwidthMhz() * 1e6) != phy.getChannelBandwidth()) {
        EV_WARN << "Ignoring HE UL Trigger whose bandwidth differs from the active link\n";
        delete packet;
        return;
    }
    auto triggerError = validateIeee80211HeUlTrigger(*trigger,
            phy.getChannelCenterFrequency());
    if (triggerError) {
        EV_WARN << "Ignoring malformed HE UL Trigger: " << *triggerError << "\n";
        delete packet;
        return;
    }
    if (isTwtSleeping(mac, mac->getMib()->bssData.bssid)) {
        delete packet;
        return;
    }
    if (!triggeredUlExchanges.empty()) {
        EV_WARN << "Ignoring HE UL Trigger while an earlier HE-TB exchange awaits Multi-STA BA\n";
        delete packet;
        return;
    }
    auto solicitingTxopDuration = getIeee80211HeSolicitingTxopDuration(packet);
    std::optional<double> triggerPathLossDb;
    auto signalPower = packet->findTag<SignalPowerInd>();
    auto modeInd = packet->findTag<physicallayer::Ieee80211ModeInd>();
    if (signalPower != nullptr && modeInd != nullptr && modeInd->getMode() != nullptr) {
        const auto receivedBandwidth = modeInd->getMode()->getDataMode()->getBandwidth();
        triggerPathLossDb = computeIeee80211HeTriggerPathLossDb(
                trigger->getApTxPowerDbm(), signalPower->getPower(), receivedBandwidth);
    }
    auto myAid = mac->getMib()->bssStationData.associationId;
    auto negotiated = mac->getMib()->findNegotiatedHeCapabilities(
            mac->getMib()->bssData.bssid);
    const auto bandwidth = Hz(trigger->getChannelBandwidthMhz() * 1e6);
    const bool ulMuDisabled = par("operatingModeUlMuDisable").boolValue();
    auto supportsUser = [&] (const Ieee80211HeTriggerUserInfo& user) {
        const int nssIndex = user.numberOfSpatialStreams - 1;
        return negotiated != nullptr && negotiated->localTxPeerRx.valid &&
                negotiated->localTxPeerRx.ofdma &&
                negotiated->localTxPeerRx.supportedChannelWidths.count(bandwidth) != 0 &&
                negotiated->localTxPeerRx.supportedRuToneSizes.count(user.ruToneSize) != 0 &&
                negotiated->localTxPeerRx.mcsNss.maxMcsPerNss[nssIndex] >= user.mcs &&
                (user.coding != physicallayer::HE_CODING_LDPC || negotiated->mutual.ldpc) &&
                (!user.muMimo ||
                 negotiated->localTxPeerRx.transmitterCanTransmitFullBandwidthUlMuMimo) &&
                !ulMuDisabled;
    };
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
            EV_INFO << "Ignoring NFRP Trigger " << triggerId
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
        if (user.randomAccess && supportsUser(user))
            randomAccessUsers.push_back(&user);
        else if (user.aid == myAid)
            selected = &user;
    }

    AccessCategory selectedAc = AC_BE;
    queueing::IPacketQueue *sourceQueue = nullptr;
    Packet *sourcePacket = nullptr;
    bool randomAccess = false;
    bool randomAccessCommitted = false;
    std::optional<HeUlCoordinator::PreparedRandomAccessSelection> randomAccessSelection;
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
            randomAccessSelection = ulCoordinator->prepareRandomAccessRu(
                    pendingAc, randomAccessUsers.size());
            int raIndex = ulCoordinator->commitRandomAccessRu(*randomAccessSelection);
            if (raIndex >= 0) {
                selected = randomAccessUsers[raIndex];
                randomAccess = true;
                randomAccessCommitted = true;
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
        EV_INFO << "Ignoring HE UL Trigger " << triggerId
                 << ": this STA has no scheduled or selected random-access RU\n";
        delete packet;
        return;
    }

    if (!supportsUser(*selected)) {
        EV_WARN << "Ignoring HE UL Trigger allocation that exceeds negotiated local-TX/peer-RX capabilities\n";
        delete packet;
        return;
    }

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
    auto maximumBlockAckLength = B(18 + 12 * trigger->getUsersArraySize() + 4);
    // 10.3.2.11 and 10.23.2.2 permit RXSTART within SIFS plus one slot; the
    // maximum Block Ack airtime below carries that start deadline through to
    // the packet's RXEND delivery point.
    exchange.expectedResponseTime = simTime() + modeSet->getSifsTime() +
            trigger->getCommonDuration() + modeSet->getSifsTime() +
            modeSet->getSlowestMandatoryMode()->getDuration(maximumBlockAckLength) +
            modeSet->getSlotTime();
    W transmitPower = phy.getMaximumTransmitPower();
    if (triggerPathLossDb.has_value())
        transmitPower = computeIeee80211HeTbTransmitPower(phy.getMaximumTransmitPower(),
                selected->targetRssiDbm, *triggerPathLossDb, selected->useMaximumTransmitPower);
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
        bool responseCommitted = false;
        try {
            responsePacket = buildTriggeredUlResponsePacket(sourcePacket, sourceQueue, selectedAc,
                    selectedTid, queueBytes, availableSlots, selected, trigger,
                    triggerId, transmitPower, solicitingTxopDuration, exchange,
                    responseHeader, responseCommitted);
        }
        catch (const std::exception& error) {
            if (responseCommitted || randomAccessCommitted)
                throw;
            EV_WARN << "HE-TB response preparation aborted before commit: "
                    << error.what() << "\n";
            delete packet;
            return;
        }
        responsePacketCount = exchange.packets.empty() ? 1 : exchange.packets.size();
        // Every solicited HE-TB response owns a terminal Multi-STA BA window,
        // including scheduled QoS Null/BSR responses with no retained MPDU.
        auto inserted = triggeredUlExchanges.emplace(triggerId, std::move(exchange));
        if (!inserted.second)
            throw cRuntimeError("Duplicate HE-TB Trigger ID reached the post-commit exchange ledger");
        EV_INFO << "Committed HE-TB exchange ledger: trigger="
                << triggerId << ", packets="
                << inserted.first->second.packets.size() << ", deadline="
                << inserted.first->second.expectedResponseTime << "\n";
        scheduleTriggeredUlResponseTimeout();
    }

    // 26.5.2.2.4, 27.3.12.5.5, and 27.4.3: the HE-TB TXVECTOR is derived from the
    // selected Trigger User Info and Common Info fields before the packet
    // crosses the MAC/PHY boundary.
    if (trigger->getTriggerType() == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER) {
        attachHeTbTxVectorFromTrigger(responsePacket, *trigger, *selected, myAid,
                phy.getChannelCenterFrequency(), transmitPower, B(0),
                mac->getMib()->heOperation.bssColor, triggerId, true,
                queueBytes > 256 ? 1 : 0, nfrpToneSetIndex,
                nfrpStartingStsNumber, solicitingTxopDuration,
                modeSet->getSifsTime());
        HeTbResponseEvent event;
        event.triggerId = triggerId;
        event.triggerType = IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER;
        event.reason = HeTbResponseEvent::NDP_FEEDBACK_REPORTED;
        event.associationId = myAid;
        event.tid = selectedTid;
        event.accessCategory = selectedAc;
        event.ruIndex = selected->ruIndex;
        event.ruToneSize = selected->ruToneSize;
        event.ruToneOffset = selected->ruToneOffset;
        event.reportedBytes = queueBytes;
        emit(heTbResponseCommittedSignal, &event);
    }
    EV_INFO << "Sending HE-TB response: trigger=" << triggerId
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
    if (multiStaBlockAck->getTransmitterAddress() != mac->getMib()->bssData.bssid) {
        delete packet;
        return;
    }
    auto correlation = packet->findTag<physicallayer::Ieee80211HeTriggerCorrelationTag>();
    if (correlation == nullptr) {
        EV_WARN << "Discarding Multi-STA Block Ack without Trigger correlation\n";
        delete packet;
        return;
    }
    auto exchangeIt = triggeredUlExchanges.find(correlation->getTriggerId());
    if (exchangeIt == triggeredUlExchanges.end()) {
        EV_WARN << "Discarding foreign Multi-STA Block Ack for Trigger "
                << correlation->getTriggerId() << "\n";
        delete packet;
        return;
    }
    if (simTime() > exchangeIt->second.expectedResponseTime) {
        EV_WARN << "Discarding late Multi-STA Block Ack for Trigger "
                << correlation->getTriggerId() << ", deadline="
                << exchangeIt->second.expectedResponseTime << "\n";
        delete packet;
        return;
    }
    auto myAid = mac->getMib()->bssStationData.associationId;
    auto& exchange = exchangeIt->second;
    const Ieee80211MultiStaBlockAckRecord *record = nullptr;
    int matchingRecordCount = 0;
    for (unsigned int i = 0; i < multiStaBlockAck->getRecordsArraySize(); ++i)
        if (multiStaBlockAck->getRecords(i).aid == myAid &&
                multiStaBlockAck->getRecords(i).tid == exchange.tid) {
            record = &multiStaBlockAck->getRecords(i);
            matchingRecordCount++;
        }
    if (matchingRecordCount > 1) {
        EV_WARN << "Discarding ambiguous Multi-STA Block Ack for Trigger "
                << correlation->getTriggerId() << "\n";
        delete packet;
        return;
    }
    // A UORA response to a BSRP Trigger is a QoS Null carrying buffer
    // status, so there is no queued data MPDU or bitmap bit to retire. A
    // positive per-AID response record is nevertheless a successful UORA
    // attempt and must reset OCW and be counted.
    bool exchangeSuccess = exchange.packets.empty() &&
            record != nullptr && record->responseReceived;
    if (record != nullptr && record->responseReceived) {
        AccessCategory ac = edca->mapTidToAc(exchange.tid);
        if (ac >= 0 && ac < 4)
            edca->getEdcaf(ac)->startMuEdcaTimer();
    }
    for (size_t i = 0; i < exchange.packets.size(); ++i) {
        bool acknowledged = false;
        if (record != nullptr && record->responseReceived) {
            int offset = (exchange.sequenceNumbers[i] -
                    record->startingSequenceNumber + 4096) % 4096;
            acknowledged = offset < 64 &&
                    (record->bitmap & (UINT64_C(1) << offset));
        }
        if (acknowledged) {
            delete exchange.packets[i];
            exchangeSuccess = true;
        }
        else {
            auto header = exchange.packets[i]->
                    peekAtFront<Ieee80211DataHeader>();
            edca->getEdcaf(edca->mapTidToAc(header->getTid()))->
                    getRecoveryProcedure()->dataFrameTransmissionFailed(
                            exchange.packets[i], header);
            auto writableHeader = exchange.packets[i]->
                    removeAtFront<Ieee80211DataHeader>();
            writableHeader->setRetry(true);
            exchange.packets[i]->insertAtFront(writableHeader);
            exchange.sourceQueue->pushPacket(exchange.packets[i], nullptr);
        }
    }
    if (exchange.randomAccess)
        ulCoordinator->reportRandomAccessResult(exchangeSuccess);
    EV_INFO << "Applied correlated Multi-STA Block Ack: trigger="
            << correlation->getTriggerId() << ", AID=" << myAid
            << ", TID=" << (int)exchange.tid
            << ", packets=" << exchange.packets.size()
            << ", success=" << exchangeSuccess << "\n";
    triggeredUlExchanges.erase(exchangeIt);
    scheduleTriggeredUlResponseTimeout();
    delete packet;
    return;
}
} // namespace ieee80211
} // namespace inet
