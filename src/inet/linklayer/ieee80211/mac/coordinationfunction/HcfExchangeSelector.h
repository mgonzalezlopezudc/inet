//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HCFEXCHANGESELECTOR_H
#define __INET_HCFEXCHANGESELECTOR_H

#include <memory>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee80211/mac/contract/IHcfFeatureSet.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangePlan.h"

namespace inet {
namespace ieee80211 {

class HcfContext;

/**
 * Deterministically selects and owns one prepared HCF exchange at a time.
 * Provider probing is side-effect free; only the selected transaction is
 * committed, and it receives exactly one terminal result.
 */
class INET_API HcfExchangeSelector
{
  private:
    std::vector<HcfExchangeProviderDescriptor> providerDescriptors;
    std::unique_ptr<PreparedHcfExchange> activeExchange;
    bool commitInProgress = false;
    std::optional<HcfExchangeAbortReason> pendingTerminalReason;

    void finishActiveExchange(HcfExchangeAbortReason abortReason);

  public:
    explicit HcfExchangeSelector(
            const std::vector<HcfExchangeProviderDescriptor>& providerDescriptors);
    ~HcfExchangeSelector();

    HcfExchangeSelector(const HcfExchangeSelector&) = delete;
    HcfExchangeSelector& operator=(const HcfExchangeSelector&) = delete;

    HcfExchangeClass selectAndCommit(const HcfContext& context,
            HcfTransactionIdentity transactionIdentity);
    void exchangeTerminated(HcfTransactionIdentity transactionIdentity,
            HcfExchangeAbortReason abortReason);
    void abortActiveExchange(HcfExchangeAbortReason abortReason);

    bool hasActiveExchange() const { return activeExchange != nullptr; }
    HcfTransactionIdentity getActiveTransactionIdentity() const
        { return activeExchange == nullptr ? HcfTransactionIdentity() :
                activeExchange->getPlan().getTransactionIdentity(); }
    std::optional<HcfExchangeClass> getActiveExchangeClass() const
        { return activeExchange == nullptr ? std::nullopt :
                std::optional<HcfExchangeClass>(activeExchange->getPlan().getExchangeClass()); }
};

} // namespace ieee80211
} // namespace inet

#endif
