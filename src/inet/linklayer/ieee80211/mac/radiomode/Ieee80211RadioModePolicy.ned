//
// Copyright (C) 2026 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.linklayer.ieee80211.mac.radiomode;

import inet.common.SimpleModule;
import inet.linklayer.ieee80211.mac.contract.IRadioModePolicy;

//
// Deterministically arbitrates MAC radio-mode intent without commanding the radio.
//
simple Ieee80211RadioModePolicy extends SimpleModule like IRadioModePolicy
{
    parameters:
        @class(Ieee80211RadioModePolicy);
        @display("i=block/control");
}
