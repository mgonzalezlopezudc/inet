//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/scheduler/HeDlSchedulerEqualSizedRUs.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeMuMimoCsiManager.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

Define_Module(HeDlSchedulerEqualSizedRUs);

namespace {

bool isCompatibleEqualRuAllocation(const IIeee80211HeDlScheduler::ScheduleContext& context,
        const IIeee80211HeDlScheduler::CandidateInfo& candidate, const Ieee80211HeRu& ru)
{
    if (context.coding == HE_CODING_BCC && ru.toneSize >= 484)
        return false;
    const auto *negotiated = candidate.hasNegotiatedHeCapabilities ?
            &candidate.negotiatedHeCapabilities : nullptr;
    if (negotiated == nullptr)
        return true;
    const auto& capabilities = negotiated->localTxPeerRx;
    return capabilities.valid && capabilities.ofdma &&
            capabilities.supportedChannelWidths.count(context.channelBandwidth) != 0 &&
            capabilities.supportedRuToneSizes.count(ru.toneSize) != 0 &&
            capabilities.mcsNss.maxMcsPerNss[0] >= 0 &&
            (context.coding != HE_CODING_LDPC || negotiated->mutual.ldpc);
}

} // namespace

void HeDlSchedulerEqualSizedRUs::initialize(int stage)
{
    HeDlSchedulerBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        maxMuStations = par("maxMuStations");
        schedulingFunction = par("schedulingFunction").stdstringValue();
        if (schedulingFunction != "fBW" && schedulingFunction != "fHoL")
            throw cRuntimeError("Unknown equal-sized HE RU scheduling function: %s", schedulingFunction.c_str());
    }
}

