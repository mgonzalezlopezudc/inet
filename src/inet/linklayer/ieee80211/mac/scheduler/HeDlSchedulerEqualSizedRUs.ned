// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.linklayer.ieee80211.mac.scheduler;

//
// Downlink OFDMA scheduler with equal-sized RU allocations.
//
// The scheduler ranks eligible stations by schedulingFunction, picks a
// standard equal-RU layout that fits the selected users, and aligns payload
// durations by adapting RU sizes where possible. Use "fBW" to prioritize
// backlog or "fHoL" to prioritize head-of-line delay.
//
simple HeDlSchedulerEqualSizedRUs like IIeee80211HeDlScheduler
{
    parameters:
        @class(HeDlSchedulerEqualSizedRUs);
        string schedulingFunction = default("fBW"); // "fBW" ranks backlog; "fHoL" ranks head-of-line delay
        int maxMuStations = default(-1); // Maximum concurrently scheduled STAs; -1 uses the channel-layout maximum
        int smallBacklogThreshold @unit(B) = default(80B);
        int mediumBacklogThreshold @unit(B) = default(500B);
        int mtuBacklogThreshold @unit(B) = default(1500B);
        int largeBacklogThreshold @unit(B) = default(6000B);
        double lowDurationRatio = default(0.5);
        double highDurationRatio = default(1.5);
        int maxDurationAlignmentIterations = default(0);
        double thermalNoisePsd = default(-174); // Thermal-noise PSD in dBm/Hz used for SNR estimation
        string heMcsSnrThresholds = default("4 7 10 13 16 19 21 24 27 30 33 36"); // Minimum SNRs in dB for HE MCS 0..11
        @display("i=block/app");
}
