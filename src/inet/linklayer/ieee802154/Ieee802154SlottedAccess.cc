// SPDX-License-Identifier: LGPL-3.0-or-later
//
#include "inet/linklayer/ieee802154/Ieee802154SlottedAccess.h"

#include <algorithm>

#include "inet/common/InitStages.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/lifecycle/NodeStatus.h"

namespace inet {

Define_Module(Ieee802154SlottedAccess);

Ieee802154SlottedAccess::~Ieee802154SlottedAccess()
{
    cancelTimer();
    delete timer;
}

int Ieee802154SlottedAccess::numInitStages() const { return NUM_INIT_STAGES; }

void Ieee802154SlottedAccess::initialize(int stage)
{
    ClockUserModuleMixin<SimpleModule>::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        client.reference(this, "clientModule", true);
        timer = new ClockEvent("slottedBackoff");
        slotsSignal = registerSignal("backoffSlots");
        countSignal = registerSignal("backoffCount");
        exponentSignal = registerSignal("backoffExponent");
        contentionWindowSignal = registerSignal("contentionWindow");
    }
    else if (stage == INITSTAGE_LINK_LAYER) {
        auto node = findContainingNode(this);
        auto status = node ? check_and_cast_nullable<NodeStatus *>(node->getSubmodule("status")) : nullptr;
        started = !status || status->getState() == NodeStatus::UP;
    }
}

void Ieee802154SlottedAccess::cancelTimer()
{
    if (timer && isScheduledClockEvent(timer))
        cancelClockEvent(timer);
}

bool Ieee802154SlottedAccess::validWindow(const Ieee802154AccessWindow& w) const
{
    return w.start < w.end && w.physicalCapStart < w.physicalCapEnd && (w.bleStart.has_value() == w.bleEnd.has_value()) &&
           (!w.bleStart || (w.start <= *w.bleStart && *w.bleStart < *w.bleEnd));
}

Ieee802154MacServiceAdmission Ieee802154SlottedAccess::start(uint64_t id, const Ieee802154AccessRequest& request)
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
    if (!request.window || !validWindow(*request.window) || request.minBe > request.maxBe || request.maxBe < 3 || request.maxBe > 8 ||
        request.maxBackoffs > 5 || request.unitBackoffPeriod <= SIMTIME_ZERO || request.ccaDuration <= SIMTIME_ZERO ||
        request.turnaroundTime < SIMTIME_ZERO || request.exchangeDuration <= SIMTIME_ZERO ||
        request.ifsDuration < SIMTIME_ZERO || request.ifsDuration > request.exchangeDuration)
        return Admission::INVALID_REQUEST;
    attempt = Attempt{id, request, *request.window};
    attempt->be = request.window->bleEnd ? std::min<uint8_t>(2, request.minBe) : request.minBe;
    if (request.exchangeDuration > request.window->physicalCapEnd - request.window->physicalCapStart) {
        complete(Ieee802154MacStatus::FRAME_TOO_LONG);
        return Admission::ACCEPTED;
    }
    advance();
    return Admission::ACCEPTED;
}

Ieee802154MacServiceAdmission Ieee802154SlottedAccess::updateWindow(uint64_t id, const Ieee802154AccessWindow& window)
{
    Enter_Method("updateWindow");
    using Admission = Ieee802154MacServiceAdmission;
    if (!attempt || attempt->id != id || !attempt->deferred)
        return Admission::INVALID_REQUEST;
    if (!validWindow(window) || window.epoch == attempt->window.epoch)
        return Admission::INVALID_REQUEST;
    attempt->window = window;
    attempt->deferred = false;
    if (attempt->parameters.exchangeDuration > window.physicalCapEnd - window.physicalCapStart) {
        complete(Ieee802154MacStatus::FRAME_TOO_LONG);
        return Admission::ACCEPTED;
    }
    advance();
    return Admission::ACCEPTED;
}

