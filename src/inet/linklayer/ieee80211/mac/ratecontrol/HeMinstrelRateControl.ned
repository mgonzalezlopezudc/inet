//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.linklayer.ieee80211.mac.ratecontrol;

import inet.common.SimpleModule;
import inet.linklayer.ieee80211.mac.contract.IIeee80211HeRateControl;
import inet.linklayer.ieee80211.mac.contract.IRateControl;

//
// Minstrel-style HE rate controller with per-peer MCS/NSS EWMA and probing.
//
simple HeMinstrelRateControl extends SimpleModule like IRateControl, IIeee80211HeRateControl
{
    parameters:
        @class(HeMinstrelRateControl);
        double initialRate @unit(bps) = default(-1bps); // inherited initialization fallback
        double updateInterval @unit(s) = default(100ms);
        double ewmaWeight = default(0.75);
        double lookaroundRatio = default(0.1);
        double initialSuccessProbability = default(0.9);
        bool seedFromSnir = default(true);
        bool enableExtendedRangeSu = default(false);
        bool preferDcm = default(false);
        double snirMcs0Threshold @unit(dB) = default(4dB);
        double snirMcsStep @unit(dB) = default(3dB);
        int minMcs = default(0);
        int maxMcs = default(11);
        int maxNss = default(1);
        displayStringTextFormat = default("HE Minstrel");
        @display("i=block/cogwheel");
        @signal[datarateChanged];
        @signal[heRateSelectedMcs](type=long);
        @signal[heRateSelectedNss](type=long);
        @signal[heRateProbe](type=long);
        @signal[heRateSuccessProbability](type=double);
        @signal[heRateTxSuccess](type=long);
        @signal[heRateRetryCount](type=long);
        @statistic[datarateChanged](title="datarate"; record=vector; interpolationmode=sample-hold);
        @statistic[heRateSelectedMcs](title="selected HE MCS"; record=vector; interpolationmode=sample-hold);
        @statistic[heRateSelectedNss](title="selected HE NSS"; record=vector; interpolationmode=sample-hold);
        @statistic[heRateProbe](title="HE probe selections"; record=count);
        @statistic[heRateSuccessProbability](title="HE EWMA success probability"; record=vector; interpolationmode=sample-hold);
        @statistic[heRateTxSuccess](title="HE transmission outcome"; record=vector; interpolationmode=none);
        @statistic[heRateRetryCount](title="HE transmission retry count"; record=vector; interpolationmode=none);
}
