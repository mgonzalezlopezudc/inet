//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEHCF_H
#define __INET_HEHCF_H

#include <vector>

#include "inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h"
#include "inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeDlScheduler.h"
#include "inet/queueing/contract/IPacketQueue.h"

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

  protected:
    virtual void initialize(int stage) override;

    /**
     * Scans the shared EDCAF queue and all per-STA queues for this access
     * category, returning one candidate per eligible destination STA.
     */
    virtual IIeee80211HeDlScheduler::ScheduleContext collectScheduleContext(AccessCategory ac) const;

    virtual queueing::IPacketQueue *findOldestPerStaQueue(AccessCategory ac) const;
    virtual bool stagePerStaFrameForSingleUserTransmission(AccessCategory ac);

    /**
     * Override: selects HeDlMuTxOpFs when ≥2 unique destination STAs are
     * queued and HE mode is active; otherwise delegates to Hcf::startFrameSequence().
     */
    virtual void startFrameSequence(AccessCategory ac) override;
    virtual void handleInternalCollision(std::vector<Edcaf *> internallyCollidedEdcafs) override;
    virtual bool hasFrameToTransmit() override;
    virtual bool hasFrameToTransmit(AccessCategory ac) override;

  public:
    virtual void originatorProcessTransmittedFrame(Packet *packet) override;
    virtual void originatorProcessFailedFrame(Packet *packet) override;
};

} // namespace ieee80211
} // namespace inet

#endif // __INET_HEHCF_H
