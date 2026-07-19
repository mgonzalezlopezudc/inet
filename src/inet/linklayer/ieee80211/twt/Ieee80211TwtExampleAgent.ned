package inet.linklayer.ieee80211.twt;

import inet.linklayer.ieee80211.mgmt.Ieee80211AgentSta;

simple Ieee80211TwtExampleAgent extends Ieee80211AgentSta
{
    parameters:
        @class(Ieee80211TwtExampleAgent);
        bool requestBroadcast = default(false);
        bool announced = default(false);
        bool implicit = default(true);
        double wakeInterval @unit(s) = default(100ms);
        double wakeDuration @unit(s) = default(10ms);
        double firstWakeOffset @unit(s) = default(20ms);
        double twtSetupRetryInterval @unit(s) = default(-1s); // Negative disables retry while a setup request is unanswered or refused
        int twtSetupMaxAttempts = default(1); // Maximum transmitted setup requests; -1 means unlimited
        int broadcastId = default(0);
}
