//
// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.linklayer.ieee80211.mac.scheduler;

import inet.linklayer.ieee80211.mac.scheduler.HeDlSchedulerEqualSizedRUs;

//
// EHT version of the Equal Sized RU scheduler (uses MRUs).
//
simple EhtDlSchedulerEqualSizedMRUs extends HeDlSchedulerEqualSizedRUs
{
    parameters:
        @class(EhtDlSchedulerEqualSizedMRUs);
}
