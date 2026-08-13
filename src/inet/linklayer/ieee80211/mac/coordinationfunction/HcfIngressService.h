//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HCFINGRESSSERVICE_H
#define __INET_HCFINGRESSSERVICE_H

#include <functional>

#include "inet/common/INETDefs.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/common/AccessCategory.h"

namespace inet {

class Packet;

namespace queueing {
class IPacketQueue;
}

namespace ieee80211 {

/**
 * Classifies one upper HCF frame and performs its checked queue submission.
 *
 * The service owns no packet, queue, EDCAF, or channel-access state. Actions
 * make the packet ownership transition explicit. A successful claimPacket()
 * transfers caller ownership to the HCF ingress owner. A successful
 * enqueuePacket() transfers it to the selected queue; throwing leaves it with
 * the ingress owner. Every failure after claim and before enqueue commit calls
 * returnClaimedPacketToCaller() exactly once. That rollback is noexcept, so
 * the original exception is preserved and ownership cannot become ambiguous.
 */
class INET_API HcfIngressService
{
  public:
    enum class QueueSelection {
        SHARED,
        PER_STA,
    };

    struct Result {
        AccessCategory accessCategory = AccessCategory(-1);
        QueueSelection queueSelection = QueueSelection::SHARED;
        queueing::IPacketQueue *queue = nullptr;
        bool channelAccessRequested = false;
    };

    class IOwnershipActions
    {
      public:
        virtual ~IOwnershipActions() {}

        // Throwing means ownership was not transferred.
        virtual void claimPacket(Packet *packet) = 0;

        // Throwing means ownership was not committed to the queue.
        virtual void enqueuePacket(queueing::IPacketQueue *queue, Packet *packet) = 0;

        // Transfers the claimed packet from the ingress owner back to caller.
        virtual void returnClaimedPacketToCaller(Packet *packet) noexcept = 0;
    };

    struct Actions {
        IOwnershipActions *ownership = nullptr;
        std::function<void()> packetClaimed;
        std::function<AccessCategory(const Ptr<const Ieee80211DataHeader>&)> classifyDataFrame;
        std::function<void(AccessCategory)> frameClassified;
        std::function<void(Packet *, const Ptr<const Ieee80211DataHeader>&)> tagMacSapServiceDataUnit;
        std::function<queueing::IPacketQueue *(const MacAddress&, AccessCategory)> resolvePerStaQueue;
        std::function<queueing::IPacketQueue *(AccessCategory)> getSharedQueue;
        std::function<void(Packet *)> ensureOriginalEnqueueTime;
        std::function<bool(AccessCategory)> hasFrameToTransmit;
        std::function<bool()> hasChannelOwner;
        std::function<bool()> isSequenceRunning;
        std::function<void()> channelAccessRequested;
        std::function<void(AccessCategory)> requestChannelAccess;
    };

  private:
    Packet *activePacket = nullptr;

    static void checkActions(const Actions& actions);
    static void checkAccessCategory(AccessCategory accessCategory);

  public:
    Result processUpperFrame(Packet *packet,
            const Ptr<const Ieee80211DataOrMgmtHeader>& header,
            const Actions& actions);
};

} // namespace ieee80211
} // namespace inet

#endif
