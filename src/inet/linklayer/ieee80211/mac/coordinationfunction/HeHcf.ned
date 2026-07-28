// Copyright (C) 2026 INET Framework contributors
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.linklayer.ieee80211.mac.coordinationfunction;

import inet.common.Module;
import inet.linklayer.ieee80211.mac.scheduler.IIeee80211HeDlScheduler;
import inet.linklayer.ieee80211.mac.scheduler.IIeee80211HeUlScheduler;

//
// IEEE 802.11ax HCF with downlink and optional trigger-based uplink OFDMA.
//
// HeHcf is a drop-in replacement for Hcf in an AP MAC. For a winning EDCAF,
// it builds a downlink MU PPDU only when two or more eligible destination
// STAs have queued traffic and negotiated HE capabilities permit it. In all
// other cases it uses the inherited single-user frame sequence. The selected
// dlScheduler chooses RUs and PHY settings; per-STA queues isolate traffic
// while it is waiting for MU service.
//
// Set enableUlMuOfdma to enable AP-initiated Basic/BSRP Trigger exchanges.
// The ulTriggerPolicy decides when to request an uplink TXOP and the
// ulScheduler assigns scheduled and random-access RUs. Both directions keep
// the configured duration, PSDU, and A-MPDU limits, falling back safely to
// single-user transmission when planning cannot produce a valid HE PPDU.
//
module HeHcf extends Hcf
{
    parameters:
        @class(HeHcf);
        double receiverNoiseFigure @unit(dB) = default(10dB); // Noise figure used by the DL scheduler's SNR estimate
        double linkEstimateMaxAge @unit(s) = default(1s); // Maximum age of a path-loss estimate considered by the scheduler
        int maxAmpduMpduCount = default(16); // Maximum MPDUs contributed by one STA to a DL MU A-MPDU
        int maxHeMuPsduLength @unit(B) = default(6500631B); // Per-user PSDU limit used while assembling a HE MU PPDU
        double maxHeMuPpduDuration @unit(s) = default(5.484ms); // Downlink MU PPDU duration ceiling
        string dlMuAckMethod = default("muBarTrigger"); // "muBarTrigger" uses a MU-BAR Trigger; "sequentialBar" sends one BAR per STA
        bool enableUlMuOfdma = default(false); // Enables AP-triggered HE uplink OFDMA exchanges
        bool enableUlMuMimo = default(false); // Enables full-bandwidth HE trigger-based UL MU-MIMO
        bool sendOperatingModeIndication = default(false);
        int operatingModeRxNss = default(1);
        int operatingModeChannelWidth = default(0);
        bool operatingModeUlMuDisable = default(false);
        bool enableNdpFeedbackReport = default(false); // Periodically sends NFRP Trigger frames when peers negotiated support
        double ulTriggerCheckInterval @unit(s) = default(1ms); // Interval at which the AP considers starting an uplink Trigger exchange
        double ulTargetRssiMargin @unit(dB) = default(10dB); // Target received-power margin requested from scheduled uplink STAs
        double maxHeTbPpduDuration @unit(s) = default(2ms); // Maximum duration requested for an HE trigger-based uplink PPDU
        // Bit 0 denotes the primary 20 MHz subchannel. Empty keeps the
        // legacy unpunctured channel; puncturing is supported for 80/160 MHz.
        string hePreamblePuncturing = default("");
        string dynamicHePreamblePuncturing = default(""); // Runtime mask used only inside the configured interval
        double dynamicHePreamblePuncturingStart @unit(s) = default(-1s);
        double dynamicHePreamblePuncturingEnd @unit(s) = default(-1s); // Negative keeps the runtime mask active indefinitely
        bool enableDlMuMimo = default(false); // Enables HE downlink MU-MIMO sounding and transmission
        double csiValidityDuration @unit(s) = default(0.1s); // Validity duration of sounding CSI measurements
        double defaultCsiLeakage = default(0.01); // Default inter-user interference leakage factor
        string csiLeakageOverrides = default(""); // Specific peer-to-peer leakage overrides e.g. "1-2:0.01"
        @signal[heTbResponseCommitted](type=inet::ieee80211::HeTbResponseEvent);
        @signal[heTbResponseTriggerId](type=unsigned long);
        @signal[heTbResponseReason](type=long);
        @signal[heTbResponseHadPendingPayload](type=long);
        @signal[heTbResponsePendingBytes](type=long);
        @signal[heTbResponseSelectedBytes](type=long);
        @signal[heTbResponseReportedBytes](type=long);
        @signal[peerOperatingModeChanged](type=inet::ieee80211::HePeerOperatingModeChangedEvent);
        @signal[peerOperatingModeAssociationId](type=unsigned long);
        @signal[peerOperatingModeRxNss](type=unsigned long);
        @signal[peerOperatingModeChannelWidth](type=unsigned long);
        @signal[peerOperatingModeUlMuDisable](type=long);
        @statistic[heTbResponseTriggerId](title="HE-TB response Trigger identifier"; source=heTbResponseTriggerId; record=vector; interpolationmode=none);
        @statistic[heTbResponseReason](title="HE-TB response reason"; source=heTbResponseReason; record=vector; interpolationmode=none);
        @statistic[heTbResponseHadPendingPayload](title="HE-TB candidate payload existed"; source=heTbResponseHadPendingPayload; record=vector; interpolationmode=none);
        @statistic[heTbResponsePendingBytes](title="HE-TB live pending bytes"; source=heTbResponsePendingBytes; record=vector; interpolationmode=none);
        @statistic[heTbResponseSelectedBytes](title="HE-TB selected bytes"; source=heTbResponseSelectedBytes; record=vector; interpolationmode=none);
        @statistic[heTbResponseReportedBytes](title="HE-TB reported bytes"; source=heTbResponseReportedBytes; record=vector; interpolationmode=none);
        @statistic[peerOperatingModeAssociationId](title="Peer operating mode association identifier"; source=peerOperatingModeAssociationId; record=vector; interpolationmode=none);
        @statistic[peerOperatingModeRxNss](title="Peer operating mode receive spatial streams"; source=peerOperatingModeRxNss; record=vector; interpolationmode=none);
        @statistic[peerOperatingModeChannelWidth](title="Peer operating mode channel width encoding"; source=peerOperatingModeChannelWidth; record=vector; interpolationmode=none);
        @statistic[peerOperatingModeUlMuDisable](title="Peer operating mode UL MU disable"; source=peerOperatingModeUlMuDisable; record=vector; interpolationmode=none);

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
        soundingCoordinator: HeSoundingCoordinator {
            parameters:
                @display("p=600,100");
        }
}
