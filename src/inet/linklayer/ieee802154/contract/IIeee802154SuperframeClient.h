// SPDX-License-Identifier: LGPL-3.0-or-later
//
#ifndef __INET_IIEEE802154SUPERFRAMECLIENT_H
#define __INET_IIEEE802154SUPERFRAMECLIENT_H

#include "inet/clock/contract/ClockTime.h"

namespace inet {

/** Notification before the boundary, with the future local-clock target. */
class INET_API IIeee802154SuperframeClient
{
  public:
    virtual ~IIeee802154SuperframeClient() = default;
    virtual void handleSuperframeBoundary(clocktime_t boundary) = 0;
    virtual void handleSuperframeCapEnd() = 0;
    virtual void handleSyncWake() = 0;
    virtual void handleSyncDeadline() = 0;
};

} // namespace inet
#endif
