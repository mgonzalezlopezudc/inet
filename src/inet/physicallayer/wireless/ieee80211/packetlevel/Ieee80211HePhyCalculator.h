//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HEPHYCALCULATOR_H
#define __INET_IEEE80211HEPHYCALCULATOR_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "inet/common/Units.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeSigCodec.h"

namespace inet {
namespace physicallayer {

using namespace inet::units::values;

/**
 * HE PPDU formats modelled by the common MU PHY calculator.
 * IEEE 802.11-2024 Clause 27.3.4 ("HE PPDU formats"), Figures 27-8 through 27-11.
 */
enum Ieee80211HePpduFormat {
    HE_MU_DOWNLINK = 0,             // HE MU PPDU format (Figure 27-9) for DL OFDMA/MU-MIMO
    HE_TRIGGER_BASED_UPLINK = 1,    // HE TB PPDU format (Figure 27-11) for UL OFDMA/MU-MIMO triggered by AP
    HE_SINGLE_USER = 2,             // HE SU PPDU format (Figure 27-8)
    HE_EXTENDED_RANGE_SU = 3        // HE ER SU PPDU format (Figure 27-10)
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
 * IEEE 802.11-2024 Clause 27.3.12.5 ("Coding").
 */
enum Ieee80211HeCoding {
    HE_CODING_BCC = 0,              // Binary Convolutional Coding
    HE_CODING_LDPC = 1              // Low-Density Parity-Check Coding
};

/** Source of the common HE-TB timing and padding parameters. */
enum class Ieee80211HeTriggerMethod {
    NONE,
    TRIGGER_FRAME,
    TRS,
};

/**
 * Trigger-derived HE-TB inputs that are common to all solicited users.
 * TRS is represented explicitly, but remains unsupported until a producer
 * supplies its UL Data Symbols value.
 */
struct Ieee80211HeTbCalculationContext
{
    Ieee80211HeTriggerMethod triggerMethod = Ieee80211HeTriggerMethod::NONE;
    uint16_t ulLength = 0;
    int preFecPaddingFactor = 0; // semantic a, in the range 1..4
    bool ldpcExtraSymbolSegment = false;
    bool peDisambiguity = false;
    int numberOfHeLtfSymbols = 0;
};

/**
 * HE long-training-field (HE-LTF) duration multiplier.
 * IEEE 802.11-2024 Clause 27.3.11.10 and Table 27-32.
 */
enum Ieee80211HeLtfType {
    HE_LTF_1X = 1,                  // 3.2 µs DFT period
    HE_LTF_2X = 2,                  // 6.4 µs DFT period
    HE_LTF_4X = 4                   // 12.8 µs DFT period
};

/** Semantically distinct HE SU bandwidth selections (Table 27-19). */
enum class Ieee80211HeSuBandwidth {
    UNKNOWN,
    MHZ_20,
    MHZ_40,
    MHZ_80,
    MHZ_160,
    MHZ_80P80,
};

/** HE ER SU RU selections carried by the format-specific bandwidth field. */
enum class Ieee80211HeErSuRuMode {
    UNKNOWN,
    PRIMARY_242_TONE,
    PRIMARY_UPPER_106_TONE,
};

/** Operating band used to resolve the timing-only signal extension. */
enum class Ieee80211HeOperatingBand {
    UNKNOWN,
    BAND_2_4_GHZ,
    BAND_5_GHZ,
    BAND_6_GHZ,
};

enum class Ieee80211HeMidamblePeriodicity {
    SYMBOLS_10,
    SYMBOLS_20,
};

/** TXOP duration is either explicitly unspecified or an exact duration to quantize. */
struct Ieee80211HeTxopDuration
{
    bool unspecified = true;
    uint16_t durationUs = 0;

