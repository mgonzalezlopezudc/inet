// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.linklayer.ieee80211.mac.coordinationfunction;

simple HeUlDefaultTriggerPolicy like IIeee80211HeUlTriggerPolicy
{
    parameters:
        @class(HeUlDefaultTriggerPolicy);
        double minimumTriggerInterval @unit(s) = default(1ms);
}
