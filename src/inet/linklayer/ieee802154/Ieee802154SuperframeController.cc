// SPDX-License-Identifier: LGPL-3.0-or-later
//
#include "inet/linklayer/ieee802154/Ieee802154SuperframeController.h"

#include <algorithm>

#include "inet/common/InitStages.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/lifecycle/NodeStatus.h"

namespace inet {

Define_Module(Ieee802154SuperframeController);

Ieee802154SuperframeController::~Ieee802154SuperframeController()
{
    if (beaconPreparation && isScheduledClockEvent(beaconPreparation))
        cancelClockEvent(beaconPreparation);
    delete beaconPreparation;
    cancelAndDelete(beaconDispatch);
    if (capEnd && isScheduledClockEvent(capEnd))
        cancelClockEvent(capEnd);
    delete capEnd;
    cancelAndDelete(cfpBoundaryEvent);
    if (syncWake && isScheduledClockEvent(syncWake)) cancelClockEvent(syncWake);
    if (syncDeadline && isScheduledClockEvent(syncDeadline)) cancelClockEvent(syncDeadline);
    delete syncWake;
    delete syncDeadline;
    if (accessSearchDeadline && isScheduledClockEvent(accessSearchDeadline)) cancelClockEvent(accessSearchDeadline);
    delete accessSearchDeadline;
}

int Ieee802154SuperframeController::numInitStages() const { return NUM_INIT_STAGES; }

void Ieee802154SuperframeController::initialize(int stage)
{
    ClockUserModuleMixin<SimpleModule>::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        client.reference(this, "clientModule", true);
        beaconPreparation = new ClockEvent("beaconPreparation");
        beaconDispatch = new cMessage("beaconDispatch");
        beaconDispatch->setSchedulingPriority(2); // Packet delivery and ACK expiry settle first at equality.
        capEnd = new ClockEvent("capEnd");
        cfpBoundaryEvent = new cMessage("cfpBoundary");
        cfpBoundaryEvent->setSchedulingPriority(1); // Delivery settles first; expiry precedes the next beacon dispatch.
        syncWake = new ClockEvent("syncWake");
        syncDeadline = new ClockEvent("syncDeadline");
        accessSearchDeadline = new ClockEvent("accessSearchDeadline");
        syncDeadline->setSchedulingPriority(1); // Equal-time packet delivery precedes search expiry.
        accessSearchDeadline->setSchedulingPriority(1);
    }
    else if (stage == INITSTAGE_LINK_LAYER) {
        auto node = findContainingNode(this);
        auto status = node ? check_and_cast_nullable<NodeStatus *>(node->getSubmodule("status")) : nullptr;
        started = !status || status->getState() == NodeStatus::UP;
    }
}

void Ieee802154SuperframeController::handleMessage(cMessage *message)
{
    if (message == beaconPreparation) {
        scheduleAt(simTime(), beaconDispatch);
        return;
    }
    if (message == beaconDispatch) {
        client->handleSuperframeBoundary(beaconBoundary);
        return;
    }
    if (message == capEnd) {
        client->handleSuperframeCapEnd();
        return;
    }
    if (message == cfpBoundaryEvent) {
        client->handleCfpBoundary();
        return;
    }
    if (message == syncWake) {
        client->handleSyncWake();
        return;
    }
    if (message == syncDeadline) {
        client->handleSyncDeadline();
        return;
    }
    if (message == accessSearchDeadline) {
        client->handleAccessSearchDeadline();
        return;
    }
    throw cRuntimeError("Unexpected superframe message %s", message->getName());
}

bool Ieee802154SuperframeController::scheduleBeaconPreparation(clocktime_t boundary, simtime_t readinessLead)
{
    Enter_Method("scheduleBeaconPreparation");
    if (isScheduledClockEvent(beaconPreparation))
        cancelClockEvent(beaconPreparation);
    if (beaconDispatch->isScheduled())
        cancelEvent(beaconDispatch);
    if (!started || readinessLead < SIMTIME_ZERO || boundary < getClockTime())
        return false;
    auto preparation = computeSimTimeFromClockTime(boundary) - readinessLead;
    if (preparation < simTime())
        return false;
    beaconBoundary = boundary;
    scheduleClockEventAt(computeClockTimeFromSimTime(preparation), beaconPreparation);
    return true;
}

