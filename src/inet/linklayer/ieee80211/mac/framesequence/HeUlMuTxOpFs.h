//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEULMUTXOPFS_H
#define __INET_HEULMUTXOPFS_H

#include <memory>
#include <vector>

#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/contract/IFrameSequence.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/IIeee80211HeUlTriggerPolicy.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeUlMuPlan.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag_m.h"

namespace inet {
namespace ieee80211 {

INET_API Ieee80211MultiStaBlockAckRecord buildHeUlMultiStaBlockAckRecord(
        uint16_t aid, uint8_t tid,
        const std::vector<physicallayer::Ieee80211MpduReceiveResult>& outcomes);

class HeHcf;
class HeUlCoordinator;

/**
 * AP-side frame sequence for a trigger-based HE uplink OFDMA TXOP.
 *
 * It sends the selected Trigger, receives the scheduled HE-TB responses, and
 * completes the exchange with a Multi-STA Block Ack.
 */
class INET_API HeUlMuTxOpFs : public IFrameSequence
{
  protected:
    static constexpr uint8_t modeledTidAggregationLimit = 1;
    HeUlCoordinator *coordinator = nullptr;
    HeHcf *callback = nullptr;
    const HeUlMuPlan plan;
    const IIeee80211HeUlScheduler::Schedule& schedule;
    const IIeee80211HeUlTriggerPolicy::TriggerType triggerType;
    physicallayer::Ieee80211ModeSet *modeSet = nullptr;
    MacAddress apAddress;
    uint32_t triggerId = 0;
    int step = -1;
    std::unique_ptr<IFrameSequence> sequence;
    std::vector<Ieee80211MultiStaBlockAckRecord> ackRecords;

  protected:
    Packet *buildTriggerPacket() const;
    IReceiveStep *buildReceiveCollectionStep() const;
    Packet *buildMultiStaBlockAckPacket() const;
    void processResponses(FrameSequenceContext *context);

  public:
    HeUlMuTxOpFs(HeUlCoordinator *coordinator, HeHcf *callback,
            const HeUlMuPlan& plan,
            physicallayer::Ieee80211ModeSet *modeSet,
            const MacAddress& apAddress);

    virtual void startSequence(FrameSequenceContext *context, int firstStep) override;
    virtual IFrameSequenceStep *prepareStep(FrameSequenceContext *context) override;
    virtual bool completeStep(FrameSequenceContext *context) override;
    virtual std::string getHistory() const override;

    uint32_t getTriggerId() const { return triggerId; }
};

} // namespace ieee80211
} // namespace inet

#endif
