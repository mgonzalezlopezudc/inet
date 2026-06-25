//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/ratecontrol/HeMinstrelRateControl.h"

#include <algorithm>
#include <cmath>

#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

Define_Module(HeMinstrelRateControl);

void HeMinstrelRateControl::initialize(int stage)
{
    RateControlBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        updateInterval = par("updateInterval");
        ewmaWeight = par("ewmaWeight");
        lookaroundRatio = par("lookaroundRatio");
        initialSuccessProbability = par("initialSuccessProbability");
        seedFromSnir = par("seedFromSnir");
        enableExtendedRangeSu = par("enableExtendedRangeSu");
        preferDcm = par("preferDcm");
        snirMcs0ThresholdDb = par("snirMcs0Threshold");
        snirMcsStepDb = par("snirMcsStep");
        minMcs = par("minMcs");
        maxMcs = par("maxMcs");
        maxNss = par("maxNss");
        if (minMcs < 0 || maxMcs > 11 || minMcs > maxMcs)
            throw cRuntimeError("Invalid HE Minstrel MCS range");
        if (maxNss < 1 || maxNss > 8)
            throw cRuntimeError("Invalid HE Minstrel maxNss");
        if (ewmaWeight < 0 || ewmaWeight > 1 || lookaroundRatio < 0 || lookaroundRatio > 1)
            throw cRuntimeError("Invalid HE Minstrel EWMA/lookaround parameters");
        selectedMcsSignal = registerSignal("heRateSelectedMcs");
        selectedNssSignal = registerSignal("heRateSelectedNss");
        probeSignal = registerSignal("heRateProbe");
        successProbabilitySignal = registerSignal("heRateSuccessProbability");
        WATCH(minMcs);
        WATCH(maxMcs);
        WATCH(maxNss);
    }
}

void HeMinstrelRateControl::handleMessage(cMessage *msg)
{
    throw cRuntimeError("This module doesn't handle self messages");
}

const MacAddress HeMinstrelRateControl::getReceiverAddress(Packet *frame) const
{
    if (frame == nullptr)
        return MacAddress::UNSPECIFIED_ADDRESS;
    auto header = dynamicPtrCast<const Ieee80211MacHeader>(frame->peekAtFront<Ieee80211MacHeader>(b(-1),
            Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED));
    return header == nullptr ? MacAddress::UNSPECIFIED_ADDRESS : header->getReceiverAddress();
}

HeMinstrelRateControl::PeerState& HeMinstrelRateControl::getPeerState(const MacAddress& peer)
{
    return peers[peer];
}

const Ieee80211HeMode *HeMinstrelRateControl::findHeMode(int mcs, int nss, Hz bandwidth,
        bool extendedRangeSu, bool ldpc) const
{
    if (modeSet == nullptr)
        return nullptr;
    for (int i = 0; i < modeSet->getNumModes(); i++) {
        auto mode = dynamic_cast<const Ieee80211HeMode *>(modeSet->getMode(i));
        if (mode == nullptr)
            continue;
        auto dataMode = mode->getDataMode();
        auto heMcs = dataMode->getModulationAndCodingScheme();
        if (heMcs->getMcsIndex() == (unsigned int)mcs &&
                heMcs->getNumNss() == (unsigned int)nss &&
                (std::isnan(bandwidth.get()) || dataMode->getBandwidth() == bandwidth) &&
                dataMode->isLdpc() == ldpc) {
            auto preambleFormat = mode->getPreambleMode()->getPreambleFormat();
            if (preambleFormat == (extendedRangeSu ? Ieee80211HePreambleMode::HE_PREAMBLE_ER_SU :
                    Ieee80211HePreambleMode::HE_PREAMBLE_SU))
                return mode;
            if (extendedRangeSu && preambleFormat == Ieee80211HePreambleMode::HE_PREAMBLE_SU)
                return Ieee80211HeCompliantModes::getCompliantMode(heMcs,
                        mode->getCenterFrequencyMode(),
                        Ieee80211HePreambleMode::HE_PREAMBLE_ER_SU,
                        Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG, ldpc);
        }
    }
    return nullptr;
}

int HeMinstrelRateControl::clampMcsForConstraints(int mcs, int ruToneSize, uint8_t ppduFormat,
        int maxNss, const Constraints& constraints) const
{
    int result = std::clamp(mcs, std::max(minMcs, constraints.minMcs),
            std::min(maxMcs, constraints.maxMcs));
    if (constraints.extendedRangeSu || ppduFormat == HE_EXTENDED_RANGE_SU)
        result = std::min(result, 2);
    if (ruToneSize > 0 && ruToneSize < 106)
        result = std::min(result, 9);
    if (constraints.dcm && result > 4)
        result = 4;
    return std::clamp(result, 0, 11);
}

double HeMinstrelRateControl::scoreRate(const RateStats& stats, const Ieee80211HeMode *mode) const
{
    return stats.ewmaSuccessProbability * mode->getDataMode()->getNetBitrate().get();
}

