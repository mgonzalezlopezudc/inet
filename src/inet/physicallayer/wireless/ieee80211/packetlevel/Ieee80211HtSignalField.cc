//
// Copyright (C) 2026
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HtSignalField.h"

#include "inet/common/INETDefs.h"

namespace inet {
namespace physicallayer {

namespace {

constexpr uint64_t HT_SIGNAL_MCS_MASK = 0x7F;
constexpr uint64_t HT_SIGNAL_LENGTH_MASK = 0xFFFF;
constexpr uint64_t HT_SIGNAL_STBC_MASK = 0x3;
constexpr uint64_t HT_SIGNAL_NESS_MASK = 0x3;
constexpr uint64_t HT_SIGNAL_TAIL_MASK = 0x3F;
constexpr uint64_t HT_SIGNAL_PROTECTED_MASK = (uint64_t(1) << 34) - 1;
constexpr uint64_t HT_SIGNAL_CRC_POLYNOMIAL = (uint64_t(1) << 8) | (uint64_t(1) << 2) | (uint64_t(1) << 1) | 1;

uint64_t packProtectedBits(const Ieee80211HtSignalField& field)
{
    if (field.mcs > HT_SIGNAL_MCS_MASK)
        throw cRuntimeError("HT-SIG MCS value %u does not fit in seven bits", field.mcs);
    if (field.stbc > HT_SIGNAL_STBC_MASK)
        throw cRuntimeError("HT-SIG STBC value %u does not fit in two bits", field.stbc);
    if (field.numberOfExtensionSpatialStreams > HT_SIGNAL_NESS_MASK)
        throw cRuntimeError("HT-SIG NESS value %u does not fit in two bits", field.numberOfExtensionSpatialStreams);
    if (field.tail > HT_SIGNAL_TAIL_MASK)
        throw cRuntimeError("HT-SIG tail value %u does not fit in six bits", field.tail);

    uint64_t signal = field.mcs & HT_SIGNAL_MCS_MASK;
    signal |= uint64_t(field.cbw) << 7;
    signal |= (uint64_t(field.length) & HT_SIGNAL_LENGTH_MASK) << 8;
    signal |= uint64_t(field.smoothing) << 24;
    signal |= uint64_t(field.notSounding) << 25;
    signal |= uint64_t(field.reserved) << 26;
    signal |= uint64_t(field.aggregation) << 27;
    signal |= (uint64_t(field.stbc) & HT_SIGNAL_STBC_MASK) << 28;
    signal |= uint64_t(field.fecCoding) << 30;
    signal |= uint64_t(field.shortGi) << 31;
    signal |= (uint64_t(field.numberOfExtensionSpatialStreams) & HT_SIGNAL_NESS_MASK) << 32;
    signal |= (uint64_t(field.tail) & HT_SIGNAL_TAIL_MASK) << 42;
    return signal;
}

uint8_t computeCrcFromProtectedBits(uint64_t protectedBits)
{
    // IEEE Std 802.11-2024 Clause 19.3.9.4.4 defines
    // M(D)=m0*D^33+...+m33 and initializes the first eight message
    // coefficients through I(D)=D^26+...+D^33.
    uint64_t message = 0;
    for (int bit = 0; bit < 34; bit++)
        if ((protectedBits >> bit) & 1)
            message |= uint64_t(1) << (33 - bit);
    const uint64_t initialization = uint64_t(0xFF) << 26;
    uint64_t value = (message ^ initialization) << 8;
    while (value >= (uint64_t(1) << 8)) {
        uint64_t leadingBit = value;
        int shift = 0;
        while (leadingBit >= 2) {
            leadingBit >>= 1;
            shift++;
        }
        shift -= 8;
        value ^= HT_SIGNAL_CRC_POLYNOMIAL << shift;
    }
    return static_cast<uint8_t>(~value);
}

} // namespace

uint64_t packIeee80211HtSignalField(const Ieee80211HtSignalField& field)
{
    uint64_t signal = packProtectedBits(field);
    // The CRC member is stored as the conventional c0..c7 polynomial byte
    // (c0 in bit 7, c7 in bit 0). The wire field is c7 first, hence the
    // explicit reversal below.
    for (int bit = 0; bit < 8; bit++)
        signal |= uint64_t((field.crc >> (7 - bit)) & 1) << (34 + bit);
    return signal;
}

Ieee80211HtSignalField unpackIeee80211HtSignalField(uint64_t signal)
{
    Ieee80211HtSignalField field;
    field.mcs = static_cast<uint8_t>(signal & HT_SIGNAL_MCS_MASK);
    field.cbw = ((signal >> 7) & 1) != 0;
    field.length = static_cast<uint16_t>((signal >> 8) & HT_SIGNAL_LENGTH_MASK);
    field.smoothing = ((signal >> 24) & 1) != 0;
    field.notSounding = ((signal >> 25) & 1) != 0;
    field.reserved = ((signal >> 26) & 1) != 0;
    field.aggregation = ((signal >> 27) & 1) != 0;
    field.stbc = static_cast<uint8_t>((signal >> 28) & HT_SIGNAL_STBC_MASK);
    field.fecCoding = ((signal >> 30) & 1) != 0;
    field.shortGi = ((signal >> 31) & 1) != 0;
    field.numberOfExtensionSpatialStreams = static_cast<uint8_t>((signal >> 32) & HT_SIGNAL_NESS_MASK);
    for (int bit = 0; bit < 8; bit++)
        field.crc |= static_cast<uint8_t>(((signal >> (34 + bit)) & 1) << (7 - bit));
    field.tail = static_cast<uint8_t>((signal >> 42) & HT_SIGNAL_TAIL_MASK);
    return field;
}

uint8_t computeIeee80211HtSignalFieldCrc(const Ieee80211HtSignalField& field)
{
    return computeCrcFromProtectedBits(packProtectedBits(field) & HT_SIGNAL_PROTECTED_MASK);
}

bool verifyIeee80211HtSignalFieldCrc(const Ieee80211HtSignalField& field)
{
    return field.crc == computeIeee80211HtSignalFieldCrc(field);
}

} // namespace physicallayer
} // namespace inet
