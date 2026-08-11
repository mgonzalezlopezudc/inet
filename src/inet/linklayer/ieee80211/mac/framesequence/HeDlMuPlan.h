//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEDLMUPLAN_H
#define __INET_HEDLMUPLAN_H

#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeDlScheduler.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeSigCodec.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"

namespace inet {
namespace ieee80211 {

/** Stable release-active failure categories for external HE scheduler output. */
enum class HeMuPlanErrorCode {
    NONE,
    INVALID_COMMON_PARAMETERS,
    TOO_FEW_USERS,
    UNKNOWN_STATION,
    NULL_QUEUE,
    DUPLICATE_STATION,
    INVALID_RU,
    OVERLAPPING_RU,
    INVALID_MCS,
    INVALID_NSS,
    INVALID_DURATION,
    UNSUPPORTED_CAPABILITY,
    INVALID_LEAKAGE,
};

struct HeMuPlanDiagnostic
{
    HeMuPlanErrorCode code = HeMuPlanErrorCode::NONE;
    int allocationIndex = -1;
    MacAddress station;
    std::string detail;

    explicit operator bool() const { return code != HeMuPlanErrorCode::NONE; }
};

/**
 * Immutable, model-only DL scheduler plan. It copies scheduler values and queue
 * handles but never owns or removes packets. Construction is possible only
 * through create(), whose validation is active in release and debug builds.
 */
class INET_API HeDlMuPlan
{
  private:
    IIeee80211HeDlScheduler::ScheduleContext scheduleContext;
    std::vector<IIeee80211HeDlScheduler::RuAllocation> allocations;

  private:
    HeDlMuPlan(const IIeee80211HeDlScheduler::ScheduleContext& scheduleContext,
            const std::vector<IIeee80211HeDlScheduler::RuAllocation>& allocations) :
        scheduleContext(scheduleContext), allocations(allocations) {}

    static bool fail(HeMuPlanDiagnostic& diagnostic, HeMuPlanErrorCode code,
            int allocationIndex, const MacAddress& station, const char *detail)
    {
        diagnostic.code = code;
        diagnostic.allocationIndex = allocationIndex;
        diagnostic.station = station;
        diagnostic.detail = detail;
        return false;
    }

