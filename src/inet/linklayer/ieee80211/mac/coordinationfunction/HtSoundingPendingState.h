//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HTSOUNDINGPENDINGSTATE_H
#define __INET_HTSOUNDINGPENDINGSTATE_H

#include "inet/common/INETDefs.h"
#include "inet/common/Units.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/coordinationfunction/HtCsiCache.h"

namespace inet {
namespace ieee80211 {

using namespace inet::units::values;

/** Owns the value snapshot that correlates an HT NDP announcement with its NDP. */
class INET_API HtSoundingPendingState
{
  public:
    struct Snapshot {
        bool valid = false;
        MacAddress peer;
        uint64_t associationGeneration = 0;
        uint8_t requestToken = 0;
        uint8_t soundingNsts = 1;
        Ieee80211HtFeedbackKind feedbackKind = Ieee80211HtFeedbackKind::COMPRESSED_BEAMFORMING;
        Hz channelWidth = Hz(0);
        int transmitterRadioId = -1;
        simtime_t announcementReceptionEnd = -1;
    };

  private:
    Snapshot snapshot;

  public:
    Snapshot getSnapshot() const { return snapshot; }
    void setSnapshot(const Snapshot& value) { snapshot = value; }
    void clear() { snapshot = {}; }
    void invalidate(const MacAddress& peer);
};

} // namespace ieee80211
} // namespace inet

#endif
