// Copyright (C) 2026 INET Framework contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

package inet.linklayer.ieee80211.mac.coordinationfunction;

import inet.linklayer.ieee80211.mac.contract.IVhtSoundingCoordinator;
import inet.linklayer.ieee80211.mac.contract.IVhtGroupIdManager;
import inet.linklayer.ieee80211.mac.contract.IIeee80211VhtDlMuScheduler;

// Opt-in packet-level VHT SU sounding and beamforming. Disabled instances are
// behaviorally identical to Hcf; MU group state and scheduling are not modeled.
module VhtHcf extends Hcf
{
    parameters:
        @class(VhtHcf);
        @display("i=block/control");
        featureSet.typename = default("VhtHcfFeatureSet");
        bool enableVhtSuBeamforming = default(false);
        bool enableVhtDlMuMimo = default(false);
        int vhtDlMuGroupId = default(1); // Selected VHT MU group ID, 1..62
        double vhtCsiValidityDuration @unit(s) = default(100ms);
        double vhtSoundingRetryInterval @unit(s) = default(100ms);
        double beamformingGain @unit(dB) = default(3dB); // Packet-level receive-power benefit only

    submodules:
        soundingCoordinator: <default("VhtSoundingCoordinator")> like IVhtSoundingCoordinator {
            parameters:
                retryInterval = parent.vhtSoundingRetryInterval;
                @display("p=750,100;i=block/control");
        }
        groupIdManager: <default("VhtGroupIdManager")> like IVhtGroupIdManager {
            parameters:
                @display("p=750,175;i=block/table");
        }
        dlMuScheduler: <default("VhtDlMuScheduler")> like IIeee80211VhtDlMuScheduler {
            parameters:
                @display("p=750,250;i=block/queue");
        }
}
