// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.linklayer.ieee80211.mac.scheduler;

//
// Capacity-aware HE uplink OFDMA scheduler. It preserves the deterministic
// least-recently-served prefix, scores standard mixed-RU layouts at the
// finalized HE-TB boundary, and retains the configured UORA reservation.
//
simple HeUlSchedulerBacklogBased like IIeee80211HeUlScheduler
{
    parameters:
        @class(HeUlSchedulerBacklogBased);
        int maxMuStations = default(8); // Maximum scheduled STAs in one Trigger exchange
        int maxOptimizedStations = default(8); // Largest fairness prefix searched exactly
        int minRandomAccessRus = default(1); // Minimum RUs reserved for UORA when space permits
        int maxRandomAccessRus = default(4); // Maximum RUs reserved for UORA
        string randomAccessTarget = default("associated"); // "associated" uses AID12=0; "unassociated" uses AID12=2045
        int defaultMcs = default(0); // MCS encoded for scheduled and random-access HE-TB responses
        string mcsSelectionPolicy = default("rateControl"); // "rateControl" or direct estimated-SNR threshold selection
        string heMcsSnrThresholds = default("4 7 10 13 16 19 21 24 27 30 33 36"); // Minimum estimated SNRs in dB for HE MCS 0..11
        string heRateControlModule = default(""); // Optional IIeee80211HeRateControl provider for scheduled users
}
