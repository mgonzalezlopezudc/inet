//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.linklayer.ieee80211.mac.scheduler;

//
// Downlink OFDMA scheduler that favors stations with backlog compatible with
// a common-duration MU PPDU. deltaPlMax bounds the path-loss spread among the
// selected STAs so that an extreme link does not dominate the allocation.
//
simple HeDlSchedulerBacklogBased like IIeee80211HeDlScheduler
{
    parameters:
        @class(HeDlSchedulerBacklogBased);
        int maxMuStations = default(-1); // Maximum concurrently scheduled STAs; -1 uses the channel-layout maximum
        double deltaPlMax @unit(dB) = default(10dB); // Largest permitted path-loss difference between selected STAs
        int smallBacklogThreshold @unit(B) = default(80B);
        int mediumBacklogThreshold @unit(B) = default(500B);
        int mtuBacklogThreshold @unit(B) = default(1500B);
        int largeBacklogThreshold @unit(B) = default(6000B);
        double lowDurationRatio = default(0.5);
        double highDurationRatio = default(1.5);
        int maxDurationAlignmentIterations = default(16);
        double thermalNoisePsd = default(-174); // Thermal-noise PSD in dBm/Hz used for SNR estimation
        string heMcsSnrThresholds = default("4 7 10 13 16 19 21 24 27 30 33 36"); // Minimum SNRs in dB for HE MCS 0..11
        string heRateControlModule = default(""); // Optional IIeee80211HeRateControl provider
        @display("i=block/app");
}
