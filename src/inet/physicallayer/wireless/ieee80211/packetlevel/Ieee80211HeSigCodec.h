//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HESIGCODEC_H
#define __INET_IEEE80211HESIGCODEC_H

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// IEEE 802.11 HE logical signaling codecs.
//
// The L-SIG, RL-SIG, and HE-SIG-A codecs below operate on uncoded logical bits.
// Bit 0 is the first transmitted logical bit (B0); multi-bit integer fields
// place their least-significant encoded bit at the lower vector index.
// They deliberately do not model BCC encoding, interleaving, modulation, or
// packet/chunk boundaries. The existing HE-SIG-B helpers map between concrete
// Ieee80211HeRu layouts and the compact allocation codes carried in HE-SIG-B.

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"

namespace inet {
namespace physicallayer {

enum class Ieee80211HeSigFormat {
    SU,
    ER_SU,
    MU,
    TB,
};

enum class Ieee80211HeSigCodecErrorCode {
    NONE,
    INVALID_FORMAT,
    INVALID_BIT_COUNT,
    INVALID_TXTIME,
    INVALID_SIGNAL_EXTENSION,
    INVALID_LENGTH,
    INVALID_RATE,
    INVALID_RESERVED_FIELD,
    INVALID_PARITY,
    INVALID_TAIL,
    RL_SIG_MISMATCH,
    INVALID_CRC,
    FIELD_OUT_OF_RANGE,
    RESERVED_FIELD_VALUE,
    INVALID_FIELD_COMBINATION,
};

struct Ieee80211HeSigCodecStatus
{
    bool valid = false;
    Ieee80211HeSigCodecErrorCode errorCode = Ieee80211HeSigCodecErrorCode::NONE;
    std::string error;

    explicit operator bool() const { return valid; }
};

/** Validated logical L-SIG value. RATE, reserved, parity, and tail are derived. */
struct Ieee80211HeLSig
{
    uint16_t length = 0;

