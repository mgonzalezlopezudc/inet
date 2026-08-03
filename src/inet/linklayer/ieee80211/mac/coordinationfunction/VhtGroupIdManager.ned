// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.linklayer.ieee80211.mac.coordinationfunction;

import inet.common.SimpleModule;
import inet.linklayer.ieee80211.mac.contract.IVhtGroupIdManager;

simple VhtGroupIdManager extends SimpleModule like IVhtGroupIdManager
{
    parameters:
        @class(VhtGroupIdManager);
        @display("i=block/table");
        double pendingTimeout @unit(s) = default(20ms);
}
