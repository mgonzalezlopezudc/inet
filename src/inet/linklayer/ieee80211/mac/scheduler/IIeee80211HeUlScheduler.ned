// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.linklayer.ieee80211.mac.scheduler;

//
// Interface for HE trigger-based uplink OFDMA schedulers. Implementations
// select scheduled users, optionally reserve random-access RUs, and return
// the common HE-TB duration and PHY settings for a Trigger exchange.
//
moduleinterface IIeee80211HeUlScheduler
{
    parameters:
        @display("i=block/control");
}
