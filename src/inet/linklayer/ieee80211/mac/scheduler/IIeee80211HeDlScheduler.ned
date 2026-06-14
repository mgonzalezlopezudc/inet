// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.linklayer.ieee80211.mac.scheduler;

//
// Module interface for IEEE 802.11ax Downlink OFDMA schedulers.
// Implementations decide how to partition the primary channel into Resource
// Units (RUs) and assign each RU to a destination STA.
//
moduleinterface IIeee80211HeDlScheduler
{
    parameters:
        @display("i=block/app");
}
