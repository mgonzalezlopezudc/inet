//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211EHTPHYCALCULATOR_H
#define __INET_IEEE80211EHTPHYCALCULATOR_H

#include <algorithm>
#include <cmath>
#include <ostream>
#include <string>
#include <vector>

#include "inet/common/Units.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211EhtMru.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h"

namespace inet {
namespace physicallayer {

using namespace inet::units::values;

/** EHT PPDU formats */
enum Ieee80211EhtPpduFormat {
    EHT_MU = 0,                     // EHT MU PPDU format (DL OFDMA/MU-MIMO or SU)
    EHT_TRIGGER_BASED_UPLINK = 1,   // EHT TB PPDU format (UL OFDMA/MU-MIMO triggered by AP)
};

/** EHT Guard Intervals (same durations as HE) */
using Ieee80211EhtGuardInterval = Ieee80211HeGuardInterval;
using Ieee80211EhtCoding = Ieee80211HeCoding;
using Ieee80211EhtLtfType = Ieee80211HeLtfType;

/** EHT preamble/common parameters */
struct Ieee80211EhtCommonPhyParameters
{
    Ieee80211EhtPpduFormat ppduFormat = EHT_MU;
    Hz channelBandwidth = Hz(NaN);
    Ieee80211EhtGuardInterval guardInterval = HE_GI_3_2_US;
    Ieee80211EhtLtfType ltfType = HE_LTF_4X;
    int numberOfEhtLtfSymbols = 1;
    bool ldpcExtraSymbol = false;
    int packetExtensionDurationUs = 0;
    
    simtime_t legacyPreambleDuration = SimTime(20, SIMTIME_US); // L-STF + L-LTF + L-SIG
    simtime_t rlSigDuration = SimTime(4, SIMTIME_US);
    simtime_t uSigDuration = SimTime(8, SIMTIME_US);
    simtime_t ehtSigDuration = SIMTIME_ZERO;
    simtime_t ehtStfDuration = SimTime(4, SIMTIME_US);
    simtime_t ehtLtfDuration = SIMTIME_ZERO;
    simtime_t commonPreambleDuration = SIMTIME_ZERO;
};

/** EHT RU-specific parameters */
struct Ieee80211EhtUserPhyParameters
{
    Ieee80211EhtMru mru;
    int mcs = 0;
    int numberOfSpatialStreams = 1;
    Ieee80211EhtGuardInterval guardInterval = HE_GI_3_2_US;
    Ieee80211EhtCoding coding = HE_CODING_BCC;
    B psduLength = B(0);
    int streamStartIndex = 0;
    uint16_t staId = 0;
    int numberOfEncoders = 1;
    int codedBitsPerSymbol = 0;
    int dataBitsPerSymbol = 0;
    int serviceBits = 16;
    int tailBits = 6;
    
    int ldpcCodewordLength = 0;
    int ldpcCodewordCount = 0;
    int ldpcShorteningBits = 0;
    int ldpcRepetitionBits = 0;
    int preFecPaddingFactor = 4;
    int postFecPaddingBits = 0;
    int numberOfDataSymbols = 0;
    int numberOfSymbols = 0;
    
    simtime_t preambleDuration = SIMTIME_ZERO;
    simtime_t headerDuration = SIMTIME_ZERO;
    simtime_t dataDuration = SIMTIME_ZERO;
    simtime_t duration = SIMTIME_ZERO;
};

struct Ieee80211EhtPpduParameters
{
    Ieee80211EhtCommonPhyParameters common;
    std::vector<Ieee80211EhtUserPhyParameters> users;
    int commonNumberOfDataSymbols = 0;
    simtime_t duration = SIMTIME_ZERO;
};

struct Ieee80211EhtPhyValidationResult
{
    bool valid = false;
    std::string error;
    Ieee80211EhtPpduParameters parameters;

    explicit operator bool() const { return valid; }
};

int getEhtMcsBitsPerSubcarrier(int mcs);
std::pair<int, int> getEhtMcsCodeRate(int mcs);
bool isEhtValidMcsNssCombination(int mcs, int nss, int ruToneSize = 0);
int getEhtNumberOfLtfSymbols(int spaceTimeStreams);

Ieee80211EhtPhyValidationResult computeEhtPpduParameters(
        const std::vector<Ieee80211EhtUserPhyParameters>& requestedUsers,
        Hz channelBandwidth,
        Ieee80211EhtPpduFormat ppduFormat,
        Ieee80211EhtGuardInterval guardInterval,
        Ieee80211EhtLtfType ltfType,
        int packetExtensionDurationUs,
        bool enforceDurationLimit);

} // namespace physicallayer
} // namespace inet

#endif
