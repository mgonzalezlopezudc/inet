# Walkthrough: 802.11n Frame Aggregation

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260804T222445Z`
- PCAP: `20260804T222445Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough demonstrates IEEE 802.11n High Throughput (HT) frame aggregation mechanisms, comparing un-aggregated transmission against MSDU aggregation (A-MSDU), MPDU aggregation (A-MPDU), and two-level combined aggregation.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- Explain how IEEE 802.11n frame aggregation reduces PHY overhead (preambles, headers, DIFS, and BACK SIFS intervals).
- Differentiate A-MSDU (packing multiple network layer SDUs into one MAC SDU) from A-MPDU (packing multiple MAC SDUs into one physical frame with subframe delimiters).
- Understand how two-level aggregation achieves maximum channel utilization under high offered load.
- Observe MAC queueing, subframe encapsulation, and throughput gains in simulation captures.

## [agent] Scenario description

The topology uses the common single-BSS network [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned) with an Access Point (`ap`), two client stations (`host[0]`, `host[1]`), and a wired server (`server`).

```text
server === wired link === AP -- 802.11n HT wireless --> host[0]
                                                     `--> host[1]
```

High-rate UDP traffic (1000-byte packets every 0.2 ms) is transmitted from `server` to `host[0]` over a 2.4 GHz 802.11n channel (`opMode = "n(mixed-2.4Ghz)"`). Four configurations are evaluated:

1. `NoAggregation`: Standard un-aggregated 802.11 transmission.
2. `AMsduOnly`: MSDU aggregation up to 3839 bytes.
3. `AMpduOnly`: MPDU aggregation up to 65535 bytes with Block ACK.
4. `TwoLevelAggregation`: Combined A-MSDU inside A-MPDU aggregation.

## [agent] Standards and INET model boundary

- **IEEE Std 802.11n-2009 / 802.11-2020 Clause 19 & 9.8**: Defines A-MSDU and A-MPDU frame structures, delimiter checks, and Block Ack requirements.
- **INET Model Boundary**: A-MSDU subframes and A-MPDU subframe delimiters are encapsulated as chunk fields inside `Packet` objects (`Ieee80211MsduAggregationPolicy` and `Ieee80211MpduAggregationPolicy`). Bit-level channel corruption is evaluated analytically via NIST error models.

## [agent] Scalar and vector analysis

<!-- BEGIN GENERATED: ieee80211-scalar-vector-frame_aggregation -->
### [script] Generated scalar/vector plot and table

![frame_aggregation scalar/vector analysis](results/20260804T222445Z/frame-aggregation-delivery-delay.png)

Figure provenance: [`results/20260804T222445Z/frame-aggregation-delivery-delay.png.json`](results/20260804T222445Z/frame-aggregation-delivery-delay.png.json). Run-level metric source: [`../../ieee80211ax/analysis/metrics.json`](../../ieee80211ax/analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)<br>vector / **.app[*] / endToEndDelay:vector
- Window / per-run aggregation / exclusions: [0.3, 0.9) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| A-MPDU Only / goodput mbps | 8.08453 | 0.00188762 |
| A-MSDU Only / goodput mbps | 8.0384 | 0.0462726 |
| No Aggregation / goodput mbps | 8.08453 | 0.00188762 |
| Two-Level Aggregation / goodput mbps | 8.0384 | 0.0462726 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **INCONCLUSIVE** | Compare application delivered bytes across frame aggregation policies | No manifest acceptance threshold defined for frame aggregation delivery comparison |
<!-- END GENERATED: ieee80211-scalar-vector-frame_aggregation -->

<!-- BEGIN GENERATED: ieee80211-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260804T222445Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260804T222445Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260804T222445Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/n/capture_manifests/20260804T222445Z.json` (SHA-256 `0c0d082031c97ef18ec0084b9d5046f8bf37ec4163e64b4e5b00babc17dbcac4`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `NoAggregation` | `none (all decoded frames)` | 10114 | QoS Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC, A-MPDU] (4411), QoS Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC] (2886), Control: Block Ack Request (BAR) (1384) | 49.47% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `AMsduOnly` | `none (all decoded frames)` | 9814 | QoS Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC, A-MPDU] (3967), QoS Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC] (3032), Control: Block Ack Request (BAR) (1378) | 48.08% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `AMpduOnly` | `none (all decoded frames)` | 10114 | QoS Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC, A-MPDU] (4411), QoS Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC] (2886), Control: Block Ack Request (BAR) (1384) | 49.47% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `TwoLevelAggregation` | `none (all decoded frames)` | 9814 | QoS Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC, A-MPDU] (3967), QoS Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC] (3032), Control: Block Ack Request (BAR) (1378) | 48.08% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | AMpduOnly produced protocol-visible wireless observations | 10114 AP/global transmission observations |
| **PASS** | AMsduOnly produced protocol-visible wireless observations | 9814 AP/global transmission observations |
| **PASS** | NoAggregation produced protocol-visible wireless observations | 10114 AP/global transmission observations |
| **PASS** | TwoLevelAggregation produced protocol-visible wireless observations | 9814 AP/global transmission observations |