clocktime_t Ieee802154SlottedAccess::boundaryAtOrAfter(clocktime_t earliest, simtime_t physicalEarliest) const
{
    const auto& w = attempt->window;
    auto period = SIMTIME_AS_CLOCKTIME(attempt->parameters.unitBackoffPeriod);
    auto now = getClockTime();
    if (earliest < now)
        earliest = now;
    if (physicalEarliest > simTime())
        earliest = std::max(earliest, computeClockTimeFromSimTime(physicalEarliest));
    auto difference = CLOCKTIME_AS_SIMTIME(earliest - w.backoffOrigin).raw();
    auto ticks = CLOCKTIME_AS_SIMTIME(period).raw();
    int64_t count = difference > 0 ? (difference + ticks - 1) / ticks : 0;
    auto boundary = w.backoffOrigin + period * count;
    while (boundary < now || computeSimTimeFromClockTime(boundary) < physicalEarliest)
        boundary += period;
    return boundary;
}

bool Ieee802154SlottedAccess::canFit(clocktime_t firstCca) const
{
    const auto& a = *attempt;
    const auto& w = a.window;
    if (firstCca >= w.end || (w.bleEnd && firstCca >= *w.bleEnd) || w.end < getClockTime())
        return false;
    auto first = computeSimTimeFromClockTime(firstCca);
    auto second = firstCca;
    if (a.cw == 2) {
        auto secondEarliest = first + a.parameters.ccaDuration;
        second = boundaryAtOrAfter(computeClockTimeFromSimTime(secondEarliest), secondEarliest);
    }
    if (second >= w.end || (w.bleEnd && second >= *w.bleEnd))
        return false;
    auto ready = computeSimTimeFromClockTime(second) + a.parameters.ccaDuration + a.parameters.turnaroundTime;
    auto tx = boundaryAtOrAfter(computeClockTimeFromSimTime(ready), ready);
    auto physicalTx = computeSimTimeFromClockTime(tx);
    return tx < w.end && (!w.bleEnd || tx < *w.bleEnd) &&
           physicalTx + a.parameters.exchangeDuration <= w.physicalCapEnd &&
           (!w.latestCompletion || physicalTx + a.parameters.exchangeDuration - a.parameters.ifsDuration <= *w.latestCompletion);
}

void Ieee802154SlottedAccess::advance()
{
    auto id = attempt->id;
    auto& a = *attempt;
    const auto& w = a.window;
    if (w.end <= getClockTime()) {
        a.deferred = true;
        return;
    }
    auto beginning = w.bleStart ? *w.bleStart : w.start;
    auto first = boundaryAtOrAfter(beginning, std::max(simTime(), w.notBefore));
    auto period = SIMTIME_AS_CLOCKTIME(a.parameters.unitBackoffPeriod);
    auto limit = w.bleEnd ? std::min(w.end, *w.bleEnd) : w.end;
    if (first >= limit) {
        a.deferred = true;
        return;
    }
    if (a.needsDraw) {
        a.remaining = intuniform(0, (1 << a.be) - 1, par("backoffRng"));
        a.needsDraw = false;
        auto count = a.nb;
        auto be = a.be;
        auto slots = a.remaining;
        auto cw = a.cw;
        emit(countSignal, long(count));
        if (!attempt || attempt->id != id) return;
        emit(exponentSignal, long(be));
        if (!attempt || attempt->id != id) return;
        emit(contentionWindowSignal, long(cw));
        if (!attempt || attempt->id != id) return;
        emit(slotsSignal, long(slots));
        if (!attempt || attempt->id != id) return;
    }
    auto available = CLOCKTIME_AS_SIMTIME(limit - first).raw() / CLOCKTIME_AS_SIMTIME(period).raw();
    if (a.remaining > available) {
        if (w.bleEnd)
            a.needsDraw = true;
        else
            a.remaining -= available;
        a.deferred = true;
        return;
    }
    auto target = first + period * a.remaining;
    a.remaining = 0;
    if (!canFit(target)) {
        a.needsDraw = true;
        a.deferred = true;
        return;
    }
    scheduleClockEventAt(target, timer);
}

