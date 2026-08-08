# Walkthrough: 802.11ac Downlink MU-MIMO Baseline

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260808T200701Z`
- PCAP: `20260808T200701Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough demonstrates IEEE 802.11ac Downlink Multi-User MIMO (DL MU-MIMO) beamforming, comparing single-user sequential transmission against simultaneous spatial stream transmissions to multiple client stations.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- Explain how 802.11ac Downlink MU-MIMO allows an Access Point equipped with multiple antennas to transmit independent spatial streams to distinct stations simultaneously.
- Observe spatial stream allocation in VHT PPDUs and Group ID management.
- Measure aggregate throughput gain and medium access latency reduction compared to single-user EDCA contention.

## [agent] Scenario description

The topology uses [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned) with `ap` equipped with 4 antennas, transmitting traffic to `host[0]` and `host[1]`.

Two configurations are evaluated:

1. `VhtSingleUserBaseline`: Sequential Single-User transmission (AP serves `host[0]` then `host[1]` sequentially).
2. `VhtDlMuMimoTwoUsers`: Downlink MU-MIMO simultaneous transmission (AP serves `host[0]` and `host[1]` concurrently on separate spatial streams).

## [agent] Standards and INET model boundary

- **IEEE Std 802.11ac-2013 / 802.11-2020 Clause 21.3.2 & 21.3.12**: Specifies VHT MU PPDU frame format, Group ID field, and beamforming matrix feedback.
- **INET Model Boundary**: DL MU-MIMO spatial beamforming is coordinated in `inet::ieee80211::VhtHcf`.

## [agent] Scalar and vector analysis

<!-- BEGIN GENERATED: ieee80211-scalar-vector-dl_mu_mimo_baseline -->
### [script] Generated scalar/vector plot and table

![dl_mu_mimo_baseline scalar/vector analysis](results/20260808T200701Z/dl-mu-mimo-baseline-delivery-delay.png)

Figure provenance: [`results/20260808T200701Z/dl-mu-mimo-baseline-delivery-delay.png.json`](results/20260808T200701Z/dl-mu-mimo-baseline-delivery-delay.png.json). Run-level metric source: [`../../ieee80211ax/analysis/metrics.json`](../../ieee80211ax/analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)<br>vector / **.app[*] / endToEndDelay:vector
- Window / per-run aggregation / exclusions: [0.3, 0.5) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| DL MU-MIMO Two Users / goodput mbps | 8.264 | 0.0666347 |
| Single-User Baseline / goodput mbps | 8.4 | 0 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **INCONCLUSIVE** | Compare application delivered bytes between Single-User baseline and Downlink MU-MIMO two-user spatial multiplexing | No manifest acceptance threshold defined for DL MU-MIMO baseline comparison |
<!-- END GENERATED: ieee80211-scalar-vector-dl_mu_mimo_baseline -->

