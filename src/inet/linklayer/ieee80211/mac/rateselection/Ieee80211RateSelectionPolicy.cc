//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/rateselection/Ieee80211RateSelectionPolicy.h"

#include <cmath>

#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtMode.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.h"

namespace inet {
namespace ieee80211 {

using namespace physicallayer;

bool Ieee80211RateSelectionPolicy::containsRate(const std::vector<Ieee80211LegacyRate> *rates, int rate)
{
    if (rates == nullptr)
        return false;
    for (const auto& value : *rates)
        if (value.rate == rate)
            return true;
    return false;
}

bool Ieee80211RateSelectionPolicy::isLegacyMode(const Context& context, const IIeee80211Mode *mode)
{
    auto family = context.modeSet->getPhyFamily(mode);
    return family == Ieee80211PhyFamily::DSSS || family == Ieee80211PhyFamily::ERP_OFDM ||
            family == Ieee80211PhyFamily::OFDM;
}

bool Ieee80211RateSelectionPolicy::hasCompatibleResponseModulationClass(const Context& context,
        const IIeee80211Mode *precedingMode, const IIeee80211Mode *responseMode)
{
    // IEEE Std 802.11-2024, 10.6.6.5.2: legacy responses retain the received modulation
    // class; HT/VHT responses use the appropriate OFDM-family non-HT reference class.
    auto responseFamily = context.modeSet->getPhyFamily(responseMode);
    if (dynamic_cast<const Ieee80211HtMode *>(precedingMode) != nullptr ||
            dynamic_cast<const Ieee80211VhtMode *>(precedingMode) != nullptr)
        return responseFamily == Ieee80211PhyFamily::ERP_OFDM || responseFamily == Ieee80211PhyFamily::OFDM;
    auto precedingFamily = context.modeSet->getPhyFamily(precedingMode);
    if (precedingFamily == Ieee80211PhyFamily::DSSS ||
            precedingFamily == Ieee80211PhyFamily::ERP_OFDM || precedingFamily == Ieee80211PhyFamily::OFDM)
        return responseFamily == precedingFamily;
    throw cRuntimeError("Pre-HT/HT/VHT response selection received an unsupported preceding modulation class");
}

const IIeee80211Mode *Ieee80211RateSelectionPolicy::findFastestLegacy(const Context& context,
        const std::vector<Ieee80211LegacyRate> *requiredRates, bps maximumRate,
        const IIeee80211Mode *responseClassReferenceMode)
{
    const IIeee80211Mode *best = nullptr;
    for (int i = 0; i < context.modeSet->getNumModes(); ++i) {
        auto mode = context.modeSet->getMode(i);
        if (!isLegacyMode(context, mode))
            continue;
        if (responseClassReferenceMode != nullptr &&
                !hasCompatibleResponseModulationClass(context, responseClassReferenceMode, mode))
            continue;
        auto bitrate = mode->getDataMode()->getNetBitrate();
        int code = (int)std::ceil(bitrate.get<Mbps>() * 2);
        if (bitrate <= maximumRate && containsRate(context.localOperationalRates, code) &&
                (requiredRates == nullptr || containsRate(requiredRates, code)) &&
                (best == nullptr || best->getDataMode()->getNetBitrate() < bitrate))
            best = mode;
    }
    return best;
}

const IIeee80211Mode *Ieee80211RateSelectionPolicy::findFastestMandatory(
        const Context& context, bps maximumRate, const IIeee80211Mode *responseClassReferenceMode)
{
    const IIeee80211Mode *best = nullptr;
    for (int i = 0; i < context.modeSet->getNumModes(); ++i) {
        auto mode = context.modeSet->getMode(i);
        auto bitrate = mode->getDataMode()->getNetBitrate();
        if (isLegacyMode(context, mode) && context.modeSet->isMandatory(i) && bitrate <= maximumRate &&
                (responseClassReferenceMode == nullptr ||
                        hasCompatibleResponseModulationClass(context, responseClassReferenceMode, mode)) &&
                (best == nullptr || best->getDataMode()->getNetBitrate() < bitrate))
            best = mode;
    }
    return best;
}

const IIeee80211Mode *Ieee80211RateSelectionPolicy::findFastestBasicAdvanced(const Context& context)
{
    const IIeee80211Mode *bestHt = nullptr;
    const IIeee80211Mode *bestVht = nullptr;
    for (int i = 0; i < context.modeSet->getNumModes(); ++i) {
        auto mode = context.modeSet->getMode(i);
        auto dataMode = mode->getDataMode();
        int nss = dataMode->getNumberOfSpatialStreams();
        if (auto ht = dynamic_cast<const Ieee80211HtMode *>(mode)) {
            int mcs = ht->getDataMode()->getMcsIndex() % 8;
            if (context.htOperation != nullptr && nss >= 1 && nss <= 4 &&
                    context.htOperation->basicMcsNss.maxMcsPerNss[nss - 1] >= mcs &&
                    dataMode->getBandwidth() <= context.htOperation->operatingChannelWidth &&
                    (bestHt == nullptr || bestHt->getDataMode()->getNetBitrate() < dataMode->getNetBitrate()))
                bestHt = mode;
        }
        else if (auto vht = dynamic_cast<const Ieee80211VhtMode *>(mode)) {
            int mcs = vht->getDataMode()->getMcsIndex();
            if (context.vhtOperation != nullptr && nss >= 1 && nss <= 8 &&
                    context.vhtOperation->basicMcsNss.maxMcsPerNss[nss - 1] >= mcs &&
                    dataMode->getBandwidth() <= context.vhtOperation->operatingChannelWidth &&
                    (bestVht == nullptr || bestVht->getDataMode()->getNetBitrate() < dataMode->getNetBitrate()))
                bestVht = mode;
        }
    }
    return bestHt != nullptr ? bestHt : bestVht;
}

const IIeee80211Mode *Ieee80211RateSelectionPolicy::selectUnicast(const Context& context,
        const IIeee80211Mode *candidate, CandidateOrigin origin)
{
    if (!isLegacyMode(context, candidate))
        return candidate;
    int code = (int)std::ceil(candidate->getDataMode()->getNetBitrate().get<Mbps>() * 2);
    bool legal = containsRate(context.localOperationalRates, code) &&
            (context.peerRates == nullptr || containsRate(context.peerRates, code));
    if (legal)
        return candidate;
    if (origin == EXPLICIT_CONFIGURATION)
        throw cRuntimeError("Explicitly configured legacy mode is outside the local operational or peer advertised rate set");
    auto fallback = findFastestLegacy(context, context.peerRates, bps(INFINITY));
    if (fallback == nullptr)
        throw cRuntimeError("No mutually supported legacy unicast rate is available");
    return fallback;
}

const IIeee80211Mode *Ieee80211RateSelectionPolicy::selectGroupOrControl(const Context& context,
        const IIeee80211Mode *candidate, CandidateOrigin origin)
{
    // HE/EHT-specific group-rate rules are outside this policy's pre-HT/HT/VHT scope.
    if (candidate != nullptr && !isLegacyMode(context, candidate) &&
            dynamic_cast<const Ieee80211HtMode *>(candidate) == nullptr &&
            dynamic_cast<const Ieee80211VhtMode *>(candidate) == nullptr)
        return candidate;
    if (candidate != nullptr && isLegacyMode(context, candidate)) {
        int code = (int)std::ceil(candidate->getDataMode()->getNetBitrate().get<Mbps>() * 2);
        if (containsRate(context.localOperationalRates, code) && containsRate(context.bssBasicRates, code))
            return candidate;
        if (origin == EXPLICIT_CONFIGURATION)
            throw cRuntimeError("Explicitly configured group/control mode is outside the current BSS Basic Rate Set");
    }
    auto selected = findFastestLegacy(context, context.bssBasicRates, bps(INFINITY));
    if (selected == nullptr)
        selected = findFastestBasicAdvanced(context);
    if (selected == nullptr)
        selected = findFastestMandatory(context, bps(INFINITY));
    if (selected == nullptr) {
        // Preserve the existing pure HE/EHT default when this pre-HT/HT/VHT policy has no applicable mode.
        for (int i = 0; i < context.modeSet->getNumModes(); ++i) {
            auto mode = context.modeSet->getMode(i);
            if (context.modeSet->isMandatory(i) &&
                    dynamic_cast<const Ieee80211HtMode *>(mode) == nullptr &&
                    dynamic_cast<const Ieee80211VhtMode *>(mode) == nullptr &&
                    (selected == nullptr || selected->getDataMode()->getNetBitrate() < mode->getDataMode()->getNetBitrate()))
                selected = mode;
        }
    }
    if (selected == nullptr)
        throw cRuntimeError("No BSS Basic or mandatory legacy mode is available");
    return selected;
}

const IIeee80211Mode *Ieee80211RateSelectionPolicy::selectResponse(const Context& context,
        const IIeee80211Mode *precedingMode, const IIeee80211Mode *configuredMode)
{
    auto maximumRate = context.modeSet->getNonHtReferenceRate(precedingMode);
    if (configuredMode != nullptr) {
        if (!isLegacyMode(context, configuredMode))
            throw cRuntimeError("Explicitly configured ACK/CTS response mode must be non-HT");
        int code = (int)std::ceil(configuredMode->getDataMode()->getNetBitrate().get<Mbps>() * 2);
        if (!containsRate(context.localOperationalRates, code) ||
                !containsRate(context.bssBasicRates, code) ||
                configuredMode->getDataMode()->getNetBitrate() > maximumRate ||
                !hasCompatibleResponseModulationClass(context, precedingMode, configuredMode))
            throw cRuntimeError("Explicitly configured ACK/CTS response mode violates the BSS Basic, modulation-class, or non-HT reference-rate constraint");
        return configuredMode;
    }
    auto selected = findFastestLegacy(context, context.bssBasicRates, maximumRate, precedingMode);
    if (selected == nullptr)
        selected = findFastestMandatory(context, maximumRate, precedingMode);
    if (selected == nullptr)
        throw cRuntimeError("No BSS Basic or mandatory response rate satisfies the non-HT reference-rate limit");
    return selected;
}

const IIeee80211Mode *Ieee80211RateSelectionPolicy::selectBlockAck(const Context& context)
{
    if (context.peerRates != nullptr) {
        auto selected = findFastestLegacy(context, context.peerRates, bps(INFINITY));
        if (selected != nullptr)
            return selected;
    }
    return selectGroupOrControl(context);
}

} // namespace ieee80211
} // namespace inet