void Ieee802154SlottedAccess::handleMessage(cMessage *message)
{
    if (message != timer)
        throw cRuntimeError("Unexpected slotted access message");
    if (attempt)
        requestCca();
}

void Ieee802154SlottedAccess::requestCca()
{
    if (!client) {
        attempt.reset();
        return;
    }
    attempt->waitingForCca = true;
    client->handleCcaRequest(attempt->id);
}

void Ieee802154SlottedAccess::complete(const Ieee802154MacServiceResult& result, std::optional<simtime_t> startAt)
{
    auto completed = *attempt;
    attempt.reset();
    cancelTimer();
    if (client)
        client->handleAccessConfirm(completed.id, result, completed.nb, startAt);
}

void Ieee802154SlottedAccess::handleCcaResult(uint64_t id, bool idle)
{
    Enter_Method("handleCcaResult");
    if (!attempt || attempt->id != id || !attempt->waitingForCca)
        return;
    auto& a = *attempt;
    a.waitingForCca = false;
    if (!idle) {
        a.nb++;
        a.cw = 2;
        a.be = std::min<uint8_t>(a.be + 1, a.parameters.maxBe);
        if (a.nb > a.parameters.maxBackoffs)
            complete(Ieee802154MacStatus::CHANNEL_ACCESS_FAILURE);
        else {
            a.needsDraw = true;
            advance();
        }
    }
    else {
        auto cw = --a.cw;
        emit(contentionWindowSignal, long(cw));
        if (!attempt || attempt->id != id)
            return;
        if (cw) {
            auto next = boundaryAtOrAfter(getClockTime(), simTime());
            if (!canFit(next)) {
                a.cw = 2;
                a.needsDraw = true;
                a.deferred = true;
            }
            else
                scheduleClockEventAt(next, timer);
        }
        else {
            auto ready = simTime() + a.parameters.turnaroundTime;
            auto target = boundaryAtOrAfter(computeClockTimeFromSimTime(ready), ready);
            auto physicalTarget = target < a.window.end ? computeSimTimeFromClockTime(target) : a.window.physicalCapEnd;
            if (target >= a.window.end || (a.window.bleEnd && target >= *a.window.bleEnd) ||
                physicalTarget + a.parameters.exchangeDuration > a.window.physicalCapEnd ||
                (a.window.latestCompletion && physicalTarget + a.parameters.exchangeDuration - a.parameters.ifsDuration > *a.window.latestCompletion)) {
                a.cw = 2;
                a.needsDraw = true;
                a.deferred = true;
            }
            else
                complete(Ieee802154MacStatus::SUCCESS, physicalTarget);
        }
    }
}

void Ieee802154SlottedAccess::deferCca(uint64_t id, simtime_t notBefore)
{
    Enter_Method("deferCca");
    if (!attempt || attempt->id != id || !attempt->waitingForCca)
        return;
    attempt->waitingForCca = false;
    auto boundary = boundaryAtOrAfter(getClockTime(), std::max(simTime(), notBefore));
    if (!canFit(boundary)) {
        attempt->cw = 2;
        attempt->needsDraw = true;
        attempt->deferred = true;
    }
    else
        scheduleClockEventAt(boundary, timer);
}

Ieee802154MacServiceCancelResult Ieee802154SlottedAccess::cancel(uint64_t id, Ieee802154MacServiceAbortReason reason)
{
    Enter_Method("cancel");
    if (!attempt || attempt->id != id)
        return Ieee802154MacServiceCancelResult::NOT_PENDING;
    complete(reason);
    return Ieee802154MacServiceCancelResult::CANCELLED;
}

bool Ieee802154SlottedAccess::handleOperationStage(LifecycleOperation *operation, IDoneCallback *)
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

} // namespace inet
