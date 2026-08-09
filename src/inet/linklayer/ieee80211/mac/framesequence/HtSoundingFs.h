//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HTSOUNDINGFS_H
#define __INET_HTSOUNDINGFS_H

#include "inet/linklayer/ieee80211/mac/contract/IFrameSequence.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HtCsiCache.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211Mib.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"

namespace inet {
namespace ieee80211 {

class INET_API HtSoundingFs : public IFrameSequence
{
  protected:
    int step = 0;
    Ieee80211Mib *mib = nullptr;
    HtCsiCache *csiCache = nullptr;
    MacAddress peer;
    uint64_t associationGeneration = 0;
    uint8_t requestToken = 0;
    uint8_t soundingNsts = 1;
    Ieee80211HtFeedbackKind feedbackKind = Ieee80211HtFeedbackKind::COMPRESSED_BEAMFORMING;
    physicallayer::Ieee80211ModeSet *modeSet = nullptr;
    const physicallayer::IIeee80211Mode *ndpMode = nullptr;

    Packet *buildAnnouncement() const;
    Packet *buildNdp() const;
    bool isExpectedFeedback(Packet *packet) const;

  public:
    HtSoundingFs(Ieee80211Mib *mib, HtCsiCache *csiCache,
            const MacAddress& peer, uint64_t associationGeneration,
            uint8_t requestToken, uint8_t soundingNsts,
            Ieee80211HtFeedbackKind feedbackKind,
            physicallayer::Ieee80211ModeSet *modeSet,
            const physicallayer::IIeee80211Mode *ndpMode);

    virtual void startSequence(FrameSequenceContext *context, int firstStep) override;
    virtual IFrameSequenceStep *prepareStep(FrameSequenceContext *context) override;
    virtual bool completeStep(FrameSequenceContext *context) override;
    virtual std::string getHistory() const override { return "HT-Sounding (+HTC-NDP-Feedback)"; }

    static bool matchesFeedback(Packet *packet, const MacAddress& localAddress,
            const MacAddress& peer, uint8_t requestToken, uint8_t soundingNsts,
            Hz channelWidth, Ieee80211HtFeedbackKind feedbackKind);
};

} // namespace ieee80211
} // namespace inet

#endif