void Ieee802154SuperframeController::cancelBeaconPreparation()
{
    Enter_Method("cancelBeaconPreparation");
    if (isScheduledClockEvent(beaconPreparation))
        cancelClockEvent(beaconPreparation);
    if (beaconDispatch->isScheduled())
        cancelEvent(beaconDispatch);
}

bool Ieee802154SuperframeController::scheduleCapEnd(clocktime_t boundary)
{
    Enter_Method("scheduleCapEnd");
    cancelCapEnd();
    if (!started || boundary <= getClockTime())
        return false;
    scheduleClockEventAt(boundary,capEnd);
    return true;
}

void Ieee802154SuperframeController::cancelCapEnd()
{
    Enter_Method("cancelCapEnd");
    if (isScheduledClockEvent(capEnd))
        cancelClockEvent(capEnd);
}

bool Ieee802154SuperframeController::scheduleCfpBoundary(simtime_t when)
{
    Enter_Method("scheduleCfpBoundary");
    cancelCfpBoundary();
    if (!started || when < simTime())
        return false;
    scheduleAt(when,cfpBoundaryEvent);
    return true;
}

void Ieee802154SuperframeController::cancelCfpBoundary()
{
    Enter_Method("cancelCfpBoundary");
    if (cfpBoundaryEvent->isScheduled()) cancelEvent(cfpBoundaryEvent);
}

void Ieee802154SuperframeController::scheduleSyncWake(clocktime_t when)
{
    Enter_Method("scheduleSyncWake");
    if (isScheduledClockEvent(syncWake)) cancelClockEvent(syncWake);
    scheduleClockEventAt(when,syncWake);
}

void Ieee802154SuperframeController::scheduleSyncDeadline(clocktime_t when)
{
    Enter_Method("scheduleSyncDeadline");
    if (isScheduledClockEvent(syncDeadline)) cancelClockEvent(syncDeadline);
    scheduleClockEventAt(when,syncDeadline);
}

void Ieee802154SuperframeController::cancelSyncEvents()
{
    Enter_Method("cancelSyncEvents");
    if (isScheduledClockEvent(syncWake)) cancelClockEvent(syncWake);
    if (isScheduledClockEvent(syncDeadline)) cancelClockEvent(syncDeadline);
}

Ieee802154AccessWindow Ieee802154SuperframeController::publishAccessWindow(const Ieee802154AccessWindow& window)
{
    Enter_Method("publishAccessWindow");
    if (accessEpoch == UINT64_MAX)
        throw cRuntimeError("IEEE 802.15.4 CAP epoch IDs exhausted");
    accessWindow = window;
    accessWindow->epoch = ++accessEpoch;
    return *accessWindow;
}

std::optional<Ieee802154AccessWindow> Ieee802154SuperframeController::getAccessWindow() const
{
    return accessWindow && accessWindow->end > getClockTime() && accessWindow->physicalCapEnd > simTime() ? accessWindow : std::nullopt;
}

void Ieee802154SuperframeController::clearAccessWindow()
{
    Enter_Method("clearAccessWindow");
    accessWindow.reset();
}

void Ieee802154SuperframeController::scheduleAccessSearchDeadline(clocktime_t when)
{
    Enter_Method("scheduleAccessSearchDeadline");
    cancelAccessSearchDeadline();
    scheduleClockEventAt(when, accessSearchDeadline);
}

void Ieee802154SuperframeController::cancelAccessSearchDeadline()
{
    Enter_Method("cancelAccessSearchDeadline");
    if (isScheduledClockEvent(accessSearchDeadline))
        cancelClockEvent(accessSearchDeadline);
}

