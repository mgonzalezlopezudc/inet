// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.linklayer.ieee80211.mac.scheduler;

//
// HE uplink OFDMA scheduler that ranks stations by reported backlog before
// assigning equal-sized scheduled RUs, while retaining a bounded UORA pool.
//
simple HeUlSchedulerBacklogBased like IIeee80211HeUlScheduler
{
    parameters:
        @class(HeUlSchedulerBacklogBased);
        int maxMuStations = default(8); // Maximum scheduled STAs in one Trigger exchange
        int minRandomAccessRus = default(1); // Minimum RUs reserved for UORA when space permits
        int maxRandomAccessRus = default(4); // Maximum RUs reserved for UORA
        int defaultMcs = default(0); // MCS encoded for scheduled and random-access HE-TB responses
}
