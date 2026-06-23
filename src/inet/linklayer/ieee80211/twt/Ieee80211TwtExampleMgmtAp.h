//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_IEEE80211TWTEXAMPLEMGMTAP_H
#define __INET_IEEE80211TWTEXAMPLEMGMTAP_H

#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtAp.h"

namespace inet {
namespace ieee80211 {

/** Scenario-only AP management module that advertises one broadcast TWT. */
class INET_API Ieee80211TwtExampleMgmtAp : public Ieee80211MgmtAp
{
  protected:
    virtual void initialize(int stage) override;
};

} // namespace ieee80211
} // namespace inet

#endif
