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
        @signal[heUlBufferStatusReportedBytes](type=long);
        @signal[heUlBufferStatusScheduledBytes](type=long);
        @signal[heUlStaleBufferStatus](type=long);
        @signal[heUlScheduledUsers](type=long);
        @signal[heUlRandomAccessRus](type=long);
        @signal[heUlRandomAccessAttempt](type=long);
        @signal[heUlRandomAccessSuccess](type=long);
        @signal[heUlTriggerDecisionCommitted](type=inet::ieee80211::HeUlTriggerDecisionEvent);
        @signal[heUlTriggerDecisionId](type=unsigned long);
        @statistic[heUlBasicTriggerSent](source=heUlBasicTriggerSent; record=count);
        @statistic[heUlBsrpTriggerSent](source=heUlBsrpTriggerSent; record=count);
        @statistic[heUlBufferStatusUpdated](source=heUlBufferStatusUpdated; record=count);
        @statistic[heUlBufferStatusReportedBytes](source=heUlBufferStatusReportedBytes; record=vector);
        @statistic[heUlBufferStatusScheduledBytes](source=heUlBufferStatusScheduledBytes; record=vector);
        @statistic[heUlStaleBufferStatus](source=heUlStaleBufferStatus; record=count);
        @statistic[heUlScheduledUsers](source=heUlScheduledUsers; record=count,sum,max);
        @statistic[heUlRandomAccessRus](source=heUlRandomAccessRus; record=count,sum,max);
        @statistic[heUlRandomAccessAttempt](source=heUlRandomAccessAttempt; record=count);
        @statistic[heUlRandomAccessSuccess](source=heUlRandomAccessSuccess; record=count);
        @statistic[heUlTriggerDecisionId](title="HE UL Trigger decision identifier"; source=heUlTriggerDecisionId; record=vector; interpolationmode=none);
        @statistic[heUlTriggerDecisionTriggerId](title="HE UL Trigger decision identifier per user"; source=heUlTriggerDecisionTriggerId(heUlTriggerDecisionCommitted); record=vector?; interpolationmode=none);
        @statistic[heUlTriggerDecisionTriggerType](title="HE UL Trigger type per user"; source=heUlTriggerDecisionTriggerType(heUlTriggerDecisionCommitted); record=vector?; interpolationmode=none);
        @statistic[heUlTriggerDecisionUserOrdinal](title="HE UL Trigger user ordinal"; source=heUlTriggerDecisionUserOrdinal(heUlTriggerDecisionCommitted); record=vector?; interpolationmode=none);
        @statistic[heUlTriggerDecisionAssociationId](title="HE UL Trigger user association identifier"; source=heUlTriggerDecisionAssociationId(heUlTriggerDecisionCommitted); record=vector?; interpolationmode=none);
        @statistic[heUlTriggerDecisionBacklogBytes](title="HE UL Trigger user total backlog"; source=heUlTriggerDecisionBacklogBytes(heUlTriggerDecisionCommitted); record=vector?; unit=B; interpolationmode=none);
        @statistic[heUlTriggerDecisionReportedBytes](title="HE UL Trigger user reported backlog"; source=heUlTriggerDecisionReportedBytes(heUlTriggerDecisionCommitted); record=vector?; unit=B; interpolationmode=none);
        @statistic[heUlTriggerDecisionPlannedBytes](title="HE UL Trigger user planned payload"; source=heUlTriggerDecisionPlannedBytes(heUlTriggerDecisionCommitted); record=vector?; unit=B; interpolationmode=none);
        @statistic[heUlTriggerDecisionTid](title="HE UL Trigger user traffic identifier"; source=heUlTriggerDecisionTid(heUlTriggerDecisionCommitted); record=vector?; interpolationmode=none);
        @statistic[heUlTriggerDecisionAccessCategory](title="HE UL Trigger user access category"; source=heUlTriggerDecisionAccessCategory(heUlTriggerDecisionCommitted); record=vector?; interpolationmode=none);
        @statistic[heUlTriggerDecisionSelected](title="HE UL Trigger user selected flag"; source=heUlTriggerDecisionSelected(heUlTriggerDecisionCommitted); record=vector?; interpolationmode=none);
        @statistic[heUlTriggerDecisionRuIndex](title="HE UL Trigger user RU index"; source=heUlTriggerDecisionRuIndex(heUlTriggerDecisionCommitted); record=vector?; interpolationmode=none);
        @statistic[heUlTriggerDecisionRuToneSize](title="HE UL Trigger user RU tone size"; source=heUlTriggerDecisionRuToneSize(heUlTriggerDecisionCommitted); record=vector?; interpolationmode=none);
        @statistic[heUlTriggerDecisionRuToneOffset](title="HE UL Trigger user RU tone offset"; source=heUlTriggerDecisionRuToneOffset(heUlTriggerDecisionCommitted); record=vector?; interpolationmode=none);
}
