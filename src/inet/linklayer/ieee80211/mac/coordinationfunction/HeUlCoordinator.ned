// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.linklayer.ieee80211.mac.coordinationfunction;

//
// Maintains AP-side state for trigger-based HE uplink OFDMA.
//
// The coordinator records buffer-status reports by association ID, delegates
// trigger selection and RU allocation to the configured policies, and tracks
// the OFDMA random-access contention window. It is created inside HeHcf and
// is not normally connected directly by a network model.
//
simple HeUlCoordinator
{
    parameters:
        @class(HeUlCoordinator);
        bool enabled = default(false); // Enables trigger scheduling and buffer-status processing
        double reportMaxAge @unit(s) = default(100ms); // Buffer-status reports older than this are not considered fresh
        int ocwMin = default(7); // Minimum OFDMA random-access contention window
        int ocwMax = default(31); // Maximum OFDMA random-access contention window
        @signal[heUlBasicTriggerSent](type=long);
        @signal[heUlBsrpTriggerSent](type=long);
        @signal[heUlBufferStatusUpdated](type=long);
        @signal[heUlScheduledUsers](type=long);
        @signal[heUlRandomAccessRus](type=long);
        @signal[heUlRandomAccessAttempt](type=long);
        @signal[heUlRandomAccessSuccess](type=long);
        @statistic[heUlBasicTriggerSent](source=heUlBasicTriggerSent; record=count);
        @statistic[heUlBsrpTriggerSent](source=heUlBsrpTriggerSent; record=count);
        @statistic[heUlBufferStatusUpdated](source=heUlBufferStatusUpdated; record=count);
        @statistic[heUlScheduledUsers](source=heUlScheduledUsers; record=count,sum,max);
        @statistic[heUlRandomAccessRus](source=heUlRandomAccessRus; record=count,sum,max);
        @statistic[heUlRandomAccessAttempt](source=heUlRandomAccessAttempt; record=count);
        @statistic[heUlRandomAccessSuccess](source=heUlRandomAccessSuccess; record=count);
}
