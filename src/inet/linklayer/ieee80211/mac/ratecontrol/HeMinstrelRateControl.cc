//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/ratecontrol/HeMinstrelRateControl.h"

#include <algorithm>
#include <cmath>

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h"

namespace inet {
namespace ieee80211 {

using namespace inet::physicallayer;

Define_Module(HeMinstrelRateControl);

void HeMinstrelRateControl::initialize(int stage)
{
    MinstrelRateControlBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        enableExtendedRangeSu = par("enableExtendedRangeSu");
        preferDcm = par("preferDcm");
        selectionPolicy = par("selectionPolicy").stdstringValue();
        if (selectionPolicy != "minstrel" && selectionPolicy != "snrThresholds")
            throw cRuntimeError("Unknown HE rate selection policy '%s'", selectionPolicy.c_str());
        registerMinstrelSignals("heRate");
    }
}

bool HeMinstrelRateControl::isRateCandidate(const IIeee80211Mode *mode) const
{
    return dynamic_cast<const Ieee80211HeMode *>(mode) != nullptr;
}

int HeMinstrelRateControl::getModeMcs(const IIeee80211Mode *mode) const
{
    return check_and_cast<const Ieee80211HeMode *>(mode)->getDataMode()->getMcsIndex();
}

int HeMinstrelRateControl::getModeNss(const IIeee80211Mode *mode) const
{
    return check_and_cast<const Ieee80211HeMode *>(mode)->getDataMode()->getNumberOfSpatialStreams();
}

const Ieee80211HeMode *HeMinstrelRateControl::findHeMode(int mcs, int nss, Hz bandwidth,
        bool extendedRangeSu, bool ldpc) const
{
    if (modeSet == nullptr)
        return nullptr;
    auto desiredPreamble = extendedRangeSu ? Ieee80211HePreambleMode::HE_PREAMBLE_ER_SU :
            Ieee80211HePreambleMode::HE_PREAMBLE_SU;
    const Ieee80211HeMode *erSuFallbackSource = nullptr;
    for (int i = 0; i < modeSet->getNumModes(); i++) {
        auto mode = dynamic_cast<const Ieee80211HeMode *>(modeSet->getMode(i));
        if (mode == nullptr)
            continue;
        auto dataMode = mode->getDataMode();
        auto heMcs = dataMode->getModulationAndCodingScheme();
        if (heMcs->getMcsIndex() == (unsigned int)mcs &&
                heMcs->getNumNss() == (unsigned int)nss &&
                (std::isnan(bandwidth.get()) || dataMode->getBandwidth() == bandwidth) &&
                !dataMode->isLdpc()) {
            auto preambleFormat = mode->getPreambleMode()->getPreambleFormat();
            if (preambleFormat == desiredPreamble) {
                if (!ldpc)
                    return mode;
                return Ieee80211HeCompliantModes::getCompliantMode(heMcs,
                        mode->getCenterFrequencyMode(), desiredPreamble,
                        dataMode->getGuardIntervalType(), true);
            }
            if (extendedRangeSu && preambleFormat == Ieee80211HePreambleMode::HE_PREAMBLE_SU)
                erSuFallbackSource = mode;
        }
    }
    if (erSuFallbackSource != nullptr)
        return Ieee80211HeCompliantModes::getCompliantMode(
                erSuFallbackSource->getDataMode()->getModulationAndCodingScheme(),
                erSuFallbackSource->getCenterFrequencyMode(),
                Ieee80211HePreambleMode::HE_PREAMBLE_ER_SU,
                Ieee80211HeModeBase::HE_GUARD_INTERVAL_LONG, ldpc);
    return nullptr;
}

int HeMinstrelRateControl::clampMcsForConstraints(int mcs, int ruToneSize, uint8_t ppduFormat,
        int, const Constraints& constraints) const
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

IIeee80211HeRateControl::Selection HeMinstrelRateControl::selectHeMode(const MacAddress& peer,
        Hz bandwidth, int ruToneSize, uint8_t ppduFormat, int requestedMaxNss,
        const Constraints& constraints)
{
    Enter_Method("selectHeMode");
    if (modeSet == nullptr || requestedMaxNss < 1 || ruToneSize < 0 ||
            constraints.minMcs < 0 || constraints.maxMcs > 11 ||
            constraints.minMcs > constraints.maxMcs)
        return {};
    requestedMaxNss = std::clamp(requestedMaxNss, 1, maxNss);
    if (constraints.directionalCapabilities) {
        const auto& directional = *constraints.directionalCapabilities;
        if (!directional.valid ||
                (!std::isnan(bandwidth.get()) &&
                 directional.supportedChannelWidths.count(bandwidth) == 0) ||
                (ruToneSize > 0 &&
                 directional.supportedRuToneSizes.count(ruToneSize) == 0) ||
                ((ppduFormat == HE_MU_DOWNLINK || ppduFormat == HE_TRIGGER_BASED_UPLINK) &&
                 !directional.ofdma))
            return {};
        requestedMaxNss = std::min(requestedMaxNss, getMaxNss(directional.mcsNss));
        if (requestedMaxNss < 1)
            return {};
    }
    if (constraints.extendedRangeSu || ppduFormat == HE_EXTENDED_RANGE_SU)
        requestedMaxNss = 1;

    std::vector<const IIeee80211Mode *> candidates;
    for (int nss = 1; nss <= requestedMaxNss; nss++) {
        for (int mcs = minMcs; mcs <= maxMcs; mcs++) {
            int constrainedMcs = clampMcsForConstraints(mcs, ruToneSize, ppduFormat,
                    requestedMaxNss, constraints);
            if (constrainedMcs != mcs ||
                    (constraints.directionalCapabilities &&
                     mcs > constraints.directionalCapabilities->mcsNss.maxMcsPerNss[nss - 1]) ||
                    !isHeValidMcsNssCombination(mcs, nss, ruToneSize))
                continue;
            auto mode = findHeMode(mcs, nss, bandwidth, constraints.extendedRangeSu, constraints.ldpc);
            if (mode == nullptr)
                mode = findHeMode(mcs, nss, bandwidth, false, constraints.ldpc);
            if (mode != nullptr)
                candidates.push_back(mode);
        }
    }
    if (candidates.empty())
        return {};

    bool probing = false;
    const IIeee80211Mode *best = nullptr;
    if (selectionPolicy == "snrThresholds") {
        auto& state = getPeerState(peer);
        state.selectionCount++;
        bool hasFreshSnir = seedFromSnir && state.snirGeneration != 0 &&
                simTime() >= state.latestSnirUpdate &&
                simTime() - state.latestSnirUpdate <= updateInterval;
        int selectedMcs = minMcs;
        if (hasFreshSnir)
            selectedMcs = std::clamp((int)std::floor((state.latestSnirDb - snirMcs0ThresholdDb) /
                    snirMcsStepDb), minMcs, maxMcs);
        for (auto mode : candidates)
            if (getModeNss(mode) == requestedMaxNss && getModeMcs(mode) <= selectedMcs &&
                    (best == nullptr || getModeMcs(mode) > getModeMcs(best)))
                best = mode;
        if (best == nullptr)
            best = candidates.front();
        state.rates.try_emplace(best);
        currentMode = best;
        emitSelection(peer, best, false);
    }
    else
        best = selectCandidate(peer, candidates, probing);

    currentMode = best;
    lastSelections[peer] = {best, ruToneSize};
    Selection selection;
    selection.mode = best;
    selection.mcs = getModeMcs(best);
    selection.numberOfSpatialStreams = getModeNss(best);
    selection.dcm = constraints.dcm;
    selection.probing = probing;
    return selection;
}

void HeMinstrelRateControl::reportHeTxResult(const MacAddress& peer, int mcs,
        int numberOfSpatialStreams, int ruToneSize, int retryCount, bool success, int64_t)
{
    Enter_Method("reportHeTxResult");
    const IIeee80211Mode *mode = nullptr;
    auto it = lastSelections.find(peer);
    if (it != lastSelections.end() && it->second.mode != nullptr &&
            it->second.ruToneSize == ruToneSize &&
            getModeMcs(it->second.mode) == mcs &&
            getModeNss(it->second.mode) == numberOfSpatialStreams)
        mode = it->second.mode;
    reportModeTxResult(peer, mode, retryCount, success);
}

void HeMinstrelRateControl::reportHeRxSnir(const MacAddress& peer, double snirDb)
{
    Enter_Method("reportHeRxSnir");
    reportModeRxSnir(peer, snirDb);
}

const IIeee80211Mode *HeMinstrelRateControl::getRate()
{
    Enter_Method("getRate");
    if (enableExtendedRangeSu && modeSet != nullptr) {
        Constraints constraints;
        constraints.extendedRangeSu = true;
        constraints.dcm = preferDcm;
        auto selection = selectHeMode(MacAddress::BROADCAST_ADDRESS, Hz(NaN), 0,
                HE_EXTENDED_RANGE_SU, maxNss, constraints);
        if (selection.mode != nullptr)
            currentMode = selection.mode;
    }
    return currentMode;
}

} // namespace ieee80211
} // namespace inet
