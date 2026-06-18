// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.linklayer.ieee80211.mac.coordinationfunction;

simple HeUlCoordinator
{
    parameters:
        @class(HeUlCoordinator);
        bool enabled = default(false);
        double reportMaxAge @unit(s) = default(100ms);
        int ocwMin = default(7);
        int ocwMax = default(31);
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
