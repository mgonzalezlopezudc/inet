// SPDX-License-Identifier: LGPL-3.0-or-later
//
#include "inet/linklayer/ieee802154/Ieee802154SuperframeController.h"

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
    if (capEnd && isScheduledClockEvent(capEnd))
        cancelClockEvent(capEnd);
    delete capEnd;
}

int Ieee802154SuperframeController::numInitStages() const { return NUM_INIT_STAGES; }

void Ieee802154SuperframeController::initialize(int stage)
{
    ClockUserModuleMixin<SimpleModule>::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        client.reference(this, "clientModule", true);
        beaconPreparation = new ClockEvent("beaconPreparation");
        capEnd = new ClockEvent("capEnd");
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
        client->handleSuperframeBoundary(beaconBoundary);
        return;
    }
    if (message == capEnd) {
        client->handleSuperframeCapEnd();
        return;
    }
    throw cRuntimeError("Unexpected superframe message %s", message->getName());
}

bool Ieee802154SuperframeController::scheduleBeaconPreparation(clocktime_t boundary, simtime_t readinessLead)
{
    Enter_Method("scheduleBeaconPreparation");
    if (isScheduledClockEvent(beaconPreparation))
        cancelClockEvent(beaconPreparation);
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

void Ieee802154SuperframeController::recordReceptionStart(const physicallayer::Ieee802154PhyReceiveStart& start)
{
    Enter_Method("recordReceptionStart");
    retireExpiredReceptionAnchors();
    if (started && start.ppduStart == simTime())
        receptionAnchors.emplace(start.transmissionId, Ieee802154SuperframeReceptionAnchor{
                start.transmissionId, start.ppduStart, getClockTime(), start.expectedPpduEnd});
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
            if (isScheduledClockEvent(capEnd))
                cancelClockEvent(capEnd);
            receptionAnchors.clear();
        }
    }
    else if (dynamic_cast<ModuleCrashOperation *>(operation)) {
        started = false;
        if (isScheduledClockEvent(beaconPreparation))
            cancelClockEvent(beaconPreparation);
        if (isScheduledClockEvent(capEnd))
            cancelClockEvent(capEnd);
        receptionAnchors.clear();
    }
    else
        throw cRuntimeError("Unsupported lifecycle operation");
    return true;
}

} // namespace inet
