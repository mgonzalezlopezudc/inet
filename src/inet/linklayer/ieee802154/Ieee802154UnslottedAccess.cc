// SPDX-License-Identifier: LGPL-3.0-or-later
//
#include "inet/linklayer/ieee802154/Ieee802154UnslottedAccess.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/lifecycle/NodeStatus.h"
#include "inet/common/InitStages.h"

namespace inet {
Define_Module(Ieee802154UnslottedAccess);

Ieee802154UnslottedAccess::~Ieee802154UnslottedAccess() { cancelAndDelete(backoffTimer); }

int Ieee802154UnslottedAccess::numInitStages() const { return NUM_INIT_STAGES; }

void Ieee802154UnslottedAccess::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL) {
        client.reference(this, "clientModule", true);
        backoffTimer = new cMessage("backoffTimer");
        slotsSignal = registerSignal("backoffSlots");
        countSignal = registerSignal("backoffCount");
        exponentSignal = registerSignal("backoffExponent");
    }
    else if (stage == INITSTAGE_LINK_LAYER) {
        auto node = findContainingNode(this);
        auto status = node ? check_and_cast_nullable<NodeStatus *>(node->getSubmodule("status")) : nullptr;
        started = !status || status->getState() == NodeStatus::UP;
    }
}

Ieee802154MacServiceAdmission Ieee802154UnslottedAccess::start(uint64_t id, const Ieee802154AccessRequest& request)
{
    Enter_Method("start");
    using Admission = Ieee802154MacServiceAdmission;
    if (!id || id <= lastId)
        return Admission::INVALID_REQUEST;
    lastId = id;
    if (!started || !client)
        return Admission::UNAVAILABLE;
    if (attempt)
        return Admission::BUSY;
    if (request.minBe > request.maxBe || request.maxBe < 3 || request.maxBe > 8 || request.maxBackoffs > 5 || request.unitBackoffPeriod <= SIMTIME_ZERO)
        return Admission::INVALID_REQUEST;
    attempt = Attempt{id, request, 0, request.minBe};
    backoff();
    return Admission::ACCEPTED;
}

void Ieee802154UnslottedAccess::backoff()
{
    // IEEE 802.15.4-2024, 6.3.2.1/Figure 6-2: random [0, 2^BE - 1].
    int slots = intuniform(0, (1 << attempt->be) - 1, par("backoffRng"));
    auto requestId = attempt->id;
    auto delay = attempt->parameters.unitBackoffPeriod * slots;
    auto nb = attempt->nb;
    auto be = attempt->be;
    emit(countSignal, long(nb));
    emit(exponentSignal, long(be));
    emit(slotsSignal, long(slots));
    if (!attempt || attempt->id != requestId)
        return;
    if (slots == 0)
        requestCca();
    else
        scheduleAfter(delay, backoffTimer);
}

void Ieee802154UnslottedAccess::requestCca()
{
    if (!client) {
        attempt.reset();
        return;
    }
    attempt->waitingForCca = true;
    client->handleCcaRequest(attempt->id);
}

void Ieee802154UnslottedAccess::handleMessage(cMessage *message)
{
    if (message != backoffTimer)
        throw cRuntimeError("Unexpected unslotted access message");
    if (attempt)
        requestCca();
}

void Ieee802154UnslottedAccess::complete(const Ieee802154MacServiceResult& result)
{
    auto completed = *attempt;
    attempt.reset();
    cancelEvent(backoffTimer);
    if (client)
        client->handleAccessConfirm(completed.id, result, completed.nb);
}

void Ieee802154UnslottedAccess::handleCcaResult(uint64_t id, bool idle)
{
    Enter_Method("handleCcaResult");
    if (!attempt || attempt->id != id || !attempt->waitingForCca)
        return;
    attempt->waitingForCca = false;
    if (idle)
        complete(Ieee802154MacStatus::SUCCESS);
    else {
        attempt->nb++;
        attempt->be = std::min<uint8_t>(attempt->be + 1, attempt->parameters.maxBe);
        if (attempt->nb > attempt->parameters.maxBackoffs)
            complete(Ieee802154MacStatus::CHANNEL_ACCESS_FAILURE);
        else
            backoff();
    }
}

Ieee802154MacServiceCancelResult Ieee802154UnslottedAccess::cancel(uint64_t id, Ieee802154MacServiceAbortReason reason)
{
    Enter_Method("cancel");
    if (!attempt || attempt->id != id)
        return Ieee802154MacServiceCancelResult::NOT_PENDING;
    complete(reason);
    return Ieee802154MacServiceCancelResult::CANCELLED;
}

bool Ieee802154UnslottedAccess::handleOperationStage(LifecycleOperation *operation, IDoneCallback *)
{
    Enter_Method("handleOperationStage");
    if (dynamic_cast<ModuleStartOperation *>(operation)) {
        if (operation->getCurrentStage() == ModuleStartOperation::STAGE_LOCAL)
            started = true;
    }
    else if (dynamic_cast<ModuleStopOperation *>(operation) || dynamic_cast<ModuleCrashOperation *>(operation)) {
        if (operation->getCurrentStage() == 0) {
            started = false;
            if (attempt)
                complete(Ieee802154MacServiceAbortReason::STOPPED);
        }
    }
    else
        throw cRuntimeError("Unsupported access lifecycle operation");
    return true;
}
}
