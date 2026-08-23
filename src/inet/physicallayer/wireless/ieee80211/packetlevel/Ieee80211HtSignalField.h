//
// Copyright (C) 2026
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HTSIGNALFIELD_H
#define __INET_IEEE80211HTSIGNALFIELD_H

#include <cstdint>

#include "inet/common/INETDefs.h"

namespace inet {
namespace physicallayer {

/**
 * The receiver-independent six-byte HT-SIG field described by IEEE Std
 * 802.11-2024 Table 19-11 and Figure 19-6.
 *
 * The crc member uses the conventional polynomial-byte representation
 * c0...c7 (c0 is bit 7 and c7 is bit 0). packIeee80211HtSignalField()
 * places those bits on the wire in the mandated c7-first order and does not
 * recalculate a caller-supplied CRC.
 */
struct Ieee80211HtSignalField
{
    uint8_t mcs = 0;
    bool cbw = false;
    uint16_t length = 0;
    bool smoothing = false;
    bool notSounding = true;
    bool reserved = true;
    bool aggregation = false;
    uint8_t stbc = 0;
    bool fecCoding = false;
    bool shortGi = false;
    uint8_t numberOfExtensionSpatialStreams = 0;
    uint8_t crc = 0;
    uint8_t tail = 0;
};

/** Packs the six HT-SIG octets into a little-endian, LSB-first bit value. */
INET_API uint64_t packIeee80211HtSignalField(const Ieee80211HtSignalField& field);

/** Unpacks the six HT-SIG octets from a little-endian, LSB-first bit value. */
INET_API Ieee80211HtSignalField unpackIeee80211HtSignalField(uint64_t signal);

/** Computes the complemented HT-SIG CRC for protected bits 0 through 33. */
INET_API uint8_t computeIeee80211HtSignalFieldCrc(const Ieee80211HtSignalField& field);

/** Verifies the caller-supplied CRC without changing the field. */
INET_API bool verifyIeee80211HtSignalFieldCrc(const Ieee80211HtSignalField& field);

} // namespace physicallayer
} // namespace inet

#endif
