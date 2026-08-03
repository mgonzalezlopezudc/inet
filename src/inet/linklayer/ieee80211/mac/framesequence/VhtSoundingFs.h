//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_VHTSOUNDINGFS_H
#define __INET_VHTSOUNDINGFS_H

#include "inet/linklayer/ieee80211/mac/contract/IFrameSequence.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/VhtCsiCache.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211Mib.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"

namespace inet {
namespace ieee80211 {

class INET_API VhtSoundingFs : public IFrameSequence
{
  protected:
    int step = 0;
    Ieee80211Mib *mib = nullptr;
    VhtCsiCache *csiCache = nullptr;
    MacAddress peer;
    uint16_t associationId = 0;
    uint64_t associationGeneration = 0;
    uint8_t dialogToken = 0;
    int soundingNsts = 2;
    physicallayer::Ieee80211ModeSet *modeSet = nullptr;
    const physicallayer::IIeee80211Mode *ndpMode = nullptr;
    double beamformingGainDb = 0;
    simtime_t expectedFeedbackStart = -1;
    simtime_t feedbackStartTolerance = SIMTIME_ZERO;

    Packet *buildNdpAnnouncement() const;
    Packet *buildNdp() const;
    bool isExpectedFeedback(Packet *packet) const;

  public:
    static bool matchesFeedback(Packet *packet, const MacAddress& localAddress,
            const MacAddress& peer, uint8_t dialogToken,
            simtime_t expectedStart = -1,
            simtime_t startTolerance = SIMTIME_ZERO);

    VhtSoundingFs(Ieee80211Mib *mib, VhtCsiCache *csiCache,
            const MacAddress& peer, uint16_t associationId,
            uint64_t associationGeneration, uint8_t dialogToken,
            int soundingNsts, physicallayer::Ieee80211ModeSet *modeSet,
            const physicallayer::IIeee80211Mode *ndpMode,
            double beamformingGainDb);

    virtual void startSequence(FrameSequenceContext *context, int firstStep) override;
    virtual IFrameSequenceStep *prepareStep(FrameSequenceContext *context) override;
    virtual bool completeStep(FrameSequenceContext *context) override;
    virtual std::string getHistory() const override { return "VHT-SU-Sounding (NDPA-NDP-Feedback)"; }
};

} // namespace ieee80211
} // namespace inet

#endif
