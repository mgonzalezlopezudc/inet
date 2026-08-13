//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HCFEXCHANGEPLAN_H
#define __INET_HCFEXCHANGEPLAN_H

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfContext.h"

namespace inet {
namespace ieee80211 {

INET_API const std::array<HcfExchangeClass, 11>& getHcfExchangeClassOrder();

class INET_API HcfTransactionToken
{
  private:
    uint64_t value = 0;

  public:
    HcfTransactionToken() = default;
    explicit HcfTransactionToken(uint64_t value) : value(value) {}

    bool isValid() const { return value != 0; }
    uint64_t getValue() const { return value; }
    bool operator==(const HcfTransactionToken& other) const { return value == other.value; }
};

class INET_API HcfTransactionGeneration
{
  private:
    uint64_t value = 0;

  public:
    HcfTransactionGeneration() = default;
    explicit HcfTransactionGeneration(uint64_t value) : value(value) {}

    bool isValid() const { return value != 0; }
    uint64_t getValue() const { return value; }
    bool operator==(const HcfTransactionGeneration& other) const { return value == other.value; }
};

class INET_API HcfTransactionIdentity
{
  private:
    HcfTransactionToken token;
    HcfTransactionGeneration generation;

  public:
    HcfTransactionIdentity() = default;
    HcfTransactionIdentity(HcfTransactionToken token,
            HcfTransactionGeneration generation) : token(token), generation(generation) {}

    HcfTransactionToken getToken() const { return token; }
    HcfTransactionGeneration getGeneration() const { return generation; }
    bool isValid() const { return token.isValid() && generation.isValid(); }
    bool operator==(const HcfTransactionIdentity& other) const
    {
        return token == other.token && generation == other.generation;
    }
};

enum class HcfExchangeRejectionCode {
    NONE,
    INCOMPLETE_CONTEXT,
    INCOMPLETE_PLAN,
    UNSUPPORTED_EXCHANGE,
    NO_ELIGIBLE_PACKET,
    STALE_TRANSACTION,
    STALE_QUEUE_TOKEN,
    STALE_ASSOCIATION_EPOCH,
    VALIDATION_FAILED,
    INVALID_TRANSACTION_STATE,
};

class INET_API HcfExchangeRejection
{
  private:
    HcfExchangeRejectionCode code = HcfExchangeRejectionCode::NONE;
    HcfExchangeClass exchangeClass = HcfExchangeClass::SINGLE_USER;
    HcfTransactionIdentity transactionIdentity;
    std::string detail;

  public:
    HcfExchangeRejection() = default;
    HcfExchangeRejection(HcfExchangeRejectionCode code,
            HcfExchangeClass exchangeClass,
            HcfTransactionIdentity transactionIdentity,
            const std::string& detail) : code(code), exchangeClass(exchangeClass),
        transactionIdentity(transactionIdentity), detail(detail) {}

    HcfExchangeRejectionCode getCode() const { return code; }
    HcfExchangeClass getExchangeClass() const { return exchangeClass; }
    HcfTransactionIdentity getTransactionIdentity() const { return transactionIdentity; }
    const std::string& getDetail() const { return detail; }
    bool isRejected() const { return code != HcfExchangeRejectionCode::NONE; }
};

/** Ordered identity of one queue reservation; it conveys no Packet ownership. */
class INET_API HcfReservationIdentity
{
  private:
    HcfPacketIdentity packetIdentity;
    HcfQueueToken sourceQueueToken;
    uint64_t associationEpoch = 0;

  public:
    HcfReservationIdentity() = default;
    HcfReservationIdentity(HcfPacketIdentity packetIdentity,
            HcfQueueToken sourceQueueToken, uint64_t associationEpoch) :
        packetIdentity(packetIdentity), sourceQueueToken(sourceQueueToken),
        associationEpoch(associationEpoch) {}

    HcfPacketIdentity getPacketIdentity() const { return packetIdentity; }
    HcfQueueToken getSourceQueueToken() const { return sourceQueueToken; }
    uint64_t getAssociationEpoch() const { return associationEpoch; }
    bool isValid() const
    {
        return packetIdentity.isValid() && sourceQueueToken.isValid() &&
                associationEpoch != 0;
    }
};

/** Immutable provider plan and ordered packet reservations. */
class INET_API HcfExchangePlan
{
  private:
    HcfExchangeClass exchangeClass = HcfExchangeClass::SINGLE_USER;
    HcfTransactionIdentity transactionIdentity;
    std::vector<HcfReservationIdentity> reservations;
    std::vector<HcfPacketIdentity> expectedResponseIdentities;

  public:
    HcfExchangePlan() = default;
    HcfExchangePlan(HcfExchangeClass exchangeClass,
            HcfTransactionIdentity transactionIdentity,
            const std::vector<HcfReservationIdentity>& reservations,
            const std::vector<HcfPacketIdentity>& expectedResponseIdentities = {}) :
        exchangeClass(exchangeClass), transactionIdentity(transactionIdentity),
        reservations(reservations),
        expectedResponseIdentities(expectedResponseIdentities) {}

