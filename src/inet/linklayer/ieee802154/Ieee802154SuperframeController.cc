// SPDX-License-Identifier: LGPL-3.0-or-later
//
#include "inet/linklayer/ieee802154/Ieee802154SuperframeController.h"

#include "inet/common/InitStages.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/lifecycle/NodeStatus.h"

namespace inet {

Define_Module(Ieee802154SuperframeController);

int Ieee802154SuperframeController::numInitStages() const { return NUM_INIT_STAGES; }

void Ieee802154SuperframeController::initialize(int stage)
{
    ClockUserModuleMixin<SimpleModule>::initialize(stage);
    if (stage == INITSTAGE_LOCAL)
        client.reference(this, "clientModule", true);
    else if (stage == INITSTAGE_LINK_LAYER) {
        auto node = findContainingNode(this);
        auto status = node ? check_and_cast_nullable<NodeStatus *>(node->getSubmodule("status")) : nullptr;
        started = !status || status->getState() == NodeStatus::UP;
    }
}

void Ieee802154SuperframeController::handleMessage(cMessage *message)
{
    throw cRuntimeError("Unexpected superframe message %s", message->getName());
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
            receptionAnchors.clear();
        }
    }
    else if (dynamic_cast<ModuleCrashOperation *>(operation)) {
        started = false;
        receptionAnchors.clear();
    }
    else
        throw cRuntimeError("Unsupported lifecycle operation");
    return true;
}

} // namespace inet
