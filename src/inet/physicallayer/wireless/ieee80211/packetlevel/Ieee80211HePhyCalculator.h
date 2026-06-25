//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HEPHYCALCULATOR_H
#define __INET_IEEE80211HEPHYCALCULATOR_H

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "inet/common/Units.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"

namespace inet {
namespace physicallayer {

using namespace inet::units::values;

/**
 * HE PPDU formats modelled by the common MU PHY calculator.
 * IEEE 802.11-2024 Clause 27.3.11 ("PPDU formats").
 */
enum Ieee80211HePpduFormat {
    HE_MU_DOWNLINK = 0,             // HE MU PPDU format (Clause 27.3.11.13) for DL OFDMA/MU-MIMO
    HE_TRIGGER_BASED_UPLINK = 1     // HE TB PPDU format (Clause 27.3.11.14) for UL OFDMA/MU-MIMO triggered by AP
};

/**
 * Guard-interval choices expressed by HE packet-level parameters.
 * IEEE 802.11-2024 Table 27-61 ("HE PHY characteristics").
 * - Short: 0.8 µs (1/16 of DFT period)
 * - Medium: 1.6 µs (1/8 of DFT period)
 * - Long: 3.2 µs (1/4 of DFT period)
 */
enum Ieee80211HeGuardInterval {
    HE_GI_0_8_US = 0,
    HE_GI_1_6_US = 1,
    HE_GI_3_2_US = 2
};

/**
 * Forward-error-correction coding used by HE user payloads.
 * IEEE 802.11-2024 Clause 27.3.11.8 ("LDPC coding").
 */
enum Ieee80211HeCoding {
    HE_CODING_BCC = 0,              // Binary Convolutional Coding
    HE_CODING_LDPC = 1              // Low-Density Parity-Check Coding
};

/**
 * HE long-training-field (HE-LTF) duration multiplier.
 * IEEE 802.11-2024 Clause 27.3.4.7 ("HE-LTF field").
 */
enum Ieee80211HeLtfType {
    HE_LTF_1X = 1,                  // 3.2 µs DFT period
    HE_LTF_2X = 2,                  // 6.4 µs DFT period
    HE_LTF_4X = 4                   // 12.8 µs DFT period
};

/**
 * HE-SIG-A fields shared by every user in the PPDU.
 * IEEE 802.11-2024 Table 27-21 ("HE-SIG-A field of an HE MU PPDU").
 * Carries common physical configuration such as BSS Color (6-bit field used for spatial reuse/OBSS PD detection).
 */
struct Ieee80211HeSigAFields
{
    Ieee80211HePpduFormat ppduFormat = HE_MU_DOWNLINK;
    uint8_t bssColor = 0;           // 6-bit BSS Color identifier (1-63, 0 means disabled)
    bool uplink = false;
    int txopDurationUs = 0;         // Remaining duration of the TXOP (NAV protection)
    bool doppler = false;
    bool stbc = false;              // Space-Time Block Coding indicator
};

/**
 * HE-SIG-B parameters for a downlink MU PPDU.
 * IEEE 802.11-2024 Clause 27.3.11.13.2 ("HE-SIG-B field").
 * The HE-SIG-B field contains a Common field (RU allocation mapping) and a User Block field (user specific MCS/NSS).
 */
struct Ieee80211HeSigBFields
{
    bool compression = false;       // Full channel MU-MIMO compression flag (bypasses RU allocation subfield)
    int mcs = 0;
    int numberOfSymbols = 0;
    int commonFieldBits = 0;
    int userFieldBits = 0;
};

/** Preamble and signaling parameters common to all users in an HE MU PPDU. */
struct Ieee80211HeCommonPhyParameters
{
    Ieee80211HePpduFormat ppduFormat = HE_MU_DOWNLINK;
    Hz channelBandwidth = Hz(NaN);
    Ieee80211HeGuardInterval guardInterval = HE_GI_3_2_US;
    Ieee80211HeLtfType ltfType = HE_LTF_4X;
    int numberOfHeLtfSymbols = 1;
    bool ldpcExtraSymbol = false;
    int packetExtensionDurationUs = 0;
    Ieee80211HeSigAFields sigA;
    Ieee80211HeSigBFields sigB;
    simtime_t legacyPreambleDuration = SimTime(20, SIMTIME_US); // L-STF + L-LTF + L-SIG = 8 + 8 + 4 = 20 µs
    simtime_t rlSigDuration = SimTime(4, SIMTIME_US);           // Repeated L-SIG (4 µs)
    simtime_t heSigADuration = SimTime(8, SIMTIME_US);          // HE-SIG-A (8 µs)
    simtime_t heSigBDuration = SIMTIME_ZERO;                    // Variable size (DL MU PPDU only)
    simtime_t heStfDuration = SimTime(4, SIMTIME_US);           // HE-STF (4 µs)
    simtime_t heLtfDuration = SIMTIME_ZERO;                    // HE-LTF (depends on spatial stream count)
    simtime_t commonPreambleDuration = SIMTIME_ZERO;
};

/** RU-specific coding, payload, and duration parameters for one HE user. */
struct Ieee80211HeUserPhyParameters
{
    Ieee80211HeRu ru;
    int mcs = 0;
    int numberOfSpatialStreams = 1;
    bool dcm = false;                                      // Dual Carrier Modulation
    Ieee80211HeGuardInterval guardInterval = HE_GI_3_2_US; // compatibility
    Ieee80211HeCoding coding = HE_CODING_BCC;
    B psduLength = B(0);
    int streamStartIndex = 0;
    uint16_t staId = 0;
    int numberOfEncoders = 1;
    int codedBitsPerSymbol = 0;
    int dataBitsPerSymbol = 0;
    int serviceBits = 16;
    int tailBits = 6;
    // Packet-level representation of the HE LDPC encoder. These fields are
    // deliberately retained in the common calculator result so scheduling,
    // transmission and reception cannot silently use different assumptions.
    int ldpcCodewordLength = 0;
    int ldpcCodewordCount = 0;
    int ldpcShorteningBits = 0;
    int ldpcRepetitionBits = 0;
    int preFecPaddingFactor = 4;
    int postFecPaddingBits = 0;
    int numberOfDataSymbols = 0;
    int numberOfSymbols = 0; // compatibility
    simtime_t preambleDuration = SIMTIME_ZERO; // compatibility
    simtime_t headerDuration = SIMTIME_ZERO; // compatibility
    simtime_t dataDuration = SIMTIME_ZERO;
    simtime_t duration = SIMTIME_ZERO; // compatibility
};

/** Fully calculated common and per-user parameters of one HE MU PPDU. */
struct Ieee80211HePpduParameters
{
    Ieee80211HeCommonPhyParameters common;
    std::vector<Ieee80211HeUserPhyParameters> users;
    int commonNumberOfDataSymbols = 0;
    simtime_t duration = SIMTIME_ZERO;
};

/** Result returned by the non-throwing HE PPDU validation/calculation API. */
struct Ieee80211HePhyValidationResult
{
    bool valid = false;
    std::string error;
    Ieee80211HePpduParameters parameters;

