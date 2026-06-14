// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.linklayer.ieee80211.mac.scheduler;

//
// Downlink OFDMA scheduler that partitions the primary channel into N
// equal-bandwidth Resource Units (RUs) and assigns one per selected STA.
// When the winning EDCAF has packets for more than one destination, at most
// maxMuStations STAs are scheduled concurrently per TXOP.
//
simple HeDlSchedulerEqualSizedRUs like IIeee80211HeDlScheduler
{
    parameters:
        @class(HeDlSchedulerEqualSizedRUs);
        int maxMuStations = default(4);   // Maximum number of STAs to schedule per MU-TXOP
        @display("i=block/app");
}