    bool operator==(const Ieee80211HeTxopDuration& other) const
    {
        return unspecified == other.unspecified && durationUs == other.durationUs;
    }
};

/**
 * Semantic FEC outcome. BCC must not carry an extra-segment value; LDPC must
 * carry the calculated value, including an explicitly calculated false.
 */
struct Ieee80211HeFecOutcome
{
    Ieee80211HeCoding coding = HE_CODING_BCC;
    std::optional<bool> ldpcExtraSymbolSegment;
};

/**
 * Common semantic inputs for HE SU and HE ER SU logical signaling.
 * Policy and outcome optionals are explicit inputs: absence is rejected so a
 * serializer cannot silently invent values, except where documented otherwise.
 */
struct Ieee80211HeSuErSigASemantics
{
    uint64_t txTimeNs = 0; // complete PPDU TXTIME, including signal extension
    Ieee80211HeOperatingBand operatingBand = Ieee80211HeOperatingBand::UNKNOWN;
    std::optional<bool> noSignalExtension;
    std::optional<bool> beamChange;
    std::optional<bool> uplink;
    int mcs = -1;
    std::optional<bool> dcmApplied;
    int bssColor = -1;
    int spatialReuse = -1;
    std::optional<Ieee80211HeGuardInterval> guardInterval;
    std::optional<Ieee80211HeLtfType> ltfType;
    int numberOfSpaceTimeStreams = 0;
    std::optional<bool> stbcApplied;
    std::optional<Ieee80211HeMidamblePeriodicity> midamblePeriodicity; // absent means Doppler is not signaled
    std::optional<Ieee80211HeTxopDuration> txopDuration;
    std::optional<Ieee80211HeFecOutcome> fec;
    std::optional<bool> beamformed;
    int preFecPaddingFactor = 0; // semantic final factor a, in the range 1..4
    uint32_t packetExtensionNs = UINT32_MAX; // actual T_PE: 0, 4000, 8000, 12000, or 16000 ns
};

struct Ieee80211HeSuSignalingRequest
{
    Ieee80211HeSuErSigASemantics common;
    Ieee80211HeSuBandwidth bandwidth = Ieee80211HeSuBandwidth::UNKNOWN;
};

struct Ieee80211HeErSuSignalingRequest
{
    Ieee80211HeSuErSigASemantics common;
    Ieee80211HeErSuRuMode ruMode = Ieee80211HeErSuRuMode::UNKNOWN;
};

struct Ieee80211HeSuSignaling
{
    Ieee80211HeLSig lSig;
    Ieee80211HeSuSigA sigA;
    uint32_t signalExtensionNs = 0;
};

struct Ieee80211HeErSuSignaling
{
    Ieee80211HeLSig lSig;
    Ieee80211HeErSuSigA sigA;
    uint32_t signalExtensionNs = 0;
};

struct Ieee80211HeSuSignalingResult : Ieee80211HeSigCodecStatus
{
    Ieee80211HeSuSignaling value;
};

struct Ieee80211HeErSuSignalingResult : Ieee80211HeSigCodecStatus
{
    Ieee80211HeErSuSignaling value;
};

/** Upper TXTIME boundary represented by an HE L-SIG LENGTH bucket. */
struct Ieee80211HeTxTimeResult : Ieee80211HeSigCodecStatus
{
    simtime_t txTime = SIMTIME_ZERO;
};

/** Builds validated logical signaling without changing or recalculating PPDU airtime. */
INET_API Ieee80211HeSuSignalingResult buildIeee80211HeSuSignaling(const Ieee80211HeSuSignalingRequest& request);
INET_API Ieee80211HeErSuSignalingResult buildIeee80211HeErSuSignaling(const Ieee80211HeErSuSignalingRequest& request);

/** Selects the explicit Trigger Common Info UL Length for a solicited HE TB TXTIME. */
INET_API Ieee80211HeLSigResult buildIeee80211HeTriggerUlLength(simtime_t txTime,
        uint32_t signalExtensionNs = 0);
INET_API Ieee80211HeTxTimeResult getIeee80211HeTriggerTxTimeUpperBound(
        uint16_t lLength, uint32_t signalExtensionNs = 0);
/** Table 27-61 signal extension selected by operating band and NO_SIG_EXTN. */
INET_API uint32_t getIeee80211HeSignalExtensionNs(Ieee80211HeOperatingBand operatingBand,
        bool noSignalExtension);
INET_API uint32_t getIeee80211HeSignalExtensionNs(Hz centerFrequency,
        bool noSignalExtension);
/**
 * HE-SIG-A fields shared by every user in the PPDU.
 * IEEE 802.11-2024 Table 27-21 ("HE-SIG-A field of an HE MU PPDU").
 * Carries common physical configuration such as BSS Color (6-bit field used for spatial reuse/OBSS PD detection).
 */
struct Ieee80211HeSigAFields
{
    Ieee80211HePpduFormat ppduFormat = HE_MU_DOWNLINK;
    uint8_t bssColor = 0;           // 6-bit BSS Color identifier (1-63, 0 means disabled)
    std::array<uint8_t, 4> spatialReuse {{0, 0, 0, 0}};
    bool uplink = false;
    bool txopUnspecified = true;
    int txopDurationUs = 0;         // Decoded, wire-quantized remaining TXOP duration
    bool doppler = false;
    uint8_t midamblePeriodicity = 0;
    bool stbc = false;              // Space-Time Block Coding indicator
};

/**
 * HE-SIG-B parameters for a downlink MU PPDU.
 * IEEE 802.11-2024 Clause 27.3.11.8 ("HE-SIG-B field").
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
    bool ndp = false; // model-only: an NDP has no Data field
    Hz channelBandwidth = Hz(NaN);
    uint8_t puncturedSubchannelMask = 0;
    uint16_t lSigLength = 0;
    bool noSignalExtension = false;
    uint32_t signalExtensionNs = 0;
    Ieee80211HeGuardInterval guardInterval = HE_GI_3_2_US;
    Ieee80211HeLtfType ltfType = HE_LTF_4X;
    int numberOfHeLtfSymbols = 1;
    int preFecPaddingFactor = 0; // common final a; zero only when the Data field is absent
    bool ldpcExtraSymbol = false;
    int nominalPacketExtensionDurationUs = 0;
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
    bool ndpFeedbackReport = false;
    uint8_t ndpFeedbackStatus = 0;
    uint8_t ndpRuToneSetIndex = 0;
    uint8_t ndpStartingStsNumber = 0;
    Ieee80211HeGuardInterval guardInterval = HE_GI_3_2_US; // compatibility
    Ieee80211HeCoding coding = HE_CODING_BCC;
    B psduLength = B(0);
    int nominalPacketPaddingDurationUs = 0;
    int streamStartIndex = 0;
    uint16_t staId = 0;
    int numberOfEncoders = 1;
    int codedBitsPerSymbol = 0;
    int dataBitsPerSymbol = 0;
    int shortDataSubcarriers = 0;
    int shortCodedBitsPerSymbol = 0;
    int shortDataBitsPerSymbol = 0;
    int serviceBits = 16;
    int tailBits = 6;
    int64_t payloadAndServiceBits = 0;
    int excessBits = 0;
    int individualInitialPreFecPaddingFactor = 0;
    int individualInitialNumberOfDataSymbols = 0;
    int initialPreFecPaddingFactor = 0;
    int initialNumberOfDataSymbols = 0;
    int initialLastCodedBitsPerSymbol = 0;
    int initialLastDataBitsPerSymbol = 0;
    int finalLastCodedBitsPerSymbol = 0;
    int finalLastDataBitsPerSymbol = 0;
    int preFecPaddingBits = 0;
    int macPreFecPaddingBits = 0;
    int phyPreFecPaddingBits = 0;
    int bccCodedPaddingBits = 0;
    // Packet-level representation of the HE LDPC encoder. These fields are
    // deliberately retained in the common calculator result so scheduling,
    // transmission and reception cannot silently use different assumptions.
    int ldpcCodewordLength = 0;
    int ldpcCodewordCount = 0;
    int ldpcPayloadBits = 0;
    int ldpcAvailableBits = 0;
    int ldpcShorteningBits = 0;
    int ldpcPuncturingBits = 0;
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

/** Exact Table 19-16 LDPC codeword selection and extra-segment predicates. */
struct Ieee80211HeLdpcCalculationResult
{
    bool valid = false;
    std::string error;
    int codewordCount = 0;
    int codewordLength = 0;
    int shorteningBits = 0;
    int puncturingBits = 0;
    bool primaryExtraSymbolCondition = false;
    bool extremePuncturingCondition = false;
    bool extraSymbolRequired = false;

