//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEHCF_H
#define __INET_HEHCF_H

#include <map>
#include <memory>
#include <vector>

#include "inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeUlCoordinator.h"
#include "inet/linklayer/ieee80211/mac/queue/StationQueueBankManager.h"
#include "inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeDlScheduler.h"
#include "inet/queueing/contract/IPacketQueue.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeMuMimoCsiManager.h"

namespace inet {
namespace ieee80211 {

/**
 * Extends Hcf to support IEEE 802.11ax Downlink OFDMA multi-user scheduling.
 *
 * When the winning EDCAF's pending queue contains packets for two or more
 * distinct destination STAs and the "ax" modeSet is active, HeHcf replaces
 * the standard HcfFs frame sequence with HeDlMuTxOpFs, which:
 *   1. Calls the DL OFDMA scheduler to obtain per-STA RU assignments.
 *   2. Dequeues one packet per selected STA.
 *   3. Assembles a container packet with explicit HE MU RU payload sections.
 *   4. Passes the container to the existing Tx pipeline where the packet-level
 *      PHY models the PPDU as a single transmission with per-RU reception.
 *
 * When fewer than two unique destination STAs are queued (or the modeSet is
 * not "ax"), HeHcf falls back transparently to the standard Hcf::startFrameSequence().
 */
class INET_API HeHcf : public Hcf
{
  protected:
    IIeee80211HeDlScheduler *dlScheduler = nullptr;
    HeUlCoordinator *ulCoordinator = nullptr;
    std::unique_ptr<StationQueueBankManager> queueBankManager;
    cMessage *ulTriggerTimer = nullptr;
    IIeee80211HeUlTriggerPolicy::TriggerType pendingUlTrigger = IIeee80211HeUlTriggerPolicy::NO_TRIGGER;
    bool ulTriggerAccessRequested = false;
    struct TriggeredUlExchange {
        Tid tid = 0;
        queueing::IPacketQueue *sourceQueue = nullptr;
        std::vector<Packet *> packets;
        std::vector<int> sequenceNumbers;
        physicallayer::Ieee80211HeRu ru;
        bool randomAccess = false;
        simtime_t expectedResponseTime = SIMTIME_ZERO;
    };
    // A response is retained by Trigger ID until its Multi-STA Block Ack is
    // processed.  This is intentionally not a single global packet: an HE-TB
    // response can be a single-TID A-MPDU and must be retried per MPDU.
    std::map<uint32_t, TriggeredUlExchange> triggeredUlExchanges;
    bool forceNextSingleUser[4] = {};

    HeMuMimoCsiManager csiManager;
    bool enableDlMuMimo = false;
    simtime_t csiValidityDuration = SimTime(0.1);
    double defaultCsiLeakage = 0.1;
    std::string csiLeakageOverrides = "";


  protected:
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    virtual queueing::IPacketQueue *getPerStaQueue(const MacAddress& staAddr, AccessCategory ac) override;

  public:
    virtual void legacyPreambleReceived(Packet *packet);

    /**
     * Scans the shared EDCAF queue and all per-STA queues for this access
     * category, returning one candidate per eligible destination STA.
     */
    virtual IIeee80211HeDlScheduler::ScheduleContext collectScheduleContext(AccessCategory ac) const;

    virtual queueing::IPacketQueue *findOldestPerStaQueue(AccessCategory ac) const;
    virtual bool stagePerStaFrameForSingleUserTransmission(AccessCategory ac);
    virtual bool tryStartUlMuFrameSequence(AccessCategory ac);
    virtual bool tryStartDlMuFrameSequence(AccessCategory ac);
    virtual bool releaseChannelIfNoFallbackFrame(AccessCategory ac);

    /**
     * Override: selects HeDlMuTxOpFs when ≥2 unique destination STAs are
     * queued and HE mode is active; otherwise delegates to Hcf::startFrameSequence().
     */
    virtual void startFrameSequence(AccessCategory ac) override;
    virtual void handleInternalCollision(std::vector<Edcaf *> internallyCollidedEdcafs) override;
    virtual bool hasFrameToTransmit() override;
    virtual bool hasFrameToTransmit(AccessCategory ac) override;
    virtual void recipientProcessReceivedFrame(Packet *packet, const Ptr<const Ieee80211MacHeader>& header) override;
    virtual void transmissionComplete(Packet *packet, const Ptr<const Ieee80211MacHeader>& header) override;
    virtual void transmitFrame(Packet *packet, simtime_t ifs) override;

  public:
    virtual ~HeHcf();
    virtual StationQueueBank *createStationQueueBank(const MacAddress& staAddr) override;
    virtual void destroyStationQueueBank(const MacAddress& staAddr) override;
    virtual StationQueueBank *getStationQueueBank(const MacAddress& staAddr) const override;
    virtual void originatorProcessTransmittedFrame(Packet *packet) override;
    virtual void originatorProcessTransmittedControlFrame(const Ptr<const Ieee80211MacHeader>& controlHeader, AccessCategory ac) override;
    virtual void originatorProcessReceivedFrame(Packet *receivedPacket, Packet *lastTransmittedPacket) override;
    virtual void originatorProcessFailedFrame(Packet *packet) override;
    uint16_t getAssociationId(const MacAddress& address) const;
    void handleDlMuPlanningFailure(AccessCategory ac);
    void processTriggeredUlFrame(Packet *packet, const Ptr<const Ieee80211DataHeader>& header, uint16_t aid);
};

} // namespace ieee80211
} // namespace inet

#endif // __INET_HEHCF_H