IIeee80211HeRateControl::Selection HeMinstrelRateControl::selectHeMode(const MacAddress& peer,
        Hz bandwidth, int ruToneSize, uint8_t ppduFormat, int requestedMaxNss,
        const Constraints& constraints)
{
    Enter_Method("selectHeMode");
    if (modeSet == nullptr)
        return {};
    requestedMaxNss = std::clamp(requestedMaxNss, 1, maxNss);
    if (constraints.extendedRangeSu || ppduFormat == HE_EXTENDED_RANGE_SU)
        requestedMaxNss = 1;
    auto& state = getPeerState(peer);
    std::vector<std::pair<RateKey, const Ieee80211HeMode *>> candidates;
    for (int nss = 1; nss <= requestedMaxNss; nss++) {
        for (int mcs = minMcs; mcs <= maxMcs; mcs++) {
            int constrainedMcs = clampMcsForConstraints(mcs, ruToneSize, ppduFormat, requestedMaxNss, constraints);
            if (constrainedMcs != mcs || !isHeValidMcsNssCombination(mcs, nss, ruToneSize))
                continue;
            auto mode = findHeMode(mcs, nss, bandwidth, constraints.extendedRangeSu, constraints.ldpc);
            if (mode == nullptr)
                mode = findHeMode(mcs, nss, bandwidth, false, constraints.ldpc);
            if (mode != nullptr)
                candidates.push_back({{mcs, nss}, mode});
        }
    }
    if (candidates.empty())
        return {};

    for (const auto& candidate : candidates) {
        auto& stats = state.rates[candidate.first];
        if (stats.attempts == 0 && stats.successes == 0)
            stats.ewmaSuccessProbability = initialSuccessProbability;
        if (seedFromSnir && !std::isnan(stats.lastSnirDb)) {
            double margin = stats.lastSnirDb - (snirMcs0ThresholdDb + snirMcsStepDb * candidate.first.mcs);
            double seeded = std::clamp(0.5 + margin / 20.0, 0.05, 0.98);
            stats.ewmaSuccessProbability = ewmaWeight * stats.ewmaSuccessProbability +
                    (1 - ewmaWeight) * seeded;
        }
    }

    state.selectionCount++;
    bool probe = lookaroundRatio > 0 && intuniform(0, 999) < (int)std::round(lookaroundRatio * 1000);
    auto best = candidates.front();
    if (probe) {
        int index = intuniform(0, candidates.size() - 1);
        best = candidates[index];
        state.rates[best.first].lastProbe = simTime();
    }
    else {
        double bestScore = -1;
        for (const auto& candidate : candidates) {
            double score = scoreRate(state.rates[candidate.first], candidate.second);
            if (score > bestScore) {
                bestScore = score;
                best = candidate;
            }
        }
    }

    currentMode = best.second;
    emit(selectedMcsSignal, (long)best.first.mcs);
    emit(selectedNssSignal, (long)best.first.numberOfSpatialStreams);
    emit(probeSignal, probe ? 1L : 0L);
    emit(successProbabilitySignal, state.rates[best.first].ewmaSuccessProbability);
    emitDatarateChangedSignal();

    Selection selection;
    selection.mode = best.second;
    selection.mcs = best.first.mcs;
    selection.numberOfSpatialStreams = best.first.numberOfSpatialStreams;
    selection.dcm = constraints.dcm;
    selection.probing = probe;
    return selection;
}

void HeMinstrelRateControl::reportHeTxResult(const MacAddress& peer, int mcs,
        int numberOfSpatialStreams, int ruToneSize, int retryCount, bool success, int64_t ackedBytes)
{
    Enter_Method("reportHeTxResult");
    auto& stats = getPeerState(peer).rates[{mcs, numberOfSpatialStreams}];
    if (stats.attempts == 0 && stats.successes == 0)
        stats.ewmaSuccessProbability = initialSuccessProbability;
    stats.attempts++;
    if (success)
        stats.successes++;
    double sample = success ? 1.0 / std::max(1, retryCount + 1) : 0.0;
    stats.ewmaSuccessProbability = ewmaWeight * stats.ewmaSuccessProbability +
            (1 - ewmaWeight) * sample;
    emit(successProbabilitySignal, stats.ewmaSuccessProbability);
}

void HeMinstrelRateControl::reportHeRxSnir(const MacAddress& peer, double snirDb)
{
    Enter_Method("reportHeRxSnir");
    auto& state = getPeerState(peer);
    for (auto& rate : state.rates)
        rate.second.lastSnirDb = snirDb;
}

const IIeee80211Mode *HeMinstrelRateControl::getRate()
{
    Enter_Method("getRate");
    if (currentMode == nullptr && modeSet != nullptr) {
        Constraints constraints;
        constraints.extendedRangeSu = enableExtendedRangeSu;
        constraints.dcm = preferDcm;
        auto selection = selectHeMode(MacAddress::BROADCAST_ADDRESS, Hz(NaN), 0,
                enableExtendedRangeSu ? HE_EXTENDED_RANGE_SU : HE_SINGLE_USER, maxNss, constraints);
        currentMode = selection.mode != nullptr ? selection.mode : modeSet->getSlowestMode();
    }
    return currentMode;
}

void HeMinstrelRateControl::frameTransmitted(Packet *frame, int retryCount, bool isSuccessful, bool isGivenUp)
{
    if (auto heMode = dynamic_cast<const Ieee80211HeMode *>(currentMode)) {
        auto mcs = heMode->getDataMode()->getModulationAndCodingScheme();
        reportHeTxResult(getReceiverAddress(frame), mcs->getMcsIndex(), mcs->getNumNss(),
                0, retryCount, isSuccessful && !isGivenUp, frame == nullptr ? 0 : frame->getByteLength());
    }
}

void HeMinstrelRateControl::frameReceived(Packet *frame)
{
}

} // namespace ieee80211
} // namespace inet