    HcfExchangeClass getExchangeClass() const { return exchangeClass; }
    HcfTransactionIdentity getTransactionIdentity() const { return transactionIdentity; }
    const std::vector<HcfReservationIdentity>& getReservations() const { return reservations; }
    const std::vector<HcfPacketIdentity>& getExpectedResponseIdentities() const { return expectedResponseIdentities; }
    bool isComplete() const;
};

enum class HcfExchangeAbortReason {
    NONE,
    RESPONSE_TIMEOUT,
    VALIDATION_FAILURE,
    CHANNEL_LOST,
    PROVIDER_ABORT,
};

enum class HcfTerminalOwnershipAction {
    NONE,
    RETAINED_BY_QUEUE,
    TRANSFERRED_TO_IN_PROGRESS,
    RETIRED,
    RETURNED_TO_QUEUE,
};

class INET_API HcfUserExchangeResult
{
  private:
    MacAddress peer;
    std::vector<HcfPacketIdentity> transmittedPacketIdentities;
    bool responseReceived = false;

  public:
    HcfUserExchangeResult() = default;
    HcfUserExchangeResult(const MacAddress& peer,
            const std::vector<HcfPacketIdentity>& transmittedPacketIdentities,
            bool responseReceived) : peer(peer),
        transmittedPacketIdentities(transmittedPacketIdentities),
        responseReceived(responseReceived) {}

    const MacAddress& getPeer() const { return peer; }
    const std::vector<HcfPacketIdentity>& getTransmittedPacketIdentities() const { return transmittedPacketIdentities; }
    bool isResponseReceived() const { return responseReceived; }
};

/** Immutable terminal result; packet identities are borrowed facts, not ownership. */
class INET_API HcfExchangeResult
{
  private:
    HcfExchangeClass exchangeClass = HcfExchangeClass::SINGLE_USER;
    HcfTransactionIdentity transactionIdentity;
    std::vector<HcfPacketIdentity> transmittedPacketIdentities;
    std::optional<HcfPacketIdentity> responseIdentity;
    std::vector<HcfUserExchangeResult> userResults;
    HcfExchangeAbortReason abortReason = HcfExchangeAbortReason::NONE;
    HcfTerminalOwnershipAction terminalOwnershipAction = HcfTerminalOwnershipAction::NONE;

  public:
    HcfExchangeResult() = default;
    HcfExchangeResult(HcfExchangeClass exchangeClass,
            HcfTransactionIdentity transactionIdentity,
            const std::vector<HcfPacketIdentity>& transmittedPacketIdentities,
            const std::optional<HcfPacketIdentity>& responseIdentity,
            const std::vector<HcfUserExchangeResult>& userResults,
            HcfExchangeAbortReason abortReason,
            HcfTerminalOwnershipAction terminalOwnershipAction) :
        exchangeClass(exchangeClass), transactionIdentity(transactionIdentity),
        transmittedPacketIdentities(transmittedPacketIdentities),
        responseIdentity(responseIdentity), userResults(userResults),
        abortReason(abortReason), terminalOwnershipAction(terminalOwnershipAction) {}

    HcfExchangeClass getExchangeClass() const { return exchangeClass; }
    HcfTransactionIdentity getTransactionIdentity() const { return transactionIdentity; }
    const std::vector<HcfPacketIdentity>& getTransmittedPacketIdentities() const { return transmittedPacketIdentities; }
    const std::optional<HcfPacketIdentity>& getResponseIdentity() const { return responseIdentity; }
    const std::vector<HcfUserExchangeResult>& getUserResults() const { return userResults; }
    HcfExchangeAbortReason getAbortReason() const { return abortReason; }
    HcfTerminalOwnershipAction getTerminalOwnershipAction() const { return terminalOwnershipAction; }
};

/** Provider-owned state mutation commands used by one prepared exchange. */
class INET_API IHcfExchangeTransaction
{
  public:
    virtual ~IHcfExchangeTransaction() {}

    virtual HcfExchangeRejection validate(const HcfExchangePlan& plan) = 0;
    virtual void commit(const HcfExchangePlan& plan) = 0;
    virtual void rollback(const HcfExchangePlan& plan) noexcept = 0;
    virtual void complete(const HcfExchangePlan& plan,
            const HcfExchangeResult& result) = 0;
};

/**
 * Move-disabled lifecycle guard for one provider transaction. Destruction
 * rolls back every prepared or validated transaction that did not commit.
 */
class INET_API PreparedHcfExchange
{
  public:
    enum class State {
        PREPARED,
        VALIDATED,
        COMMITTED,
        COMPLETING,
        COMPLETION_FAILED,
        ROLLED_BACK,
        COMPLETED,
    };

  private:
    HcfExchangePlan plan;
    std::unique_ptr<IHcfExchangeTransaction> transaction;
    State state = State::PREPARED;

    void requireIdentity(HcfTransactionIdentity transactionIdentity) const;
    void requireState(const char *operation, State expected) const;

  public:
    PreparedHcfExchange(const HcfExchangePlan& plan,
            std::unique_ptr<IHcfExchangeTransaction> transaction);
    ~PreparedHcfExchange();

    PreparedHcfExchange(const PreparedHcfExchange&) = delete;
    PreparedHcfExchange& operator=(const PreparedHcfExchange&) = delete;

    const HcfExchangePlan& getPlan() const { return plan; }
    State getState() const { return state; }

    bool validate(HcfTransactionIdentity transactionIdentity,
            HcfExchangeRejection& rejection);
    void commit(HcfTransactionIdentity transactionIdentity);
    void rollback(HcfTransactionIdentity transactionIdentity);
    /**
     * Invokes the terminal callback at most once. If it throws after commit,
     * the exchange enters COMPLETION_FAILED: neither retry nor rollback is
     * permitted because external terminal effects may already have occurred.
     */
    void complete(HcfTransactionIdentity transactionIdentity,
            const HcfExchangeResult& result);
};

} // namespace ieee80211
} // namespace inet

#endif
