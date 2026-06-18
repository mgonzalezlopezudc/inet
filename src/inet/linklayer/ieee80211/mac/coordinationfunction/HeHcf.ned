// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.linklayer.ieee80211.mac.coordinationfunction;

import inet.common.Module;
import inet.linklayer.ieee80211.mac.scheduler.IIeee80211HeDlScheduler;
import inet.linklayer.ieee80211.mac.scheduler.IIeee80211HeUlScheduler;

//
// Extends Hcf with IEEE 802.11ax Downlink OFDMA multi-user scheduling.
// When the winning EDCAF has packets queued for two or more distinct
// destination STAs and the "ax" modeSet is active, this module launches
// a HeDlMuTxOpFs frame sequence instead of the standard HcfFs, enabling
// concurrent per-RU transmission to multiple STAs in a single TXOP.
//
module HeHcf extends Hcf
{
    parameters:
        @class(HeHcf);
        double totalTransmitPower @unit(W) = default(20mW);
        double receiverNoiseFigure @unit(dB) = default(10dB);
        double linkEstimateMaxAge @unit(s) = default(1s);
        int maxAmpduMpduCount = default(16);
        int maxHeMuPsduLength @unit(B) = default(6500631B);
        double maxHeMuPpduDuration @unit(s) = default(5.484ms);
        bool enableUlMuOfdma = default(false);
        double ulTriggerCheckInterval @unit(s) = default(1ms);
        double ulTargetRssiMargin @unit(dB) = default(10dB);
        double maxHeTbPpduDuration @unit(s) = default(1ms);

    submodules:
        dlScheduler: <default("HeDlSchedulerEqualSizedRUs")> like IIeee80211HeDlScheduler {
            parameters:
                @display("p=750,100");
        }
        ulScheduler: <default("HeUlSchedulerBacklogBased")> like IIeee80211HeUlScheduler {
            parameters:
                @display("p=900,100");
        }
        ulTriggerPolicy: <default("HeUlDefaultTriggerPolicy")> like IIeee80211HeUlTriggerPolicy {
            parameters:
                @display("p=900,180");
        }
        ulCoordinator: HeUlCoordinator {
            parameters:
                enabled = parent.enableUlMuOfdma;
                @display("p=900,260");
        }
        queueBanks: Module {
            parameters:
                @display("p=750,250;i=block/queue");
        }
}
