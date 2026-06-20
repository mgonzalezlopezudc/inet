// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.linklayer.ieee80211.mac.coordinationfunction;

//
// Interface for policies that decide whether an HE AP should send a Basic or
// BSRP Trigger in its next uplink OFDMA opportunity.
//
moduleinterface IIeee80211HeUlTriggerPolicy
{
    parameters:
        @display("i=block/control");
}
