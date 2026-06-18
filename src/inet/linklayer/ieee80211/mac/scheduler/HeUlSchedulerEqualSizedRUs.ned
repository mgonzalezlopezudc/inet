// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.linklayer.ieee80211.mac.scheduler;

simple HeUlSchedulerEqualSizedRUs like IIeee80211HeUlScheduler
{
    parameters:
        @class(HeUlSchedulerEqualSizedRUs);
        int maxMuStations = default(8);
        int minRandomAccessRus = default(1);
        int maxRandomAccessRus = default(4);
        int defaultMcs = default(0);
}
