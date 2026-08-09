//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/ratecontrol/MinstrelRateControlBase.h"

#include <algorithm>
#include <cmath>

#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"

namespace inet {
namespace ieee80211 {

void MinstrelRateControlBase::initialize(int stage)
{
    RateControlBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        updateInterval = par("updateInterval");
        ewmaWeight = par("ewmaWeight");
        lookaroundRatio = par("lookaroundRatio");
        initialSuccessProbability = par("initialSuccessProbability");
        seedFromSnir = par("seedFromSnir");
        snirMcs0ThresholdDb = par("snirMcs0Threshold");
        snirMcsStepDb = par("snirMcsStep");
        minMcs = par("minMcs");
        maxMcs = par("maxMcs");
        maxNss = par("maxNss");
        if (updateInterval <= SIMTIME_ZERO || !std::isfinite(ewmaWeight) || ewmaWeight < 0 ||
                ewmaWeight > 1 || !std::isfinite(lookaroundRatio) || lookaroundRatio < 0 ||
                lookaroundRatio > 1 || !std::isfinite(initialSuccessProbability) ||
                initialSuccessProbability < 0 || initialSuccessProbability > 1 ||
                !std::isfinite(snirMcs0ThresholdDb) || !std::isfinite(snirMcsStepDb) ||
                snirMcsStepDb <= 0 || minMcs < 0 || minMcs > maxMcs || maxNss < 1)
            throw cRuntimeError("Invalid Minstrel parameters");
        WATCH(peers);
    }
}

void MinstrelRateControlBase::handleMessage(cMessage *)
{
    throw cRuntimeError("This rate-control module does not handle messages");
}

void MinstrelRateControlBase::registerMinstrelSignals(const std::string& prefix)
{
    selectedMcsSignal = registerSignal((prefix + "SelectedMcs").c_str());
    selectedNssSignal = registerSignal((prefix + "SelectedNss").c_str());
    probeSignal = registerSignal((prefix + "Probe").c_str());
    successProbabilitySignal = registerSignal((prefix + "SuccessProbability").c_str());
    txSuccessSignal = registerSignal((prefix + "TxSuccess").c_str());
    retryCountSignal = registerSignal((prefix + "RetryCount").c_str());
}

MinstrelRateControlBase::PeerState& MinstrelRateControlBase::getPeerState(const MacAddress& peer)
{
    return peers[peer];
}

const MacAddress MinstrelRateControlBase::getReceiverAddress(Packet *frame) const
{
    if (frame == nullptr)
        return MacAddress::UNSPECIFIED_ADDRESS;
    auto header = dynamicPtrCast<const Ieee80211MacHeader>(frame->peekAtFront<Ieee80211MacHeader>(b(-1),
            Chunk::PF_ALLOW_INCORRECT | Chunk::PF_ALLOW_INCOMPLETE | Chunk::PF_ALLOW_IMPROPERLY_REPRESENTED));
    return header == nullptr ? MacAddress::UNSPECIFIED_ADDRESS : header->getReceiverAddress();
}

const physicallayer::IIeee80211Mode *MinstrelRateControlBase::selectCandidate(const MacAddress& peer,
        const std::vector<const physicallayer::IIeee80211Mode *>& candidates, bool& probing)
{
    auto& state = getPeerState(peer);
    bool hasFreshSnir = seedFromSnir && state.snirGeneration != 0 &&
            simTime() >= state.latestSnirUpdate &&
            simTime() - state.latestSnirUpdate <= updateInterval;
    for (auto mode : candidates) {
        auto [it, inserted] = state.rates.try_emplace(mode);
        if (inserted)
            it->second.ewmaSuccessProbability = initialSuccessProbability;
        if (hasFreshSnir && it->second.appliedSnirGeneration != state.snirGeneration) {
            double margin = state.latestSnirDb -
                    (snirMcs0ThresholdDb + snirMcsStepDb * getModeMcs(mode));
            double seeded = std::clamp(0.5 + margin / 20.0, 0.05, 0.98);
            it->second.ewmaSuccessProbability = ewmaWeight * it->second.ewmaSuccessProbability +
                    (1 - ewmaWeight) * seeded;
            it->second.appliedSnirGeneration = state.snirGeneration;
        }
    }
    state.selectionCount++;
    probing = lookaroundRatio > 0 && intuniform(0, 999) < (int)std::round(lookaroundRatio * 1000);
    const auto *best = candidates.front();
    if (probing) {
        best = candidates[intuniform(0, candidates.size() - 1)];
        state.rates[best].lastProbe = simTime();
    }
    else {
        double bestScore = -1;
        for (auto mode : candidates) {
            double score = state.rates[mode].ewmaSuccessProbability *
                    mode->getDataMode()->getNetBitrate().get();
            if (score > bestScore) {
                bestScore = score;
                best = mode;
            }
        }
    }
    currentMode = best;
    emitSelection(peer, best, probing);
    return best;
}

void MinstrelRateControlBase::emitSelection(const MacAddress& peer,
        const physicallayer::IIeee80211Mode *mode, bool probing)
{
    auto& state = getPeerState(peer);
    auto it = state.rates.find(const_cast<physicallayer::IIeee80211Mode *>(mode));
    if (it == state.rates.end())
        return;
    if (selectedMcsSignal >= 0)
        emit(selectedMcsSignal, (long)getModeMcs(mode));
    if (selectedNssSignal >= 0)
        emit(selectedNssSignal, (long)getModeNss(mode));
    if (probeSignal >= 0)
        emit(probeSignal, probing ? 1L : 0L);
    if (successProbabilitySignal >= 0)
        emit(successProbabilitySignal, it->second.ewmaSuccessProbability);
    emitDatarateChangedSignal();
}

void MinstrelRateControlBase::reportModeTxResult(const MacAddress& peer,
        const physicallayer::IIeee80211Mode *mode, int retryCount, bool success)
{
    if (mode == nullptr || !isRateCandidate(mode))
        return;
    auto& state = getPeerState(peer);
    auto [it, inserted] = state.rates.try_emplace(mode);
    if (inserted)
        it->second.ewmaSuccessProbability = initialSuccessProbability;
    auto& stats = it->second;
    stats.attempts++;
    if (success)
        stats.successes++;
    double sample = success ? 1.0 / std::max(1, retryCount + 1) : 0.0;
    stats.ewmaSuccessProbability = ewmaWeight * stats.ewmaSuccessProbability +
            (1 - ewmaWeight) * sample;
    if (selectedMcsSignal >= 0)
        emit(selectedMcsSignal, (long)getModeMcs(mode));
    if (selectedNssSignal >= 0)
        emit(selectedNssSignal, (long)getModeNss(mode));
    if (successProbabilitySignal >= 0)
        emit(successProbabilitySignal, stats.ewmaSuccessProbability);
    if (txSuccessSignal >= 0)
        emit(txSuccessSignal, success ? 1L : 0L);
    if (retryCountSignal >= 0)
        emit(retryCountSignal, (long)retryCount);
}

void MinstrelRateControlBase::reportModeRxSnir(const MacAddress& peer, double snirDb)
{
    if (!std::isfinite(snirDb))
        throw cRuntimeError("Minstrel SNIR observation must be finite");
    auto& state = getPeerState(peer);
    state.latestSnirDb = snirDb;
    state.latestSnirUpdate = simTime();
    state.snirGeneration++;
    if (state.snirGeneration == 0) {
        state.snirGeneration = 1;
        for (auto& rate : state.rates)
            rate.second.appliedSnirGeneration = 0;
    }
}

void MinstrelRateControlBase::frameTransmitted(Packet *frame, int retryCount,
        bool isSuccessful, bool isGivenUp)
{
    const auto *mode = currentMode;
    if (frame != nullptr) {
        auto modeReq = frame->findTag<physicallayer::Ieee80211ModeReq>();
        if (modeReq != nullptr && modeReq->getMode() != nullptr)
            mode = modeReq->getMode();
    }
    reportModeTxResult(getReceiverAddress(frame), mode, retryCount,
            isSuccessful && !isGivenUp);
}

void MinstrelRateControlBase::frameReceived(Packet *)
{
}

const physicallayer::IIeee80211Mode *MinstrelRateControlBase::selectRate(const MacAddress& peer,
        const std::vector<const physicallayer::IIeee80211Mode *>& candidates)
{
    std::vector<const physicallayer::IIeee80211Mode *> filtered;
    for (auto mode : candidates)
        if (mode != nullptr && isRateCandidate(mode) &&
                getModeMcs(mode) >= minMcs && getModeMcs(mode) <= maxMcs &&
                getModeNss(mode) <= maxNss)
            filtered.push_back(mode);
    if (filtered.empty())
        return nullptr;
    bool probing = false;
    currentMode = selectCandidate(peer, filtered, probing);
    return currentMode;
}

void MinstrelRateControlBase::invalidatePeer(const MacAddress& peer)
{
    peers.erase(peer);
}

} // namespace ieee80211
} // namespace inet
