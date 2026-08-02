//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEULMUPLAN_H
#define __INET_HEULMUPLAN_H

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee80211/mac/contract/Ieee80211HePreamblePuncturing.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/IIeee80211HeUlTriggerPolicy.h"
#include "inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeUlScheduler.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211HeCapabilities.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"

namespace inet {
namespace ieee80211 {

enum class HeUlMuPlanErrorCode {
    NONE,
    INVALID_TRIGGER_TYPE,
    INVALID_COMMON_PARAMETERS,
    EMPTY_ALLOCATIONS,
    UNKNOWN_STATION,
    INVALID_AID,
    DUPLICATE_STATION,
    INVALID_RU,
    OVERLAPPING_RU,
    INVALID_MCS,
    INVALID_NSS,
    INVALID_DURATION,
    INVALID_POWER,
    UNSUPPORTED_CAPABILITY,
    INCONSISTENT_TXVECTOR,
};

struct HeUlMuPlanDiagnostic
{
    HeUlMuPlanErrorCode code = HeUlMuPlanErrorCode::NONE;
    int allocationIndex = -1;
    MacAddress station;
    std::string detail;

    explicit operator bool() const { return code != HeUlMuPlanErrorCode::NONE; }
};

/**
 * Immutable, model-only UL Trigger plan. Scheduler and internally generated
 * values are copied and canonicalized; no queue, timer, or coordinator state
 * is owned or changed by construction.
 */
class INET_API HeUlMuPlan
{
  public:
    struct StationContract {
        MacAddress station;
        uint16_t associationId = 0;
        Ieee80211NegotiatedHeCapabilities capabilities;
        bool schedulerCandidate = false;
        bool ulMuDisabled = false;
    };

    struct ValidationContext {
        Hz centerFrequency = Hz(NaN);
        std::vector<StationContract> stations;
        bool requireSchedulerCandidate = false;
    };

  private:
    IIeee80211HeUlScheduler::Schedule schedule;
    IIeee80211HeUlTriggerPolicy::TriggerType triggerType;

  private:
    HeUlMuPlan(const IIeee80211HeUlScheduler::Schedule& schedule,
            IIeee80211HeUlTriggerPolicy::TriggerType triggerType) :
        schedule(schedule), triggerType(triggerType) {}

    static void fail(HeUlMuPlanDiagnostic& diagnostic, HeUlMuPlanErrorCode code,
            int allocationIndex, const MacAddress& station, const char *detail)
    {
        diagnostic.code = code;
        diagnostic.allocationIndex = allocationIndex;
        diagnostic.station = station;
        diagnostic.detail = detail;
    }