    bool operator==(const Ieee80211HeLSig& other) const { return length == other.length; }
    bool operator!=(const Ieee80211HeLSig& other) const { return !(*this == other); }
};

struct Ieee80211HeLSigResult : Ieee80211HeSigCodecStatus
{
    Ieee80211HeLSig value;
};

struct Ieee80211HeLSigBitsResult : Ieee80211HeSigCodecStatus
{
    std::vector<bool> bits;
};

struct Ieee80211HeRlSigResult : Ieee80211HeSigCodecStatus
{
    Ieee80211HeLSig value;
};

/**
 * Builds the L-SIG value using the ceiling operation in IEEE 802.11-2024 Equation 27-11.
 * Durations are exact integer nanoseconds; the signal extension must be 0 ns or 6000 ns.
 */
INET_API Ieee80211HeLSigResult buildHeLSig(Ieee80211HeSigFormat format, uint64_t txTimeNs,
        uint32_t signalExtensionNs = 0);
INET_API Ieee80211HeLSigResult buildHeTbLSig(uint16_t lLength);
INET_API Ieee80211HeLSigBitsResult encodeHeLSig(const Ieee80211HeLSig& value, Ieee80211HeSigFormat format);
INET_API Ieee80211HeLSigResult decodeHeLSig(const std::vector<bool>& bits, Ieee80211HeSigFormat format);
INET_API Ieee80211HeLSigBitsResult encodeHeRlSig(const Ieee80211HeLSig& value, Ieee80211HeSigFormat format);
INET_API Ieee80211HeRlSigResult decodeHeRlSigRepeat(const std::vector<bool>& lSigBits,
        const std::vector<bool>& rlSigBits, Ieee80211HeSigFormat format);

/** IEEE 802.11-2024 27.3.11.7.3 CRC-4 bits, in transmitted c7..c4 order. */
INET_API std::array<bool, 4> computeHeCrc4(const std::vector<bool>& protectedBits);

struct Ieee80211HeSigALayout
{
    Ieee80211HeSigFormat format = Ieee80211HeSigFormat::SU;
    uint8_t logicalBitCount = 52;
    uint8_t ofdmSymbolCount = 2;
    uint8_t repetitionCount = 1;
};

struct Ieee80211HeSigABitsResult : Ieee80211HeSigCodecStatus
{
    std::vector<bool> bits;
    Ieee80211HeSigALayout layout;
};

struct Ieee80211HeSuSigA
{
    bool beamChange = false;
    bool uplink = false;
    uint8_t mcs = 0;
    bool dcm = false;
    uint8_t bssColor = 0;
    uint8_t spatialReuse = 0;
    uint8_t bandwidth = 0;
    uint8_t giLtfSize = 0;
    uint8_t numberOfSpaceTimeStreams = 1;
    uint8_t midamblePeriodicity = 0;
    uint8_t txop = 127;
    bool ldpcCoding = false;
    bool ldpcExtraSymbolSegment = false;
    bool stbc = false;
    bool beamformed = false;
    uint8_t preFecPaddingFactor = 0;
    bool peDisambiguity = false;
    bool doppler = false;
};

struct Ieee80211HeErSuSigA
{
    bool beamChange = false;
    bool uplink = false;
    uint8_t mcs = 0;
    bool dcm = false;
    uint8_t bssColor = 0;
    uint8_t spatialReuse = 0;
    uint8_t bandwidth = 0;
    uint8_t giLtfSize = 0;
    uint8_t numberOfSpaceTimeStreams = 1;
    uint8_t midamblePeriodicity = 0;
    uint8_t txop = 127;
    bool ldpcCoding = false;
    bool ldpcExtraSymbolSegment = false;
    bool stbc = false;
    bool beamformed = false;
    uint8_t preFecPaddingFactor = 0;
    bool peDisambiguity = false;
    bool doppler = false;
};

struct Ieee80211HeMuSigA
{
    bool uplink = false;
    uint8_t heSigBMcs = 0;
    bool heSigBDcm = false;
    uint8_t bssColor = 0;
    uint8_t spatialReuse = 0;
    uint8_t bandwidth = 0;
    bool heSigBCompression = false;
    uint8_t numberOfHeSigBSymbols = 1;
    bool numberOfHeSigBSymbolsIsSaturated = false; // true means 16 or more symbols
    uint8_t numberOfMuMimoUsers = 0;
    uint8_t giLtfSize = 0;
    bool doppler = false;
    uint8_t txop = 127;
    uint8_t numberOfHeLtfSymbols = 1;
    uint8_t midamblePeriodicity = 0;
    bool ldpcExtraSymbolSegment = false;
    bool stbc = false;
    uint8_t preFecPaddingFactor = 0;
    bool peDisambiguity = false;
};

struct Ieee80211HeTbSigA
{
    uint8_t bssColor = 0;
    std::array<uint8_t, 4> spatialReuse = {{0, 0, 0, 0}};
    uint8_t bandwidth = 0;
    uint8_t txop = 127;
    uint16_t triggerReserved = 511;
};

struct Ieee80211HeSuSigAResult : Ieee80211HeSigCodecStatus
{
    Ieee80211HeSuSigA value;
    Ieee80211HeSigALayout layout;
};

struct Ieee80211HeErSuSigAResult : Ieee80211HeSigCodecStatus
{
    Ieee80211HeErSuSigA value;
    Ieee80211HeSigALayout layout;
};

struct Ieee80211HeMuSigAResult : Ieee80211HeSigCodecStatus
{
    Ieee80211HeMuSigA value;
    Ieee80211HeSigALayout layout;
};

struct Ieee80211HeTbSigAResult : Ieee80211HeSigCodecStatus
{
    Ieee80211HeTbSigA value;
    Ieee80211HeSigALayout layout;
};

INET_API Ieee80211HeSigABitsResult encodeHeSuSigA(const Ieee80211HeSuSigA& value);
INET_API Ieee80211HeSuSigAResult decodeHeSuSigA(const std::vector<bool>& bits);
INET_API Ieee80211HeSigABitsResult encodeHeErSuSigA(const Ieee80211HeErSuSigA& value);
INET_API Ieee80211HeErSuSigAResult decodeHeErSuSigA(const std::vector<bool>& bits);
INET_API Ieee80211HeSigABitsResult encodeHeMuSigA(const Ieee80211HeMuSigA& value);
INET_API Ieee80211HeMuSigAResult decodeHeMuSigA(const std::vector<bool>& bits);
INET_API Ieee80211HeSigABitsResult encodeHeTbSigA(const Ieee80211HeTbSigA& value);
INET_API Ieee80211HeTbSigAResult decodeHeTbSigA(const std::vector<bool>& bits);

/** One HE-SIG-B Common-field block carried on a single content channel. */
struct Ieee80211HeSigBCommonBlock
{
    std::vector<uint8_t> ruAllocationSubfields; // 1, 2, or 4 Table 27-27 values
    bool center26ToneRuBitPresent = false;
    bool hasCenter26ToneRu = false;
};

struct Ieee80211HeSigBCommonBlockResult : Ieee80211HeSigCodecStatus
{
    Ieee80211HeSigBCommonBlock value;
};

struct Ieee80211HeSigBBitsResult : Ieee80211HeSigCodecStatus
{
    std::vector<bool> bits;
};

/** Table 27-29 User field for an RU that is not allocated for MU-MIMO. */
struct Ieee80211HeSigBNonMuMimoUser
{
    uint16_t staId = 0;
    uint8_t numberOfSpaceTimeStreams = 1;
    bool beamformed = false;
    uint8_t mcs = 0;
    bool dcm = false;
    bool ldpcCoding = false;
};

/** Table 27-30 User field for an RU allocated for MU-MIMO. */
struct Ieee80211HeSigBMuMimoUser
{
    uint16_t staId = 0;
    uint8_t spatialConfiguration = 0;
    uint8_t mcs = 0;
    bool reserved = false; // arbitrary only for the special STA-ID 2046
    bool ldpcCoding = false;
};

struct Ieee80211HeSigBNonMuMimoUserResult : Ieee80211HeSigCodecStatus
{
    Ieee80211HeSigBNonMuMimoUser value;
};

struct Ieee80211HeSigBMuMimoUserResult : Ieee80211HeSigCodecStatus
{
    Ieee80211HeSigBMuMimoUser value;
};

struct Ieee80211HeSigBUserBlockResult : Ieee80211HeSigCodecStatus
{
    std::vector<std::vector<bool>> userFields; // one or two 21-bit User fields
};

/**
 * Logical HE-SIG-B codecs for IEEE 802.11-2024 Tables 27-25 and 27-28--27-30.
 * The returned vectors contain uncoded bits in transmitted B0-first order.
 */
INET_API Ieee80211HeSigBBitsResult encodeHeSigBCommonBlock(const Ieee80211HeSigBCommonBlock& value);
INET_API Ieee80211HeSigBCommonBlockResult decodeHeSigBCommonBlock(const std::vector<bool>& bits);
INET_API Ieee80211HeSigBBitsResult encodeHeSigBNonMuMimoUser(const Ieee80211HeSigBNonMuMimoUser& value);
INET_API Ieee80211HeSigBNonMuMimoUserResult decodeHeSigBNonMuMimoUser(const std::vector<bool>& bits);
INET_API Ieee80211HeSigBBitsResult encodeHeSigBMuMimoUser(const Ieee80211HeSigBMuMimoUser& value);
INET_API Ieee80211HeSigBMuMimoUserResult decodeHeSigBMuMimoUser(const std::vector<bool>& bits);
INET_API Ieee80211HeSigBBitsResult encodeHeSigBUserBlock(const std::vector<std::vector<bool>>& userFields);
INET_API Ieee80211HeSigBUserBlockResult decodeHeSigBUserBlock(const std::vector<bool>& bits);

/** One content channel's allocation subfields. */
struct Ieee80211HeSigBContentChannel {
    std::vector<uint8_t> ruAllocationSubfields; // Table 27-27 B7..B0 values
    bool hasCenterRu = false;                   // Center 26-tone RU present
};

/** Wire-level HE-SIG-B Common field layout. */
struct Ieee80211HeSigBCommonField {
    std::vector<Ieee80211HeSigBContentChannel> contentChannels; // 1 or 2
    std::vector<Ieee80211HeRu> rus;             // resolved RU objects
};

struct Ieee80211HeSigBCommonFieldResult {
    bool valid = false;
    std::string error;
    Ieee80211HeSigBCommonField commonField;

