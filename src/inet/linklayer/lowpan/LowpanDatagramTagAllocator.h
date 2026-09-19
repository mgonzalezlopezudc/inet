// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef __INET_LOWPANDATAGRAMTAGALLOCATOR_H
#define __INET_LOWPANDATAGRAMTAGALLOCATOR_H

#include <array>
#include <optional>
#include "inet/common/INETDefs.h"

namespace inet { namespace lowpan {

// Intentionally retained by the adaptation module across stop/start and crash.
class INET_API LowpanDatagramTagAllocator
{
  protected:
    std::array<simtime_t, 65536> guardedUntil{};
    uint16_t nextTag = 0;

  public:
    explicit LowpanDatagramTagAllocator(uint16_t initialTag = 0) : nextTag(initialTag) {}
    std::optional<uint16_t> allocate(simtime_t now, simtime_t reuseAfter);
};

} } // namespace inet::lowpan
#endif
