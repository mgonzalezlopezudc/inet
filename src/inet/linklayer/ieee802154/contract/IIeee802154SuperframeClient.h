// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IIEEE802154SUPERFRAMECLIENT_H
#define __INET_IIEEE802154SUPERFRAMECLIENT_H

#include "inet/clock/contract/ClockTime.h"

namespace inet {

/** Direct notification at a controller-owned superframe boundary. */
class INET_API IIeee802154SuperframeClient
{
  public:
    virtual ~IIeee802154SuperframeClient() = default;
    virtual void handleSuperframeBoundary(clocktime_t boundary) = 0;
};

} // namespace inet
#endif
