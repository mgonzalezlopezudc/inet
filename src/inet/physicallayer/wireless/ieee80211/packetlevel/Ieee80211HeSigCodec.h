//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HESIGCODEC_H
#define __INET_IEEE80211HESIGCODEC_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// HE-SIG-B Common field codec.
//
// Encodes and decodes the RU allocation subfields of the HE-SIG-B Common field
// as defined in IEEE 802.11-2024 Clause 27.3.11.8 and Table 27-27.  The
// codec maps between a concrete Ieee80211HeRu layout and the compact 8-bit
// allocation codes carried in HE-SIG-B content channels.

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h"

namespace inet {
namespace physicallayer {

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