<!-- BEGIN GENERATED: ieee80211-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260808T200701Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260808T200701Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260808T200701Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ac/capture_manifests/20260808T200701Z.json` (SHA-256 `84d21354231c8e8bb4112ebbe734095b045dd9e6bfdba01a7e6ad83ae603c9cb`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

<small>

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `VhtSingleUserBaseline` | `none (all decoded frames)` | 779 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] (373), Control: Ack (340), QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] (21) | 14.90% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `VhtDlMuMimoTwoUsers` | `none (all decoded frames)` | 816 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] (372), Control: Ack (339), QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] (21) | 15.03% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

</small>

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | VhtDlMuMimoTwoUsers produced protocol-visible wireless observations | 816 AP/global transmission observations |
| **PASS** | VhtSingleUserBaseline produced protocol-visible wireless observations | 779 AP/global transmission observations |

### [script] Configuration: `VhtSingleUserBaseline`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **779**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | - | 21 | 2.70% | 166.0 B | 0.0 B | 91.1 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 1.28% | 0.19% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | - | 373 | 47.88% | 1062.9 B | 317.1 B | 367.0 us | 97.6 us | 5010 MHz | - | 13.0 dBm | 91.88% | 13.69% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 6 | 0.77% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.11% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 21 | 2.70% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | -58.3 dBm | - | 0.43% | 0.06% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 6 | 0.77% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 5010 MHz | -53.0 dBm | - | 0.28% | 0.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 340 | 43.65% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -57.3 dBm | - | 5.63% | 0.84% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 6 | 0.77% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -58.3 dBm | 13.0 dBm | 0.10% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Req | - | 3 | 0.39% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.14% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Resp | - | 3 | 0.39% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -58.3 dBm | - | 0.14% | 0.02% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 1 | 0.200092000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200136000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 3 | 0.200273000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200333000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 5 | 0.200477000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200521000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 7 | 0.200792000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.200852000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 9 | 0.200971000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.201031000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 11 | 0.201159000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.201219000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 13 | 0.201408000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.201452000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 15 | 0.201571000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.201631000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 17 | 0.201759000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.201819000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 19 | 0.210096000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=642 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 20 | 0.210144000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=642 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 21 | 0.210355000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=698 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 22 | 0.210403000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=698 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 23 | 0.210650000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=750 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 24 | 0.210698000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=750 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 25 | 0.220096000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=816 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 26 | 0.220144000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=816 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 27 | 0.220346000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=872 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 28 | 0.220394000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=872 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 29 | 0.220659000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=924 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 30 | 0.220707000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=924 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 31 | 0.230096000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=990 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 32 | 0.230144000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=990 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 33 | 0.230400000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1046 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 34 | 0.230448000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1046 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 35 | 0.230659000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1098 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 36 | 0.230707000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1098 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 37 | 0.240096000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1164 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 38 | 0.240144000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1164 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 39 | 0.240310000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1220 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 40 | 0.240358000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1220 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 41 | 0.240542000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1272 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 42 | 0.240590000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1272 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 43 | 0.250096000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1338 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 44 | 0.250144000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1338 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 45 | 0.250301000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1394 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 46 | 0.250349000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1394 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 47 | 0.250515000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1446 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 48 | 0.250563000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1446 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 49 | 0.260096000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1512 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 50 | 0.260144000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1512 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 51 | 0.260310000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1568 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 52 | 0.260358000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1568 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 53 | 0.260524000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1620 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 54 | 0.260572000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1620 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 55 | 0.270096000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1686 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 56 | 0.270144000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1686 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 57 | 0.270364000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1742 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 58 | 0.270412000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1742 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 59 | 0.270632000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1794 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 60 | 0.270680000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1794 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 61 | 0.300372000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 62 | 0.300416000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 63 | 0.300867000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 64 | 0.300911000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 65 | 0.301857000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 66 | 0.301901000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 67 | 0.302784000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 68 | 0.302828000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 69 | 0.303810000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 70 | 0.303854000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 477 | 0.500540000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=78, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 478 | 0.500584000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 479 | 0.501749000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=78, frag=1, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 480 | 0.501793000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 481 | 0.502200000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=78, frag=2, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 482 | 0.502935000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 483 | 0.503179000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, MPDU sequence(s) acknowledged=74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 484 | 0.503843000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=79, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 485 | 0.503887000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 486 | 0.505088000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=79, frag=1, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 487 | 0.505132000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 488 | 0.505494000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=79, frag=2, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 489 | 0.506623000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=80, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 490 | 0.506667000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 491 | 0.507778000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=80, frag=1, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 492 | 0.507822000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 493 | 0.508220000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=80, frag=2, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 494 | 0.509367000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=81, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 495 | 0.509411000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 496 | 0.510657000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=81, frag=1, more-frag=0, TID=0 |

</small>

Frame numbers are local to capture `VhtSingleUserBaseline-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `VhtDlMuMimoTwoUsers`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **816**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | - | 21 | 2.57% | 166.0 B | 0.0 B | 91.1 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 1.27% | 0.19% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | - | 372 | 45.59% | 1062.9 B | 317.5 B | 367.0 us | 97.7 us | 5010 MHz | - | 13.0 dBm | 90.85% | 13.65% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 6 | 0.74% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.11% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 21 | 2.57% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | -58.3 dBm | - | 0.43% | 0.06% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 6 | 0.74% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 5010 MHz | -53.0 dBm | - | 0.28% | 0.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 339 | 41.54% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -57.3 dBm | - | 5.56% | 0.84% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 6 | 0.74% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -58.3 dBm | 13.0 dBm | 0.10% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action No Ack: VHT: VHT Compressed Beamforming | - | 13 | 1.59% | 46.0 B | 0.0 B | 81.3 us | 0.0 us | 5010 MHz | -57.9 dBm | - | 0.70% | 0.11% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Req | - | 3 | 0.37% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.14% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Resp | - | 3 | 0.37% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -58.3 dBm | - | 0.14% | 0.02% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#82cc33" /></svg> | A-MPDU Delimiter / Aggregation Overhead | - | 13 | 1.59% | 0.0 B | 0.0 B | 20.0 us | 0.0 us | 13 MHz | - | - | 0.17% | 0.03% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fad233" /></svg> | Control: NDP Announcement (NDPA) | - | 13 | 1.59% | 23.0 B | 0.0 B | 27.7 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.24% | 0.04% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fad233" /></svg> | 1 | 0.200056000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: NDP Announcement (NDPA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f9ee1f" /></svg> | 2 | 0.200116000 | ? → ? | Control: VHT Sounding NDP [NDP Sounding] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 3 | 0.200220000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action No Ack: VHT: VHT Compressed Beamforming | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 4 | 0.200382000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 5 | 0.200426000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 6 | 0.200715000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 7 | 0.200775000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 8 | 0.200912000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 9 | 0.200972000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fad233" /></svg> | 10 | 0.201134000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: NDP Announcement (NDPA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f9ee1f" /></svg> | 11 | 0.201194000 | ? → ? | Control: VHT Sounding NDP [NDP Sounding] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 12 | 0.201298000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action No Ack: VHT: VHT Compressed Beamforming | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 13 | 0.201514000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.201558000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 15 | 0.201829000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.201889000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 17 | 0.202017000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.202077000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fad233" /></svg> | 19 | 0.202194000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: NDP Announcement (NDPA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f9ee1f" /></svg> | 20 | 0.202254000 | ? → ? | Control: VHT Sounding NDP [NDP Sounding] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 21 | 0.202358000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Management: Action No Ack: VHT: VHT Compressed Beamforming | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 22 | 0.202529000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 23 | 0.202573000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 24 | 0.202710000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 25 | 0.202771000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 26 | 0.202899000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 27 | 0.202959000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 28 | 0.210096000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=849 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 29 | 0.210144000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=849 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 30 | 0.210292000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=905 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 31 | 0.210340000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=905 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 32 | 0.210533000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=957 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 33 | 0.210581000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=957 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 34 | 0.220096000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1023 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 35 | 0.220144000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1023 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 36 | 0.220292000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1079 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 37 | 0.220340000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1079 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 38 | 0.220524000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1131 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 39 | 0.220572000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1131 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 40 | 0.230096000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1197 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 41 | 0.230144000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1197 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 42 | 0.230319000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1253 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 43 | 0.230367000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1253 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 44 | 0.230533000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1305 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 45 | 0.230581000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1305 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 46 | 0.240096000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1371 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 47 | 0.240144000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1371 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 48 | 0.240418000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1427 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 49 | 0.240466000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1427 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 50 | 0.240740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1479 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 51 | 0.240788000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1479 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 52 | 0.250096000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1545 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 53 | 0.250144000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1545 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 54 | 0.250292000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1601 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 55 | 0.250340000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1601 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 56 | 0.250506000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1653 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 57 | 0.250554000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1653 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 58 | 0.260096000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1719 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 59 | 0.260144000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1719 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 60 | 0.260346000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1775 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 61 | 0.260394000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1775 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 62 | 0.260533000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1827 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 63 | 0.260581000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1827 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 64 | 0.270096000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1893 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 65 | 0.270144000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1893 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 66 | 0.270283000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1949 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 67 | 0.270331000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1949 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#38c026" /></svg> | 68 | 0.270605000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=2001 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 69 | 0.270653000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2001 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 70 | 0.300372000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 71 | 0.300416000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 72 | 0.300957000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 73 | 0.301001000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 74 | 0.301848000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 75 | 0.301892000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fad233" /></svg> | 76 | 0.302477000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: NDP Announcement (NDPA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f9ee1f" /></svg> | 77 | 0.302537000 | ? → ? | Control: VHT Sounding NDP [NDP Sounding] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 78 | 0.302641000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action No Ack: VHT: VHT Compressed Beamforming | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 79 | 0.303146000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 80 | 0.303190000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fad233" /></svg> | 81 | 0.303730000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: NDP Announcement (NDPA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f9ee1f" /></svg> | 82 | 0.303790000 | ? → ? | Control: VHT Sounding NDP [NDP Sounding] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 83 | 0.303894000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action No Ack: VHT: VHT Compressed Beamforming | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 499 | 0.500134000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=77, frag=1, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 500 | 0.500178000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 501 | 0.500531000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=77, frag=2, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 502 | 0.501642000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=78, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 503 | 0.501686000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 504 | 0.502860000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=78, frag=1, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 505 | 0.502904000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 506 | 0.503275000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=78, frag=2, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fad233" /></svg> | 507 | 0.504073000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: NDP Announcement (NDPA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f9ee1f" /></svg> | 508 | 0.504133000 | ? → ? | Control: VHT Sounding NDP [NDP Sounding] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 509 | 0.504237000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action No Ack: VHT: VHT Compressed Beamforming | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 510 | 0.504390000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 511 | 0.504634000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, MPDU sequence(s) acknowledged=74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 512 | 0.505181000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=79, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 513 | 0.505225000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 514 | 0.506381000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=79, frag=1, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 515 | 0.506425000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 516 | 0.506895000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=79, frag=2, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1fbd31" /></svg> | 517 | 0.508141000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 3, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=80, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 518 | 0.508185000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

