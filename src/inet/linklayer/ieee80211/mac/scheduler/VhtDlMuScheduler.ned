// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.linklayer.ieee80211.mac.scheduler;

import inet.common.SimpleModule;
import inet.linklayer.ieee80211.mac.contract.IIeee80211VhtDlMuScheduler;

simple VhtDlMuScheduler extends SimpleModule like IIeee80211VhtDlMuScheduler
{
    parameters:
        @class(VhtDlMuScheduler);
        @display("i=block/queue");
}
