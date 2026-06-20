// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.linklayer.ieee80211.mac.scheduler;

//
// HE uplink OFDMA scheduler that assigns an equal-sized RU to each selected
// station and reserves the configured number of remaining RUs for random
// access when possible.
//
simple HeUlSchedulerEqualSizedRUs like IIeee80211HeUlScheduler
{
    parameters:
        @class(HeUlSchedulerEqualSizedRUs);
        int maxMuStations = default(8); // Maximum scheduled STAs in one Trigger exchange
        int minRandomAccessRus = default(1); // Minimum RUs reserved for UORA when space permits
        int maxRandomAccessRus = default(4); // Maximum RUs reserved for UORA
        int defaultMcs = default(0); // MCS encoded for scheduled and random-access HE-TB responses
}