    explicit operator bool() const { return valid; }
};

inline int getHeMcsBitsPerSubcarrier(int mcs)
{
    static const int values[] = {1, 2, 2, 4, 4, 6, 6, 6, 8, 8, 10, 10};
    if (mcs < 0 || mcs > 11)
        throw cRuntimeError("Invalid HE MCS: %d", mcs);
    return values[mcs];
}

inline std::pair<int, int> getHeMcsCodeRate(int mcs)
{
    static const std::pair<int, int> values[] = {
        {1, 2}, {1, 2}, {3, 4}, {1, 2}, {3, 4}, {2, 3},
        {3, 4}, {5, 6}, {3, 4}, {5, 6}, {3, 4}, {5, 6}
    };
    if (mcs < 0 || mcs > 11)
        throw cRuntimeError("Invalid HE MCS: %d", mcs);
    return values[mcs];
}

inline simtime_t getHeGuardIntervalDuration(Ieee80211HeGuardInterval guardInterval)
{
    switch (guardInterval) {
        case HE_GI_0_8_US: return SimTime(800, SIMTIME_NS);
        case HE_GI_1_6_US: return SimTime(1600, SIMTIME_NS);
        case HE_GI_3_2_US: return SimTime(3200, SIMTIME_NS);
        default: throw cRuntimeError("Invalid HE guard interval: %d", (int)guardInterval);
    }
}

inline bool isHeDcmCombinationSupported(int mcs, int numberOfSpatialStreams)
{
    return (mcs == 0 || mcs == 1 || mcs == 3 || mcs == 4) &&
           numberOfSpatialStreams >= 1 && numberOfSpatialStreams <= 2;
}

/**
 * Returns false for <MCS, Nss> combinations that are marked N/A in the
 * IEEE 802.11-2024 HE rate tables (Tables 27-62 through 27-117).
 *
 * Three classes of N/A entries exist across all RU sizes:
 *
 * 1. MCS 6, Nss in {3, 6}: The resulting coded bits per symbol is not an
 *    integer multiple of the code-rate denominator for any BW.  These are
 *    marked N/A throughout Tables 27-62..27-117.
 *    (IEEE 802.11-2024 Clause 27.5, Tables 27-62..27-117)
 *
 * 2. MCS 9, Nss in {3, 6} at 20 MHz (242-tone full-BW RU only): the data
 *    rate formula yields a non-integer number of data bits per symbol.
 *    For wider channels the same Nss values are valid.
 *    (IEEE 802.11-2024 Clause 27.5, Tables 27-74..27-81 onward)
 *
 * 3. MCS 10 and MCS 11 (1024-QAM) require a minimum of 106 data subcarriers
 *    to achieve the smallest standardized data rate. 26-tone (12 data
 *    subcarriers) and 52-tone (24 data subcarriers) RUs produce a zero or
 *    non-standard number of data bits per symbol and are not listed in
 *    Tables 27-62..27-69 (26-tone) or Tables 27-70..27-77 (52-tone).
 *    (IEEE 802.11-2024 Clause 27.5)
 *
 * The ruToneSize argument is the number of tones of the RU to be used;
 * pass 0 or a negative value to skip the tone-size check (e.g., when
 * validating a (MCS, Nss) pair independent of RU geometry).
 */
inline bool isHeValidMcsNssCombination(int mcs, int nss, int ruToneSize = 0)
{
    // MCS 6, Nss=3 or Nss=6: N/A for all RU sizes and bandwidths.
    if (mcs == 6 && (nss == 3 || nss == 6))
        return false;
    // MCS 9, Nss=3 or Nss=6: N/A for 20 MHz (242-tone full-BW) and smaller RUs.
    if (mcs == 9 && (nss == 3 || nss == 6) && ruToneSize > 0 && ruToneSize <= 242)
        return false;
    // MCS 10/11: require at least 106 data subcarriers; 26-tone and 52-tone RUs
    // are not listed in the standard's rate tables.
    if ((mcs == 10 || mcs == 11) && ruToneSize > 0 && ruToneSize < 106)
        return false;
    return true;
}

/**
 * Returns the HE-LTF symbol count based on space-time streams.
 * IEEE 802.11-2024 Table 27-14 ("Number of HE-LTF symbols").
 */
inline int getHeNumberOfLtfSymbols(int spaceTimeStreams)
{
    if (spaceTimeStreams <= 1)
        return 1;
    if (spaceTimeStreams == 2)
        return 2;
    if (spaceTimeStreams <= 4)
        return 4;
    if (spaceTimeStreams <= 6)
        return 6;
    return 8;
}

inline simtime_t getHeLtfSymbolDuration(Ieee80211HeLtfType ltfType)
{
    switch (ltfType) {
        case HE_LTF_1X: return SimTime(4, SIMTIME_US);
        case HE_LTF_2X: return SimTime(8, SIMTIME_US);
        case HE_LTF_4X: return SimTime(16, SIMTIME_US);
        default: throw cRuntimeError("Invalid HE-LTF type: %d", (int)ltfType);
    }
}

inline int getHeSigBContentChannelCount(Hz channelBandwidth)
{
    int widthMhz = std::lround(channelBandwidth.get() / 1e6);
    if (widthMhz == 20)
        return 1;
    if (widthMhz == 40 || widthMhz == 80 || widthMhz == 160)
        return 2;
    throw cRuntimeError("Unsupported HE channel bandwidth: %g MHz", channelBandwidth.get() / 1e6);
}

inline int getHeSigBSymbolCount(Hz channelBandwidth, int numberOfUsers)
{
    // IEEE 802.11-2024 Clause 27.3.11.13.2 ("HE-SIG-B field"):
    // HE-SIG-B is encoded with BPSK and code rate 1/2. Each content channel
    // uses 52 OFDM subcarriers for data: 52 coded bits × 1/2 = 26 information
    // bits per symbol.
    constexpr int HE_SIG_B_DATA_BITS_PER_SYMBOL = 26;
    // IEEE 802.11-2024 Table 27-26 ("Contents of the User Specific field"):
    // each User Specific subfield is 21 bits wide.
    constexpr int HE_SIG_B_USER_FIELD_BITS_PER_USER = 21;
    // Each content channel also carries a 10-bit tail/pad in both the Common
    // field and the User field (6 tail bits + 4 pad bits, Clause 27.3.11.13.2).
    constexpr int HE_SIG_B_TAIL_BITS_PER_CONTENT_CHANNEL = 10;

    int widthMhz = std::lround(channelBandwidth.get() / 1e6);
    int contentChannels = getHeSigBContentChannelCount(channelBandwidth);
    int twentyMhzChannels = widthMhz / 20;
    int commonBitsPerContentChannel = 8 * ((twentyMhzChannels + contentChannels - 1) / contentChannels)
            + HE_SIG_B_TAIL_BITS_PER_CONTENT_CHANNEL;
    int usersPerContentChannel = (numberOfUsers + contentChannels - 1) / contentChannels;
    int userBitsPerContentChannel = usersPerContentChannel * HE_SIG_B_USER_FIELD_BITS_PER_USER
            + HE_SIG_B_TAIL_BITS_PER_CONTENT_CHANNEL;
    return std::max(1, (commonBitsPerContentChannel + userBitsPerContentChannel
            + HE_SIG_B_DATA_BITS_PER_SYMBOL - 1) / HE_SIG_B_DATA_BITS_PER_SYMBOL);
}

/**
 * Validates and calculates a common-duration HE MU or trigger-based PPDU.
 *
 * IEEE 802.11-2024 Clause 27.3.11.13 and Clause 27.3.11.14.
 *
 * The returned result contains either a complete set of parameters used by
 * scheduling, transmission, and reception, or a diagnostic error string.
 */
inline Ieee80211HePhyValidationResult computeHePpduParameters(
        const std::vector<Ieee80211HeUserPhyParameters>& requestedUsers,
        Hz channelBandwidth,
        Ieee80211HePpduFormat ppduFormat = HE_MU_DOWNLINK,
        Ieee80211HeGuardInterval guardInterval = HE_GI_3_2_US,
        Ieee80211HeLtfType ltfType = HE_LTF_4X,
        int packetExtensionDurationUs = 0,
        bool enforceDurationLimit = true)
{
    Ieee80211HePhyValidationResult result;
    if (requestedUsers.empty()) {
        result.error = "HE PPDU has no users";
        return result;
    }

    // Group users by RU index to detect and validate MU-MIMO.
    // IEEE 802.11-2024, Clause 27.3.11.13 ("HE MU PPDU").
    // Validates standard spatial stream limits:
    // - Maximum MU-MIMO group size is 8 users.
    // - Maximum spatial streams per user is 4.
    // - Total spatial streams (N_STS) in a group cannot exceed 8.
    // - User spatial streams must be contiguous (no gaps or overlapping indices).
    std::map<int, std::vector<Ieee80211HeUserPhyParameters>> ruGroups;
    for (const auto& requested : requestedUsers) {
        ruGroups[requested.ru.index].push_back(requested);
    }
    for (const auto& pair : ruGroups) {
        const auto& group = pair.second;
        if (group.size() > 1) {
            if (group.size() > 8) {
                result.error = "HE MU-MIMO group has too many users (max 8)";
                return result;
            }
            uint16_t toneSize = group[0].ru.toneSize;
            uint16_t toneOffset = group[0].ru.toneOffset;
            std::set<uint16_t> staIds;
            std::vector<std::pair<int, int>> streams; // {startIndex, nss}
            int groupTotalNsts = 0;
            for (const auto& user : group) {
                if (user.ru.toneSize != toneSize || user.ru.toneOffset != toneOffset) {
                    result.error = "HE MU-MIMO users on the same RU must have matching tone size and offset";
                    return result;
                }
                if (staIds.count(user.staId) > 0) {
                    result.error = "HE MU-MIMO group contains duplicate STA IDs";
                    return result;
                }
                staIds.insert(user.staId);
                if (user.numberOfSpatialStreams > 4) {
                    result.error = "HE MU-MIMO user cannot have more than 4 spatial streams";
                    return result;
                }
                streams.push_back({user.streamStartIndex, user.numberOfSpatialStreams});
                groupTotalNsts += user.numberOfSpatialStreams;
            }
            if (groupTotalNsts > 8) {
                result.error = "HE MU-MIMO group total spatial streams exceeds 8";
                return result;
            }
            std::sort(streams.begin(), streams.end());
            int expectedStart = 0;
            for (const auto& stream : streams) {
                if (stream.first != expectedStart) {
                    result.error = "HE MU-MIMO spatial streams are not contiguous or have gaps/overlaps";
                    return result;
                }
                expectedStart += stream.second;
            }
        }
    }

    if (packetExtensionDurationUs != 0 && packetExtensionDurationUs != 4 &&
            packetExtensionDurationUs != 8 && packetExtensionDurationUs != 12 && packetExtensionDurationUs != 16) {
        result.error = "invalid HE packet extension duration";
        return result;
    }
    try {
        getHeSigBContentChannelCount(channelBandwidth);
        getHeGuardIntervalDuration(guardInterval);
        getHeLtfSymbolDuration(ltfType);
    }
    catch (const cRuntimeError& error) {
        result.error = error.what();
        return result;
    }

    auto& parameters = result.parameters;
    parameters.common.ppduFormat = ppduFormat;
    parameters.common.channelBandwidth = channelBandwidth;
    parameters.common.guardInterval = guardInterval;
    parameters.common.ltfType = ltfType;
    parameters.common.packetExtensionDurationUs = packetExtensionDurationUs;
    parameters.common.sigA.ppduFormat = ppduFormat;
    parameters.common.sigA.uplink = ppduFormat == HE_TRIGGER_BASED_UPLINK;
    parameters.common.sigB.numberOfSymbols = ppduFormat == HE_MU_DOWNLINK ?
            getHeSigBSymbolCount(channelBandwidth, requestedUsers.size()) : 0;
    // See getHeSigBSymbolCount() for the origin of these constants.
    constexpr int HE_SIG_B_USER_FIELD_BITS_PER_USER = 21;
    constexpr int HE_SIG_B_TAIL_BITS_PER_CONTENT_CHANNEL = 10;
    parameters.common.sigB.commonFieldBits = ppduFormat == HE_MU_DOWNLINK ?
            8 * std::lround(channelBandwidth.get() / 20e6) + HE_SIG_B_TAIL_BITS_PER_CONTENT_CHANNEL : 0;
    parameters.common.sigB.userFieldBits = ppduFormat == HE_MU_DOWNLINK ?
            HE_SIG_B_USER_FIELD_BITS_PER_USER * (int)requestedUsers.size() + HE_SIG_B_TAIL_BITS_PER_CONTENT_CHANNEL : 0;
    parameters.common.heSigBDuration =
            parameters.common.sigB.numberOfSymbols * SimTime(4, SIMTIME_US);

    int totalSpaceTimeStreams = 0;
    for (const auto& requested : requestedUsers)
        totalSpaceTimeStreams += requested.numberOfSpatialStreams;
    if (totalSpaceTimeStreams < 1 || totalSpaceTimeStreams > 8) {
        result.error = "HE PPDU has an unsupported total number of space-time streams";
        return result;
    }
    parameters.common.numberOfHeLtfSymbols = getHeNumberOfLtfSymbols(totalSpaceTimeStreams);
    parameters.common.heLtfDuration =
            parameters.common.numberOfHeLtfSymbols * getHeLtfSymbolDuration(ltfType);
    parameters.common.commonPreambleDuration =
            parameters.common.legacyPreambleDuration +
            parameters.common.rlSigDuration +
            parameters.common.heSigADuration +
            parameters.common.heSigBDuration +
            parameters.common.heStfDuration +
            parameters.common.heLtfDuration;

    auto symbolDuration = SimTime(12800, SIMTIME_NS) + getHeGuardIntervalDuration(guardInterval);
    for (const auto& requested : requestedUsers) {
        auto user = requested;
        if (user.ru.toneSize <= 0) {
            result.error = "invalid HE RU tone size";
            return result;
        }
        if (user.numberOfSpatialStreams < 1 || user.numberOfSpatialStreams > 8) {
            result.error = "invalid HE number of spatial streams";
            return result;
        }
        if (user.coding != HE_CODING_BCC && user.coding != HE_CODING_LDPC) {
            result.error = "invalid HE coding type";
            return result;
        }
        // IEEE Std 802.11-2024 Clause 27.3.12.5 ("Coding"):
        // "LDPC is the only FEC coding scheme in the HE PPDU Data field for a 484-, 996-, and 2x996-tone RU."
        // "LDPC is the only FEC coding scheme in the HE PPDU Data field for HE-MCSs 10 and 11."
        // "Support for BCC coding is limited to less than or equal to four spatial streams..."
        if (user.coding == HE_CODING_BCC) {
            if (user.mcs == 10 || user.mcs == 11) {
                result.error = "HE BCC coding is not supported for MCS 10 or 11";
                return result;
            }
            if (user.ru.toneSize >= 484) {
                result.error = "HE BCC coding is not supported for 484-tone RUs or larger";
                return result;
            }
            if (user.numberOfSpatialStreams > 4) {
                result.error = "HE BCC coding is limited to less than or equal to 4 spatial streams";
                return result;
            }
        }
        if (user.dcm && !isHeDcmCombinationSupported(user.mcs, user.numberOfSpatialStreams)) {
            result.error = "unsupported HE DCM combination";
            return result;
        }
        // IEEE 802.11-2024 Tables 27-62..27-117: reject N/A (MCS, Nss, RU) triples.
        if (!isHeValidMcsNssCombination(user.mcs, user.numberOfSpatialStreams, user.ru.toneSize)) {
            result.error = std::string("HE MCS ") + std::to_string(user.mcs)
                    + ", Nss=" + std::to_string(user.numberOfSpatialStreams)
                    + ", RU=" + std::to_string(user.ru.toneSize)
                    + "-tone is N/A per IEEE 802.11-2024 Tables 27-62..27-117";
            return result;
        }
        int dataSubcarriers = user.ru.dataSubcarriers > 0 ? user.ru.dataSubcarriers :
                getHeRuDataSubcarrierCount(user.ru.toneSize);
        auto codeRate = getHeMcsCodeRate(user.mcs);
        user.guardInterval = guardInterval;
        user.codedBitsPerSymbol = dataSubcarriers * getHeMcsBitsPerSubcarrier(user.mcs) *
                user.numberOfSpatialStreams;
        user.dataBitsPerSymbol = user.codedBitsPerSymbol * codeRate.first / codeRate.second;
        if (user.dcm)
            user.dataBitsPerSymbol /= 2;
        if (user.dataBitsPerSymbol <= 0) {
            result.error = "HE user has no data bits per symbol";
            return result;
        }
        // IEEE Std 802.11-2024 Clause 27.3.12.5.1 ("BCC coding and puncturing"):
        // "When conducting BCC FEC encoding for an HE PPDU, the number of encoders is always 1."
        if (user.coding == HE_CODING_BCC) {
            user.numberOfEncoders = 1;
        } else {
            user.numberOfEncoders = std::max(1, (user.dataBitsPerSymbol + 647) / 648);
        }
        user.tailBits = user.coding == HE_CODING_LDPC ? 0 : 6 * user.numberOfEncoders;
        int64_t uncodedBits = user.serviceBits + user.psduLength.get<B>() * 8 + user.tailBits;
        if (user.coding == HE_CODING_LDPC) {
            // 802.11 LDPC uses 648/1296/1944-bit codewords. At packet level
            // we model codeword selection, shortening and repetition while
            // retaining the standard NDBPS symbol rounding used by the PHY.
            // The largest legal codeword which can carry a single shortened
            // payload is chosen first; additional payload is split over equal
            // codewords. This makes the boundary behaviour deterministic and
            // keeps the accounting shared by DL and HE-TB calculations.
            const int candidates[] = {648, 1296, 1944};
            int codeRateNumerator = codeRate.first;
            int codeRateDenominator = codeRate.second;
            user.ldpcCodewordLength = 1944;
            for (int candidate : candidates) {
                if (uncodedBits <= candidate * codeRateNumerator / codeRateDenominator) {
                    user.ldpcCodewordLength = candidate;
                    break;
                }
            }
            int informationBitsPerCodeword = user.ldpcCodewordLength * codeRateNumerator / codeRateDenominator;
            user.ldpcCodewordCount = std::max<int64_t>(1,
                    (uncodedBits + informationBitsPerCodeword - 1) / informationBitsPerCodeword);
            int64_t ldpcInformationCapacity = (int64_t)user.ldpcCodewordCount * informationBitsPerCodeword;
            user.ldpcShorteningBits = std::max<int64_t>(0, ldpcInformationCapacity - uncodedBits);
            int64_t codedBits = (int64_t)user.ldpcCodewordCount * user.ldpcCodewordLength;
            user.numberOfDataSymbols = std::max<int64_t>(1,
                    (codedBits + user.codedBitsPerSymbol - 1) / user.codedBitsPerSymbol);
            int64_t symbolCapacity = (int64_t)user.numberOfDataSymbols * user.codedBitsPerSymbol;
            user.ldpcRepetitionBits = std::max<int64_t>(0, symbolCapacity - codedBits);
        }
        else {
            user.numberOfDataSymbols = std::max<int64_t>(1,
                    (uncodedBits + user.dataBitsPerSymbol - 1) / user.dataBitsPerSymbol);
        }
        int64_t bitsInLastSymbol = uncodedBits -
                (int64_t)(user.numberOfDataSymbols - 1) * user.dataBitsPerSymbol;
        user.preFecPaddingFactor = std::clamp<int>(
                (4 * bitsInLastSymbol + user.dataBitsPerSymbol - 1) / user.dataBitsPerSymbol, 1, 4);
        int effectiveLastSymbolBits =
                (user.preFecPaddingFactor * user.dataBitsPerSymbol + 3) / 4;
        user.postFecPaddingBits = std::max<int64_t>(0, effectiveLastSymbolBits - bitsInLastSymbol);
        user.numberOfSymbols = user.numberOfDataSymbols;
        parameters.commonNumberOfDataSymbols =
                std::max(parameters.commonNumberOfDataSymbols, user.numberOfDataSymbols);
        parameters.users.push_back(user);
    }

    parameters.duration = parameters.common.commonPreambleDuration +
            parameters.commonNumberOfDataSymbols * symbolDuration +
            SimTime(packetExtensionDurationUs, SIMTIME_US);
    
    // IEEE 802.11ax imposes a strict physical limit of 5.484 ms (5484 µs) on total PPDU transmission duration
    // to guarantee fair medium access and compatibility (Clause 27.3.11.8).
    if (enforceDurationLimit && parameters.duration > SimTime(5.484, SIMTIME_MS)) {
        result.error = "HE PPDU exceeds the 5.484 ms duration limit";
        return result;
    }
    for (auto& user : parameters.users) {
        user.dataDuration = parameters.commonNumberOfDataSymbols * symbolDuration;
        user.preambleDuration = parameters.common.commonPreambleDuration;
        user.headerDuration = SIMTIME_ZERO;
        user.duration = parameters.duration;
    }
    result.valid = true;
    return result;
}

inline Ieee80211HeUserPhyParameters computeHeUserPhyParameters(
        B psduLength, const Ieee80211HeRu& ru, int mcs,
        int numberOfSpatialStreams = 1, bool dcm = false,
        Ieee80211HeGuardInterval guardInterval = HE_GI_3_2_US,
        Ieee80211HeCoding coding = HE_CODING_BCC)
{
    Ieee80211HeUserPhyParameters request;
    request.ru = ru;
    request.mcs = mcs;
    request.numberOfSpatialStreams = numberOfSpatialStreams;
    request.dcm = dcm;
    request.coding = coding;
    request.psduLength = psduLength;
    Hz bandwidth = ru.toneSize >= 1992 ? Hz(160e6) :
            ru.toneSize >= 996 ? Hz(80e6) :
            ru.toneSize >= 484 ? Hz(40e6) : Hz(20e6);
    auto result = computeHePpduParameters({request}, bandwidth,
            HE_MU_DOWNLINK, guardInterval, HE_LTF_4X, 0, false);
    if (!result)
        throw cRuntimeError("%s", result.error.c_str());
    return result.parameters.users.front();
}

inline simtime_t estimateHeMuUserDuration(B psduLength, int toneSize, int mcs,
        int numberOfSpatialStreams = 1, bool dcm = false,
        Ieee80211HeGuardInterval guardInterval = HE_GI_3_2_US)
{
    Ieee80211HeRu ru;
    ru.toneSize = std::max(toneSize, 26);
    ru.dataSubcarriers = getHeRuDataSubcarrierCount(ru.toneSize);
    ru.pilotSubcarriers = getHeRuPilotSubcarrierCount(ru.toneSize);
    ru.bandwidth = Hz(ru.toneSize * 78125.0);
    Ieee80211HeCoding coding = (ru.toneSize >= 484 || mcs >= 10) ? HE_CODING_LDPC : HE_CODING_BCC;
    return computeHeUserPhyParameters(psduLength, ru, mcs,
            numberOfSpatialStreams, dcm, guardInterval, coding).duration;
}

} // namespace physicallayer
} // namespace inet

#endif
