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
        @display("i=block/clock");
}
