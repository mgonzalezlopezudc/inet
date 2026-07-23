//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/linklayer/ieee80211/mac/framesequence/EhtUlMuTxOpFs.h"

namespace inet {
namespace ieee80211 {

EhtUlMuTxOpFs::EhtUlMuTxOpFs(HeUlCoordinator *coordinator, HeHcf *callback, const HeUlMuPlan& plan,
                             physicallayer::Ieee80211ModeSet *modeSet,
                             MacAddress apAddress, bool ehtEnabled)
    : HeUlMuTxOpFs(coordinator, callback, plan, modeSet, apAddress)
{
    this->ehtEnabled = ehtEnabled;
}

EhtUlMuTxOpFs::~EhtUlMuTxOpFs()
{
}

} // namespace ieee80211
} // namespace inet
