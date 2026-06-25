//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/scheduler/HeDlSchedulerBase.h"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

void HeDlSchedulerBase::initialize(int stage)
{
    SimpleModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        maxMuStations = par("maxMuStations");
        smallBacklogThreshold = par("smallBacklogThreshold");
        mediumBacklogThreshold = par("mediumBacklogThreshold");
        mtuBacklogThreshold = par("mtuBacklogThreshold");
        largeBacklogThreshold = par("largeBacklogThreshold");
        thermalNoisePsdDbmHz = par("thermalNoisePsd");
        lowDurationRatio = par("lowDurationRatio");
        highDurationRatio = par("highDurationRatio");
        maxDurationAlignmentIterations = par("maxDurationAlignmentIterations");
        const char *heRateControlModule = par("heRateControlModule");
        heRateControl = *heRateControlModule == '\0' ? nullptr :
                dynamic_cast<IIeee80211HeRateControl *>(getModuleByPath(heRateControlModule));
        if (*heRateControlModule != '\0' && heRateControl == nullptr)
            throw cRuntimeError("heRateControlModule '%s' is not an IIeee80211HeRateControl", heRateControlModule);
        if (lowDurationRatio < 0 || highDurationRatio <= lowDurationRatio)
            throw cRuntimeError("Invalid HE duration alignment ratios");
        if (maxDurationAlignmentIterations < 0)
            throw cRuntimeError("maxDurationAlignmentIterations must be nonnegative");
        std::stringstream stream(par("heMcsSnrThresholds").stdstringValue());
        double value;
        while (stream >> value)
            mcsSnrThresholds.push_back(value);
        if (mcsSnrThresholds.size() != 12)
            throw cRuntimeError("heMcsSnrThresholds must contain exactly 12 values");
        for (size_t i = 1; i < mcsSnrThresholds.size(); ++i)
            if (mcsSnrThresholds[i] < mcsSnrThresholds[i - 1])
                throw cRuntimeError("heMcsSnrThresholds must be nondecreasing");
    }
}

int HeDlSchedulerBase::requestRuForBytes(int64_t bytes, Hz channelBandwidth) const
{
    // Map queue backlog size to standard RU tone sizes (Clause 27.3.2.2).
    // Larger queue backlogs warrant wider RUs to achieve higher throughput,
    // while small payloads are scheduled on narrower RUs (e.g. 26-tone RUs)
    // to maximize frequency-division multiplexing efficiency.
    ASSERT(bytes >= 0);
    ASSERT(!std::isnan(channelBandwidth.get()) && channelBandwidth > Hz(0));
    if (bytes <= smallBacklogThreshold)
        return 26;
    if (bytes <= mediumBacklogThreshold)
        return 52;
    if (bytes <= mtuBacklogThreshold)
        return 106;
    if (bytes <= largeBacklogThreshold)
        return getHeChannelToneCount(channelBandwidth) >= 996 ? 484 : 242;
    return getHeChannelToneCount(channelBandwidth);
}

int HeDlSchedulerBase::getNextSmallerRu(int toneSize) const
{
    switch (toneSize) {
        case 1992: return 996;
        case 996: return 484;
        case 484: return 242;
        case 242: return 106;
        case 106: return 52;
        case 52: return 26;
        default: return 0;
    }
}

int HeDlSchedulerBase::getNextLargerRu(int toneSize) const
{
    switch (toneSize) {
        case 26: return 52;
        case 52: return 106;
        case 106: return 242;
        case 242: return 484;
        case 484: return 996;
        case 996: return 1992;
        default: return 0;
    }
}

double HeDlSchedulerBase::estimateSnrDb(const ScheduleContext& context, const CandidateInfo& candidate,
        const Ieee80211HeRu& ru) const
{
    if (!candidate.hasFreshPathLoss || std::isnan(context.totalTransmitPower.get()) ||
            context.totalTransmitPower.get() <= 0)
        return NaN;
    // Per-RU SNR calculation:
    // Scale total transmit power down to the RU bandwidth (assuming uniform power spectral density
    // across all subcarriers, Clause 27.3.21).
    // SNR = P_rx_RU - N_thermal_RU - NoiseFigure
    double totalDbm = 10 * std::log10(context.totalTransmitPower.get() / 1e-3);
    double ruPowerDbm = totalDbm + 10 * std::log10(ru.bandwidth.get() / context.channelBandwidth.get());
    double noiseDbm = thermalNoisePsdDbmHz + 10 * std::log10(ru.bandwidth.get()) + context.noiseFigureDb;
    return ruPowerDbm - candidate.pathLossDb - noiseDbm;
}

