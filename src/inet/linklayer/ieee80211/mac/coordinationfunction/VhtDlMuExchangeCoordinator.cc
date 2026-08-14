//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/VhtDlMuExchangeCoordinator.h"

#include <limits>

namespace inet {
namespace ieee80211 {

void VhtDlMuExchangeCoordinator::configure(IActions *actions)
{
    if (actions == nullptr)
        throw cRuntimeError("VHT DL MU coordinator requires typed actions");
    this->actions = actions;
}

VhtDlMuExchangeId VhtDlMuExchangeCoordinator::beginPendingExchange()
{
    if (pendingExchangeId != NO_VHT_DL_MU_EXCHANGE || activeExchange != nullptr)
        throw cRuntimeError("Another VHT DL MU exchange is pending or active");
    if (nextExchangeId == NO_VHT_DL_MU_EXCHANGE ||
            nextExchangeId == std::numeric_limits<VhtDlMuExchangeId>::max())
        throw cRuntimeError("VHT DL MU exchange ID exhausted");
    pendingExchangeId = nextExchangeId++;
    return pendingExchangeId;
}

void VhtDlMuExchangeCoordinator::abandonPendingExchange(VhtDlMuExchangeId id)
{
    if (id == pendingExchangeId)
        pendingExchangeId = NO_VHT_DL_MU_EXCHANGE;
}

void VhtDlMuExchangeCoordinator::retireExchange(VhtDlMuExchangeId id)
{
    if (activeExchange == nullptr || activeExchange->getId() != id)
        return;
    lastRetiredExchangeId = id;
    activeExchange.reset();
}

void VhtDlMuExchangeCoordinator::abortActiveExchange()
{
    if (activeExchange == nullptr)
        return;
    lastRetiredExchangeId = activeExchange->getId();
    activeExchange.reset();
}

void VhtDlMuExchangeCoordinator::shutdown()
{
    pendingExchangeId = NO_VHT_DL_MU_EXCHANGE;
    abortActiveExchange();
    actions = nullptr;
}

void VhtDlMuExchangeCoordinator::vhtDlMuPlanCommitted(VhtDlMuExchangeId id,
        Packet *containerPacket,
        const std::vector<std::vector<Packet *>>& userPackets)
{
    if (id == NO_VHT_DL_MU_EXCHANGE || id != pendingExchangeId ||
            activeExchange != nullptr)
        throw cRuntimeError("Invalid VHT DL MU commit event");
    activeExchange = std::make_unique<VhtDlMuExchange>(id, containerPacket,
            std::vector<std::vector<Packet *>>(userPackets));
    pendingExchangeId = NO_VHT_DL_MU_EXCHANGE;
}

void VhtDlMuExchangeCoordinator::vhtDlMuFrameFailed(VhtDlMuExchangeId id,
        Packet *packet)
{
    if (actions == nullptr || activeExchange == nullptr ||
            activeExchange->getId() != id ||
            !activeExchange->recordFailedPacket(packet))
        return;
    actions->processVhtDlMuFailedFrame(packet);
}

void VhtDlMuExchangeCoordinator::vhtDlMuUserResult(VhtDlMuExchangeId id,
        unsigned int userIndex, VhtDlMuUserResult result)
{
    if (activeExchange == nullptr || activeExchange->getId() != id ||
            !activeExchange->recordUserResult(userIndex, result))
        return;
    if (activeExchange->isComplete())
        retireExchange(id);
}

} // namespace ieee80211
} // namespace inet
