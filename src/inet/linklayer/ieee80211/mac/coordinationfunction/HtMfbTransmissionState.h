//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_HTMFBTRANSMISSIONSTATE_H
#define __INET_HTMFBTRANSMISSIONSTATE_H

#include "inet/common/INETDefs.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/linklayer/ieee80211/mac/Ieee80211HtControl.h"

namespace inet {
namespace ieee80211 {

/** Owns pending HT MFB values and the standalone-transmission lifecycle token. */
class INET_API HtMfbTransmissionState
{
  public:
    struct Snapshot {
        MacAddress peer;
        Ieee80211HtMcsControl control;
    };

  private:
    Snapshot pending;
    bool standaloneTransmissionInProgress = false;

  public:
    Snapshot getPending() const { return pending; }
    void setPending(const MacAddress& peer, const Ieee80211HtMcsControl& control);
    void clearPending() { pending = {}; }
    void invalidate(const MacAddress& peer);

    void startStandaloneTransmission() { standaloneTransmissionInProgress = true; }
    bool completeStandaloneTransmission();
};

} // namespace ieee80211
} // namespace inet

#endif
