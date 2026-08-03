// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.linklayer.ieee80211.mac.coordinationfunction;

import inet.common.SimpleModule;
import inet.linklayer.ieee80211.mac.contract.IVhtSoundingCoordinator;

simple VhtSoundingCoordinator extends SimpleModule like IVhtSoundingCoordinator
{
    parameters:
        double retryInterval @unit(s) = default(100ms);
        @class(VhtSoundingCoordinator);
        @display("i=block/control");
}
