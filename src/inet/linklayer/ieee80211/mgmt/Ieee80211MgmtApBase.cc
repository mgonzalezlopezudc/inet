//
// Copyright (C) 2006 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/common/ProtocolTag_m.h"
#include "inet/linklayer/common/MacAddressTag_m.h"

#ifdef INET_WITH_ETHERNET
#include "inet/linklayer/ethernet/common/EthernetMacHeader_m.h"
#endif // ifdef INET_WITH_ETHERNET

#include "inet/linklayer/ieee80211/mgmt/Ieee80211MgmtApBase.h"

namespace inet {

namespace ieee80211 {

void Ieee80211MgmtApBase::initialize(int stage)
{
    Ieee80211MgmtBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        mib->mode = Ieee80211Mib::INFRASTRUCTURE;
        mib->setBssStationType(Ieee80211Mib::ACCESS_POINT);
        mib->setBssIdentity(par("ssid").stdstringValue(), mib->getBssid());
    }
    else if (stage == INITSTAGE_LINK_LAYER)
        mib->setBssIdentity(mib->getSsid(), mib->address);
}

} // namespace ieee80211

} // namespace inet