### [script] Configuration: `NoAggregation`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **10114**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | QoS Data | - | 3 | 0.03% | 84.0 B | 0.0 B | 48.0 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.03% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#13aa1d" /></svg> | QoS Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC, A-MPDU] | - | 4411 | 43.61% | 166.0 B | 0.0 B | 56.4 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 50.31% | 24.89% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ad440" /></svg> | QoS Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC] | - | 2886 | 28.53% | 165.9 B | 3.2 B | 56.4 us | 0.4 us | 2412 MHz | -52.3 dBm | 13.0 dBm | 32.91% | 16.28% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 1384 | 13.68% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 7.83% | 3.88% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 1384 | 13.68% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 2412 MHz | -50.0 dBm | - | 8.58% | 4.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 34 | 0.34% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -50.8 dBm | 13.0 dBm | 0.17% | 0.08% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 12 | 0.12% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 2412 MHz | -52.3 dBm | 13.0 dBm | 0.17% | 0.08% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 1 | 0.200058000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 2 | 0.200246000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 3 | 0.200502000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 4 | 0.200660000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 5 | 0.200720000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 6 | 0.200872000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 7 | 0.200932000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 8 | 0.201084000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action | direction=direct/IBSS, retry=0, seq=2, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 9 | 0.201144000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 10 | 0.201214000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 11 | 0.201274000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 12 | 0.201344000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.201404000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 14 | 0.201496000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action | direction=direct/IBSS, retry=0, seq=3, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.201556000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 16 | 0.201626000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 17 | 0.201686000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 18 | 0.201756000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 19 | 0.201817000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 20 | 0.201887000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 21 | 0.201947000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 22 | 0.202017000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 23 | 0.202077000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 24 | 0.202147000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 25 | 0.202207000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 26 | 0.202277000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 27 | 0.202337000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 28 | 0.202407000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 29 | 0.202467000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 30 | 0.202659000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=1, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 31 | 0.202719000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 32 | 0.202973000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=7, A-MPDU=1209 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 33 | 0.202973000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=7, A-MPDU=1209 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 34 | 0.202973000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=7, A-MPDU=1209 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 35 | 0.202973000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=7, A-MPDU=1209 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 36 | 0.202973000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=7, A-MPDU=1209 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 37 | 0.202973000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=7, A-MPDU=1209 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 38 | 0.203085000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 39 | 0.203169000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 40 | 0.203239000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 41 | 0.203309000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 42 | 0.203460000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 43 | 0.203598000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 44 | 0.203658000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 45 | 0.203810000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 46 | 0.203870000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 47 | 0.204022000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action | direction=direct/IBSS, retry=0, seq=3, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 48 | 0.204082000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 49 | 0.204152000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 50 | 0.204212000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