    explicit operator bool() const { return valid; }
};

inline std::ostream& operator<<(std::ostream& os, const Ieee80211HeUserPhyParameters& user)
{
    os << "staId=" << user.staId
       << " ru={" << user.ru << "}"
       << " mcs=" << user.mcs
       << " nss=" << user.numberOfSpatialStreams
       << " dcm=" << (user.dcm ? "yes" : "no")
       << " coding=" << (user.coding == HE_CODING_LDPC ? "LDPC" : "BCC")
       << " psdu=" << user.psduLength
       << " dataBitsPerSymbol=" << user.dataBitsPerSymbol
       << " symbols=" << user.numberOfDataSymbols
       << " ldpcCwLen=" << user.ldpcCodewordLength
       << " ldpcCwCount=" << user.ldpcCodewordCount
       << " ldpcShortening=" << user.ldpcShorteningBits
       << " tailBits=" << user.tailBits
       << " duration=" << user.duration;
    return os;
}

/** Stable error codes returned by HE PHY validation factories and calculators. */
enum class Ieee80211HeValidationErrorCode {
    NONE = 0,
    INVALID_PPDU_FORMAT = 1,
    INVALID_CHANNEL_BANDWIDTH = 2,
    INVALID_CENTER_FREQUENCY = 3,
    INVALID_GUARD_INTERVAL = 4,
    INVALID_LTF_TYPE = 5,
    INVALID_GI_LTF_COMBINATION = 6,
    INVALID_PACKET_EXTENSION = 7,
    EMPTY_USER_LIST = 8,
    INVALID_USER_COUNT = 9,
    INVALID_RU_LAYOUT = 10,
    INVALID_MCS = 11,
    INVALID_SPATIAL_STREAMS = 12,
    INVALID_STREAM_MAPPING = 13,
    INVALID_STA_ID = 14,
    INVALID_CODING = 15,
    INVALID_PSDU_LENGTH = 16,
    INVALID_FEC_COMBINATION = 17,
    INVALID_DCM_COMBINATION = 18,
    INVALID_MCS_NSS_COMBINATION = 19,
    INVALID_DATA_RATE = 20,
    PPDU_DURATION_EXCEEDED = 21,
    INTERNAL_ERROR = 22,
    INVALID_BSS_COLOR = 23,
    INVALID_TXOP_DURATION = 24,
    INVALID_L_SIG_LENGTH = 25,
    UNSUPPORTED_DOPPLER_TIMING = 26,
    INVALID_NOMINAL_PACKET_PADDING = 27,
    INVALID_TRIGGER_CONTEXT = 28,
    UNSUPPORTED_STBC = 29,
    INVALID_SPATIAL_REUSE = 30,
};

/** Machine-readable location and human-readable detail for an HE validation error. */
struct Ieee80211HeValidationContext
{
    std::optional<size_t> userIndex;
    std::optional<size_t> physicalRuIndex;
    std::string fieldName;
    std::string detail;
};

/**
 * HE PPDU validation/calculation result. Ordinary malformed inputs are reported
 * as diagnostics; allocation failures are not covered by that guarantee.
 */
struct Ieee80211HePhyValidationResult
{
    bool valid = false;
    Ieee80211HeValidationErrorCode errorCode = Ieee80211HeValidationErrorCode::NONE;
    Ieee80211HeValidationContext context;
    // Compatibility diagnostic; new callers should use errorCode and context.
    std::string error;
    Ieee80211HePpduParameters parameters;

