# Walkthrough: 802.11n Preamble Modes (Mixed-Mode vs Greenfield)

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260809T153540Z`
- PCAP: `20260809T153540Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough demonstrates IEEE 802.11n preamble formats, comparing HT Mixed-Mode preambles (with legacy 802.11a/g preamble headers for backwards compatibility) against HT Greenfield preambles (optimized pure-HT PHY header).

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- Explain the structural difference between HT Mixed-Mode (L-STF, L-LTF, L-SIG, HT-SIG, HT-STF, HT-LTF) and HT Greenfield (HT-GF-STF, HT-LTF1, HT-SIG, HT-LTF).
- Understand how Mixed-Mode allows legacy 802.11a/g stations to decode L-SIG and update their Network Allocation Vector (NAV) duration even if they cannot decode the HT payload.
- Measure the overhead reduction achieved by Greenfield preambles in networks with homogeneous 802.11n hardware.

## [agent] Scenario description

The topology uses [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned). Three UDP flows are transmitted from `server` to `host[0..2]` (200-byte packets every 0.2 ms per station, 24 Mbps aggregate offered load). No legacy station is included, so this is a pure-HT preamble comparison rather than a coexistence test.

Two configurations are evaluated:

1. `HtMixedMode`: `opMode = "n(mixed-2.4Ghz)"` operating with legacy spoofing header fields.
2. `HtGreenfield`: `opMode = "n(greenfield-2.4Ghz)"` operating with streamlined Greenfield headers.

## [agent] Standards and INET model boundary

- **IEEE Std 802.11n-2009 / 802.11-2020 Clause 19.3.2 & 19.3.9**: Specifies HT Mixed-Mode and HT Greenfield PPDU frame structure formats.
- **INET Model Boundary**: HT preamble formats are selected via the `opMode` parameter and validated by `Ieee80211HtPreambleMode`.

## [agent] Scalar and vector analysis

<!-- BEGIN GENERATED: ieee80211-scalar-vector-preamble_modes -->
### [script] Generated scalar/vector plot and table

![preamble_modes scalar/vector analysis](results/20260809T153540Z/preamble-modes-delivery-delay.png)

Figure provenance: [`results/20260809T153540Z/preamble-modes-delivery-delay.png.json`](results/20260809T153540Z/preamble-modes-delivery-delay.png.json). Run-level metric source: [`../../ieee80211ax/analysis/metrics.json`](../../ieee80211ax/analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)<br>vector / **.app[*] / endToEndDelay:vector
- Window / per-run aggregation / exclusions: [0.3, 0.5) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| HT Greenfield / goodput mbps | 8.7648 | 0.0638365 |
| HT Mixed-Mode / goodput mbps | 8.8544 | 0.101883 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **PASS** | HT Greenfield preserves at least 95% of HT Mixed-Mode application delivery | Every matched run preserves at least 0.950 of baseline delivery. |
<!-- END GENERATED: ieee80211-scalar-vector-preamble_modes -->