std::vector<IIeee80211HeDlScheduler::RuAllocation>
HeDlSchedulerEqualSizedRUs::schedule(const ScheduleContext& context)
{
    ASSERT(!std::isnan(context.channelCenterFrequency.get()) && context.channelCenterFrequency > Hz(0));
    ASSERT(!std::isnan(context.channelBandwidth.get()) && context.channelBandwidth > Hz(0));
    if (context.candidates.empty()) {
        recordSchedule(context, {}, {}, false, "no DL MU candidates");
        return {};
    }

    bool enableDlMuMimo = context.enableDlMuMimo;
    bool isApBeamformer = context.localHeCapabilities.dlMuMimoBeamformer;

    EV_DEBUG << "HeDlSchedulerEqualSizedRUs::schedule: " << context.candidates.size()
             << " candidates, schedulingFunction = " << schedulingFunction
             << ", enableDlMuMimo = " << (enableDlMuMimo ? "true" : "false") << "\n";

    if (enableDlMuMimo && isApBeamformer && context.csiManager != nullptr) {
        EV_DEBUG << "HeDlSchedulerEqualSizedRUs::schedule: attempting DL MU-MIMO group selection\n";
        // Collect MU-MIMO eligible candidates who have fresh CSI
        std::vector<CandidateInfo> eligibleCandidates;
        for (const auto& candidate : context.candidates) {
            auto negotiated = candidate.hasNegotiatedHeCapabilities ?
                    &candidate.negotiatedHeCapabilities : nullptr;
            if (negotiated == nullptr || !negotiated->localTxPeerRx.valid)
                continue;
            const Ieee80211HeCapabilities *staCapabilities = candidate.hasAdvertisedHeCapabilities ?
                    &candidate.advertisedHeCapabilities : nullptr;
            if (staCapabilities == nullptr)
                continue;
            if (isDlMuMimoEligible(context.localHeCapabilities, *staCapabilities, *negotiated, context.channelBandwidth, context.numApAntennas) &&
                candidate.hasFreshCsi &&
                context.csiManager->hasFreshCsi(candidate.staAddress, context.channelBandwidth)) {
                eligibleCandidates.push_back(candidate);
            }
        }

        if (eligibleCandidates.size() >= 2) {
            EV_DEBUG << "HeDlSchedulerEqualSizedRUs::schedule: " << eligibleCandidates.size()
                     << " backlogged candidates are MU-MIMO eligible\n";
            std::sort(eligibleCandidates.begin(), eligibleCandidates.end(),
                    [](const CandidateInfo& left, const CandidateInfo& right) {
                        return left.staAddress < right.staAddress;
                    });
            auto anchor = eligibleCandidates.begin();
            if (hasLastMuMimoAnchor) {
                anchor = std::upper_bound(eligibleCandidates.begin(), eligibleCandidates.end(), lastMuMimoAnchor,
                        [](const MacAddress& address, const CandidateInfo& candidate) {
                            return address < candidate.staAddress;
                        });
                if (anchor == eligibleCandidates.end())
                    anchor = eligibleCandidates.begin();
            }
            const MacAddress anchorAddress = anchor->staAddress;
            lastMuMimoAnchor = anchorAddress;
            hasLastMuMimoAnchor = true;
            EV_DEBUG << "HeDlSchedulerEqualSizedRUs::schedule: selected anchor " << anchorAddress << "\n";

            // Rotate the instantaneous eligible set after the persistent MAC-key cursor.
            std::vector<CandidateInfo> orderedMuCandidates;
            orderedMuCandidates.insert(orderedMuCandidates.end(), anchor, eligibleCandidates.end());
            orderedMuCandidates.insert(orderedMuCandidates.end(), eligibleCandidates.begin(), anchor);

            // Get full channel RU
            auto rus = getHeEqualRuLayout(context.channelCenterFrequency, context.channelBandwidth, 1);
            Ieee80211HeRu fullChannelRu = rus[0];
            if (context.coding == HE_CODING_BCC && fullChannelRu.toneSize >= 484) {
                EV_DEBUG << "DL EqualSizedRUs scheduler: skipping MU-MIMO full-channel RU because BCC is not legal for "
                         << fullChannelRu.toneSize << "-tone RUs\n";
                recordSchedule(context, eligibleCandidates, {}, true, "MU-MIMO full-channel RU rejected by BCC coding");
                return {};
            }
            int maxGroupNsts = std::min(8, context.numApAntennas);

            // Build MU-MIMO group greedily
            std::vector<CandidateInfo> groupCandidates;
            std::vector<int> finalNss;
            std::vector<double> finalSnirDb;

            for (const auto& candidate : orderedMuCandidates) {
                auto tempGroup = groupCandidates;
                tempGroup.push_back(candidate);

                // Check total group size limit
                if ((int)tempGroup.size() > maxGroupNsts || (int)tempGroup.size() > 8)
                    continue;

                // Apportion streams: start with 1 stream per user, then round-robin distribute up to 4 per user and 8 total
                std::vector<int> tempNss(tempGroup.size(), 1);
                int totalAllocated = tempGroup.size();
                std::vector<int> limitNss(tempGroup.size());
                for (size_t i = 0; i < tempGroup.size(); ++i) {
                    int maxRxNss = getMaxNss(tempGroup[i].negotiatedHeCapabilities.localTxPeerRx.mcsNss);
                    if (tempGroup[i].operatingModeRxNss > 0)
                        maxRxNss = std::min(maxRxNss, tempGroup[i].operatingModeRxNss);
                    limitNss[i] = std::min(maxRxNss, 4);
                }

                bool progress = true;
                while (totalAllocated < maxGroupNsts && progress) {
                    progress = false;
                    for (size_t i = 0; i < tempGroup.size(); ++i) {
                        if (totalAllocated < maxGroupNsts && tempNss[i] < limitNss[i]) {
                            tempNss[i]++;
                            totalAllocated++;
                            progress = true;
                        }
                    }
                }

                // Check compatibility and calculate SINRs
                std::vector<double> tempSnirDb;
                bool compatible = true;
                for (size_t i = 0; i < tempGroup.size(); ++i) {
                    double snrDb = estimateSnrDb(context, tempGroup[i], fullChannelRu);
                    if (std::isnan(snrDb)) {
                        compatible = false;
                        break;
                    }
                    double snr = std::pow(10.0, snrDb / 10.0);
                    double leakageSum = 0.0;
                    for (size_t j = 0; j < tempGroup.size(); ++j) {
                        if (i == j)
                            continue;
                        leakageSum += context.csiManager->getLeakage(tempGroup[i].staAddress, tempGroup[j].staAddress, context.channelBandwidth);
                    }
                    double snir = (snr * tempNss[i] / totalAllocated) / (1.0 + snr * leakageSum / totalAllocated);
                    if (snir <= 0) {
                        compatible = false;
                        break;
                    }
                    double snirDb = 10.0 * std::log10(snir);
                    if (snirDb < mcsSnrThresholds[0]) {
                        compatible = false;
                        break;
                    }
                    tempSnirDb.push_back(snirDb);
                }

                if (compatible) {
                    groupCandidates = tempGroup;
                    finalNss = tempNss;
                    finalSnirDb = tempSnirDb;
                    EV_DEBUG << "DL EqualSizedRUs scheduler: expanded MU-MIMO group to "
                             << groupCandidates.size() << " STTas\n";
                }
            }

            if (groupCandidates.size() >= 2) {
                std::vector<RuAllocation> result;
                for (size_t i = 0; i < groupCandidates.size(); ++i) {
                    RuAllocation alloc;
                    alloc.staAddress = groupCandidates[i].staAddress;
                    alloc.ru = fullChannelRu;
                    alloc.numberOfSpatialStreams = finalNss[i];
                    alloc.estimatedSnrDb = finalSnirDb[i];
                    alloc.mcs = selectMcs(context, groupCandidates[i], alloc.ru,
                            selectMcs(alloc.estimatedSnrDb, groupCandidates[i].hasFreshPathLoss),
                            finalNss[i]);
                    if (context.coding == HE_CODING_BCC)
                        alloc.mcs = std::min(alloc.mcs, 9);
                    int maxMcs = groupCandidates[i].negotiatedHeCapabilities.localTxPeerRx.mcsNss.maxMcsPerNss[finalNss[i] - 1];
                    if (maxMcs >= 0) {
                        alloc.mcs = std::min(alloc.mcs, maxMcs);
                    }
                    alloc.estimatedDuration = estimateHeMuUserDuration(
                            B(std::max<int64_t>({groupCandidates[i].backlogBytes,
                                    groupCandidates[i].holPacketBytes, 1})),
                            fullChannelRu.toneSize, alloc.mcs, finalNss[i], false, context.guardInterval);
                    result.push_back(alloc);
                }
                EV_INFO << "DL EqualSizedRUs scheduler: selected DL MU-MIMO group of "
                        << result.size() << " STAs on full-channel RU\n";
                recordSchedule(context, groupCandidates, result, true, "DL MU-MIMO group");
                return result;
            }
            EV_DEBUG << "DL EqualSizedRUs scheduler: no compatible MU-MIMO group found\n";
        }
        else {
            EV_DEBUG << "DL EqualSizedRUs scheduler: fewer than two MU-MIMO eligible candidates\n";
        }
    }

    // Fallback: standard equal-sized RU scheduling
    auto selectedCandidates = context.candidates;
    if (schedulingFunction == "fBW") {
        std::sort(selectedCandidates.begin(), selectedCandidates.end(), [] (const CandidateInfo& a, const CandidateInfo& b) {
            if (a.anchor != b.anchor)
                return a.anchor;
            if (a.backlogBytes != b.backlogBytes)
                return a.backlogBytes > b.backlogBytes;
            if (a.holDelay != b.holDelay)
                return a.holDelay > b.holDelay;
            return a.staAddress < b.staAddress;
        });
    }
    else
        std::sort(selectedCandidates.begin(), selectedCandidates.end(), defaultCandidateLess);
    auto validCounts = getHeEqualRuCounts(context.channelBandwidth);
    int candidateLimit = maxMuStations < 0 ? getHeMaxRuCount(context.channelBandwidth) : maxMuStations;
    int candidates = std::min((int)selectedCandidates.size(), candidateLimit);
    if (candidates <= 0) {
        recordSchedule(context, selectedCandidates, {}, false, "no candidates within DL MU station limit");
        return {};
    }
    int ruCount = validCounts.front();
    if (schedulingFunction == "fBW") {
        for (int count : validCounts)
            if (count <= candidates)
                ruCount = count;
    }
    else {
        ruCount = validCounts.back();
        for (int count : validCounts) {
            if (count >= candidates) {
                ruCount = count;
                break;
            }
        }
    }
    std::vector<Ieee80211HeRu> rus;
    int numSelected = 0;
    for (int count : validCounts) {
        if (count < ruCount)
            continue;
        auto layout = getHeEqualRuLayout(context.channelCenterFrequency, context.channelBandwidth, count);
        if ((int)layout.size() != count)
            continue;
        int selected = std::min(candidates, count);
        bool compatible = true;
        for (int i = 0; i < selected; ++i)
            if (!isCompatibleEqualRuAllocation(context, selectedCandidates[i], layout[i])) {
                compatible = false;
                break;
            }
        if (compatible) {
            ruCount = count;
            numSelected = selected;
            rus = std::move(layout);
            break;
        }
    }
    if (rus.empty()) {
        recordSchedule(context, selectedCandidates, {}, false,
                "no equal-sized RU layout satisfies negotiated HE capabilities");
        return {};
    }
    ASSERT(numSelected > 0);

    EV_DEBUG << "DL EqualSizedRUs scheduler: falling back to " << ruCount
            << " equal-sized RUs, scheduling " << numSelected << " STAs\n";

    std::vector<RuAllocation> result;
    result.reserve(numSelected);
    for (int i = 0; i < numSelected; ++i) {
        RuAllocation alloc;
        alloc.staAddress = selectedCandidates[i].staAddress;
        alloc.ru = rus[i];
        alloc.estimatedSnrDb = estimateSnrDb(context, selectedCandidates[i], alloc.ru);
        alloc.mcs = selectMcs(context, selectedCandidates[i], alloc.ru,
                selectMcs(alloc.estimatedSnrDb, selectedCandidates[i].hasFreshPathLoss));
        if (context.coding == HE_CODING_BCC)
            alloc.mcs = std::min(alloc.mcs, 9);
        int maxMcs = -1;
        if (selectedCandidates[i].hasNegotiatedHeCapabilities) {
            maxMcs = selectedCandidates[i].negotiatedHeCapabilities.localTxPeerRx.mcsNss.maxMcsPerNss[0];
        }
        if (maxMcs >= 0) {
            alloc.mcs = std::min(alloc.mcs, maxMcs);
        }
        // The packing planner uses this value as the common per-user packing
        // horizon. Estimate it from all currently eligible MPDUs instead of
        // the HoL MPDU alone, otherwise a symmetric equal-RU schedule can
        // never grow beyond one MPDU per user despite maxAmpduMpduCount.
        alloc.estimatedDuration = estimateDuration(
                std::max<int64_t>({selectedCandidates[i].backlogBytes,
                        selectedCandidates[i].holPacketBytes, 1}),
                alloc.ru.toneSize, alloc.mcs, context.guardInterval);
        result.push_back(alloc);
    }
    recordSchedule(context, selectedCandidates, result, false, schedulingFunction.c_str());
    return result;
}

} // namespace ieee80211
} // namespace inet