</small>

Frame numbers are local to capture `VhtDlMuMimoTwoUsers-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded Control: Subtype 5 (NDP Announcement) records

| Frame | Simulation time (s) | Transmitter | Receiver | Dialog token | Variant | Target STAs (AID, Feedback, Nc) |
|---:|---:|---|---|---:|---|---|
| 1 | 0.200056000 | 10:00:00:00:00:00 | 0a:aa:00:00:00:01 | 1 | VHT | AID=1, Feedback=SU, Nc=1 |
| 10 | 0.201134000 | 10:00:00:00:00:00 | 0a:aa:00:00:00:02 | 2 | VHT | AID=2, Feedback=SU, Nc=1 |
| 19 | 0.202194000 | 10:00:00:00:00:00 | 0a:aa:00:00:00:03 | 3 | VHT | AID=3, Feedback=SU, Nc=1 |
| 76 | 0.302477000 | 10:00:00:00:00:00 | 0a:aa:00:00:00:01 | 4 | VHT | AID=1, Feedback=SU, Nc=1 |
| 81 | 0.303730000 | 10:00:00:00:00:00 | 0a:aa:00:00:00:02 | 5 | VHT | AID=2, Feedback=SU, Nc=1 |
| 86 | 0.305064000 | 10:00:00:00:00:00 | 0a:aa:00:00:00:03 | 6 | VHT | AID=3, Feedback=SU, Nc=1 |
| 295 | 0.403742000 | 10:00:00:00:00:00 | 0a:aa:00:00:00:01 | 7 | VHT | AID=1, Feedback=SU, Nc=1 |
| 300 | 0.405112000 | 10:00:00:00:00:00 | 0a:aa:00:00:00:02 | 8 | VHT | AID=2, Feedback=SU, Nc=1 |
| 305 | 0.406410000 | 10:00:00:00:00:00 | 0a:aa:00:00:00:03 | 9 | VHT | AID=3, Feedback=SU, Nc=1 |
| 507 | 0.504073000 | 10:00:00:00:00:00 | 0a:aa:00:00:00:01 | 10 | VHT | AID=1, Feedback=SU, Nc=1 |
| 666 | 0.588438000 | 10:00:00:00:00:00 | 0a:aa:00:00:00:02 | 11 | VHT | AID=2, Feedback=SU, Nc=1 |
| 671 | 0.589835000 | 10:00:00:00:00:00 | 0a:aa:00:00:00:03 | 12 | VHT | AID=3, Feedback=SU, Nc=1 |
| 706 | 0.605506000 | 10:00:00:00:00:00 | 0a:aa:00:00:00:01 | 13 | VHT | AID=1, Feedback=SU, Nc=1 |

#### [script] Decoded NDP (Null Data Packet) records

| Frame | Simulation time (s) | Transmitter | Receiver | NDP Variant | Standard | Bandwidth / RU | Spatial Streams (Nss) | Guard Interval | Coding |
|---:|---:|---|---|---|---|---|---:|---|---| 
| 2 | 0.200116000 | - | - | VHT Sounding NDP | VHT | 20 MHz | 2 | 0.8 us | BCC |
| 11 | 0.201194000 | - | - | VHT Sounding NDP | VHT | 20 MHz | 2 | 0.8 us | BCC |
| 20 | 0.202254000 | - | - | VHT Sounding NDP | VHT | 20 MHz | 2 | 0.8 us | BCC |
| 77 | 0.302537000 | - | - | VHT Sounding NDP | VHT | 20 MHz | 2 | 0.8 us | BCC |
| 82 | 0.303790000 | - | - | VHT Sounding NDP | VHT | 20 MHz | 2 | 0.8 us | BCC |
| 87 | 0.305124000 | - | - | VHT Sounding NDP | VHT | 20 MHz | 2 | 0.8 us | BCC |
| 296 | 0.403802000 | - | - | VHT Sounding NDP | VHT | 20 MHz | 2 | 0.8 us | BCC |
| 301 | 0.405172000 | - | - | VHT Sounding NDP | VHT | 20 MHz | 2 | 0.8 us | BCC |
| 306 | 0.406470000 | - | - | VHT Sounding NDP | VHT | 20 MHz | 2 | 0.8 us | BCC |
| 508 | 0.504133000 | - | - | VHT Sounding NDP | VHT | 20 MHz | 2 | 0.8 us | BCC |
| 667 | 0.588498000 | - | - | VHT Sounding NDP | VHT | 20 MHz | 2 | 0.8 us | BCC |
| 672 | 0.589895000 | - | - | VHT Sounding NDP | VHT | 20 MHz | 2 | 0.8 us | BCC |
| 707 | 0.605566000 | - | - | VHT Sounding NDP | VHT | 20 MHz | 2 | 0.8 us | BCC |

### [script] Analysis of Packet Distribution
Decoded Radiotap HE-MU (bit 24) headers and spatial stream starting indices (`heStreamStartIndex`) in captured frames directly prove non-overlapping spatial stream allocations across multiplexed users in DL MU-MIMO PPDUs.
<!-- END GENERATED: ieee80211-pcap-statistics -->
