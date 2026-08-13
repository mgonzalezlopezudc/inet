//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangeSelector.h"

#include <algorithm>
#include <set>

#include "inet/linklayer/ieee80211/mac/contract/IHcfExchangeProvider.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfContext.h"

namespace inet {
namespace ieee80211 {

HcfExchangeSelector::HcfExchangeSelector(
        const std::vector<HcfExchangeProviderDescriptor>& providerDescriptors) :
    providerDescriptors(providerDescriptors)
{
    const auto& order = getHcfExchangeClassOrder();
    auto rank = [&order] (HcfExchangeClass exchangeClass) {
        auto it = std::find(order.begin(), order.end(), exchangeClass);
        if (it == order.end())
            throw cRuntimeError("HCF selector received an unknown exchange class");
        return std::distance(order.begin(), it);
    };
    std::stable_sort(this->providerDescriptors.begin(),
            this->providerDescriptors.end(), [&rank] (const auto& left, const auto& right) {
                return rank(left.getExchangeClass()) < rank(right.getExchangeClass());
            });
    std::set<HcfExchangeClass> classes;
    for (const auto& descriptor : this->providerDescriptors)
        if (!classes.insert(descriptor.getExchangeClass()).second)
            throw cRuntimeError("HCF selector received duplicate providers for one exchange class");
}

HcfExchangeSelector::~HcfExchangeSelector()
{
    abortActiveExchange(HcfExchangeAbortReason::CHANNEL_LOST);
}

HcfExchangeClass HcfExchangeSelector::selectAndCommit(const HcfContext& context,
        HcfTransactionIdentity transactionIdentity)
{
    if (!transactionIdentity.isValid())
        throw cRuntimeError("HCF exchange selection requires a valid transaction identity");
    if (activeExchange != nullptr)
        throw cRuntimeError("Cannot select an HCF exchange while another exchange is active");

    for (const auto& descriptor : providerDescriptors) {
        HcfExchangeRejection rejection;
        auto prepared = descriptor.getExecutableProvider().prepareExchange(
                context, transactionIdentity, rejection);
        if (prepared == nullptr) {
            if (!rejection.isRejected())
                throw cRuntimeError("HCF exchange provider returned neither a prepared exchange nor a rejection");
            continue;
        }
        const auto& plan = prepared->getPlan();
        if (plan.getExchangeClass() != descriptor.getExchangeClass())
            throw cRuntimeError("Prepared HCF exchange class does not match its provider descriptor");
        if (!(plan.getTransactionIdentity() == transactionIdentity))
            throw cRuntimeError("Prepared HCF exchange does not use the active engine transaction identity");
        if (!prepared->validate(transactionIdentity, rejection))
            continue;
        activeExchange = std::move(prepared);
        auto selectedClass = activeExchange->getPlan().getExchangeClass();
        commitInProgress = true;
        try {
            activeExchange->commit(transactionIdentity);
            commitInProgress = false;
        }
        catch (...) {
            commitInProgress = false;
            activeExchange.reset();
            pendingTerminalReason.reset();
            throw;
        }
        if (pendingTerminalReason.has_value()) {
            auto reason = *pendingTerminalReason;
            pendingTerminalReason.reset();
            finishActiveExchange(reason);
        }
        return selectedClass;
    }

    throw cRuntimeError("No HCF exchange provider prepared a transaction for the channel grant");
}

void HcfExchangeSelector::finishActiveExchange(
        HcfExchangeAbortReason abortReason)
{
    ASSERT(activeExchange != nullptr);
    auto identity = activeExchange->getPlan().getTransactionIdentity();
    auto exchangeClass = activeExchange->getPlan().getExchangeClass();
    HcfExchangeResult result(exchangeClass, identity, {}, std::nullopt, {},
            abortReason, HcfTerminalOwnershipAction::NONE);
    activeExchange->complete(identity, result);
    activeExchange.reset();
}

void HcfExchangeSelector::exchangeTerminated(
        HcfTransactionIdentity transactionIdentity,
        HcfExchangeAbortReason abortReason)
{
    if (activeExchange == nullptr ||
            !(activeExchange->getPlan().getTransactionIdentity() == transactionIdentity))
        throw cRuntimeError("HCF exchange terminal callback has stale transaction identity");
    if (commitInProgress) {
        if (pendingTerminalReason.has_value())
            throw cRuntimeError("HCF exchange received duplicate terminal callbacks during commit");
        pendingTerminalReason = abortReason;
        return;
    }
    finishActiveExchange(abortReason);
}

void HcfExchangeSelector::abortActiveExchange(
        HcfExchangeAbortReason abortReason)
{
    if (activeExchange == nullptr)
        return;
    if (commitInProgress) {
        pendingTerminalReason = abortReason;
        return;
    }
    finishActiveExchange(abortReason);
}

} // namespace ieee80211
} // namespace inet
