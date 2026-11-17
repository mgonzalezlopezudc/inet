// SPDX-License-Identifier: LGPL-3.0-or-later
#include "inet/linklayer/lowpan/LowpanDatagramTagAllocator.h"

namespace inet { namespace lowpan {

std::optional<uint16_t> LowpanDatagramTagAllocator::allocate(simtime_t now, simtime_t reuseAfter)
{
    if (reuseAfter <= now)
        throw cRuntimeError("LoWPAN datagram tag reuse guard must extend beyond allocation");
    if (guardedUntil[nextTag] > now)
        return std::nullopt;
    uint16_t result = nextTag++;
    guardedUntil[result] = reuseAfter;
    return result;
}

std::optional<uint16_t> LowpanDatagramTagAllocator::allocate(simtime_t now, simtime_t reuseAfter, uint16_t preferred)
{
    if (reuseAfter <= now)
        throw cRuntimeError("LoWPAN datagram tag reuse guard must extend beyond allocation");
    for (unsigned int i = 0; i < 65536; i++) {
        uint16_t candidate = uint16_t(preferred + i);
        if (guardedUntil[candidate] <= now) {
            guardedUntil[candidate] = reuseAfter;
            return candidate;
        }
    }
    return std::nullopt;
}

} } // namespace inet::lowpan
