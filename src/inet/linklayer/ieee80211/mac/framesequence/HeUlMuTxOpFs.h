//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HEULMUTXOPFS_H
#define __INET_HEULMUTXOPFS_H

#include "inet/linklayer/ieee80211/mac/Ieee80211Frame_m.h"
#include "inet/linklayer/ieee80211/mac/contract/IFrameSequence.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/IIeee80211HeUlTriggerPolicy.h"
#include "inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeUlScheduler.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"

namespace inet {
namespace ieee80211 {

class HeHcf;
class HeUlCoordinator;

class INET_API HeUlMuTxOpFs : public IFrameSequence
{
  protected:
    HeUlCoordinator *coordinator = nullptr;
    HeHcf *callback = nullptr;
    IIeee80211HeUlScheduler::Schedule schedule;
    IIeee80211HeUlTriggerPolicy::TriggerType triggerType;
    physicallayer::Ieee80211ModeSet *modeSet = nullptr;
    MacAddress apAddress;
    uint32_t triggerId = 0;
    int step = -1;
    std::vector<Ieee80211MultiStaBlockAckRecord> ackRecords;

  protected:
    Packet *buildTriggerPacket() const;
    Packet *buildMultiStaBlockAckPacket() const;
    void processResponses(FrameSequenceContext *context);

  public:
    HeUlMuTxOpFs(HeUlCoordinator *coordinator, HeHcf *callback,
            const IIeee80211HeUlScheduler::Schedule& schedule,
            IIeee80211HeUlTriggerPolicy::TriggerType triggerType,
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
