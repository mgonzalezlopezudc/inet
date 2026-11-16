// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanRecoveryTagAllocator.h"
namespace inet { namespace lowpan {
std::optional<uint8_t> LowpanRecoveryTagAllocator::allocate(simtime_t now, simtime_t deadline, uint8_t preferred)
{
    if (deadline <= now) throw cRuntimeError("Recovery tag guard must end after allocation");
    for (int offset = 0; offset < 256; offset++) {
        uint8_t tag = preferred + offset;
        if (guardedUntil[tag] <= now) { guardedUntil[tag] = deadline; return tag; }
    }
    return std::nullopt;
}
} }