<!-- BEGIN GENERATED: ieee80211-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260809T153540Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260809T153540Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260809T153540Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/n/capture_manifests/20260809T153540Z.json` (SHA-256 `a05858dcc840546ed08b8d2893641b656ba48fdf8a4e6dccce4651a4dfe9a031`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

<small>

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `HtMixedMode` | `none (all decoded frames)` | 1825 | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC, A-MPDU] (1750), Control: Block Ack (BA) (57), Control: Ack (9) | 35.44% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `HtGreenfield` | `none (all decoded frames)` | 1831 | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC, A-MPDU] (1756), Control: Block Ack (BA) (57), Control: Ack (9) | 35.54% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

</small>

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | HtGreenfield produced protocol-visible wireless observations | 1831 AP/global transmission observations |
| **PASS** | HtMixedMode produced protocol-visible wireless observations | 1825 AP/global transmission observations |

### [script] Configuration: `HtMixedMode`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1825**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#259c21" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC, A-MPDU] | - | 1750 | 95.89% | 267.8 B | 22.0 B | 200.8 us | 13.5 us | 2412 MHz | - | 13.0 dBm | 99.16% | 35.14% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35e01f" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC] | - | 3 | 0.16% | 266.0 B | 0.0 B | 199.7 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.17% | 0.06% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 57 | 3.12% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 2412 MHz | -52.3 dBm | - | 0.49% | 0.17% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 9 | 0.49% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -52.3 dBm | 13.0 dBm | 0.06% | 0.02% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Req | - | 3 | 0.16% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.06% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Resp | - | 3 | 0.16% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 2412 MHz | -52.3 dBm | - | 0.06% | 0.02% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 1 | 0.200204000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200264000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 3 | 0.200416000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200476000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 5 | 0.200688000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200748000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 7 | 0.201102000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.201162000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 9 | 0.201688000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.201748000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 11 | 0.201880000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.201940000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 13 | 0.202574000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.202634000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 15 | 0.203040000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.203100000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 17 | 0.203272000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.203332000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 19 | 0.206406000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=907 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 20 | 0.206406000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=907 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 21 | 0.206406000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=907 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 22 | 0.206406000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=907 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 23 | 0.206406000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=907 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 24 | 0.206406000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=907 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 25 | 0.206406000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=907 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 26 | 0.206406000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=907 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 27 | 0.206406000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=907 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 28 | 0.206406000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=907 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 29 | 0.206406000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=907 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 30 | 0.206406000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=907 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 31 | 0.206406000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=907 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 32 | 0.206406000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=907 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 33 | 0.206406000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=907 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 34 | 0.206406000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=907 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 35 | 0.206406000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=0, A-MPDU=907 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 36 | 0.206490000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=907 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 37 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 38 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 39 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 40 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 41 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 42 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 43 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 44 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 45 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 46 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 47 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 48 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 49 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 50 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 51 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 52 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 53 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 54 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 55 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 56 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 57 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 58 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 59 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 60 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=24, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 61 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=25, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 62 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=26, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 63 | 0.211516000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=27, frag=0, more-frag=0, TID=0, A-MPDU=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 64 | 0.211600000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1311 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 65 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 66 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 67 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 68 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 69 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 70 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 71 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 72 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 73 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 74 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 75 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 76 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 77 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 78 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 79 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 80 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 81 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 82 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 83 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 84 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 85 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 86 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 87 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 88 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=24, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 89 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=25, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 90 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=26, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 91 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=27, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 92 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=28, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 93 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=29, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 94 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=30, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 95 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=31, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 96 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=32, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 97 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=33, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 98 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=34, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 99 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=35, frag=0, more-frag=0, TID=0, A-MPDU=1871 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 100 | 0.217734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=36, frag=0, more-frag=0, TID=0, A-MPDU=1871 |

</small>

Frame numbers are local to capture `HtMixedMode-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `HtGreenfield`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1831**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#259c21" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC, A-MPDU] | - | 1756 | 95.90% | 267.7 B | 21.0 B | 200.7 us | 12.9 us | 2412 MHz | - | 13.0 dBm | 99.16% | 35.25% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35e01f" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC] | - | 3 | 0.16% | 266.0 B | 0.0 B | 199.7 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.17% | 0.06% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 57 | 3.11% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 2412 MHz | -52.3 dBm | - | 0.49% | 0.17% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 9 | 0.49% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -52.3 dBm | 13.0 dBm | 0.06% | 0.02% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Req | - | 3 | 0.16% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.06% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Resp | - | 3 | 0.16% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 2412 MHz | -52.3 dBm | - | 0.06% | 0.02% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 1 | 0.200192000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200252000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 3 | 0.200404000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200464000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 5 | 0.200676000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200736000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 7 | 0.201078000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.201138000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 9 | 0.201652000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.201712000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 11 | 0.201844000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.201904000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 13 | 0.202526000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.202586000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 15 | 0.203040000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.203100000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 17 | 0.203272000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.203332000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 19 | 0.206762000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=925 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 20 | 0.206762000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=925 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 21 | 0.206762000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=925 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 22 | 0.206762000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=925 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 23 | 0.206762000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=925 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 24 | 0.206762000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=925 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 25 | 0.206762000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=925 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 26 | 0.206762000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=925 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 27 | 0.206762000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=925 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 28 | 0.206762000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=925 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 29 | 0.206762000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=925 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 30 | 0.206762000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=925 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 31 | 0.206762000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=925 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 32 | 0.206762000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=925 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 33 | 0.206762000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=925 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 34 | 0.206762000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=925 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 35 | 0.206762000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=0, A-MPDU=925 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 36 | 0.206762000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=0, A-MPDU=925 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 37 | 0.206846000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=925 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 38 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 39 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 40 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 41 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 42 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 43 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 44 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 45 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 46 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 47 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 48 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 49 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 50 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 51 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 52 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 53 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 54 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 55 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 56 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 57 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 58 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 59 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 60 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 61 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=24, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 62 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=25, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 63 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=26, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 64 | 0.211740000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=27, frag=0, more-frag=0, TID=0, A-MPDU=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 65 | 0.211824000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1332 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 66 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 67 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 68 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 69 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 70 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 71 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 72 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 73 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 74 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 75 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 76 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 77 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 78 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 79 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 80 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 81 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 82 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 83 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 84 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 85 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 86 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 87 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 88 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 89 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=24, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 90 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=25, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 91 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=26, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 92 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=27, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 93 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=28, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 94 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=29, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 95 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=30, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 96 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=31, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 97 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=32, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 98 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=33, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 99 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=34, frag=0, more-frag=0, TID=0, A-MPDU=1910 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 100 | 0.218226000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=35, frag=0, more-frag=0, TID=0, A-MPDU=1910 |

</small>

Frame numbers are local to capture `HtGreenfield-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Analysis of Packet Distribution
Across these configurations, **QoS Data** frames constitute the primary payload delivery mechanism, while **Block Ack (BA)** and **Block Ack Request (BAR)** control frames ensure reliable transport via the MAC-level acknowledgment protocol. Management frames, specifically **Beacons**, are transmitted periodically by the Access Point to maintain BSS time synchronization and broadcast network capabilities. The ratio of control/management overhead to actual data frames indicates the relative MAC efficiency of the chosen configurations.
<!-- END GENERATED: ieee80211-pcap-statistics -->
