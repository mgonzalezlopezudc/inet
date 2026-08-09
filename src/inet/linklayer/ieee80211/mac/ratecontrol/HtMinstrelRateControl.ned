//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.linklayer.ieee80211.mac.ratecontrol;

import inet.common.SimpleModule;
import inet.linklayer.ieee80211.mac.contract.IIeee80211PeerRateControl;
import inet.linklayer.ieee80211.mac.contract.IRateControl;

// Minstrel-style HT rate controller for peer-aware unicast selection.
simple HtMinstrelRateControl extends SimpleModule like IRateControl, IIeee80211PeerRateControl
{
    parameters:
        @class(HtMinstrelRateControl);
        double initialRate @unit(bps) = default(-1bps);
        double updateInterval @unit(s) = default(100ms);
        double ewmaWeight = default(0.75);
        double lookaroundRatio = default(0.1);
        double initialSuccessProbability = default(0.9);
        bool seedFromSnir = default(true);
        double snirMcs0Threshold @unit(dB) = default(4dB);
        double snirMcsStep @unit(dB) = default(3dB);
        int minMcs = default(0);
        int maxMcs = default(32);
        int maxNss = default(4);
        displayStringTextFormat = default("HT Minstrel");
        @display("i=block/cogwheel");
        @signal[datarateChanged];
        @signal[htRateSelectedMcs](type=long);
        @signal[htRateSelectedNss](type=long);
        @signal[htRateProbe](type=long);
        @signal[htRateSuccessProbability](type=double);
        @signal[htRateTxSuccess](type=long);
        @signal[htRateRetryCount](type=long);
        @statistic[datarateChanged](title="datarate"; record=vector; interpolationmode=sample-hold);
        @statistic[htRateSelectedMcs](title="selected HT MCS"; record=vector; interpolationmode=sample-hold);
        @statistic[htRateSelectedNss](title="selected HT NSS"; record=vector; interpolationmode=sample-hold);
        @statistic[htRateProbe](title="HT probe selections"; record=count);
        @statistic[htRateSuccessProbability](title="HT EWMA success probability"; record=vector; interpolationmode=sample-hold);
        @statistic[htRateTxSuccess](title="HT transmission outcome"; record=vector; interpolationmode=none);
        @statistic[htRateRetryCount](title="HT transmission retry count"; record=vector; interpolationmode=none);
}
