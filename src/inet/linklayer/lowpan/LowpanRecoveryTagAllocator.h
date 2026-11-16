// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANRECOVERYTAGALLOCATOR_H
#define __INET_LOWPANRECOVERYTAGALLOCATOR_H
#include <array>
#include <optional>
#include "inet/common/INETDefs.h"
namespace inet { namespace lowpan {
// One pool per transmitting native identity, shared by local sources and relay swaps.
class INET_API LowpanRecoveryTagAllocator
{
  protected:
    std::array<simtime_t, 256> guardedUntil{};
  public:
    std::optional<uint8_t> allocate(simtime_t now, simtime_t deadline, uint8_t preferred);
};
} }
#endif
