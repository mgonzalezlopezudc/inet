//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211HELINKPHYCONTEXTADAPTER_H
#define __INET_IEEE80211HELINKPHYCONTEXTADAPTER_H

#include "inet/linklayer/ieee80211/mac/contract/IIeee80211HeLinkPhyContext.h"

namespace inet {
namespace ieee80211 {

class Ieee80211Mac;

/** The sole typed projection from concrete IEEE 802.11 radio/MIB state. */
class INET_API Ieee80211HeLinkPhyContextAdapter final : public IIeee80211HeLinkPhyContext
{
  private:
    cModule *owner;
    Ieee80211Mac *mac;

  public:
    Ieee80211HeLinkPhyContextAdapter(cModule *owner, Ieee80211Mac *mac) : owner(owner), mac(mac) {}

    virtual Ieee80211HeLinkPhySnapshot getSnapshot() const override;
    virtual Ieee80211HePeerLinkSnapshot getPeerSnapshot(const MacAddress& address,
            simtime_t maximumLinkEstimateAge) const override;
};

} // namespace ieee80211
} // namespace inet

#endif
