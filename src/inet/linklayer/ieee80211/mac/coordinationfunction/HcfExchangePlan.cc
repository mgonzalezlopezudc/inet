//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangePlan.h"

#include <set>

namespace inet {
namespace ieee80211 {

const std::array<HcfExchangeClass, 11>& getHcfExchangeClassOrder()
{
    static const std::array<HcfExchangeClass, 11> order = {
        HcfExchangeClass::FORCED_SINGLE_USER,
        HcfExchangeClass::HE_UL_TRIGGER,
        HcfExchangeClass::HE_SOUNDING,
        HcfExchangeClass::RECOVERY_SINGLE_USER,
        HcfExchangeClass::HE_DL_MULTIUSER,
        HcfExchangeClass::VHT_GROUP_MANAGEMENT,
        HcfExchangeClass::VHT_DL_MULTIUSER,
        HcfExchangeClass::VHT_SU_SOUNDING,
        HcfExchangeClass::HT_SOUNDING,
        HcfExchangeClass::SINGLE_USER,
        HcfExchangeClass::CHANNEL_RELEASE,
    };
    return order;
}

bool HcfExchangePlan::isComplete() const
{
    if (!transactionIdentity.isValid())
        return false;
    switch (exchangeClass) {
        case HcfExchangeClass::FORCED_SINGLE_USER:
        case HcfExchangeClass::HE_UL_TRIGGER:
        case HcfExchangeClass::HE_SOUNDING:
        case HcfExchangeClass::RECOVERY_SINGLE_USER:
        case HcfExchangeClass::HE_DL_MULTIUSER:
        case HcfExchangeClass::VHT_GROUP_MANAGEMENT:
        case HcfExchangeClass::VHT_DL_MULTIUSER:
        case HcfExchangeClass::VHT_SU_SOUNDING:
        case HcfExchangeClass::HT_SOUNDING:
        case HcfExchangeClass::SINGLE_USER:
        case HcfExchangeClass::CHANNEL_RELEASE:
            break;
        default:
            return false;
    }
    std::set<HcfPacketIdentity> identities;
    for (const auto& reservation : reservations)
        if (!reservation.isValid() ||
                !identities.insert(reservation.getPacketIdentity()).second)
            return false;
    for (auto identity : expectedResponseIdentities)
        if (!identity.isValid())
            return false;
    return true;
}

PreparedHcfExchange::PreparedHcfExchange(const HcfExchangePlan& plan,
        std::unique_ptr<IHcfExchangeTransaction> transaction) :
    plan(plan), transaction(std::move(transaction))
{
    if (!plan.isComplete())
        throw cRuntimeError("Cannot prepare an incomplete HCF exchange plan");
    if (this->transaction == nullptr)
        throw cRuntimeError("Cannot prepare an HCF exchange without transaction commands");
}

PreparedHcfExchange::~PreparedHcfExchange()
{
    if (state == State::PREPARED || state == State::VALIDATED) {
        state = State::ROLLED_BACK;
        transaction->rollback(plan);
    }
}

void PreparedHcfExchange::requireIdentity(
        HcfTransactionIdentity transactionIdentity) const
{
    if (!(transactionIdentity == plan.getTransactionIdentity()))
        throw cRuntimeError("Stale HCF transaction token or generation");
}

void PreparedHcfExchange::requireState(const char *operation, State expected) const
{
    if (state != expected)
        throw cRuntimeError("Invalid HCF transaction operation '%s' in state %d",
                operation, static_cast<int>(state));
}

bool PreparedHcfExchange::validate(HcfTransactionIdentity transactionIdentity,
        HcfExchangeRejection& rejection)
{
    if (!(transactionIdentity == plan.getTransactionIdentity())) {
        rejection = HcfExchangeRejection(HcfExchangeRejectionCode::STALE_TRANSACTION,
                plan.getExchangeClass(), transactionIdentity,
                "transaction token or generation is stale");
        return false;
    }
    requireState("validate", State::PREPARED);
    rejection = transaction->validate(plan);
    if (rejection.isRejected()) {
        state = State::ROLLED_BACK;
        transaction->rollback(plan);
        return false;
    }
    state = State::VALIDATED;
    return true;
}

void PreparedHcfExchange::commit(HcfTransactionIdentity transactionIdentity)
{
    requireIdentity(transactionIdentity);
    requireState("commit", State::VALIDATED);
    try {
        transaction->commit(plan);
        state = State::COMMITTED;
    }
    catch (...) {
        state = State::ROLLED_BACK;
        transaction->rollback(plan);
        throw;
    }
}

void PreparedHcfExchange::rollback(HcfTransactionIdentity transactionIdentity)
{
    requireIdentity(transactionIdentity);
    if (state != State::PREPARED && state != State::VALIDATED)
        throw cRuntimeError("Invalid HCF transaction operation 'rollback' in state %d",
                static_cast<int>(state));
    state = State::ROLLED_BACK;
    transaction->rollback(plan);
}

void PreparedHcfExchange::complete(HcfTransactionIdentity transactionIdentity,
        const HcfExchangeResult& result)
{
    requireIdentity(transactionIdentity);
    requireState("complete", State::COMMITTED);
    if (!(result.getTransactionIdentity() == plan.getTransactionIdentity()) ||
            result.getExchangeClass() != plan.getExchangeClass())
        throw cRuntimeError("HCF exchange result does not identify the committed transaction");
    state = State::COMPLETING;
    try {
        transaction->complete(plan, result);
        state = State::COMPLETED;
    }
    catch (...) {
        // Commit already transferred ownership. Retrying an ambiguous terminal
        // callback could repeat external effects, while rollback is no longer
        // valid. Keep a distinct terminal failure state and propagate once.
        state = State::COMPLETION_FAILED;
        throw;
    }
}

} // namespace ieee80211
} // namespace inet
