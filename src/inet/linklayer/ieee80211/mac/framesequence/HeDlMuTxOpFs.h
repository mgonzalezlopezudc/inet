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
#include "inet/linklayer/ieee80211/mac/contract/IFrameSequenceHandler.h"
#include "inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeDlScheduler.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"
#include "inet/queueing/contract/IPacketQueue.h"

namespace inet {
namespace ieee80211 {

using namespace inet::units::values;

/**
 * Frame sequence for IEEE 802.11ax Downlink MU-OFDMA TXOP.
 *
 * Implements a sequential acknowledgment sequence where the AP transmits
 * the HE MU container frame in Step 0, and then sequentially waits for Block Ack
 * responses from each station in subsequent steps.
 */
class INET_API HeDlMuTxOpFs : public IFrameSequence
{
  public:
    struct ActiveAllocation {
        MacAddress staAddress;
        Tid tid = 0;
        int ruIndex;
        Packet *packet = nullptr;
    };

  protected:
    int firstStep = -1;
    int step = -1;

    IIeee80211HeDlScheduler *dlScheduler = nullptr;
    std::vector<MacAddress> candidates;
    physicallayer::Ieee80211ModeSet *modeSet = nullptr;
    queueing::IPacketQueue *pendingQueue = nullptr;
    IAckHandler *ackHandler = nullptr;
    IFrameSequenceHandler::ICallback *callback = nullptr;

    std::vector<ActiveAllocation> activeAllocations;

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
                 IAckHandler *ackHandler,
                 IFrameSequenceHandler::ICallback *callback);
    virtual ~HeDlMuTxOpFs() {}

    virtual void startSequence(FrameSequenceContext *context, int firstStep) override;
    virtual IFrameSequenceStep *prepareStep(FrameSequenceContext *context) override;
    virtual bool completeStep(FrameSequenceContext *context) override;

    virtual bool isContainerPacket(Packet *packet) const { return packet == containerPacket; }
    virtual const std::vector<ActiveAllocation>& getActiveAllocations() const { return activeAllocations; }

    virtual std::string getHistory() const override { return "HE-DL-MU"; }
};

} // namespace ieee80211
} // namespace inet

#endif // __INET_HEDLMUTXOPFS_H
