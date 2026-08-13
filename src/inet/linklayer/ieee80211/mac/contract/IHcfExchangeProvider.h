//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IHCFEXCHANGEPROVIDER_H
#define __INET_IHCFEXCHANGEPROVIDER_H

#include <memory>

#include "inet/common/INETDefs.h"

namespace inet {
namespace ieee80211 {

class HcfContext;
class HcfExchangeRejection;
class HcfTransactionIdentity;
class PreparedHcfExchange;
enum class HcfExchangeClass;

/** Narrow feature contract for side-effect-free probing and explicit preparation. */
class INET_API IHcfExchangeProvider
{
  public:
    virtual ~IHcfExchangeProvider() {}

    virtual HcfExchangeClass getExchangeClass() const = 0;

    /**
     * Returns a provider-owned prepared transaction or a typed rejection.
     * Probing must not dequeue packets, change BA state, schedule, or consume RNG.
     */
    virtual std::unique_ptr<PreparedHcfExchange> prepareExchange(
            const HcfContext& context,
            HcfTransactionIdentity transactionIdentity,
            HcfExchangeRejection& rejection) = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
