package inet.linklayer.ieee80211.twt;

import inet.linklayer.ieee80211.mgmt.Ieee80211MgmtAp;

simple Ieee80211TwtExampleMgmtAp extends Ieee80211MgmtAp
{
    parameters:
        @class(Ieee80211TwtExampleMgmtAp);
        bool createBroadcastSchedule = default(false);
        int broadcastId = default(0);
        bool implicit = default(true);
        bool announced = default(false);
        bool triggerEnabled = default(false);
        int persistence = default(8);
        double wakeInterval @unit(s) = default(100ms);
        double wakeDuration @unit(s) = default(10ms);
        double firstWakeOffset @unit(s) = default(20ms);
}