    explicit operator bool() const { return valid; }
};

/** HE-SIG-B representation of an RU layout and its compact allocation codes. */
struct Ieee80211HeSigBRuAllocation
{
    std::vector<uint8_t> allocationCodes;
    std::vector<Ieee80211HeRu> rus;
};

/** Non-throwing result of HE-SIG-B RU allocation encoding or decoding. */
struct Ieee80211HeSigCodecResult
{
    bool valid = false;
    std::string error;
    Ieee80211HeSigBRuAllocation allocation;

    explicit operator bool() const { return valid; }
};

bool decodeTable27_27(uint8_t code, std::vector<std::pair<int, int>>& RUs, std::vector<int>& userCounts);

Ieee80211HeSigBCommonFieldResult encodeHeSigBCommonField(
        const std::vector<Ieee80211HeRu>& rus, Hz channelBandwidth,
        const std::vector<bool>& puncturedSubchannels = {});

Ieee80211HeSigBCommonFieldResult decodeHeSigBCommonField(
        const Ieee80211HeSigBCommonField& commonField, Hz channelCenterFrequency,
        Hz channelBandwidth);

/** Encodes a validated HE RU layout using its canonical allocation-catalog indices. */
Ieee80211HeSigCodecResult encodeHeSigBRuAllocation(
        const std::vector<Ieee80211HeRu>& rus, Hz channelBandwidth);

/** Decodes HE-SIG-B allocation codes into channel-centered RU descriptions. */
Ieee80211HeSigCodecResult decodeHeSigBRuAllocation(
        const std::vector<uint8_t>& allocationCodes, Hz channelCenterFrequency,
        Hz channelBandwidth);

} // namespace physicallayer
} // namespace inet

#endif
