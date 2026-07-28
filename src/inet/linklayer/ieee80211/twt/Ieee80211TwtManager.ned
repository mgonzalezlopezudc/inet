package inet.linklayer.ieee80211.twt;

import inet.common.SimpleModule;

simple Ieee80211TwtManager extends SimpleModule like IIeee80211TwtManager
{
    parameters:
        @class(Ieee80211TwtManager);
        string macModule;
        string mibModule;
        bool enabled = default(false);
        int maxIndividualAgreementsPerPeer = default(8);
        int maxBroadcastMembers = default(64);
        double defaultWakeInterval @unit(s) = default(100ms);
        double defaultWakeDuration @unit(s) = default(10ms);
        double firstWakeOffset @unit(s) = default(10ms);
        int defaultPersistence = default(255);
        bool allowExplicit = default(true);
        bool allowAnnounced = default(true);
        bool allowBroadcast = default(true);
        bool allowTriggerEnabled = default(true);
        @signal[twtStateChanged](type=inet::ieee80211::TwtStateChangedEvent);
        @signal[twtStationAwake](type=long);
        @signal[twtActiveServicePeriodCount](type=unsigned long);
        @signal[twtAgreementCount](type=unsigned long);
        @statistic[twtStationAwake](title="TWT station awake state"; source=twtStationAwake; record=vector; interpolationmode=sample-hold);
        @statistic[twtActiveServicePeriodCount](title="Active TWT service periods"; source=twtActiveServicePeriodCount; record=vector; interpolationmode=sample-hold);
        @statistic[twtAgreementCount](title="Installed TWT agreements and broadcast schedules"; source=twtAgreementCount; record=vector; interpolationmode=sample-hold);
        @display("i=block/clock");
}