Frame numbers are local to capture `NoAggregation-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `AMsduOnly`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **9814**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | QoS Data | - | 2 | 0.02% | 125.0 B | 41.0 B | 61.7 us | 13.7 us | 2412 MHz | - | 13.0 dBm | 0.03% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#13aa1d" /></svg> | QoS Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC, A-MPDU] | - | 3967 | 40.42% | 166.0 B | 2.6 B | 56.4 us | 0.3 us | 2412 MHz | - | 13.0 dBm | 46.57% | 22.39% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ad440" /></svg> | QoS Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC] | - | 3032 | 30.89% | 173.7 B | 95.5 B | 57.4 us | 11.8 us | 2412 MHz | -52.3 dBm | 13.0 dBm | 36.19% | 17.40% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 1378 | 14.04% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 8.03% | 3.86% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 1378 | 14.04% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 2412 MHz | -50.0 dBm | - | 8.79% | 4.23% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 45 | 0.46% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -52.5 dBm | 13.0 dBm | 0.23% | 0.11% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 12 | 0.12% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 2412 MHz | -52.3 dBm | 13.0 dBm | 0.17% | 0.08% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 1 | 0.200058000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 2 | 0.200274000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 3 | 0.200452000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200512000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 5 | 0.200644000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200704000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 7 | 0.200876000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action | direction=direct/IBSS, retry=0, seq=1, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.200936000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 9 | 0.201062000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.201122000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 11 | 0.201192000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.201252000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 13 | 0.201344000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action | direction=direct/IBSS, retry=0, seq=2, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.201404000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 15 | 0.201494000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.201554000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 17 | 0.201624000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.201685000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 19 | 0.201755000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.201815000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 21 | 0.201885000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.201945000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 23 | 0.202097000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=1, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 24 | 0.202157000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 25 | 0.202327000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 26 | 0.202445000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 27 | 0.202505000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 28 | 0.202637000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 29 | 0.202697000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 30 | 0.202869000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action | direction=direct/IBSS, retry=0, seq=2, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 31 | 0.202929000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 32 | 0.203039000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=7, A-MPDU=1290 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 33 | 0.203039000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=7, A-MPDU=1290 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 34 | 0.203109000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 35 | 0.203260000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 36 | 0.203398000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 37 | 0.203458000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 38 | 0.203590000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 39 | 0.203650000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 40 | 0.203822000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 41 | 0.203906000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 42 | 0.203998000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action | direction=direct/IBSS, retry=0, seq=2, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 43 | 0.204058000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 44 | 0.204292000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 45 | 0.204352000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 46 | 0.204558000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 47 | 0.204618000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 48 | 0.204816000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=7, A-MPDU=1953 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 49 | 0.204816000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=7, A-MPDU=1953 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 50 | 0.204816000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=7, A-MPDU=1953 |

Frame numbers are local to capture `AMsduOnly-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `AMpduOnly`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **10114**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | QoS Data | - | 3 | 0.03% | 84.0 B | 0.0 B | 48.0 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.03% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#13aa1d" /></svg> | QoS Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC, A-MPDU] | - | 4411 | 43.61% | 166.0 B | 0.0 B | 56.4 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 50.31% | 24.89% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ad440" /></svg> | QoS Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC] | - | 2886 | 28.53% | 165.9 B | 3.2 B | 56.4 us | 0.4 us | 2412 MHz | -52.3 dBm | 13.0 dBm | 32.91% | 16.28% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 1384 | 13.68% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 7.83% | 3.88% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 1384 | 13.68% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 2412 MHz | -50.0 dBm | - | 8.58% | 4.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 34 | 0.34% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -50.8 dBm | 13.0 dBm | 0.17% | 0.08% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 12 | 0.12% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 2412 MHz | -52.3 dBm | 13.0 dBm | 0.17% | 0.08% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 1 | 0.200058000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 2 | 0.200246000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 3 | 0.200502000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 4 | 0.200660000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 5 | 0.200720000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 6 | 0.200872000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 7 | 0.200932000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 8 | 0.201084000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action | direction=direct/IBSS, retry=0, seq=2, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 9 | 0.201144000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 10 | 0.201214000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 11 | 0.201274000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 12 | 0.201344000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.201404000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 14 | 0.201496000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action | direction=direct/IBSS, retry=0, seq=3, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.201556000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 16 | 0.201626000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 17 | 0.201686000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 18 | 0.201756000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 19 | 0.201817000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 20 | 0.201887000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 21 | 0.201947000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 22 | 0.202017000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 23 | 0.202077000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 24 | 0.202147000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 25 | 0.202207000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 26 | 0.202277000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 27 | 0.202337000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 28 | 0.202407000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 29 | 0.202467000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 30 | 0.202659000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=1, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 31 | 0.202719000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 32 | 0.202973000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=7, A-MPDU=1209 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 33 | 0.202973000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=7, A-MPDU=1209 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 34 | 0.202973000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=7, A-MPDU=1209 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 35 | 0.202973000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=7, A-MPDU=1209 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 36 | 0.202973000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=7, A-MPDU=1209 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 37 | 0.202973000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=7, A-MPDU=1209 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 38 | 0.203085000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 39 | 0.203169000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 40 | 0.203239000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 41 | 0.203309000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 42 | 0.203460000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 43 | 0.203598000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 44 | 0.203658000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 45 | 0.203810000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 46 | 0.203870000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 47 | 0.204022000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action | direction=direct/IBSS, retry=0, seq=3, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 48 | 0.204082000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 49 | 0.204152000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 50 | 0.204212000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

