//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/coordinationfunction/HcfFeatureSet.h"

namespace inet {
namespace ieee80211 {

Define_Module(CommonHcfFeatureSet);
Define_Module(VhtHcfFeatureSet);
Define_Module(HeHcfFeatureSet);

HeHcfFeatureSet::~HeHcfFeatureSet()
{
    triggeredUlExchangeService.shutdown();
}

} // namespace ieee80211
} // namespace inet
