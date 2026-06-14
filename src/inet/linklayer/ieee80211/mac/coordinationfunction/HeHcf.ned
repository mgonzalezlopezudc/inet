// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.linklayer.ieee80211.mac.coordinationfunction;

import inet.linklayer.ieee80211.mac.scheduler.IIeee80211HeDlScheduler;

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

    submodules:
        dlScheduler: <default("HeDlSchedulerEqualSizedRUs")> like IIeee80211HeDlScheduler {
            parameters:
                @display("p=750,100");
        }
}
