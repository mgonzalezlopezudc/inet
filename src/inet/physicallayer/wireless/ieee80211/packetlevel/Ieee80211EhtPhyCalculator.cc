//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211EhtPhyCalculator.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211EhtPreamblePuncturing.h"

namespace inet {
namespace physicallayer {

int getEhtMcsBitsPerSubcarrier(int mcs)
{
    static const int values[] = {1, 2, 2, 4, 4, 6, 6, 6, 8, 8, 10, 10, 12, 12, 1, 1};
    if (mcs < 0 || mcs > 15)
        throw cRuntimeError("Invalid EHT MCS: %d", mcs);
    return values[mcs];
}

std::pair<int, int> getEhtMcsCodeRate(int mcs)
{
    static const std::pair<int, int> values[] = {
        {1, 2}, {1, 2}, {3, 4}, {1, 2}, {3, 4}, {2, 3},
        {3, 4}, {5, 6}, {3, 4}, {5, 6}, {3, 4}, {5, 6},
        {3, 4}, {5, 6}, {1, 2}, {1, 2}
    };
    if (mcs < 0 || mcs > 15)
        throw cRuntimeError("Invalid EHT MCS: %d", mcs);
    return values[mcs];
}

bool isEhtValidMcsNssCombination(int mcs, int nss, int ruToneSize)
{
    if (mcs < 0 || mcs > 15 || nss < 1 || nss > 8)
        return false;
    // IEEE Std 802.11be-2024, 36.5: EHT-MCS 14/15 require NSS=1;
    // EHT-MCS 14 is EHT DUP for SU 80/160/320 MHz full-bandwidth operation.
    if ((mcs == 14 || mcs == 15) && nss != 1)
        return false;
    if (mcs == 14 && ruToneSize != 996 && ruToneSize != 1992 && ruToneSize != 3984)
        return false;
    return true;
}

int getEhtMcsDataSubcarrierCount(int mcs, int ruToneSize)
{
    int dataSubcarriers = getEhtMruDataSubcarrierCount(ruToneSize);
    if (mcs == 15)
        return dataSubcarriers / 2;
    else if (mcs == 14) {
        switch (ruToneSize) {
            case 996: return 234;
            case 1992: return 490;
            case 3984: return 980;
            default: throw cRuntimeError("EHT-MCS 14 requires a full-bandwidth 80, 160, or 320 MHz RU");
        }
    }
    else
        return dataSubcarriers;
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
        bool enforceDurationLimit,
        bool singleUser,
        Ieee80211EhtOperatingBand operatingBand,
        uint16_t puncturedSubchannelMask,
        bool ehtDupMcs14Supported,
        bool mcs15Disabled)
{
    Ieee80211EhtPhyValidationResult result;
    if (requestedUsers.empty()) {
        result.error = "EHT PPDU has no users";
        return result;
    }

    // EHT-TB carries no puncturing indication in U-SIG (36.3.12.11.4).
    if (ppduFormat == EHT_TRIGGER_BASED_UPLINK && puncturedSubchannelMask != 0) {
        result.error = "EHT-TB PPDU cannot carry an EHT puncturing indication";
        return result;
    }
    auto puncturingMode = singleUser ? Ieee80211EhtPreamblePuncturingMode::NON_OFDMA :
            Ieee80211EhtPreamblePuncturingMode::OFDMA;
    if (!isValidIeee80211EhtPreamblePuncturing(puncturedSubchannelMask,
            channelBandwidth, -1, puncturingMode)) {
        result.error = "Illegal EHT preamble puncturing pattern for PPDU bandwidth or format";
        return result;
    }

    result.parameters.common.ppduFormat = ppduFormat;
    result.parameters.common.singleUser = singleUser;
    result.parameters.common.operatingBand = operatingBand;
    result.parameters.common.puncturedSubchannelMask = puncturedSubchannelMask;
    result.parameters.common.channelBandwidth = channelBandwidth;
    result.parameters.common.guardInterval = guardInterval;
    result.parameters.common.ltfType = ltfType;
    result.parameters.common.packetExtensionDurationUs = packetExtensionDurationUs;
    result.parameters.common.ehtSigDuration = SimTime(8, SIMTIME_US); // Base approximation for EHT-SIG

    int maxSpaceTimeStreams = 0;
    for (const auto& u : requestedUsers)
        maxSpaceTimeStreams = std::max(maxSpaceTimeStreams, u.numberOfSpatialStreams);

    result.parameters.common.numberOfEhtLtfSymbols = getEhtNumberOfLtfSymbols(maxSpaceTimeStreams);
    result.parameters.common.ehtLtfDuration = getHeLtfSymbolDuration(ltfType, guardInterval) * result.parameters.common.numberOfEhtLtfSymbols;
    result.parameters.common.commonPreambleDuration = result.parameters.common.legacyPreambleDuration
            + result.parameters.common.rlSigDuration
            + result.parameters.common.uSigDuration
            + result.parameters.common.ehtSigDuration
            + result.parameters.common.ehtStfDuration
            + result.parameters.common.ehtLtfDuration;

    simtime_t maxUserDuration = SIMTIME_ZERO;
    int maxDataSymbols = 0;

    for (auto user : requestedUsers) {
        // IEEE Std 802.11be-2024, 35.14.2 and 36.5. The caller supplies the
        // effective negotiated/operational gates; this PHY helper does not own
        // association state. The current EHT SU model covers full bandwidth only.
        if (user.mcs == 14 && (!ehtDupMcs14Supported || ppduFormat != EHT_MU || !singleUser ||
                operatingBand != EHT_BAND_6_GHZ || puncturedSubchannelMask != 0 ||
                requestedUsers.size() != 1 || user.coding != HE_CODING_LDPC ||
                user.mru.toneSize != getEhtChannelToneCount(channelBandwidth))) {
            result.error = "EHT-MCS 14 requires negotiated EHT DUP support, EHT MU with U-SIG SU indication, 6 GHz, LDPC, and unpunctured full bandwidth";
            return result;
        }
        if (user.mcs == 15 && mcs15Disabled) {
            result.error = "EHT-MCS 15 is disabled by the effective EHT operation state";
            return result;
        }
        if (!isEhtValidMcsNssCombination(user.mcs, user.numberOfSpatialStreams, user.mru.toneSize)) {
            result.error = "Invalid EHT MCS/NSS combination for user";
            return result;
        }

        user.dataBitsPerSymbol = getEhtMcsDataSubcarrierCount(user.mcs, user.mru.toneSize)
                * getEhtMcsBitsPerSubcarrier(user.mcs)
                * user.numberOfSpatialStreams;

        auto codeRate = getEhtMcsCodeRate(user.mcs);
        
        // Compute symbols required for user PSDU length
        if (user.coding == HE_CODING_LDPC)
            user.tailBits = 0;
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