  public:
    static std::optional<HeDlMuPlan> create(
            const IIeee80211HeDlScheduler::ScheduleContext& scheduleContext,
            const std::vector<IIeee80211HeDlScheduler::RuAllocation>& allocations,
            HeMuPlanDiagnostic& diagnostic)
    {
        using namespace physicallayer;
        diagnostic = HeMuPlanDiagnostic();
        if (!std::isfinite(scheduleContext.channelCenterFrequency.get()) ||
                !std::isfinite(scheduleContext.channelBandwidth.get()) ||
                scheduleContext.channelBandwidth <= Hz(0) ||
                (scheduleContext.channelBandwidth != MHz(20) &&
                 scheduleContext.channelBandwidth != MHz(40) &&
                 scheduleContext.channelBandwidth != MHz(80) &&
                 scheduleContext.channelBandwidth != MHz(160))) {
            fail(diagnostic, HeMuPlanErrorCode::INVALID_COMMON_PARAMETERS, -1,
                    MacAddress(), "channel center frequency/bandwidth is missing or invalid");
            return std::nullopt;
        }
        if (allocations.size() < 2) {
            fail(diagnostic, HeMuPlanErrorCode::TOO_FEW_USERS, -1,
                    MacAddress(), "fewer than two users were scheduled");
            return std::nullopt;
        }

        std::map<MacAddress, const IIeee80211HeDlScheduler::CandidateInfo *> candidates;
        for (const auto& candidate : scheduleContext.candidates)
            candidates[candidate.staAddress] = &candidate;

        std::set<MacAddress> scheduledStations;
        std::map<std::pair<int, int>, int> usersPerRu;
        std::map<std::pair<int, int>, std::vector<size_t>> allocationsPerRu;
        std::vector<Ieee80211HeRu> physicalRus;
        auto normalizedAllocations = allocations;
        Ieee80211HeRuCatalog catalog(scheduleContext.channelCenterFrequency,
                scheduleContext.channelBandwidth);
        for (size_t i = 0; i < allocations.size(); ++i) {
            const auto& allocation = allocations[i];
            auto candidateIt = candidates.find(allocation.staAddress);
            if (candidateIt == candidates.end()) {
                fail(diagnostic, HeMuPlanErrorCode::UNKNOWN_STATION, i,
                        allocation.staAddress, "scheduled station is not a candidate");
                return std::nullopt;
            }
            const auto& candidate = *candidateIt->second;
            if (candidate.sourceQueue == nullptr) {
                fail(diagnostic, HeMuPlanErrorCode::NULL_QUEUE, i,
                        allocation.staAddress, "candidate has no source queue");
                return std::nullopt;
            }
            if (!scheduledStations.insert(allocation.staAddress).second) {
                fail(diagnostic, HeMuPlanErrorCode::DUPLICATE_STATION, i,
                        allocation.staAddress, "station occurs more than once");
                return std::nullopt;
            }
            auto canonical = catalog.findHeRuByKey(getIeee80211HeRuKey(allocation.ru));
            if (!canonical) {
                fail(diagnostic, HeMuPlanErrorCode::INVALID_RU, i,
                        allocation.staAddress, "RU is not canonical for the channel");
                return std::nullopt;
            }
            // Never retain the scheduler-owned RU object. The identifying
            // triplet is extension input; all remaining geometry and channel
            // fields must come from the band-aware canonical catalog.
            normalizedAllocations[i].ru = *canonical;
            if (allocation.mcs < 0 || allocation.mcs > 11) {
                fail(diagnostic, HeMuPlanErrorCode::INVALID_MCS, i,
                        allocation.staAddress, "MCS is outside 0..11");
                return std::nullopt;
            }
            if (allocation.dcm && allocation.mcs != 0 && allocation.mcs != 1 &&
                    allocation.mcs != 3 && allocation.mcs != 4) {
                fail(diagnostic, HeMuPlanErrorCode::INVALID_MCS, i,
                        allocation.staAddress, "MCS does not have an HE DCM encoding");
                return std::nullopt;
            }
            if (allocation.numberOfSpatialStreams < 1 || allocation.numberOfSpatialStreams > 8) {
                fail(diagnostic, HeMuPlanErrorCode::INVALID_NSS, i,
                        allocation.staAddress, "NSS is outside 1..8");
                return std::nullopt;
            }
            if (!std::isfinite(allocation.leakageSum) || allocation.leakageSum < 0) {
                fail(diagnostic, HeMuPlanErrorCode::INVALID_LEAKAGE, i,
                        allocation.staAddress, "leakage sum must be finite and nonnegative");
                return std::nullopt;
            }
            if (allocation.estimatedDuration <= SIMTIME_ZERO) {
                fail(diagnostic, HeMuPlanErrorCode::INVALID_DURATION, i,
                        allocation.staAddress, "estimated duration must be positive");
                return std::nullopt;
            }
            // The scheduler estimate may cover the complete queued backlog and
            // therefore exceed aPPDUMaxTime. Keep it as a sizing hint here;
            // HeDlMuPackingPlanner performs exact MPDU trimming and the PHY
            // remains the final legality guard (IEEE 802.11-2024 Table 27-61,
            // Clause 10.13).
            if (!candidate.hasNegotiatedHeCapabilities) {
                fail(diagnostic, HeMuPlanErrorCode::UNSUPPORTED_CAPABILITY, i,
                        allocation.staAddress, "station has no negotiated HE capability contract");
                return std::nullopt;
            }
            const auto& capabilities = candidate.negotiatedHeCapabilities;
            int nssIndex = allocation.numberOfSpatialStreams - 1;
            int requiredDcmConstellation = allocation.mcs == 0 ? 1 :
                    allocation.mcs == 1 ? 2 : 4;
            if (!capabilities.localTxPeerRx.valid || !capabilities.localTxPeerRx.ofdma ||
                    capabilities.localTxPeerRx.supportedChannelWidths.count(scheduleContext.channelBandwidth) == 0 ||
                    capabilities.localTxPeerRx.supportedRuToneSizes.count(allocation.ru.toneSize) == 0 ||
                    capabilities.localTxPeerRx.mcsNss.maxMcsPerNss[nssIndex] < allocation.mcs ||
                    (candidate.operatingModeRxNss > 0 &&
                            allocation.numberOfSpatialStreams > candidate.operatingModeRxNss) ||
                    (allocation.dcm && (!capabilities.mutual.dcm ||
                            requiredDcmConstellation > capabilities.mutual.maxDcmConstellation ||
                            allocation.numberOfSpatialStreams > capabilities.mutual.maxDcmNss))) {
                fail(diagnostic, HeMuPlanErrorCode::UNSUPPORTED_CAPABILITY, i,
                        allocation.staAddress, "allocation exceeds negotiated local-TX/peer-RX capabilities");
                return std::nullopt;
            }

            auto geometry = std::make_pair(allocation.ru.toneSize, allocation.ru.toneOffset);
            allocationsPerRu[geometry].push_back(i);
            if (usersPerRu[geometry]++ == 0)
                physicalRus.push_back(*canonical);
        }

        if (!validateHeRuLayout(physicalRus, scheduleContext.channelBandwidth)) {
            fail(diagnostic, HeMuPlanErrorCode::OVERLAPPING_RU, -1,
                    MacAddress(), "physical RU layout is duplicate, overlapping, or out of band");
            return std::nullopt;
        }
        for (const auto& entry : usersPerRu) {
            if (entry.second > 1) {
                if (!scheduleContext.enableDlMuMimo) {
                    fail(diagnostic, HeMuPlanErrorCode::OVERLAPPING_RU, -1,
                            MacAddress(), "multiple users share an RU while DL MU-MIMO is disabled");
                    return std::nullopt;
                }
                if (entry.first.first == 26 || entry.first.first == 52) {
                    fail(diagnostic, HeMuPlanErrorCode::INVALID_RU, -1,
                            MacAddress(), "26-tone and 52-tone RUs cannot carry MU-MIMO users");
                    return std::nullopt;
                }
                int totalNsts = 0;
                std::vector<int> nsts;
                for (auto allocationIndex : allocationsPerRu.at(entry.first)) {
                    const auto& allocation = normalizedAllocations[allocationIndex];
                    const auto& candidate = *candidates.at(allocation.staAddress);
                    totalNsts += allocation.numberOfSpatialStreams;
                    nsts.push_back(allocation.numberOfSpatialStreams);
                    if (allocation.numberOfSpatialStreams > 4) {
                        fail(diagnostic, HeMuPlanErrorCode::INVALID_NSS,
                                allocationIndex, allocation.staAddress,
                                "an HE MU-MIMO user cannot have more than four spatial streams");
                        return std::nullopt;
                    }
                    if (scheduleContext.csiManager == nullptr || !candidate.hasFreshCsi ||
                            !candidate.hasAdvertisedHeCapabilities ||
                            !isDlMuMimoEligible(scheduleContext.localHeCapabilities,
                                    candidate.advertisedHeCapabilities,
                                    candidate.negotiatedHeCapabilities,
                                    scheduleContext.channelBandwidth,
                                    scheduleContext.numApAntennas)) {
                        fail(diagnostic, HeMuPlanErrorCode::UNSUPPORTED_CAPABILITY,
                                allocationIndex, allocation.staAddress,
                                "shared-RU allocation lacks beamforming capability, CSI, or antenna support");
                        return std::nullopt;
                    }
                }
                if (totalNsts > 8 || totalNsts > scheduleContext.numApAntennas) {
                    fail(diagnostic, HeMuPlanErrorCode::UNSUPPORTED_CAPABILITY, -1,
                            MacAddress(), "shared-RU aggregate Nsts exceeds the AP transmit dimensions");
                    return std::nullopt;
                }
                try {
                    (void)encodeHeMuSpatialConfiguration(nsts);
                }
                catch (const std::exception&) {
                    fail(diagnostic, HeMuPlanErrorCode::INVALID_NSS, -1,
                            MacAddress(), "shared-RU spatial streams have no HE-SIG-B spatial configuration encoding");
                    return std::nullopt;
                }
            }
        }
        return HeDlMuPlan(scheduleContext, normalizedAllocations);
    }

    const IIeee80211HeDlScheduler::ScheduleContext& getScheduleContext() const { return scheduleContext; }
    const std::vector<IIeee80211HeDlScheduler::RuAllocation>& getAllocations() const { return allocations; }
};

} // namespace ieee80211
} // namespace inet

#endif
