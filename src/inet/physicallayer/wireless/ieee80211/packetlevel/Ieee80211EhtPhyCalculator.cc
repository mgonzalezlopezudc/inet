//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211EhtPhyCalculator.h"

namespace inet {
namespace physicallayer {

int getEhtMcsBitsPerSubcarrier(int mcs)
{
    static const int values[] = {1, 2, 2, 4, 4, 6, 6, 6, 8, 8, 10, 10, 12, 12};
    if (mcs < 0 || mcs > 13)
        throw cRuntimeError("Invalid EHT MCS: %d", mcs);
    return values[mcs];
}

std::pair<int, int> getEhtMcsCodeRate(int mcs)
{
    static const std::pair<int, int> values[] = {
        {1, 2}, {1, 2}, {3, 4}, {1, 2}, {3, 4}, {2, 3},
        {3, 4}, {5, 6}, {3, 4}, {5, 6}, {3, 4}, {5, 6},
        {3, 4}, {5, 6}
    };
    if (mcs < 0 || mcs > 13)
        throw cRuntimeError("Invalid EHT MCS: %d", mcs);
    return values[mcs];
}

bool isEhtValidMcsNssCombination(int mcs, int nss, int ruToneSize)
{
    // EHT inherits the HE constraints and extends them.
    if (mcs == 6 && (nss == 3 || nss == 6 || nss == 9 || nss == 12))
        return false;
    if (mcs == 9 && (nss == 3 || nss == 6 || nss == 9 || nss == 12) && ruToneSize > 0 && ruToneSize <= 242)
        return false;
    // MCS 10, 11, 12, 13 (1024-QAM and 4096-QAM) require at least 106 data subcarriers.
    // They are not allowed on 26-tone and 52-tone RUs.
    if (mcs >= 10 && ruToneSize > 0 && ruToneSize < 106)
        return false;
    return true;
}

int getEhtNumberOfLtfSymbols(int spaceTimeStreams)
{
    if (spaceTimeStreams <= 1) return 1;
    if (spaceTimeStreams == 2) return 2;
    if (spaceTimeStreams <= 4) return 4;
    if (spaceTimeStreams <= 6) return 6;
    if (spaceTimeStreams <= 8) return 8;
    return 16;
}

Ieee80211EhtPhyValidationResult computeEhtPpduParameters(
        const std::vector<Ieee80211EhtUserPhyParameters>& requestedUsers,
        Hz channelBandwidth,
        Ieee80211EhtPpduFormat ppduFormat,
        Ieee80211EhtGuardInterval guardInterval,
        Ieee80211EhtLtfType ltfType,
        int packetExtensionDurationUs,
        bool enforceDurationLimit)
{
    Ieee80211EhtPhyValidationResult result;
    if (requestedUsers.empty()) {
        result.error = "EHT PPDU has no users";
        return result;
    }

    result.parameters.common.ppduFormat = ppduFormat;
    result.parameters.common.channelBandwidth = channelBandwidth;
    result.parameters.common.guardInterval = guardInterval;
    result.parameters.common.ltfType = ltfType;
    result.parameters.common.packetExtensionDurationUs = packetExtensionDurationUs;
    result.parameters.common.ehtSigDuration = SimTime(8, SIMTIME_US); // Base approximation for EHT-SIG

    int maxSpaceTimeStreams = 0;
    for (const auto& u : requestedUsers)
        maxSpaceTimeStreams = std::max(maxSpaceTimeStreams, u.numberOfSpatialStreams);

    result.parameters.common.numberOfEhtLtfSymbols = getEhtNumberOfLtfSymbols(maxSpaceTimeStreams);
    result.parameters.common.ehtLtfDuration = getHeLtfSymbolDuration(ltfType) * result.parameters.common.numberOfEhtLtfSymbols;
    result.parameters.common.commonPreambleDuration = result.parameters.common.legacyPreambleDuration
            + result.parameters.common.rlSigDuration
            + result.parameters.common.uSigDuration
            + result.parameters.common.ehtSigDuration
            + result.parameters.common.ehtStfDuration
            + result.parameters.common.ehtLtfDuration;

    simtime_t maxUserDuration = SIMTIME_ZERO;
    int maxDataSymbols = 0;

    for (auto user : requestedUsers) {
        if (!isEhtValidMcsNssCombination(user.mcs, user.numberOfSpatialStreams, user.mru.toneSize)) {
            result.error = "Invalid EHT MCS/NSS combination for user";
            return result;
        }

        user.dataBitsPerSymbol = getEhtMruDataSubcarrierCount(user.mru.toneSize)
                * getEhtMcsBitsPerSubcarrier(user.mcs)
                * user.numberOfSpatialStreams;

        auto codeRate = getEhtMcsCodeRate(user.mcs);
        
        // Compute symbols required for user PSDU length
        long long bits = user.psduLength.get<b>() + user.serviceBits + user.tailBits;
        long long dataBitsPerSymbol = user.dataBitsPerSymbol * codeRate.first / codeRate.second;
        
        user.numberOfDataSymbols = (bits + dataBitsPerSymbol - 1) / dataBitsPerSymbol;
        user.numberOfSymbols = user.numberOfDataSymbols;
        
        user.dataDuration = getHeGuardIntervalDuration(guardInterval) * user.numberOfDataSymbols 
                          + SimTime(12800, SIMTIME_NS) * user.numberOfDataSymbols; // 12.8us + GI
                          
        user.preambleDuration = result.parameters.common.commonPreambleDuration;
        user.duration = user.preambleDuration + user.dataDuration;

        maxUserDuration = std::max(maxUserDuration, user.duration);
        maxDataSymbols = std::max(maxDataSymbols, user.numberOfDataSymbols);
        
        result.parameters.users.push_back(user);
    }

    result.parameters.commonNumberOfDataSymbols = maxDataSymbols;
    result.parameters.duration = maxUserDuration;
    result.valid = true;
    return result;
}

} // namespace physicallayer
} // namespace inet
