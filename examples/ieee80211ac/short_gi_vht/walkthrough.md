# Walkthrough: 802.11ac Short Guard Interval on Wide Channels

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260809T214000Z`
- PCAP: `20260809T214000Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough demonstrates IEEE 802.11ac Very High Throughput (VHT) Short Guard Interval (400 ns) timing on 80 MHz wide subchannels, comparing physical layer symbol duration and throughput gains against standard 800 ns Long Guard Interval.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- Explain the combination of wide 80 MHz channel bonding and Short Guard Interval (400 ns).
- Calculate peak PHY bitrates (e.g., 433.3 Mbps with Short GI vs 390 Mbps with Long GI for 1x1 VHT MCS 9 in 80 MHz).
- Measure overall frame transmission duration reduction.

## [agent] Scenario description

The topology uses [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned). Traffic flows from `server` to `host[0]`.

Two configurations are evaluated:

1. `VhtLongGI`: Standard 800 ns Long Guard Interval in an 80 MHz VHT channel.
2. `VhtShortGI`: Short 400 ns Guard Interval in an 80 MHz VHT channel.

## [agent] Standards and INET model boundary

- **IEEE Std 802.11ac-2013 / 802.11-2020 Clause 21.3.2.4**: Specifies short and long GI timing for 80 MHz and 160 MHz VHT PPDUs.
- **INET Model Boundary**: VHT short-GI capability is configured through `mib.vhtShortGi80`; the selected bitrate and guard interval are then exposed by the packet-level VHT mode and PCAP radiotap fields.

## [agent] Scalar and vector analysis

<!-- BEGIN GENERATED: ieee80211-scalar-vector-short_gi_vht -->
### [script] Generated scalar/vector plot and table

![short_gi_vht scalar/vector analysis](results/20260809T214000Z/short-gi-vht-delivery-delay.png)

Figure provenance: [`results/20260809T214000Z/short-gi-vht-delivery-delay.png.json`](results/20260809T214000Z/short-gi-vht-delivery-delay.png.json). Run-level metric source: [`../../ieee80211ax/analysis/metrics.json`](../../ieee80211ax/analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)<br>vector / **.app[*] / endToEndDelay:vector
- Window / per-run aggregation / exclusions: [0.3, 0.5) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| VHT Long GI (800 ns) / goodput mbps | 49.52 | 1.01056 |
| VHT Short GI (400 ns) / goodput mbps | 53.368 | 0.626665 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **PASS** | Compare application delivered bytes between VHT Long (800 ns) and Short (400 ns) Guard Intervals | Every matched run preserves at least 1.000 of baseline delivery. |
<!-- END GENERATED: ieee80211-scalar-vector-short_gi_vht -->