    explicit operator bool() const { return valid; }
};

/**
 * Complete PHY inputs for finalizing the common timing fields of a
 * response-soliciting Trigger frame. When durationBudget is present, the
 * longest legal HE-TB duration not exceeding it is selected; otherwise the
 * shortest legal duration that contains every requested PSDU is selected.
 */
struct Ieee80211HeTriggerResponseFinalizationRequest
{
    std::vector<Ieee80211HeUserPhyParameters> users;
    Hz centerFrequency = Hz(NaN);
    Hz channelBandwidth = Hz(NaN);
    Ieee80211HeGuardInterval guardInterval = HE_GI_1_6_US;
    Ieee80211HeLtfType ltfType = HE_LTF_2X;
    int packetExtensionDurationUs = 0;
    bool noSignalExtension = false;
    std::optional<simtime_t> durationBudget;
};

/** Canonical common Trigger fields and the resolved HE-TB calculation. */
struct Ieee80211HeTriggerResponseFinalizationResult
{
    bool valid = false;
    Ieee80211HeValidationErrorCode errorCode = Ieee80211HeValidationErrorCode::NONE;
    Ieee80211HeValidationContext context;
    std::string error;
    uint16_t ulLength = 0;
    simtime_t commonDuration = SIMTIME_ZERO;
    bool commonDurationExact = false;
    bool peDisambiguity = false;
    simtime_t resolvedTxTime = SIMTIME_ZERO;
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
 * IEEE 802.11-2024 Clause 27.3.11.10 reuses Table 21-13, replacing
 * N_VHT-LTF with N_HE-LTF for the 1/2/4/6/8 symbol mapping.
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

/**
 * Returns the deterministic HE-LTF type used when no authoritative LTF type
 * is available from signaling metadata. This preserves the existing 4x policy
 * for 0.8 us and 3.2 us GI while selecting the legal 2x type for 1.6 us GI.
 */
inline Ieee80211HeLtfType getHeDefaultLtfType(Ieee80211HeGuardInterval guardInterval)
{
    switch (guardInterval) {
        case HE_GI_0_8_US: return HE_LTF_4X;
        case HE_GI_1_6_US: return HE_LTF_2X;
        case HE_GI_3_2_US: return HE_LTF_4X;
        default: throw cRuntimeError("Invalid HE guard interval: %d", (int)guardInterval);
    }
}

/**
 * Returns the duration of one HE-LTF symbol, including its guard interval.
 * IEEE 802.11-2024 Table 27-13 and the GI/LTF encodings admit only the six
 * combinations handled below.
 */
inline simtime_t getHeLtfSymbolDuration(Ieee80211HeLtfType ltfType,
        Ieee80211HeGuardInterval guardInterval)
{
    switch (ltfType) {
        case HE_LTF_1X:
            if (guardInterval == HE_GI_0_8_US)
                return SimTime(4, SIMTIME_US);
            if (guardInterval == HE_GI_1_6_US)
                return SimTime(4800, SIMTIME_NS);
            break;
        case HE_LTF_2X:
            if (guardInterval == HE_GI_0_8_US)
                return SimTime(7200, SIMTIME_NS);
            if (guardInterval == HE_GI_1_6_US)
                return SimTime(8, SIMTIME_US);
            break;
        case HE_LTF_4X:
            if (guardInterval == HE_GI_0_8_US)
                return SimTime(13600, SIMTIME_NS);
            if (guardInterval == HE_GI_3_2_US)
                return SimTime(16, SIMTIME_US);
            break;
        default: throw cRuntimeError("Invalid HE-LTF type: %d", (int)ltfType);
    }
    if (guardInterval != HE_GI_0_8_US && guardInterval != HE_GI_1_6_US &&
            guardInterval != HE_GI_3_2_US)
        throw cRuntimeError("Invalid HE guard interval: %d", (int)guardInterval);
    throw cRuntimeError("Unsupported HE-LTF/GI combination: %dx LTF with GI %d",
            (int)ltfType, (int)guardInterval);
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

/**
 * Estimates uncompressed HE-SIG-B MCS 0 symbols by evenly distributing users
 * over the content channels. It accounts for the CRC and tail of every User
 * Block, but is intentionally not a general normative HE-SIG-B API: MCS, DCM,
 * compression, and the actual RU-to-content-channel distribution are absent.
 */
int getHeSigBSymbolCount(Hz channelBandwidth, int numberOfUsers,
        bool compression = false, int mcs = 0, bool dcm = false);

/** Exact symbol count from the complete per-content-channel logical plan. */
int getHeSigBSymbolCount(const Ieee80211HeSigBCommonField& commonField,
        Hz channelBandwidth, int mcs = 0, bool dcm = false);

/**
 * Calculates IEEE 802.11-2024 Table 19-16 LDPC codeword parameters and the
 * Clause 27.3.12.5.2 extra-symbol conditions without throwing for malformed
 * scalar input.
 */
INET_API Ieee80211HeLdpcCalculationResult computeHeLdpcParameters(
        int64_t payloadBits, int64_t availableBits,
        int rateNumerator, int rateDenominator);

/**
 * Validates and calculates a common-duration HE MU or trigger-based PPDU.
 *
 * IEEE 802.11-2024 Clause 27.3.4, Clause 27.3.11.8, and Clause 27.3.12.5.
 *
 * The returned result contains either a complete set of parameters used by
 * scheduling, transmission, and reception, or a diagnostic error string.
 */
Ieee80211HePhyValidationResult computeHePpduParameters(
        const std::vector<Ieee80211HeUserPhyParameters>& requestedUsers,
        Hz channelBandwidth,
        Ieee80211HePpduFormat ppduFormat = HE_MU_DOWNLINK,
        Ieee80211HeGuardInterval guardInterval = HE_GI_3_2_US,
        Ieee80211HeLtfType ltfType = HE_LTF_4X,
        int packetExtensionDurationUs = 0,
        bool enforceDurationLimit = true,
        const std::optional<Ieee80211HeTbCalculationContext>& tbContext = std::nullopt);

/**
 * Finalizes UL Length, HE-LTF count, pre-FEC padding, LDPC extra segment,
 * PE disambiguity, and packet extension through one HE-TB calculation path.
 */
INET_API Ieee80211HeTriggerResponseFinalizationResult finalizeHeTriggerResponse(
        const Ieee80211HeTriggerResponseFinalizationRequest& request);

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
            HE_MU_DOWNLINK, guardInterval, getHeDefaultLtfType(guardInterval), 0, false);
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
