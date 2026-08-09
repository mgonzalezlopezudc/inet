# Walkthrough: 802.11ac Downlink MU-MIMO Baseline

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260809T215000Z`
- PCAP: `20260809T215000Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough demonstrates IEEE 802.11ac Downlink Multi-User MIMO (DL MU-MIMO) beamforming, comparing single-user sequential transmission against simultaneous spatial stream transmissions to multiple client stations.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- Explain how 802.11ac Downlink MU-MIMO allows an Access Point equipped with multiple antennas to transmit independent spatial streams to distinct stations simultaneously.
- Observe spatial stream allocation in VHT PPDUs and Group ID management.
- Measure aggregate throughput gain and medium access latency reduction compared to single-user EDCA contention.

## [agent] Scenario description

The topology uses [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned) with `ap` equipped with 4 antennas, transmitting traffic to `host[0]`, `host[1]`, and `host[2]`.

Two configurations are evaluated:

1. `VhtSingleUserBaseline`: Sequential Single-User transmission (AP serves the three hosts sequentially).
2. `VhtDlMuMimoThreeUsers`: Downlink MU-MIMO simultaneous transmission (AP serves all three hosts concurrently on separate spatial streams).

The VHT MU PPDU is one over-the-air container carrying one per-user A-MPDU for each selected station. The recorder now unpacks the canonical per-user PSDU ranges and writes one valid A-MPDU record per user, so TShark exposes the nested QoS Data addresses in separate logical timeline rows. The scheduler selects three users in this configuration; the PCAP radiotap header can expose up to four VHT user slots, while the decoded MAC addresses identify the actual stations. The packet summary counts these as per-user `VHT MU QoS Data` observations; the physical PPDU remains the common container.

The VHT MU frame sequence uses `BLOCK_ACK` in each per-user QoS Data header and then polls each station with an explicit BAR/BA exchange. This is a standards-conformant non-immediate response strategy, not a claim that VHT MU universally requires `BLOCK_ACK`. INET's `useImplicitBlockAck` switch currently governs the ordinary single-user/A-MPDU implicit-BA path; applying it to this MU exchange would require coordinated receiver and originator ACK-state support for one immediate responder plus explicit polling of the remaining users.

## [agent] Standards and INET model boundary

- **IEEE Std 802.11ac-2013 / 802.11-2020 Clause 21.3.2 & 21.3.12**: Specifies VHT MU PPDU frame format, Group ID field, and beamforming matrix feedback.
- **INET Model Boundary**: DL MU-MIMO scheduling and VHT PPDU construction are coordinated in `inet::ieee80211::VhtHcf`. The example uses INET's abstract beamforming gain and scalar medium; it does not establish a realistic channel-estimated beamforming matrix.

## [agent] Scalar and vector analysis

<!-- BEGIN GENERATED: ieee80211-scalar-vector-dl_mu_mimo_baseline -->
### [script] Generated scalar/vector plot and table

![dl_mu_mimo_baseline scalar/vector analysis](results/20260809T215000Z/dl-mu-mimo-baseline-delivery-delay.png)

Figure provenance: [`results/20260809T215000Z/dl-mu-mimo-baseline-delivery-delay.png.json`](results/20260809T215000Z/dl-mu-mimo-baseline-delivery-delay.png.json). Run-level metric source: [`../../ieee80211ax/analysis/metrics.json`](../../ieee80211ax/analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)<br>vector / **.app[*] / endToEndDelay:vector
- Window / per-run aggregation / exclusions: [0.3, 0.5) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| DL MU-MIMO Three Users / goodput mbps | 95.08 | 0.494176 |
| Single-User Baseline / goodput mbps | 63.744 | 1.20055 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **PASS** | Compare application delivered bytes between the single-user baseline and the three-user VHT MU-MIMO configuration | Every matched run preserves at least 0.800 of baseline delivery. |
<!-- END GENERATED: ieee80211-scalar-vector-dl_mu_mimo_baseline -->

