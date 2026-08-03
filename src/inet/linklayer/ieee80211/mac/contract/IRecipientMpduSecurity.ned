//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.linklayer.ieee80211.mac.contract;

//
// Interface for optional recipient-side MPDU integrity and replay checking.
//
moduleinterface IRecipientMpduSecurity
{
    parameters:
        @display("i=block/checker");
}
