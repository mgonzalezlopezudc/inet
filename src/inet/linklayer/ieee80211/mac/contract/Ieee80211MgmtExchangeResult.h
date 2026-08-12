//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211MGMTEXCHANGERESULT_H
#define __INET_IEEE80211MGMTEXCHANGERESULT_H

#include <cstdint>

#include "inet/common/INETDefs.h"

namespace inet {
namespace ieee80211 {

enum class Ieee80211MgmtExchangeResultKind {
    ACKNOWLEDGED,
    RETRY_LIMIT_REACHED,
};

class INET_API Ieee80211MgmtExchangeResult final
{
  public:
    uint64_t transactionId;
    Ieee80211MgmtExchangeResultKind kind;

    Ieee80211MgmtExchangeResult(uint64_t transactionId,
            Ieee80211MgmtExchangeResultKind kind) :
        transactionId(transactionId), kind(kind) {}
};

class INET_API IIeee80211MgmtExchangeResultHandler
{
  public:
    virtual ~IIeee80211MgmtExchangeResultHandler() = default;
    virtual void handleIeee80211MgmtExchangeResult(
            const Ieee80211MgmtExchangeResult& result) = 0;
};

} // namespace ieee80211
} // namespace inet

#endif