<!-- BEGIN GENERATED: ieee80211-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260809T215000Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260809T215000Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260809T215000Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ac/capture_manifests/20260809T215000Z.json` (SHA-256 `e66b29f9f8b616a50ffb7b144811a35674d3310474908f75f4dd6dbd2fb6ea07`). VHT PPDU format, MCS, coding, bandwidth, GI, and NSTS are decoded directly from standards-compliant radiotap VHT fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. VHT SU and VHT MU use modeled preambles; per-user VHT MU signaling remains approximate because radiotap carries common MU metadata alongside each logical user record.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

<small>

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `VhtSingleUserBaseline` | `none (all decoded frames)` | 1899 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] (1741), Control: Block Ack (BA) (98), QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC] (14) | 26.48% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `VhtDlMuMimoThreeUsers` | `none (all decoded frames)` | 3108 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] (2413), Control: Block Ack Request (BAR) (314), Control: Block Ack (BA) (314) | 349.71% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

</small>

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | VhtDlMuMimoThreeUsers produced protocol-visible wireless observations | 3108 AP/global transmission observations |
| **PASS** | VhtSingleUserBaseline produced protocol-visible wireless observations | 1899 AP/global transmission observations |

### [script] Configuration: `VhtSingleUserBaseline`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1899**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | - | 1741 | 91.68% | 1052.6 B | 103.1 B | 148.0 us | 10.6 us | 5010 MHz | - | 13.0 dBm | 97.28% | 25.76% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17a625" /></svg> | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC] | - | 14 | 0.74% | 1214.1 B | 547.4 B | 164.5 us | 56.1 us | 5010 MHz | - | 13.0 dBm | 0.87% | 0.23% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 10 | 0.53% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.11% | 0.03% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 98 | 5.16% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | -58.8 dBm | - | 1.13% | 0.30% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 10 | 0.53% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 5010 MHz | -53.0 dBm | - | 0.27% | 0.07% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 14 | 0.74% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -54.1 dBm | - | 0.13% | 0.03% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 6 | 0.32% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -58.3 dBm | 13.0 dBm | 0.06% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Req | - | 3 | 0.16% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.08% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Resp | - | 3 | 0.16% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -58.3 dBm | - | 0.08% | 0.02% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17a625" /></svg> | 1 | 0.200060000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200104000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 3 | 0.200241000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200301000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17a625" /></svg> | 5 | 0.200413000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200457000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 7 | 0.200696000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.200756000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 9 | 0.200875000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.200935000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 11 | 0.201063000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.201123000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17a625" /></svg> | 13 | 0.201280000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.201324000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 15 | 0.201443000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.201503000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 17 | 0.201631000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.201691000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 19 | 0.210060000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=624 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 20 | 0.210108000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=624 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 21 | 0.210283000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=678 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 22 | 0.210331000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=678 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 23 | 0.210542000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=728 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 24 | 0.210590000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=728 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 25 | 0.220060000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=792 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 26 | 0.220108000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=792 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 27 | 0.220274000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=846 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 28 | 0.220322000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=846 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 29 | 0.220551000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=896 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 30 | 0.220599000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=896 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 31 | 0.230060000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=960 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 32 | 0.230108000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=960 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 33 | 0.230328000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1014 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 34 | 0.230376000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1014 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 35 | 0.230551000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1064 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 36 | 0.230599000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1064 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 37 | 0.240060000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1128 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 38 | 0.240108000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1128 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 39 | 0.240238000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1182 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 40 | 0.240286000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1182 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 41 | 0.240434000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1232 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 42 | 0.240482000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1232 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 43 | 0.250060000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1296 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 44 | 0.250108000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1296 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 45 | 0.250229000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1350 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 46 | 0.250277000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1350 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 47 | 0.250407000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1400 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 48 | 0.250455000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1400 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 49 | 0.260060000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1464 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 50 | 0.260108000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1464 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 51 | 0.260238000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1518 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 52 | 0.260286000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1518 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 53 | 0.260416000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1568 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 54 | 0.260464000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1568 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 55 | 0.270060000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1632 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 56 | 0.270108000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1632 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 57 | 0.270292000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1686 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 58 | 0.270340000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1686 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 59 | 0.270524000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1736 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 60 | 0.270572000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1736 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 61 | 0.300152000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1806 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 62 | 0.300200000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1806 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 63 | 0.300539000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1881 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 64 | 0.300539000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1881 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 65 | 0.300587000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1881 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 66 | 0.301110000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1952 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 67 | 0.301110000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1952 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 68 | 0.301110000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1952 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 69 | 0.301158000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1952 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 70 | 0.301847000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=2062 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 71 | 0.301847000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=2062 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 72 | 0.301847000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=2062 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 73 | 0.301847000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=2062 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 74 | 0.301847000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=2062 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 75 | 0.301895000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2062 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 76 | 0.302822000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=2178 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 77 | 0.302822000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=2178 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 78 | 0.302822000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=2178 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 79 | 0.302822000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=2178 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 80 | 0.302822000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=2178 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 81 | 0.302822000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=2178 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 82 | 0.302822000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=2178 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 83 | 0.302870000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2178 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 84 | 0.303945000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=2300 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 85 | 0.303945000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=2300 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 86 | 0.303945000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=2300 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 87 | 0.303945000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=2300 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 88 | 0.303945000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=2300 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 89 | 0.303945000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=2300 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 90 | 0.303945000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=0, A-MPDU=2300 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 91 | 0.303945000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=0, A-MPDU=2300 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 92 | 0.303945000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=0, A-MPDU=2300 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 93 | 0.303993000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2300 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17a625" /></svg> | 1769 | 0.500301000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=543, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 1770 | 0.500345000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 1771 | 0.501494000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=543, frag=1, more-frag=0, TID=0, A-MPDU=25452 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 1772 | 0.501494000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=544, frag=0, more-frag=0, TID=0, A-MPDU=25452 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 1773 | 0.501494000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=545, frag=0, more-frag=0, TID=0, A-MPDU=25452 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 1774 | 0.501494000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=546, frag=0, more-frag=0, TID=0, A-MPDU=25452 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 1775 | 0.501494000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=547, frag=0, more-frag=0, TID=0, A-MPDU=25452 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 1776 | 0.501494000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=548, frag=0, more-frag=0, TID=0, A-MPDU=25452 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 1777 | 0.501494000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=549, frag=0, more-frag=0, TID=0, A-MPDU=25452 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 1778 | 0.501494000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=550, frag=0, more-frag=0, TID=0, A-MPDU=25452 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 1779 | 0.504006000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=550, frag=0, more-frag=0, TID=0, A-MPDU=25504 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 1780 | 0.504006000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=551, frag=0, more-frag=0, TID=0, A-MPDU=25504 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 1781 | 0.504006000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=552, frag=0, more-frag=0, TID=0, A-MPDU=25504 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 1782 | 0.504006000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=553, frag=0, more-frag=0, TID=0, A-MPDU=25504 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 1783 | 0.504006000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=554, frag=0, more-frag=0, TID=0, A-MPDU=25504 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 1784 | 0.504006000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=555, frag=0, more-frag=0, TID=0, A-MPDU=25504 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 1785 | 0.504006000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=556, frag=0, more-frag=0, TID=0, A-MPDU=25504 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 1786 | 0.504006000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=557, frag=0, more-frag=0, TID=0, A-MPDU=25504 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 1787 | 0.504006000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=558, frag=0, more-frag=0, TID=0, A-MPDU=25504 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 1788 | 0.504006000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=559, frag=0, more-frag=0, TID=0, A-MPDU=25504 |

</small>

Frame numbers are local to capture `VhtSingleUserBaseline-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `VhtDlMuMimoThreeUsers`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **3108**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | - | 8 | 0.26% | 278.5 B | 297.6 B | 68.6 us | 30.5 us | 5010 MHz | - | 13.0 dBm | 0.02% | 0.05% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17a625" /></svg> | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC] | - | 3 | 0.10% | 166.0 B | 0.0 B | 57.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.00% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | - | 2413 | 77.64% | 1060.8 B | 68.4 B | 1434.4 us | 91.1 us | 5010 MHz | - | 13.0 dBm | 98.97% | 346.11% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 314 | 10.10% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.25% | 0.88% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 8 | 0.26% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | -53.0 dBm | - | 0.01% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 314 | 10.10% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 5010 MHz | -58.5 dBm | - | 0.63% | 2.22% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 9 | 0.29% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -58.3 dBm | 13.0 dBm | 0.01% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 3 | 0.10% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -58.3 dBm | - | 0.00% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action No Ack: VHT: VHT Compressed Beamforming | - | 9 | 0.29% | 201.0 B | 0.0 B | 288.0 us | 0.0 us | 5010 MHz | -58.3 dBm | - | 0.07% | 0.26% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Req | - | 3 | 0.10% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.01% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Resp | - | 3 | 0.10% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -58.3 dBm | - | 0.01% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: VHT: Group ID Management | - | 3 | 0.10% | 54.0 B | 0.0 B | 92.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.01% | 0.03% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#82cc33" /></svg> | A-MPDU Delimiter / Aggregation Overhead | - | 9 | 0.29% | 0.0 B | 0.0 B | 20.0 us | 0.0 us | 26 MHz | - | - | 0.01% | 0.02% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fad233" /></svg> | Control: NDP Announcement (NDPA) | - | 9 | 0.29% | 23.0 B | 0.0 B | 27.7 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.01% | 0.02% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 1 | 0.200096000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action: VHT: Group ID Management | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200156000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 3 | 0.200322000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action: VHT: Group ID Management | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200382000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 5 | 0.200740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action: VHT: Group ID Management | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200800000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fad233" /></svg> | 7 | 0.201046000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: NDP Announcement (NDPA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f9ee1f" /></svg> | 8 | 0.201114000 | ? → ? | Control: VHT Sounding NDP [NDP Sounding] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 9 | 0.201422000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action No Ack: VHT: VHT Compressed Beamforming | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17a625" /></svg> | 10 | 0.201588000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 11 | 0.201632000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 12 | 0.201889000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.201949000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 14 | 0.202068000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.202128000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fad233" /></svg> | 16 | 0.202290000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: NDP Announcement (NDPA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f9ee1f" /></svg> | 17 | 0.202358000 | ? → ? | Control: VHT Sounding NDP [NDP Sounding] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 18 | 0.202666000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action No Ack: VHT: VHT Compressed Beamforming | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17a625" /></svg> | 19 | 0.202787000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.202831000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 21 | 0.203088000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.203148000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 23 | 0.203276000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 24 | 0.203336000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fad233" /></svg> | 25 | 0.203444000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: NDP Announcement (NDPA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f9ee1f" /></svg> | 26 | 0.203512000 | ? → ? | Control: VHT Sounding NDP [NDP Sounding] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 27 | 0.203820000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Management: Action No Ack: VHT: VHT Compressed Beamforming | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17a625" /></svg> | 28 | 0.203995000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 29 | 0.204039000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 30 | 0.204149000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 31 | 0.204210000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 32 | 0.204320000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 33 | 0.204380000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 34 | 0.210060000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=960 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 35 | 0.210108000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=960 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 36 | 0.210278000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=2654435909 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 37 | 0.210278000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1013903502 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 38 | 0.210350000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 39 | 0.210594000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654435909 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 40 | 0.210666000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 41 | 0.210910000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013903502 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 42 | 0.220060000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1152 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 43 | 0.220108000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1152 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 44 | 0.220296000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=2654436613 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 45 | 0.220296000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1013905358 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 46 | 0.220368000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 47 | 0.220612000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654436613 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 48 | 0.220684000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 49 | 0.220928000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013905358 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 50 | 0.230060000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1344 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 51 | 0.230108000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1344 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 52 | 0.230242000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=2654436549 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 53 | 0.230242000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1013904910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 54 | 0.230314000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 55 | 0.230558000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654436549 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 56 | 0.230630000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 57 | 0.230874000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013904910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 58 | 0.240060000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1536 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 59 | 0.240108000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1536 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 60 | 0.240350000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=2654437253 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 61 | 0.240350000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1013904718 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 62 | 0.240422000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 63 | 0.240666000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654437253 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 64 | 0.240738000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 65 | 0.240982000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013904718 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 66 | 0.250060000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1728 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 67 | 0.250108000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1728 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 68 | 0.250287000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=2654437189 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 69 | 0.250287000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1013904782 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 70 | 0.250359000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 71 | 0.250603000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654437189 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 72 | 0.250675000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 73 | 0.250919000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013904782 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 74 | 0.260060000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1920 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 75 | 0.260108000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1920 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 76 | 0.260242000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=2654436869 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 77 | 0.260242000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1013904590 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 78 | 0.260314000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 79 | 0.260558000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654436869 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 80 | 0.260630000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 81 | 0.260874000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013904590 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 82 | 0.270060000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=2112 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 83 | 0.270108000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2112 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 84 | 0.270296000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=2654433733 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 85 | 0.270296000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1013906190 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 86 | 0.270368000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 87 | 0.270612000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654433733 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 88 | 0.270684000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 89 | 0.270928000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013906190 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 90 | 0.300152000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=2310 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 91 | 0.300200000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2310 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 92 | 0.300609000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=2654433511 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 93 | 0.300609000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=2654433511 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 94 | 0.300609000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1013905964 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 95 | 0.300609000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1013905964 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 96 | 0.300609000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=3668337781 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 97 | 0.300681000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 98 | 0.300925000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654433511 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 99 | 0.300997000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 100 | 0.301241000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013905964 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 101 | 0.301313000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 102 | 0.301557000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=3668337781 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#fad233" /></svg> | 103 | 0.301791000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: NDP Announcement (NDPA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f9ee1f" /></svg> | 104 | 0.301859000 | ? → ? | Control: VHT Sounding NDP [NDP Sounding] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 105 | 0.302167000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action No Ack: VHT: VHT Compressed Beamforming | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 106 | 0.303043000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=2654434125 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 107 | 0.303043000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=2654434125 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 108 | 0.303043000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=2654434125 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 109 | 0.303043000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=2654434125 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 110 | 0.303043000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=2654434125 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 111 | 0.303043000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=2654434125 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 112 | 0.303043000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=2654434125 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 113 | 0.303043000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=1013905798 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 114 | 0.303043000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1013905798 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 115 | 0.303043000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1013905798 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 116 | 0.303043000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=1013905798 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 117 | 0.303043000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=1013905798 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 118 | 0.303043000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=1013905798 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 119 | 0.303043000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=1013905798 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 120 | 0.303043000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=3668338655 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 121 | 0.303043000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=3668338655 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 122 | 0.303043000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=3668338655 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 123 | 0.303043000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=3668338655 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 124 | 0.303043000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=3668338655 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 125 | 0.303043000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=3668338655 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 126 | 0.303043000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=3668338655 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 127 | 0.303115000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 128 | 0.303359000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654434125 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 129 | 0.303431000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 130 | 0.303675000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013905798 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 131 | 0.303747000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 132 | 0.303992000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=3668338655 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 3086 | 0.500115000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654463902 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 3087 | 0.500187000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 3088 | 0.500431000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013869909 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 3089 | 0.500503000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 3090 | 0.500747000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=3668374284 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 3091 | 0.501304000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=804, frag=0, more-frag=0, TID=0, A-MPDU=2654463730 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 3092 | 0.501304000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=805, frag=0, more-frag=0, TID=0, A-MPDU=2654463730 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 3093 | 0.501304000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=806, frag=0, more-frag=0, TID=0, A-MPDU=2654463730 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 3094 | 0.501304000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=807, frag=0, more-frag=0, TID=0, A-MPDU=2654463730 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 3095 | 0.501304000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=804, frag=0, more-frag=0, TID=0, A-MPDU=1013869625 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 3096 | 0.501304000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=805, frag=0, more-frag=0, TID=0, A-MPDU=1013869625 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 3097 | 0.501304000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=806, frag=0, more-frag=0, TID=0, A-MPDU=1013869625 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 3098 | 0.501304000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=807, frag=0, more-frag=0, TID=0, A-MPDU=1013869625 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 3099 | 0.501304000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=804, frag=0, more-frag=0, TID=0, A-MPDU=3668374112 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 3100 | 0.501304000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=805, frag=0, more-frag=0, TID=0, A-MPDU=3668374112 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 3101 | 0.501304000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=806, frag=0, more-frag=0, TID=0, A-MPDU=3668374112 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#29c224" /></svg> | 3102 | 0.501304000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | VHT MU QoS Data [VHT, VHT-MCS 8, 20 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=807, frag=0, more-frag=0, TID=0, A-MPDU=3668374112 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 3103 | 0.501376000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 3104 | 0.501620000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654463730 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 3105 | 0.501692000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

</small>

Frame numbers are local to capture `VhtDlMuMimoThreeUsers-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded Control: Subtype 5 (NDP Announcement) records

| Frame | Simulation time (s) | Transmitter | Receiver | Dialog token | Variant | Target STAs (AID, Feedback, Nc) |
|---:|---:|---|---|---:|---|---|
| 7 | 0.201046000 | 10:00:00:00:00:00 | 0a:aa:00:00:00:01 | 1 | VHT | AID=1, Feedback=MU, Nc=4 |
| 16 | 0.202290000 | 10:00:00:00:00:00 | 0a:aa:00:00:00:02 | 2 | VHT | AID=2, Feedback=MU, Nc=4 |
| 25 | 0.203444000 | 10:00:00:00:00:00 | 0a:aa:00:00:00:03 | 3 | VHT | AID=3, Feedback=MU, Nc=4 |
| 103 | 0.301791000 | 10:00:00:00:00:00 | 0a:aa:00:00:00:01 | 4 | VHT | AID=1, Feedback=MU, Nc=4 |
| 133 | 0.304145000 | 10:00:00:00:00:00 | 0a:aa:00:00:00:02 | 5 | VHT | AID=2, Feedback=MU, Nc=4 |
| 136 | 0.304746000 | 10:00:00:00:00:00 | 0a:aa:00:00:00:03 | 6 | VHT | AID=3, Feedback=MU, Nc=4 |
| 1627 | 0.403493000 | 10:00:00:00:00:00 | 0a:aa:00:00:00:01 | 7 | VHT | AID=1, Feedback=MU, Nc=4 |
| 1663 | 0.406156000 | 10:00:00:00:00:00 | 0a:aa:00:00:00:02 | 8 | VHT | AID=2, Feedback=MU, Nc=4 |
| 1666 | 0.406703000 | 10:00:00:00:00:00 | 0a:aa:00:00:00:03 | 9 | VHT | AID=3, Feedback=MU, Nc=4 |

#### [script] Decoded NDP (Null Data Packet) records

| Frame | Simulation time (s) | Transmitter | Receiver | NDP Variant | Standard | Bandwidth / RU | Spatial Streams (Nss) | Guard Interval | Coding |
|---:|---:|---|---|---|---|---|---:|---|---| 
| 8 | 0.201114000 | - | - | VHT Sounding NDP | VHT | 20 MHz | 4 | 0.8 us | BCC |
| 17 | 0.202358000 | - | - | VHT Sounding NDP | VHT | 20 MHz | 4 | 0.8 us | BCC |
| 26 | 0.203512000 | - | - | VHT Sounding NDP | VHT | 20 MHz | 4 | 0.8 us | BCC |
| 104 | 0.301859000 | - | - | VHT Sounding NDP | VHT | 20 MHz | 4 | 0.8 us | BCC |
| 134 | 0.304213000 | - | - | VHT Sounding NDP | VHT | 20 MHz | 4 | 0.8 us | BCC |
| 137 | 0.304814000 | - | - | VHT Sounding NDP | VHT | 20 MHz | 4 | 0.8 us | BCC |
| 1628 | 0.403561000 | - | - | VHT Sounding NDP | VHT | 20 MHz | 4 | 0.8 us | BCC |
| 1664 | 0.406224000 | - | - | VHT Sounding NDP | VHT | 20 MHz | 4 | 0.8 us | BCC |
| 1667 | 0.406771000 | - | - | VHT Sounding NDP | VHT | 20 MHz | 4 | 0.8 us | BCC |

### [script] Analysis of Packet Distribution
Decoded Radiotap VHT fields with multiple per-user MCS/NSS entries directly prove non-overlapping spatial stream allocations across multiplexed users in DL MU-MIMO PPDUs.
<!-- END GENERATED: ieee80211-pcap-statistics -->
