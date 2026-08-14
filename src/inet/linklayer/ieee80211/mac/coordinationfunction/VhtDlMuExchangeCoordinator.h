//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_VHTDLMUEXCHANGECOORDINATOR_H
#define __INET_VHTDLMUEXCHANGECOORDINATOR_H

#include <memory>

#include "inet/linklayer/ieee80211/mac/contract/IVhtDlMuExchangeEvents.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/VhtDlMuExchange.h"

namespace inet {
namespace ieee80211 {

/** Owns the VHT DL MU pending/active lifecycle and rejects stale reports. */
class INET_API VhtDlMuExchangeCoordinator : public IVhtDlMuExchangeEvents
{
  public:
    class INET_API IActions
    {
      public:
        virtual ~IActions() = default;
        virtual void processVhtDlMuFailedFrame(Packet *packet) = 0;
    };

  private:
    IActions *actions = nullptr;
    VhtDlMuExchangeId nextExchangeId = 1;
    VhtDlMuExchangeId pendingExchangeId = NO_VHT_DL_MU_EXCHANGE;
    VhtDlMuExchangeId lastRetiredExchangeId = NO_VHT_DL_MU_EXCHANGE;
    std::unique_ptr<VhtDlMuExchange> activeExchange;

  public:
    explicit VhtDlMuExchangeCoordinator(IActions *actions = nullptr)
        : actions(actions) {}

    void configure(IActions *actions);
    VhtDlMuExchangeId beginPendingExchange();
    void abandonPendingExchange(VhtDlMuExchangeId id);
    void retireExchange(VhtDlMuExchangeId id);
    void abortActiveExchange();
    void shutdown();
    VhtDlMuExchangeId getPendingExchangeId() const { return pendingExchangeId; }
    VhtDlMuExchangeId getLastRetiredExchangeId() const { return lastRetiredExchangeId; }
    const VhtDlMuExchange *getActiveExchange() const { return activeExchange.get(); }

    virtual void vhtDlMuPlanCommitted(VhtDlMuExchangeId id,
            Packet *containerPacket,
            const std::vector<std::vector<Packet *>>& userPackets) override;
    virtual void vhtDlMuFrameFailed(VhtDlMuExchangeId id,
            Packet *packet) override;
    virtual void vhtDlMuUserResult(VhtDlMuExchangeId id,
            unsigned int userIndex, VhtDlMuUserResult result) override;
};

} // namespace ieee80211
} // namespace inet

#endif