simtime_t Ieee802154SuperframeController::getForecastUncertainty(simtime_t elapsed, simtime_t maxPropagationDelay) const
{
#ifdef INET_WITH_CLOCK
    if (clock) {
        auto relativeGuard = [](int64_t ticks) { return (ticks / 1000000) * 81 + ((ticks % 1000000) * 81 + 999999) / 1000000; };
        auto ticks = std::max<int64_t>(0,elapsed.raw());
        auto base = 2 * SimTime(16,SIMTIME_US).raw() + std::max<int64_t>(0,maxPropagationDelay.raw()) + 2;
        return SimTime::fromRaw(relativeGuard(ticks) + base + relativeGuard(base));
    }
#endif
    return SIMTIME_ZERO;
}

void Ieee802154SuperframeController::recordReceptionStart(const physicallayer::Ieee802154PhyReceiveStart& start,
        std::optional<Ieee802154AccessWindow> capWindow, std::vector<Ieee802154CfpReceptionCandidate> cfpCandidates)
{
    Enter_Method("recordReceptionStart");
    retireExpiredReceptionAnchors();
    if (started && start.ppduStart == simTime())
        receptionAnchors.emplace(start.transmissionId, Ieee802154SuperframeReceptionAnchor{
                start.transmissionId, start.ppduStart, getClockTime(), start.expectedPpduEnd, capWindow, std::move(cfpCandidates)});
}

std::optional<Ieee802154SuperframeReceptionAnchor> Ieee802154SuperframeController::takeReceptionAnchor(int transmissionId)
{
    Enter_Method("takeReceptionAnchor");
    retireExpiredReceptionAnchors();
    auto it = receptionAnchors.find(transmissionId);
    if (it == receptionAnchors.end())
        return {};
    auto result = it->second;
    receptionAnchors.erase(it);
    return result;
}

void Ieee802154SuperframeController::retireExpiredReceptionAnchors()
{
    Enter_Method("retireExpiredReceptionAnchors");
    for (auto it = receptionAnchors.begin(); it != receptionAnchors.end(); ) {
        if (it->second.expectedEnd < simTime())
            it = receptionAnchors.erase(it);
        else
            ++it;
    }
}

void Ieee802154SuperframeController::clearReceptionAnchors()
{
    Enter_Method("clearReceptionAnchors");
    receptionAnchors.clear();
}

bool Ieee802154SuperframeController::handleOperationStage(LifecycleOperation *operation, IDoneCallback *)
{
    Enter_Method("handleOperationStage");
    if (dynamic_cast<ModuleStartOperation *>(operation)) {
        if (operation->getCurrentStage() == ModuleStartOperation::STAGE_LINK_LAYER)
            started = true;
    }
    else if (dynamic_cast<ModuleStopOperation *>(operation)) {
        if (operation->getCurrentStage() == ModuleStopOperation::STAGE_LOCAL) {
            started = false;
            if (isScheduledClockEvent(beaconPreparation))
                cancelClockEvent(beaconPreparation);
            if (beaconDispatch->isScheduled())
                cancelEvent(beaconDispatch);
            if (isScheduledClockEvent(capEnd))
                cancelClockEvent(capEnd);
            cancelSyncEvents();
            cancelAccessSearchDeadline();
            cancelCfpBoundary();
            accessWindow.reset();
            receptionAnchors.clear();
        }
    }
    else if (dynamic_cast<ModuleCrashOperation *>(operation)) {
        started = false;
        if (isScheduledClockEvent(beaconPreparation))
            cancelClockEvent(beaconPreparation);
        if (beaconDispatch->isScheduled())
            cancelEvent(beaconDispatch);
        if (isScheduledClockEvent(capEnd))
            cancelClockEvent(capEnd);
        cancelSyncEvents();
        cancelAccessSearchDeadline();
        cancelCfpBoundary();
        accessWindow.reset();
        receptionAnchors.clear();
    }
    else
        throw cRuntimeError("Unsupported lifecycle operation");
    return true;
}

} // namespace inet
