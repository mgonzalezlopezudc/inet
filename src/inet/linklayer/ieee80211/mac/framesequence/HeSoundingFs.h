//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HESOUNDINGFS_H
#define __INET_HESOUNDINGFS_H

#include <vector>
#include "inet/common/Units.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/contract/IFrameSequence.h"
#include "inet/linklayer/ieee80211/mac/framesequence/FrameSequenceStep.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HeMuMimoCsiManager.h"
#include "inet/linklayer/ieee80211/mib/Ieee80211Mib.h"
#include "inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h"

namespace inet {
namespace ieee80211 {

using namespace inet::units::values;

class INET_API HeSoundingFs : public IFrameSequence
{
  public:
    struct TargetSta {
        MacAddress address;
        uint16_t aid = 0;
        int maxNss = 1;
    };

  protected:
    int firstStep = -1;
    int step = -1;

    Ieee80211Mib *mib = nullptr;
    std::vector<TargetSta> targets;
    physicallayer::Ieee80211ModeSet *modeSet = nullptr;
    HeMuMimoCsiManager *csiManager = nullptr;
    Hz bandwidth;
    uint8_t dialogToken = 1;
    uint32_t triggerId = 0;
    MacAddress apAddress;

  protected:
    Packet *buildNdpaFrame(FrameSequenceContext *context);
    Packet *buildNdpFrame(FrameSequenceContext *context);
    Packet *buildBfrpTriggerFrame(FrameSequenceContext *context);
    void processFeedbacks(FrameSequenceContext *context);

  public:
    HeSoundingFs(Ieee80211Mib *mib,
                 const std::vector<TargetSta>& targets,
                 physicallayer::Ieee80211ModeSet *modeSet,
                 HeMuMimoCsiManager *csiManager,
                 Hz bandwidth,
                 uint8_t dialogToken,
                 uint32_t triggerId);
    virtual ~HeSoundingFs() {}

    virtual void startSequence(FrameSequenceContext *context, int firstStep) override;
    virtual IFrameSequenceStep *prepareStep(FrameSequenceContext *context) override;
    virtual bool completeStep(FrameSequenceContext *context) override;

    virtual std::string getHistory() const override { return "HE-Sounding (NDPA-NDP-BFRP-Feedback)"; }
};

} // namespace ieee80211
} // namespace inet

#endif // __INET_HESOUNDINGFS_H
