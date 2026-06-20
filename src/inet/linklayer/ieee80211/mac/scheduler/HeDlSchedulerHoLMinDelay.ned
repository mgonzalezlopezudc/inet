//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.linklayer.ieee80211.mac.scheduler;

//
// Downlink OFDMA scheduler that gives preference to the oldest eligible
// head-of-line packet, then uses HE RU fitting to align the selected users'
// transmission durations.
//
simple HeDlSchedulerHoLMinDelay like IIeee80211HeDlScheduler
{
    parameters:
        @class(HeDlSchedulerHoLMinDelay);
        int maxMuStations = default(-1); // Maximum concurrently scheduled STAs; -1 uses the channel-layout maximum
        int smallBacklogThreshold @unit(B) = default(80B);
        int mediumBacklogThreshold @unit(B) = default(500B);
        int mtuBacklogThreshold @unit(B) = default(1500B);
        int largeBacklogThreshold @unit(B) = default(6000B);
        double lowDurationRatio = default(0.5);
        double highDurationRatio = default(1.5);
        int maxDurationAlignmentIterations = default(16);
        double thermalNoisePsd = default(-174); // Thermal-noise PSD in dBm/Hz used for SNR estimation
        string heMcsSnrThresholds = default("4 7 10 13 16 19 21 24 27 30 33 36"); // Minimum SNRs in dB for HE MCS 0..11
        @display("i=block/app");
}
