// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef INET_IEE_E802154_GTSVALUES_H
#define INET_IEE_E802154_GTSVALUES_H

#include <cstdint>
#include "inet/common/INETDefs.h"

namespace inet {

enum class Ieee802154GtsDirection : uint8_t { DEVICE_TRANSMIT = 0, DEVICE_RECEIVE = 1 };

struct Ieee802154GtsCharacteristics
{
    uint8_t length = 0;
    Ieee802154GtsDirection direction = Ieee802154GtsDirection::DEVICE_TRANSMIT;
    bool allocate = false;

    [[nodiscard]] uint8_t encode() const {
        if (length > 15 || uint8_t(direction) > 1)
            throw cRuntimeError("Invalid IEEE 802.15.4 GTS characteristics");
        return length | (uint8_t(direction) << 4) | (allocate ? 0x20 : 0);
    }
    static Ieee802154GtsCharacteristics decode(uint8_t value) { return {uint8_t(value & 15), Ieee802154GtsDirection((value >> 4) & 1), bool(value & 0x20)}; }
};

struct Ieee802154GtsDescriptor
{
    uint16_t shortAddress = 0;
    uint8_t start = 0;
    uint8_t length = 0;
    Ieee802154GtsDirection direction = Ieee802154GtsDirection::DEVICE_TRANSMIT;
};

} // namespace inet
#endif
