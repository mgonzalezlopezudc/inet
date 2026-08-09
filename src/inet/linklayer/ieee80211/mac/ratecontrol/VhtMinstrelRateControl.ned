//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.linklayer.ieee80211.mac.ratecontrol;

import inet.common.SimpleModule;
import inet.linklayer.ieee80211.mac.contract.IIeee80211PeerRateControl;
import inet.linklayer.ieee80211.mac.contract.IRateControl;

// Minstrel-style VHT rate controller for peer-aware unicast selection.
simple VhtMinstrelRateControl extends SimpleModule like IRateControl, IIeee80211PeerRateControl
{
    parameters:
        @class(VhtMinstrelRateControl);
        double initialRate @unit(bps) = default(-1bps);
        double updateInterval @unit(s) = default(100ms);
        double ewmaWeight = default(0.75);
        double lookaroundRatio = default(0.1);
        double initialSuccessProbability = default(0.9);
        bool seedFromSnir = default(true);
        double snirMcs0Threshold @unit(dB) = default(4dB);
        double snirMcsStep @unit(dB) = default(3dB);
        int minMcs = default(0);
        int maxMcs = default(9);
        int maxNss = default(8);
        displayStringTextFormat = default("VHT Minstrel");
        @display("i=block/cogwheel");
        @signal[datarateChanged];
        @signal[vhtRateSelectedMcs](type=long);
        @signal[vhtRateSelectedNss](type=long);
        @signal[vhtRateProbe](type=long);
        @signal[vhtRateSuccessProbability](type=double);
        @signal[vhtRateTxSuccess](type=long);
        @signal[vhtRateRetryCount](type=long);
        @statistic[datarateChanged](title="datarate"; record=vector; interpolationmode=sample-hold);
        @statistic[vhtRateSelectedMcs](title="selected VHT MCS"; record=vector; interpolationmode=sample-hold);
        @statistic[vhtRateSelectedNss](title="selected VHT NSS"; record=vector; interpolationmode=sample-hold);
        @statistic[vhtRateProbe](title="VHT probe selections"; record=count);
        @statistic[vhtRateSuccessProbability](title="VHT EWMA success probability"; record=vector; interpolationmode=sample-hold);
        @statistic[vhtRateTxSuccess](title="VHT transmission outcome"; record=vector; interpolationmode=none);
        @statistic[vhtRateRetryCount](title="VHT transmission retry count"; record=vector; interpolationmode=none);
}