Frame numbers are local to capture `AMpduOnly-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `TwoLevelAggregation`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **9814**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | QoS Data | - | 2 | 0.02% | 125.0 B | 41.0 B | 61.7 us | 13.7 us | 2412 MHz | - | 13.0 dBm | 0.03% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#13aa1d" /></svg> | QoS Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC, A-MPDU] | - | 3967 | 40.42% | 166.0 B | 2.6 B | 56.4 us | 0.3 us | 2412 MHz | - | 13.0 dBm | 46.57% | 22.39% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ad440" /></svg> | QoS Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC] | - | 3032 | 30.89% | 173.7 B | 95.5 B | 57.4 us | 11.8 us | 2412 MHz | -52.3 dBm | 13.0 dBm | 36.19% | 17.40% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 1378 | 14.04% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 8.03% | 3.86% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 1378 | 14.04% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 2412 MHz | -50.0 dBm | - | 8.79% | 4.23% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 45 | 0.46% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -52.5 dBm | 13.0 dBm | 0.23% | 0.11% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 12 | 0.12% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 2412 MHz | -52.3 dBm | 13.0 dBm | 0.17% | 0.08% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 1 | 0.200058000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 2 | 0.200274000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 3 | 0.200452000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200512000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 5 | 0.200644000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200704000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 7 | 0.200876000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action | direction=direct/IBSS, retry=0, seq=1, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.200936000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 9 | 0.201062000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.201122000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 11 | 0.201192000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.201252000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 13 | 0.201344000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action | direction=direct/IBSS, retry=0, seq=2, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.201404000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 15 | 0.201494000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.201554000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 17 | 0.201624000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.201685000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 19 | 0.201755000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.201815000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 21 | 0.201885000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.201945000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 23 | 0.202097000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=1, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 24 | 0.202157000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 25 | 0.202327000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 26 | 0.202445000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 27 | 0.202505000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 28 | 0.202637000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 29 | 0.202697000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 30 | 0.202869000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action | direction=direct/IBSS, retry=0, seq=2, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 31 | 0.202929000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 32 | 0.203039000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=7, A-MPDU=1290 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 33 | 0.203039000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=7, A-MPDU=1290 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 34 | 0.203109000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 35 | 0.203260000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 36 | 0.203398000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 37 | 0.203458000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 38 | 0.203590000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 39 | 0.203650000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 40 | 0.203822000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 41 | 0.203906000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 42 | 0.203998000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action | direction=direct/IBSS, retry=0, seq=2, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 43 | 0.204058000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 44 | 0.204292000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 45 | 0.204352000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 46 | 0.204558000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 47 | 0.204618000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 48 | 0.204816000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=7, A-MPDU=1953 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 49 | 0.204816000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=7, A-MPDU=1953 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 50 | 0.204816000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=7, A-MPDU=1953 |

Frame numbers are local to capture `TwoLevelAggregation-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Analysis of Packet Distribution
Across these configurations, **QoS Data** frames constitute the primary payload delivery mechanism, while **Block Ack (BA)** and **Block Ack Request (BAR)** control frames ensure reliable transport via the MAC-level acknowledgment protocol. Management frames, specifically **Beacons**, are transmitted periodically by the Access Point to maintain BSS time synchronization and broadcast network capabilities. The ratio of control/management overhead to actual data frames indicates the relative MAC efficiency of the chosen configurations.
<!-- END GENERATED: ieee80211-pcap-statistics -->