int HeDlSchedulerBase::selectMcs(double snrDb, bool hasFreshPathLoss) const
{
    // Without fresh link information fall back to the most robust MCS (MCS 0) for safety.
    if (!hasFreshPathLoss || std::isnan(snrDb))
        return 0;
    int mcs = 0;
    for (int i = 0; i < (int)mcsSnrThresholds.size(); ++i)
        if (snrDb >= mcsSnrThresholds[i])
            mcs = i;
    return mcs;
}

int HeDlSchedulerBase::selectMcs(const ScheduleContext& context, const CandidateInfo& candidate,
        const Ieee80211HeRu& ru, int fallbackMcs, int maxNss) const
{
    if (heRateControl == nullptr)
        return fallbackMcs;
    IIeee80211HeRateControl::Constraints constraints;
    constraints.ldpc = context.coding == HE_CODING_LDPC;
    constraints.maxMcs = context.coding == HE_CODING_BCC ? 9 : 11;
    if (candidate.negotiatedHeCapabilities != nullptr &&
            candidate.negotiatedHeCapabilities->valid) {
        int peerMaxMcs = candidate.negotiatedHeCapabilities->intersection.txMcsNss.maxMcsPerNss[0];
        if (peerMaxMcs >= 0)
            constraints.maxMcs = std::min(constraints.maxMcs, peerMaxMcs);
    }
    if (candidate.hasFreshPathLoss && !std::isnan(context.totalTransmitPower.get()))
        heRateControl->reportHeRxSnir(candidate.staAddress, estimateSnrDb(context, candidate, ru));
    auto selection = heRateControl->selectHeMode(candidate.staAddress, context.channelBandwidth,
            ru.toneSize, HE_MU_DOWNLINK, maxNss, constraints);
    return selection.mode == nullptr ? fallbackMcs : selection.mcs;
}

simtime_t HeDlSchedulerBase::estimateDuration(int64_t bytes, int toneSize, int mcs,
        Ieee80211HeGuardInterval guardInterval) const
{
    return estimateHeMuUserDuration(B(bytes), toneSize, mcs, 1, false, guardInterval);
}

bool HeDlSchedulerBase::defaultCandidateLess(const CandidateInfo& a, const CandidateInfo& b)
{
    if (a.anchor != b.anchor)
        return a.anchor;
    if (a.holDelay != b.holDelay)
        return a.holDelay > b.holDelay;
    if (a.backlogBytes != b.backlogBytes)
        return a.backlogBytes > b.backlogBytes;
    return a.staAddress < b.staAddress;
}

std::vector<IIeee80211HeDlScheduler::RuAllocation> HeDlSchedulerBase::schedule(
        const std::vector<MacAddress>& candidates, Hz channelCenterFrequency, Hz channelBandwidth)
{
    ASSERT(!std::isnan(channelCenterFrequency.get()) && channelCenterFrequency > Hz(0));
    ASSERT(!std::isnan(channelBandwidth.get()) && channelBandwidth > Hz(0));
    ScheduleContext context;
    context.channelCenterFrequency = channelCenterFrequency;
    context.channelBandwidth = channelBandwidth;
    for (size_t i = 0; i < candidates.size(); ++i) {
        CandidateInfo candidate;
        candidate.staAddress = candidates[i];
        candidate.anchor = i == 0;
        context.candidates.push_back(candidate);
    }
    if (!candidates.empty())
        context.anchorSta = candidates.front();
    return schedule(context);
}

