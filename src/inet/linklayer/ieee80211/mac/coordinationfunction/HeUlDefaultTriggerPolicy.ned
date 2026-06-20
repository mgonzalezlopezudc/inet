// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.linklayer.ieee80211.mac.coordinationfunction;

//
// Default policy for selecting HE uplink Trigger frames.
//
// It requests a BSRP Trigger while associated STAs need fresh buffer-status
// reports, then requests a Basic Trigger when known backlog or a pending retry
// can be served. minimumTriggerInterval prevents a succession of triggers
// from consuming every AP TXOP.
//
simple HeUlDefaultTriggerPolicy like IIeee80211HeUlTriggerPolicy
{
    parameters:
        @class(HeUlDefaultTriggerPolicy);
        double minimumTriggerInterval @unit(s) = default(1ms); // Minimum time between two AP Trigger frames
}
