//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HESOUNDINGCOORDINATOR_H
#define __INET_HESOUNDINGCOORDINATOR_H

#include <vector>
#include "omnetpp.h"
#include "inet/common/INETDefs.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211Mac.h"
#include "inet/linklayer/ieee80211/mac/contract/IFrameSequenceHandler.h"
#include "inet/linklayer/ieee80211/mac/framesequence/HeSoundingFs.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeMuMimoCsiManager.h"
#include "inet/linklayer/ieee80211/mac/contract/ITx.h"
#include "inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeDlScheduler.h"

namespace inet {
namespace ieee80211 {

class HeHcf;
class FrameSequenceContext;

class INET_API HeSoundingCoordinator : public omnetpp::cSimpleModule
{
  public:
    struct SoundingTarget {
        MacAddress address;
        uint16_t aid = 0;
        int maxNss = 1;
    };

  protected:
    // State machine for the STA-side sounding protocol.
    bool ndpAnnouncementReceived = false;
    bool ndpReceived = false;
    uint8_t soundingDialogToken = 0;
    std::vector<SoundingTarget> soundingTargets;

    uint8_t nextSoundingDialogToken = 1;
    uint32_t nextTriggerId = 1;

  public:
    HeSoundingCoordinator() {}
    virtual ~HeSoundingCoordinator() {}

    // AP: check if we need sounding and initiate it
    bool tryStartSoundingSequence(AccessCategory ac,
                                  const IIeee80211HeDlScheduler::ScheduleContext& scheduleContext,
                                  IFrameSequenceHandler *frameSequenceHandler,
                                  Ieee80211Mac *mac,
                                  physicallayer::Ieee80211ModeSet *modeSet,
                                  HeMuMimoCsiManager& csiManager,
                                  FrameSequenceContext *context,
                                  HeHcf *hcf);

    // AP/STA: process received action/trigger sounding frames
    bool processSoundingFrame(Packet *packet,
                              const Ptr<const Ieee80211MacHeader>& header,
                              Ieee80211Mac *mac,
                              physicallayer::Ieee80211ModeSet *modeSet,
                              HeMuMimoCsiManager& csiManager,
                              ITx *tx,
                              ITx::ICallback *callback);

    // STA: process legacy preamble (NDP)
    void processLegacyPreamble(Packet *packet) {
        if (ndpAnnouncementReceived) {
            ndpReceived = true;
        }
    }

    void resetStaSoundingState() {
        ndpAnnouncementReceived = false;
        ndpReceived = false;
    }
};

} // namespace ieee80211
} // namespace inet

#endif // __INET_HESOUNDINGCOORDINATOR_H
