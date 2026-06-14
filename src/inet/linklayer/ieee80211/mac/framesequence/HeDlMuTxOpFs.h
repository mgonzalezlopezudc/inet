//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEDLMUTXOPFS_H
#define __INET_HEDLMUTXOPFS_H

#include <vector>

#include "inet/common/Units.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/contract/IFrameSequence.h"
#include "inet/linklayer/ieee80211/mac/contract/IAckHandler.h"
#include "inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeDlScheduler.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"
#include "inet/queueing/contract/IPacketQueue.h"

namespace inet {
namespace ieee80211 {

using namespace inet::units::values;

/**
 * Frame sequence for IEEE 802.11ax Downlink MU-OFDMA TXOP.
 *
 * Implements a two-step sequence:
 *   Step 0 (TRANSMIT): Invokes the DL OFDMA scheduler, dequeues one packet
 *       per selected STA from the pending queue, assembles a container Packet
 *       tagged with Ieee80211HeMuTag, and returns a TransmitStep.
 *   Step 1 (complete): Reports success and terminates the sequence.
 *
 * The container Packet is handled by the existing Tx path and the
 * Ieee80211RadioMedium (Phase 2), which splits it into parallel per-RU
 * sub-transmissions on the radio medium.
 */
class INET_API HeDlMuTxOpFs : public IFrameSequence
{
  protected:
    int firstStep = -1;
    int step = -1;

    IIeee80211HeDlScheduler *dlScheduler = nullptr;
    std::vector<MacAddress> candidates;
    physicallayer::Ieee80211ModeSet *modeSet = nullptr;
    queueing::IPacketQueue *pendingQueue = nullptr;
    IAckHandler *ackHandler = nullptr;

    // Assembled container packet (owned until handed to TransmitStep).
    Packet *containerPacket = nullptr;

  protected:
    /** Build the MU container Packet from the scheduler allocation and pending queue. */
    Packet *buildMuContainerPacket(FrameSequenceContext *context);

  public:
    HeDlMuTxOpFs(IIeee80211HeDlScheduler *dlScheduler,
                 const std::vector<MacAddress>& candidates,
                 physicallayer::Ieee80211ModeSet *modeSet,
                 queueing::IPacketQueue *pendingQueue,
                 IAckHandler *ackHandler);
    virtual ~HeDlMuTxOpFs() {}

    virtual void startSequence(FrameSequenceContext *context, int firstStep) override;
    virtual IFrameSequenceStep *prepareStep(FrameSequenceContext *context) override;
    virtual bool completeStep(FrameSequenceContext *context) override;

    virtual std::string getHistory() const override { return "HE-DL-MU"; }
};

} // namespace ieee80211
} // namespace inet

#endif // __INET_HEDLMUTXOPFS_H
