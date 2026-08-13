//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IHCFEXCHANGECALLBACK_H
#define __INET_IHCFEXCHANGECALLBACK_H

#include "inet/common/INETDefs.h"

namespace inet {
namespace ieee80211 {

class HcfExchangeResult;
class HcfPreparedTransmission;
class HcfResponseResult;
class HcfResponseTimeout;
class HcfTransmissionResult;

/**
 * HCF-only typed event sink; it does not widen the generic DCF callback.
 * Every const-reference argument is borrowed for the duration of the call and
 * must not be retained by reference. Implementations may safely copy the value.
 */
class INET_API IHcfExchangeCallback
{
  public:
    virtual ~IHcfExchangeCallback() {}

    virtual void transmissionRequested(const HcfPreparedTransmission& transmission) = 0;
    virtual void transmissionCompleted(const HcfTransmissionResult& result) = 0;
    virtual void responseReceived(const HcfResponseResult& result) = 0;
    virtual void responseTimedOut(const HcfResponseTimeout& timeout) = 0;
    virtual void exchangeCompleted(const HcfExchangeResult& result) = 0;
    virtual void exchangeAborted(const HcfExchangeResult& result) = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