std::vector<IIeee80211HeDlScheduler::RuAllocation> HeDlSchedulerBase::fitRequestedRus(
        const ScheduleContext& context, const std::vector<CandidateInfo>& candidates,
        std::vector<int> requestedTones, const std::vector<int64_t>& payloadBytes) const
{
    ASSERT(!std::isnan(context.channelCenterFrequency.get()) && context.channelCenterFrequency > Hz(0));
    ASSERT(!std::isnan(context.channelBandwidth.get()) && context.channelBandwidth > Hz(0));
    if (candidates.size() < 2)
        return {};
    if (requestedTones.size() != candidates.size() || payloadBytes.size() != candidates.size())
        throw cRuntimeError("HE scheduler fitting inputs have different sizes");
    if (context.coding == HE_CODING_BCC) {
        for (auto& tones : requestedTones)
            if (tones >= 484)
                tones = 242;
    }
    std::vector<int> requestOrder(candidates.size());
    for (size_t i = 0; i < requestOrder.size(); ++i)
        requestOrder[i] = i;

    EV_DEBUG << "HeDlSchedulerBase::fitRequestedRus: fitting " << candidates.size()
             << " candidates, initial requested tones =";
    for (int tones : requestedTones)
        EV_DEBUG << " " << tones;
    EV_DEBUG << "\n";

    // Helper that maps requested tone sizes to actual RU allocations and computes per-user MCS/duration.
    auto buildAllocations = [&] (const std::vector<int>& toneSizes,
            std::vector<RuAllocation>& allocations) {
        std::sort(requestOrder.begin(), requestOrder.end(), [&] (int a, int b) {
            if (toneSizes[a] != toneSizes[b])
                return toneSizes[a] > toneSizes[b];
            if (candidates[a].anchor != candidates[b].anchor)
                return candidates[a].anchor;
            return a < b;
        });
        std::vector<int> sortedRequests;
        for (int index : requestOrder)
            sortedRequests.push_back(toneSizes[index]);
        std::vector<Ieee80211HeRu> fittedRus;
        if (!allocateHeRus(context.channelCenterFrequency, context.channelBandwidth,
                sortedRequests, fittedRus, context.puncturedSubchannels))
            return false;

        std::vector<Ieee80211HeRu> rusByCandidate(candidates.size());
        for (size_t i = 0; i < requestOrder.size(); ++i)
            rusByCandidate[requestOrder[i]] = fittedRus[i];

        allocations.clear();
        allocations.reserve(candidates.size());
        for (size_t i = 0; i < candidates.size(); ++i) {
            const auto *negotiated = candidates[i].negotiatedHeCapabilities;
            if (negotiated != nullptr &&
                    (!negotiated->valid ||
                     !negotiated->intersection.dlOfdma ||
                     negotiated->intersection.supportedChannelWidths.count(context.channelBandwidth) == 0 ||
                     negotiated->intersection.supportedRuToneSizes.count(rusByCandidate[i].toneSize) == 0))
                return false;
            RuAllocation allocation;
            allocation.staAddress = candidates[i].staAddress;
            allocation.ru = rusByCandidate[i];
            allocation.estimatedSnrDb = estimateSnrDb(context, candidates[i], allocation.ru);
            allocation.mcs = selectMcs(context, candidates[i], allocation.ru,
                    selectMcs(allocation.estimatedSnrDb, candidates[i].hasFreshPathLoss));
            if (context.coding == HE_CODING_BCC)
                allocation.mcs = std::min(allocation.mcs, 9);
            if (negotiated != nullptr) {
                int maxMcs = negotiated->intersection.txMcsNss.maxMcsPerNss[0];
                if (maxMcs < 0)
                    return false;
                allocation.mcs = std::min(allocation.mcs, maxMcs);
            }
            allocation.estimatedDuration = estimateDuration(
                    std::max<int64_t>(payloadBytes[i], 1), toneSizes[i], allocation.mcs,
                    context.guardInterval);
            allocations.push_back(allocation);
        }
        return true;
    };

    std::vector<RuAllocation> result;
    // Iteratively shrink the largest non-anchor RU request until a valid HE RU layout is found.
    // If the combined requests exceed the channel's tone layout boundaries or overlap (Clause 27.3.2.2),
    // we search for candidate RUs to downgrade to standard smaller tone sizes.
    while (!buildAllocations(requestedTones, result)) {
        int downgrade = -1;
        for (int i = 0; i < (int)requestedTones.size(); ++i) {
            if (requestedTones[i] <= 26)
                continue;
            if (downgrade == -1 ||
                    (candidates[downgrade].anchor && !candidates[i].anchor) ||
                    (candidates[downgrade].anchor == candidates[i].anchor &&
                     requestedTones[i] > requestedTones[downgrade]) ||
                    (candidates[downgrade].anchor == candidates[i].anchor &&
                     requestedTones[i] == requestedTones[downgrade] && i > downgrade))
                downgrade = i;
        }
        if (downgrade == -1) {
            EV_DEBUG << "HeDlSchedulerBase::fitRequestedRus: cannot fit requested RUs, no further downgrades possible\n";
            return {};
        }
        EV_DEBUG << "HeDlSchedulerBase::fitRequestedRus: downgrading candidate " << downgrade
                 << " (" << candidates[downgrade].staAddress << ") from " << requestedTones[downgrade]
                 << " to " << getNextSmallerRu(requestedTones[downgrade]) << " tones\n";
        requestedTones[downgrade] = getNextSmallerRu(requestedTones[downgrade]);
    }

    auto durationVariance = [] (const std::vector<RuAllocation>& allocations) {
        double mean = 0;
        for (const auto& allocation : allocations)
            mean += allocation.estimatedDuration.dbl();
        mean /= allocations.size();
        double variance = 0;
        for (const auto& allocation : allocations) {
            double delta = allocation.estimatedDuration.dbl() - mean;
            variance += delta * delta;
        }
        return variance / allocations.size();
    };

    int channelTones = getHeChannelToneCount(context.channelBandwidth);
    
    // Duration Alignment (IEEE 802.11-2024 Clause 27.3.11.13):
    // In HE MU-OFDMA, all users sharing the PPDU container must have identical transmission durations.
    // However, users with different backlogs, MCS schemes, or stream counts will finish at different times,
    // leading to channel waste (padding overhead) or synchronization issues.
    // This optimization loop shifts RU bandwidth (tone sizes) from "fast" users (short duration)
    // to "slow" users (long duration) to minimize the variance around the mean duration.
    for (int iteration = 0; iteration < maxDurationAlignmentIterations; ++iteration) {
        double mean = 0;
        for (const auto& allocation : result)
            mean += allocation.estimatedDuration.dbl();
        mean /= result.size();
        double bestVariance = durationVariance(result);
        std::vector<int> bestTones = requestedTones;
        std::vector<RuAllocation> bestResult = result;

        EV_DEBUG << "HE DL base scheduler: duration alignment iteration " << iteration
                 << ", mean duration = " << mean << ", variance = " << bestVariance << "\n";

        auto tryProposal = [&] (const std::vector<int>& proposal) {
            std::vector<RuAllocation> proposalResult;
            if (!buildAllocations(proposal, proposalResult))
                return;
            double proposalVariance = durationVariance(proposalResult);
            if (proposalVariance + 1e-18 < bestVariance) {
                bestVariance = proposalVariance;
                bestTones = proposal;
                bestResult = proposalResult;
            }
        };

        std::vector<int> slowCandidates;
        std::vector<int> fastCandidates;
        for (int i = 0; i < (int)result.size(); ++i) {
            double duration = result[i].estimatedDuration.dbl();
            // A "slow" candidate exceeds the high threshold ratio: attempt to expand its RU to speed it up.
            if (duration > highDurationRatio * mean) {
                int larger = getNextLargerRu(requestedTones[i]);
                if (larger != 0 && larger <= channelTones) {
                    slowCandidates.push_back(i);
                    auto proposal = requestedTones;
                    proposal[i] = larger;
                    tryProposal(proposal);
                }
            }
            // A "fast" candidate is below the low threshold ratio: attempt to shrink its RU to slow it down.
            if (duration < lowDurationRatio * mean && requestedTones[i] > 26) {
                fastCandidates.push_back(i);
                auto proposal = requestedTones;
                proposal[i] = getNextSmallerRu(requestedTones[i]);
                tryProposal(proposal);
            }
        }

        // Try co-tuning: shrink a fast user and enlarge a slow user simultaneously.
        for (int slow : slowCandidates) {
            for (int fast : fastCandidates) {
                if (slow == fast)
                    continue;
                auto proposal = requestedTones;
                proposal[slow] = getNextLargerRu(proposal[slow]);
                proposal[fast] = getNextSmallerRu(proposal[fast]);
                tryProposal(proposal);
            }
        }

        if (bestTones == requestedTones) {
            EV_DEBUG << "HE DL base scheduler: duration alignment converged after "
                     << (iteration + 1) << " iterations\n";
            break;
        }
        requestedTones = bestTones;
        result = bestResult;
    }

    if (!validateHeRuLayout([&] {
            std::vector<Ieee80211HeRu> layout;
            for (const auto& allocation : result)
                layout.push_back(allocation.ru);
            return layout;
        }(), context.channelBandwidth))
        throw cRuntimeError("HE scheduler produced an invalid RU layout");
    ASSERT(result.empty() || result.size() == candidates.size());
    EV_DEBUG << "HE DL base scheduler: produced " << result.size()
            << " RU allocations, durations =";
    for (const auto& allocation : result)
        EV_DEBUG << " " << allocation.estimatedDuration;
    EV_DEBUG << "\n";

    return result;
}

} // namespace ieee80211
} // namespace inet