<!-- BEGIN GENERATED: ieee80211-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260809T214000Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260809T214000Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260809T214000Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ac/capture_manifests/20260809T214000Z.json` (SHA-256 `a07bc6517fdd9514bd2c7c5e90bd75d993b588c894cf32a02e7cacb741173aee`). VHT PPDU format, MCS, coding, bandwidth, GI, and NSTS are decoded directly from standards-compliant radiotap VHT fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. VHT SU and VHT MU use modeled preambles; per-user VHT MU signaling remains approximate because radiotap carries common MU metadata alongside each logical user record.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

<small>

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `VhtLongGI` | `none (all decoded frames)` | 1498 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] (1376), Control: Block Ack (BA) (78), Control: Ack (18) | 25.32% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `VhtShortGI` | `none (all decoded frames)` | 1600 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] (1448), Control: Block Ack (BA) (90), Control: Ack (24) | 24.78% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

</small>

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | VhtLongGI produced protocol-visible wireless observations | 1498 AP/global transmission observations |
| **PASS** | VhtShortGI produced protocol-visible wireless observations | 1600 AP/global transmission observations |

### [script] Configuration: `VhtLongGI`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1498**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | - | 1376 | 91.86% | 1055.5 B | 89.6 B | 179.6 us | 11.8 us | 5040 MHz | - | 13.0 dBm | 97.58% | 24.71% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2bc31d" /></svg> | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC] | - | 12 | 0.80% | 1166.5 B | 577.6 B | 194.2 us | 76.4 us | 5040 MHz | - | 13.0 dBm | 0.92% | 0.23% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 8 | 0.53% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.09% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 78 | 5.21% | 44.3 B | 36.4 B | 34.8 us | 12.1 us | 5010 MHz | -58.2 dBm | - | 1.07% | 0.27% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 18 | 1.20% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -55.1 dBm | 13.0 dBm | 0.18% | 0.04% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Req | - | 3 | 0.20% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.08% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Resp | - | 3 | 0.20% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -58.3 dBm | - | 0.08% | 0.02% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2bc31d" /></svg> | 1 | 0.200064000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200124000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 3 | 0.200261000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200321000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2bc31d" /></svg> | 5 | 0.200437000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200497000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 7 | 0.200756000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.200816000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 9 | 0.200935000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.200995000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 11 | 0.201123000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.201183000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2bc31d" /></svg> | 13 | 0.201344000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.201404000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 15 | 0.201523000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.201583000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 17 | 0.201711000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.201771000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 19 | 0.210064000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=624 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 20 | 0.210148000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=624 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 21 | 0.210327000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=678 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 22 | 0.210411000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=678 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 23 | 0.210626000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=728 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 24 | 0.210710000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=728 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 25 | 0.220064000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=792 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 26 | 0.220148000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=792 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 27 | 0.220318000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=846 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 28 | 0.220402000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=846 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 29 | 0.220635000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=896 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 30 | 0.220719000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=896 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 31 | 0.230064000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=960 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 32 | 0.230148000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=960 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 33 | 0.230372000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1014 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 34 | 0.230456000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1014 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 35 | 0.230635000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1064 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 36 | 0.230719000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1064 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 37 | 0.240064000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1128 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 38 | 0.240148000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1128 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 39 | 0.240282000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1182 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 40 | 0.240366000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1182 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 41 | 0.240518000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1232 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 42 | 0.240602000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1232 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 43 | 0.300188000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1302 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 44 | 0.300272000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1302 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 45 | 0.300660000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1377 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 46 | 0.300660000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1377 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 47 | 0.300744000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1377 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 48 | 0.301442000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1466 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 49 | 0.301442000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1466 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 50 | 0.301442000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1466 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 51 | 0.301442000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1466 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 52 | 0.301526000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1466 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 53 | 0.302516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1579 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 54 | 0.302516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1579 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 55 | 0.302516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1579 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 56 | 0.302516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1579 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 57 | 0.302516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1579 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 58 | 0.302516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1579 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 59 | 0.302600000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1579 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 60 | 0.304075000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1716 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 61 | 0.304075000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1716 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 62 | 0.304075000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1716 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 63 | 0.304075000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1716 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 64 | 0.304075000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1716 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 65 | 0.304075000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=1716 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 66 | 0.304075000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=1716 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 67 | 0.304075000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=1716 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 68 | 0.304075000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=1716 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 69 | 0.304159000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1716 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 70 | 0.306424000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1916 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 71 | 0.306424000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1916 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 72 | 0.306424000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1916 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 73 | 0.306424000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=1916 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 74 | 0.306424000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=1916 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 75 | 0.306424000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=1916 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 76 | 0.306424000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=1916 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 77 | 0.306424000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=1916 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 78 | 0.306424000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=0, A-MPDU=1916 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 79 | 0.306424000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=0, A-MPDU=1916 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 80 | 0.306424000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=0, A-MPDU=1916 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 81 | 0.306424000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=0, A-MPDU=1916 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 82 | 0.306424000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=0, A-MPDU=1916 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 83 | 0.306424000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=0, A-MPDU=1916 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 84 | 0.306508000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1916 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 85 | 0.309572000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=2167 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 86 | 0.309572000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=2167 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 87 | 0.309572000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=2167 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 88 | 0.309572000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=2167 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 89 | 0.309572000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=2167 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 90 | 0.309572000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=0, A-MPDU=2167 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 91 | 0.309572000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=0, A-MPDU=2167 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 92 | 0.309572000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=0, A-MPDU=2167 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 93 | 0.309572000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=0, A-MPDU=2167 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 94 | 0.309572000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=0, A-MPDU=2167 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 95 | 0.309572000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=0, A-MPDU=2167 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 96 | 0.309572000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=0, A-MPDU=2167 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 97 | 0.309572000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=24, frag=0, more-frag=0, TID=0, A-MPDU=2167 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 98 | 0.309572000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=25, frag=0, more-frag=0, TID=0, A-MPDU=2167 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 99 | 0.309572000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=26, frag=0, more-frag=0, TID=0, A-MPDU=2167 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22ca16" /></svg> | 100 | 0.309572000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=27, frag=0, more-frag=0, TID=0, A-MPDU=2167 |

</small>

Frame numbers are local to capture `VhtLongGI-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `VhtShortGI`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1600**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | - | 1448 | 90.50% | 1054.4 B | 90.9 B | 165.5 us | 10.8 us | 5040 MHz | - | 13.0 dBm | 96.70% | 23.96% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17ab1c" /></svg> | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC] | - | 18 | 1.12% | 1277.7 B | 497.2 B | 192.1 us | 59.2 us | 5040 MHz | - | 13.0 dBm | 1.40% | 0.35% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 14 | 0.88% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.16% | 0.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 90 | 5.62% | 50.7 B | 43.5 B | 36.9 us | 14.5 us | 5010 MHz | -58.0 dBm | - | 1.34% | 0.33% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 24 | 1.50% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -54.5 dBm | 13.0 dBm | 0.24% | 0.06% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Req | - | 3 | 0.19% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.08% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Resp | - | 3 | 0.19% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -58.3 dBm | - | 0.08% | 0.02% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17ab1c" /></svg> | 1 | 0.200064000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200124000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 3 | 0.200261000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200321000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17ab1c" /></svg> | 5 | 0.200437000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200497000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 7 | 0.200756000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.200816000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 9 | 0.200935000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.200995000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 11 | 0.201123000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.201183000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17ab1c" /></svg> | 13 | 0.201344000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.201404000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 15 | 0.201523000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.201583000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 17 | 0.201711000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.201771000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 19 | 0.210064000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=624 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 20 | 0.210148000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=624 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 21 | 0.210327000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=678 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 22 | 0.210411000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=678 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 23 | 0.210626000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=728 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 24 | 0.210710000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=728 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 25 | 0.220064000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=792 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 26 | 0.220148000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=792 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 27 | 0.220318000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=846 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 28 | 0.220402000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=846 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 29 | 0.220635000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=896 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 30 | 0.220719000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=896 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 31 | 0.230064000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=960 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 32 | 0.230148000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=960 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 33 | 0.230372000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1014 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 34 | 0.230456000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1014 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 35 | 0.230635000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1064 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 36 | 0.230719000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1064 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 37 | 0.240064000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1128 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 38 | 0.240148000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1128 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 39 | 0.240282000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1182 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 40 | 0.240366000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1182 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 41 | 0.240518000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1232 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 42 | 0.240602000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1232 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 43 | 0.300176000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1302 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 44 | 0.300260000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1302 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 45 | 0.300620000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1377 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 46 | 0.300620000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1377 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 47 | 0.300704000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1377 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 48 | 0.301346000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1466 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 49 | 0.301346000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1466 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 50 | 0.301346000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1466 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 51 | 0.301346000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1466 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 52 | 0.301430000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1466 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 53 | 0.302395000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1579 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 54 | 0.302395000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1579 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 55 | 0.302395000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1579 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 56 | 0.302395000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1579 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 57 | 0.302395000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1579 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 58 | 0.302395000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1579 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 59 | 0.302479000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1579 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 60 | 0.303795000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1716 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 61 | 0.303795000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1716 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 62 | 0.303795000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1716 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 63 | 0.303795000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1716 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 64 | 0.303795000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1716 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 65 | 0.303795000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=1716 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 66 | 0.303795000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=1716 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 67 | 0.303795000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=1716 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 68 | 0.303795000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=1716 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 69 | 0.303879000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1716 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 70 | 0.305600000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1880 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 71 | 0.305600000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1880 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 72 | 0.305600000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1880 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 73 | 0.305600000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=1880 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 74 | 0.305600000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=1880 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 75 | 0.305600000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=1880 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 76 | 0.305600000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=1880 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 77 | 0.305600000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=1880 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 78 | 0.305600000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=0, A-MPDU=1880 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 79 | 0.305600000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=0, A-MPDU=1880 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 80 | 0.305600000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=0, A-MPDU=1880 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 81 | 0.305600000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=0, A-MPDU=1880 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 82 | 0.305684000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1880 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 83 | 0.308123000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=2107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 84 | 0.308123000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=2107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 85 | 0.308123000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=2107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 86 | 0.308123000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=2107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 87 | 0.308123000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=2107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 88 | 0.308123000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=0, A-MPDU=2107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 89 | 0.308123000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=0, A-MPDU=2107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 90 | 0.308123000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=0, A-MPDU=2107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 91 | 0.308123000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=0, A-MPDU=2107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 92 | 0.308123000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=0, A-MPDU=2107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 93 | 0.308123000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=0, A-MPDU=2107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 94 | 0.308123000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=0, A-MPDU=2107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 95 | 0.308123000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=24, frag=0, more-frag=0, TID=0, A-MPDU=2107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 96 | 0.308123000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=25, frag=0, more-frag=0, TID=0, A-MPDU=2107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 97 | 0.308123000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=26, frag=0, more-frag=0, TID=0, A-MPDU=2107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 98 | 0.308123000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=27, frag=0, more-frag=0, TID=0, A-MPDU=2107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1eae2a" /></svg> | 99 | 0.308123000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=28, frag=0, more-frag=0, TID=0, A-MPDU=2107 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 100 | 0.308208000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2107 |

</small>

Frame numbers are local to capture `VhtShortGI-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Analysis of Packet Distribution
Across these configurations, **QoS Data** frames constitute the primary payload delivery mechanism, while **Block Ack (BA)** and **Block Ack Request (BAR)** control frames ensure reliable transport via the MAC-level acknowledgment protocol. Management frames, specifically **Beacons**, are transmitted periodically by the Access Point to maintain BSS time synchronization and broadcast network capabilities. The ratio of control/management overhead to actual data frames indicates the relative MAC efficiency of the chosen configurations.
<!-- END GENERATED: ieee80211-pcap-statistics -->