  public:
    static std::optional<HeUlMuPlan> create(const ValidationContext& context,
            const IIeee80211HeUlScheduler::Schedule& proposedSchedule,
            IIeee80211HeUlTriggerPolicy::TriggerType triggerType,
            HeUlMuPlanDiagnostic& diagnostic)
    {
        using namespace physicallayer;
        diagnostic = HeUlMuPlanDiagnostic();
        auto rejected = [&] (HeUlMuPlanErrorCode code, int index,
                const MacAddress& station, const char *detail) {
            fail(diagnostic, code, index, station, detail);
            return std::optional<HeUlMuPlan>();
        };
        if (triggerType != IIeee80211HeUlTriggerPolicy::BASIC_TRIGGER &&
                triggerType != IIeee80211HeUlTriggerPolicy::BSRP_TRIGGER &&
                triggerType != IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER)
            return rejected(HeUlMuPlanErrorCode::INVALID_TRIGGER_TYPE, -1,
                    MacAddress(), "Trigger type is not Basic, BSRP, or NFRP");

        auto normalized = proposedSchedule;
        const auto bandwidth = normalized.channelBandwidth;
        const auto centerFrequency = context.centerFrequency;
        if (!std::isfinite(centerFrequency.get()) || centerFrequency <= Hz(0) ||
                !std::isfinite(bandwidth.get()) ||
                (bandwidth != MHz(20) && bandwidth != MHz(40) &&
                 bandwidth != MHz(80) && bandwidth != MHz(160)) ||
                normalized.ulLength > 4095 || normalized.ulLength % 3 != 1 ||
                normalized.commonDuration <= SIMTIME_ZERO ||
                normalized.commonDuration > SimTime(5.484, SIMTIME_MS) ||
                (normalized.packetExtensionDurationUs != 0 && normalized.packetExtensionDurationUs != 4 &&
                 normalized.packetExtensionDurationUs != 8 && normalized.packetExtensionDurationUs != 12 &&
                 normalized.packetExtensionDurationUs != 16) ||
                normalized.preFecPaddingFactor < 1 || normalized.preFecPaddingFactor > 4 ||
                (normalized.numberOfHeLtfSymbols != 1 && normalized.numberOfHeLtfSymbols != 2 &&
                 normalized.numberOfHeLtfSymbols != 4 && normalized.numberOfHeLtfSymbols != 6 &&
                 normalized.numberOfHeLtfSymbols != 8) ||
                normalized.apTxPowerDbm < -20 || normalized.apTxPowerDbm > 40)
            return rejected(HeUlMuPlanErrorCode::INVALID_COMMON_PARAMETERS, -1,
                    MacAddress(), "Trigger common parameters are missing, inexact, or unencodable");
        const bool validGiLtf =
                (normalized.guardInterval == HE_GI_1_6_US &&
                 (normalized.ltfType == HE_LTF_1X || normalized.ltfType == HE_LTF_2X)) ||
                (normalized.guardInterval == HE_GI_3_2_US && normalized.ltfType == HE_LTF_4X);
        if (!validGiLtf)
            return rejected(HeUlMuPlanErrorCode::INVALID_COMMON_PARAMETERS, -1,
                    MacAddress(), "Trigger GI/HE-LTF pair has no wire encoding");
        if (normalized.allocations.size() > 255)
            return rejected(HeUlMuPlanErrorCode::INVALID_COMMON_PARAMETERS, -1,
                    MacAddress(), "Trigger contains more than 255 User Info records");

        const bool nfrp = triggerType == IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER;
        if (nfrp) {
            if (!normalized.allocations.empty() || normalized.nfrpStartingAid > 4095 ||
                    normalized.nfrpFeedbackType != 0 ||
                    (!normalized.nfrpUseMaximumTransmitPower &&
                     (normalized.nfrpTargetRssiDbm < -110 || normalized.nfrpTargetRssiDbm > -20)) ||
                    normalized.guardInterval != HE_GI_3_2_US ||
                    normalized.ltfType != HE_LTF_4X || normalized.numberOfHeLtfSymbols != 2 ||
                    normalized.coding != HE_CODING_BCC || normalized.packetExtensionDurationUs != 0)
                return rejected(HeUlMuPlanErrorCode::INVALID_COMMON_PARAMETERS, -1,
                        MacAddress(), "NFRP range, power, coding, or HE-LTF fields are invalid");
            try {
                int count = IIeee80211HeUlScheduler::getNfrpScheduledStaCount(
                        bandwidth, normalized.nfrpMultiplexingFlag);
                if (normalized.nfrpStartingAid + count > 4096)
                    return rejected(HeUlMuPlanErrorCode::INVALID_AID, -1,
                            MacAddress(), "NFRP scheduled AID range exceeds 12 bits");
            }
            catch (const std::exception&) {
                return rejected(HeUlMuPlanErrorCode::INVALID_COMMON_PARAMETERS, -1,
                        MacAddress(), "NFRP bandwidth is unsupported");
            }
            const int nfrpStationCount = IIeee80211HeUlScheduler::getNfrpScheduledStaCount(
                    bandwidth, normalized.nfrpMultiplexingFlag);
            bool hasCapableResponder = false;
            for (const auto& station : context.stations)
                if (station.associationId >= normalized.nfrpStartingAid &&
                        station.associationId < normalized.nfrpStartingAid + nfrpStationCount &&
                        !station.ulMuDisabled &&
                        station.capabilities.localRxPeerTx.valid &&
                        station.capabilities.localRxPeerTx.supportedChannelWidths.count(bandwidth) != 0 &&
                        station.capabilities.localRxPeerTx.transmitterCanTransmitNdpFeedbackReport)
                    hasCapableResponder = true;
            if (!hasCapableResponder)
                return rejected(HeUlMuPlanErrorCode::UNSUPPORTED_CAPABILITY, -1,
                        MacAddress(), "NFRP AID range contains no capable associated responder");
        }
        else if (normalized.allocations.empty())
            return rejected(HeUlMuPlanErrorCode::EMPTY_ALLOCATIONS, -1,
                    MacAddress(), "Basic/BSRP Trigger has no RU allocations");

        const int subchannelCount = std::lround(bandwidth.get() / 20e6);
        if (normalized.puncturedSubchannelMask != 0)
            return rejected(HeUlMuPlanErrorCode::INVALID_COMMON_PARAMETERS, -1,
                    MacAddress(), "HE-TB Trigger plans cannot signal punctured subchannels");
        std::vector<bool> puncturedSubchannels(subchannelCount, false);
        for (int i = 0; i < subchannelCount; ++i)
            puncturedSubchannels[i] = (normalized.puncturedSubchannelMask & (1U << i)) != 0;
        if (normalized.puncturedSubchannelMask != 0 &&
                !isValidHePreamblePuncturing(puncturedSubchannels,
                        std::lround(bandwidth.get() / 1e6)))
            return rejected(HeUlMuPlanErrorCode::INVALID_COMMON_PARAMETERS, -1,
                    MacAddress(), "punctured-subchannel mask is not a permitted HE pattern");

        std::map<MacAddress, const StationContract *> stationContracts;
        for (const auto& station : context.stations)
            stationContracts[station.station] = &station;
        std::set<MacAddress> stations;
        std::set<uint16_t> aids;
        std::map<std::pair<int, int>, std::vector<size_t>> perRu;
        std::vector<Ieee80211HeRu> physicalRus;
        auto catalog = getHeRuAllocationCatalog(centerFrequency, bandwidth);
        const int channelTones = getHeChannelToneCount(bandwidth);
        for (size_t i = 0; i < normalized.allocations.size(); ++i) {
            auto& allocation = normalized.allocations[i];
            auto canonical = std::find_if(catalog.begin(), catalog.end(), [&] (const auto& ru) {
                return ru.index == allocation.ru.index && ru.toneSize == allocation.ru.toneSize &&
                        ru.toneOffset == allocation.ru.toneOffset;
            });
            if (canonical == catalog.end())
                return rejected(HeUlMuPlanErrorCode::INVALID_RU, i,
                        allocation.staAddress, "RU is not canonical for the Trigger channel");
            allocation.ru = *canonical;
            for (int subchannel = allocation.ru.toneOffset * subchannelCount / channelTones;
                    subchannel <= (allocation.ru.toneOffset + allocation.ru.toneSize - 1) * subchannelCount / channelTones;
                    ++subchannel)
                if (puncturedSubchannels[subchannel])
                    return rejected(HeUlMuPlanErrorCode::INVALID_RU, i,
                            allocation.staAddress, "RU overlaps a punctured 20 MHz subchannel");
            if (allocation.mcs < 0 || allocation.mcs > 11)
                return rejected(HeUlMuPlanErrorCode::INVALID_MCS, i,
                        allocation.staAddress, "MCS is outside 0..11");
            if (allocation.numberOfSpatialStreams < 1 || allocation.numberOfSpatialStreams > 8 ||
                    allocation.streamStartIndex < 0 || allocation.streamStartIndex > 7 ||
                    allocation.streamStartIndex + allocation.numberOfSpatialStreams > 8)
                return rejected(HeUlMuPlanErrorCode::INVALID_NSS, i,
                        allocation.staAddress, "spatial-stream allocation is outside 0..7");
            if (allocation.estimatedDuration <= SIMTIME_ZERO ||
                    allocation.estimatedDuration > SimTime(5.484, SIMTIME_MS))
                return rejected(HeUlMuPlanErrorCode::INVALID_DURATION, i,
                        allocation.staAddress, "estimated duration is outside the HE PPDU limit");
            if (!allocation.useMaximumTransmitPower &&
                    (allocation.targetRssiDbm < -110 || allocation.targetRssiDbm > -20))
                return rejected(HeUlMuPlanErrorCode::INVALID_POWER, i,
                        allocation.staAddress, "UL target RSSI has no Trigger encoding");
            if (allocation.tid > 15 || allocation.accessCategory < AC_BK ||
                    allocation.accessCategory >= AC_NUMCATEGORIES)
                return rejected(HeUlMuPlanErrorCode::INVALID_COMMON_PARAMETERS, i,
                        allocation.staAddress, "TID or access category is invalid");

            if (allocation.randomAccess) {
                if (allocation.associationId != 0 ||
                        allocation.staAddress != MacAddress::UNSPECIFIED_ADDRESS)
                    return rejected(HeUlMuPlanErrorCode::INVALID_AID, i,
                            allocation.staAddress, "random-access RU must use AID 0 and no station address");
                if (allocation.numberOfSpatialStreams != 1 || allocation.streamStartIndex != 0 ||
                        allocation.muMimo)
                    return rejected(HeUlMuPlanErrorCode::INVALID_NSS, i,
                            allocation.staAddress, "random-access RU must use one stream and cannot use MU-MIMO");
                bool hasEligibleContender = false;
                for (const auto& station : context.stations) {
                    if (station.ulMuDisabled)
                        continue;
                    hasEligibleContender = true;
                    const auto& capabilities = station.capabilities;
                    if (!capabilities.localRxPeerTx.valid ||
                            !capabilities.localRxPeerTx.ofdma ||
                            capabilities.localRxPeerTx.supportedChannelWidths.count(bandwidth) == 0 ||
                            capabilities.localRxPeerTx.supportedRuToneSizes.count(allocation.ru.toneSize) == 0 ||
                            capabilities.localRxPeerTx.mcsNss.maxMcsPerNss[
                                    allocation.numberOfSpatialStreams - 1] < allocation.mcs ||
                            (allocation.coding == HE_CODING_LDPC && !capabilities.mutual.ldpc))
                        return rejected(HeUlMuPlanErrorCode::UNSUPPORTED_CAPABILITY, i,
                                station.station, "random-access parameters exceed an associated contender's capabilities");
                }
                if (!hasEligibleContender)
                    return rejected(HeUlMuPlanErrorCode::UNSUPPORTED_CAPABILITY, i,
                            allocation.staAddress, "random-access RU has no eligible associated contender");
            }
            else {
                auto station = stationContracts.find(allocation.staAddress);
                if (station == stationContracts.end())
                    return rejected(HeUlMuPlanErrorCode::UNKNOWN_STATION, i,
                            allocation.staAddress, "scheduled station is not associated");
                if (allocation.associationId == 0 || allocation.associationId > 2007 ||
                        station->second->associationId != allocation.associationId)
                    return rejected(HeUlMuPlanErrorCode::INVALID_AID, i,
                            allocation.staAddress, "AID does not identify the scheduled station");
                if (!stations.insert(allocation.staAddress).second || !aids.insert(allocation.associationId).second)
                    return rejected(HeUlMuPlanErrorCode::DUPLICATE_STATION, i,
                            allocation.staAddress, "station or AID occurs more than once");
                if (context.requireSchedulerCandidate && !station->second->schedulerCandidate)
                    return rejected(HeUlMuPlanErrorCode::UNKNOWN_STATION, i,
                            allocation.staAddress, "scheduled station was not an eligible scheduler candidate");
                if (station->second->ulMuDisabled)
                    return rejected(HeUlMuPlanErrorCode::UNSUPPORTED_CAPABILITY, i,
                            allocation.staAddress, "station disabled UL MU operation through OMI");
                const auto& capabilities = station->second->capabilities;
                int nssIndex = allocation.numberOfSpatialStreams - 1;
                if (!capabilities.localRxPeerTx.valid ||
                        !capabilities.localRxPeerTx.ofdma ||
                        capabilities.localRxPeerTx.supportedChannelWidths.count(bandwidth) == 0 ||
                        capabilities.localRxPeerTx.supportedRuToneSizes.count(allocation.ru.toneSize) == 0 ||
                        capabilities.localRxPeerTx.mcsNss.maxMcsPerNss[nssIndex] < allocation.mcs ||
                        (allocation.coding == HE_CODING_LDPC && !capabilities.mutual.ldpc) ||
                        (allocation.muMimo &&
                         !capabilities.localRxPeerTx.fullBandwidthUlMuMimo))
                    return rejected(HeUlMuPlanErrorCode::UNSUPPORTED_CAPABILITY, i,
                            allocation.staAddress, "allocation exceeds negotiated peer-TX/local-RX capabilities");
            }
            auto geometry = std::make_pair(allocation.ru.toneSize, allocation.ru.toneOffset);
            if (perRu[geometry].empty())
                physicalRus.push_back(allocation.ru);
            perRu[geometry].push_back(i);
            if (allocation.muMimo && (allocation.randomAccess ||
                    allocation.ru.toneSize != getHeEqualRuLayout(centerFrequency, bandwidth, 1).front().toneSize ||
                    allocation.ru.toneOffset != getHeEqualRuLayout(centerFrequency, bandwidth, 1).front().toneOffset ||
                    allocation.numberOfSpatialStreams > 4))
                return rejected(HeUlMuPlanErrorCode::INVALID_NSS, i,
                        allocation.staAddress, "UL MU-MIMO requires a full-bandwidth RU and at most four streams per user");
        }
        if (!validateHeRuLayout(physicalRus, bandwidth))
            return rejected(HeUlMuPlanErrorCode::OVERLAPPING_RU, -1,
                    MacAddress(), "physical RU layout is overlapping or out of band");
        const auto fullRu = getHeEqualRuLayout(centerFrequency, bandwidth, 1).front();
        for (const auto& entry : perRu) {
            if (entry.second.size() == 1) {
                const auto& allocation = normalized.allocations[entry.second.front()];
                if (allocation.muMimo || allocation.streamStartIndex != 0)
                    return rejected(HeUlMuPlanErrorCode::INVALID_NSS, entry.second.front(),
                            allocation.staAddress, "single-user RU cannot be MU-MIMO and must start at stream zero");
                continue;
            }
            std::set<int> streams;
            for (auto index : entry.second) {
                const auto& allocation = normalized.allocations[index];
                if (!allocation.muMimo || allocation.randomAccess ||
                        allocation.ru.toneSize != fullRu.toneSize ||
                        allocation.ru.toneOffset != fullRu.toneOffset)
                    return rejected(HeUlMuPlanErrorCode::OVERLAPPING_RU, index,
                            allocation.staAddress, "shared RU is not full-bandwidth scheduled UL MU-MIMO");
                for (int stream = allocation.streamStartIndex;
                        stream < allocation.streamStartIndex + allocation.numberOfSpatialStreams; ++stream)
                    if (!streams.insert(stream).second)
                        return rejected(HeUlMuPlanErrorCode::INVALID_NSS, index,
                                allocation.staAddress, "UL MU-MIMO spatial streams overlap");
            }
            if (streams.empty() || *streams.begin() != 0 ||
                    *streams.rbegin() + 1 != static_cast<int>(streams.size()))
                return rejected(HeUlMuPlanErrorCode::INVALID_NSS, -1,
                        MacAddress(), "UL MU-MIMO spatial streams must form one contiguous range from zero");
        }
        if (normalized.ltfType == HE_LTF_1X &&
                std::any_of(normalized.allocations.begin(), normalized.allocations.end(),
                        [] (const auto& allocation) { return !allocation.muMimo; }))
            return rejected(HeUlMuPlanErrorCode::INVALID_COMMON_PARAMETERS, -1,
                    MacAddress(), "1x HE-LTF is limited to full-bandwidth UL MU-MIMO");

        std::vector<Ieee80211HeUserPhyParameters> users;
        if (nfrp) {
            Ieee80211HeUserPhyParameters user;
            user.ru = fullRu;
            user.mcs = 0;
            user.numberOfSpatialStreams = 1;
            user.coding = HE_CODING_BCC;
            user.psduLength = B(0);
            user.ndpFeedbackReport = true;
            user.ndpRuToneSetIndex = 1;
            users.push_back(user);
        }
        else
            for (const auto& allocation : normalized.allocations) {
                Ieee80211HeUserPhyParameters user;
                user.ru = allocation.ru;
                user.mcs = allocation.mcs;
                user.numberOfSpatialStreams = allocation.numberOfSpatialStreams;
                user.streamStartIndex = allocation.streamStartIndex;
                user.staId = allocation.associationId;
                user.coding = allocation.coding;
                user.psduLength = B(1);
                users.push_back(user);
            }
        Ieee80211HeTriggerResponseFinalizationRequest request;
        request.users = users;
        request.centerFrequency = centerFrequency;
        request.channelBandwidth = bandwidth;
        request.guardInterval = normalized.guardInterval;
        request.ltfType = normalized.ltfType;
        request.packetExtensionDurationUs = normalized.packetExtensionDurationUs;
        request.noSignalExtension = normalized.noSignalExtension;
        request.durationBudget = normalized.commonDuration;
        if (!nfrp) {
            Ieee80211HeTbCapacityBoundary boundary;
            boundary.channelBandwidth = bandwidth;
            boundary.ulLength = normalized.ulLength;
            boundary.guardInterval = normalized.guardInterval;
            boundary.ltfType = normalized.ltfType;
            boundary.preFecPaddingFactor = normalized.preFecPaddingFactor;
            boundary.ldpcExtraSymbolSegment = normalized.ldpcExtraSymbolSegment;
            boundary.peDisambiguity = normalized.peDisambiguity;
            boundary.numberOfHeLtfSymbols = normalized.numberOfHeLtfSymbols;
            boundary.packetExtensionDurationUs = normalized.packetExtensionDurationUs;
            request.fixedBoundary = boundary;
        }
        try {
            auto finalization = finalizeHeTriggerResponse(request);
            if (!finalization || finalization.ulLength != normalized.ulLength ||
                    finalization.commonDuration != normalized.commonDuration ||
                    finalization.commonDurationExact != normalized.commonDurationExact ||
                    finalization.parameters.common.numberOfHeLtfSymbols != normalized.numberOfHeLtfSymbols ||
                    (!nfrp && (finalization.parameters.common.preFecPaddingFactor != normalized.preFecPaddingFactor ||
                     finalization.parameters.common.ldpcExtraSymbol != normalized.ldpcExtraSymbolSegment ||
                     finalization.peDisambiguity != normalized.peDisambiguity ||
                     finalization.parameters.common.packetExtensionDurationUs != normalized.packetExtensionDurationUs)))
                return rejected(HeUlMuPlanErrorCode::INCONSISTENT_TXVECTOR, -1,
                        MacAddress(), "Trigger fields do not match the canonical HE-TB TxVector calculation");
        }
        catch (const std::exception&) {
            return rejected(HeUlMuPlanErrorCode::INCONSISTENT_TXVECTOR, -1,
                    MacAddress(), "canonical HE-TB TxVector validation failed");
        }
        return HeUlMuPlan(normalized, triggerType);
    }

    const IIeee80211HeUlScheduler::Schedule& getSchedule() const { return schedule; }
    IIeee80211HeUlTriggerPolicy::TriggerType getTriggerType() const { return triggerType; }
};

} // namespace ieee80211
} // namespace inet

#endif
