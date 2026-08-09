# Walkthrough: 802.11ac VHT MCS Rates & 256-QAM

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260809T211000Z`
- PCAP: `20260809T211000Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough compares the HT baseline with the supported fixed VHT 256-QAM mode at 80 MHz and a 40 MHz width control. It keeps the traffic, NSS, guard interval, and topology fixed while making the VHT modulation/width settings explicit.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- Understand how 256-QAM (8 bits per subcarrier) increases spectral efficiency over VHT 64-QAM (6 bits per subcarrier).
- Compare the HT baseline with VHT MCS 8 at fixed 80 MHz, then use the 40 MHz control to separate modulation from channel width.
- Use the separate wide-channel example for bandwidth changes; this scenario does not establish SNR thresholds.

## [agent] Scenario description

The topology uses [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned). UDP data is transmitted from `server` to `host[0]`.

Three configurations are evaluated:

1. `BaselineHtMcs7`: 802.11n HT MCS 7 baseline (64-QAM 5/6, 65 Mbps).
2. `VhtMcs8_256QAM`: 802.11ac VHT MCS 8 in 80 MHz (256-QAM 3/4, 351 Mbps).
3. `VhtMcs8_256QAM_40Mhz`: VHT MCS 8 in 40 MHz (162 Mbps), a width control.

## [agent] Standards and INET model boundary

- **IEEE Std 802.11ac-2013 / 802.11-2020 Clause 21.3.5 & Table 21-22**: Defines VHT MCS parameters, coding rates, and modulation schemes up to 256-QAM.
- **INET Model Boundary**: The packet-level VHT mode table supplies the legal MCS/bandwidth/rate tuples; this illustrative example does not model a MIMO channel matrix or derive an SNR threshold.

## [agent] Scalar and vector analysis

<!-- BEGIN GENERATED: ieee80211-scalar-vector-vht_mcs_256qam -->
### [script] Generated scalar/vector plot and table

![vht_mcs_256qam scalar/vector analysis](results/20260809T211000Z/vht-mcs-256qam-delivery-delay.png)

Figure provenance: [`results/20260809T211000Z/vht-mcs-256qam-delivery-delay.png.json`](results/20260809T211000Z/vht-mcs-256qam-delivery-delay.png.json). Run-level metric source: [`../../ieee80211ax/analysis/metrics.json`](../../ieee80211ax/analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)<br>vector / **.app[*] / endToEndDelay:vector
- Window / per-run aggregation / exclusions: [0.3, 0.5) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| HT MCS 7 Baseline / goodput mbps | 11.8384 | 0.116351 |
| VHT MCS 8 (256-QAM) / goodput mbps | 28.1456 | 0.248769 |
| VHT MCS 8 at 40 MHz / goodput mbps | 23.4864 | 0.105452 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **PASS** | Compare application delivered bytes between the HT baseline, supported fixed 80 MHz VHT 256-QAM mode, and a 40 MHz VHT width control | Every matched run preserves at least 0.800 of baseline delivery. |
<!-- END GENERATED: ieee80211-scalar-vector-vht_mcs_256qam -->

<!-- BEGIN GENERATED: ieee80211-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260809T211000Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260809T211000Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260809T211000Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ac/capture_manifests/20260809T211000Z.json` (SHA-256 `09afbf638f52a515f4c0a12b61db181600a47c47a5e0ad56b7a20ef9017d1da9`). VHT PPDU format, MCS, coding, bandwidth, GI, and NSTS are decoded directly from standards-compliant radiotap VHT fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. VHT SU and VHT MU use modeled preambles; per-user VHT MU signaling remains approximate because radiotap carries common MU metadata alongside each logical user record.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

<small>

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `BaselineHtMcs7` | `none (all decoded frames)` | 960 | QoS Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC] (480), Control: Ack (480) | 11.60% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `VhtMcs8_256QAM` | `none (all decoded frames)` | 1834 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] (917), Control: Ack (917) | 8.90% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `VhtMcs8_256QAM_40Mhz` | `none (all decoded frames)` | 1584 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] (792), Control: Ack (792) | 10.88% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

</small>

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | BaselineHtMcs7 produced protocol-visible wireless observations | 960 AP/global transmission observations |
| **PASS** | VhtMcs8_256QAM produced protocol-visible wireless observations | 1834 AP/global transmission observations |
| **PASS** | VhtMcs8_256QAM_40Mhz produced protocol-visible wireless observations | 1584 AP/global transmission observations |

### [script] Configuration: `BaselineHtMcs7`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **960**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ad440" /></svg> | QoS Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC] | - | 480 | 50.00% | 1470.8 B | 12.8 B | 217.0 us | 1.6 us | 2412 MHz | - | 13.0 dBm | 89.79% | 10.42% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 480 | 50.00% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -52.8 dBm | - | 10.21% | 1.18% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 1 | 0.200220000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200280000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 3 | 0.200630000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200690000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 5 | 0.201430000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.201490000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 7 | 0.202290000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.202350000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 9 | 0.202990000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.203050000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 11 | 0.204030000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.204090000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 13 | 0.205270000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.205330000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 15 | 0.206250000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.206310000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 17 | 0.207150000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.207210000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 19 | 0.208410000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.208470000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 21 | 0.209630000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.209690000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 23 | 0.210530000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 24 | 0.210590000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 25 | 0.211410000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 26 | 0.211470000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 27 | 0.212550000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 28 | 0.212610000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 29 | 0.213330000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 30 | 0.213390000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 31 | 0.214530000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 32 | 0.214590000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 33 | 0.215630000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 34 | 0.215690000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 35 | 0.216470000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 36 | 0.216530000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 37 | 0.217650000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 38 | 0.217710000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 39 | 0.218510000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 40 | 0.218570000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 41 | 0.219530000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 42 | 0.219590000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 43 | 0.220650000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 44 | 0.220710000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 45 | 0.221370000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 46 | 0.221430000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 47 | 0.222590000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 48 | 0.222650000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 49 | 0.223250000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 50 | 0.223310000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 51 | 0.224030000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 52 | 0.224090000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 53 | 0.225270000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 54 | 0.225330000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 55 | 0.226470000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 56 | 0.226530000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 57 | 0.227190000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 58 | 0.227250000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 59 | 0.228370000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 60 | 0.228430000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 61 | 0.229630000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 62 | 0.229690000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 63 | 0.230510000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 64 | 0.230570000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 65 | 0.231250000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 66 | 0.231310000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 67 | 0.232350000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 68 | 0.232410000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 69 | 0.233330000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 70 | 0.233390000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 71 | 0.233990000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 72 | 0.234050000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 73 | 0.234810000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 74 | 0.234870000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 75 | 0.235530000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 76 | 0.235590000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 77 | 0.236390000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 78 | 0.236450000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 79 | 0.237310000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 80 | 0.237370000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 81 | 0.238170000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 82 | 0.238230000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 83 | 0.239090000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 84 | 0.239150000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 85 | 0.239950000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 86 | 0.240010000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 87 | 0.240970000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 88 | 0.241030000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 89 | 0.241970000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 90 | 0.242030000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 91 | 0.243130000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 92 | 0.243190000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 93 | 0.243930000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 94 | 0.243990000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 95 | 0.244750000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 96 | 0.244810000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 97 | 0.245430000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 98 | 0.245490000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 99 | 0.246170000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 100 | 0.246230000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

</small>

Frame numbers are local to capture `BaselineHtMcs7-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `VhtMcs8_256QAM`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1834**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | - | 917 | 50.00% | 1471.3 B | 13.4 B | 72.4 us | 0.3 us | 5040 MHz | - | 13.0 dBm | 74.59% | 6.64% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 917 | 50.00% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -56.4 dBm | - | 25.41% | 2.26% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 1 | 0.200076000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200136000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 3 | 0.200264000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200324000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 5 | 0.200685000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200745000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 7 | 0.201133000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.201193000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 9 | 0.201536000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.201596000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 11 | 0.201957000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.202017000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 13 | 0.202315000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.202375000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 15 | 0.202772000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.202832000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 17 | 0.203229000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.203289000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 19 | 0.203632000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.203692000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 21 | 0.203990000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.204050000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 23 | 0.204348000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 24 | 0.204408000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 25 | 0.204760000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 26 | 0.204820000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 27 | 0.205100000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 28 | 0.205160000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 29 | 0.205449000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 30 | 0.205509000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 31 | 0.205825000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 32 | 0.205885000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 33 | 0.206210000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 34 | 0.206270000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 35 | 0.206604000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 36 | 0.206664000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 37 | 0.207061000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 38 | 0.207121000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 39 | 0.207518000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 40 | 0.207578000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 41 | 0.207957000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 42 | 0.208017000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 43 | 0.208360000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 44 | 0.208420000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 45 | 0.208790000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 46 | 0.208850000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 47 | 0.209238000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 48 | 0.209298000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 49 | 0.209659000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 50 | 0.209719000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 51 | 0.210053000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 52 | 0.210113000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 53 | 0.210510000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 54 | 0.210570000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 55 | 0.210895000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 56 | 0.210955000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 57 | 0.211334000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 58 | 0.211394000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 59 | 0.211764000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 60 | 0.211824000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 61 | 0.212113000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 62 | 0.212173000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 63 | 0.212543000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 64 | 0.212603000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 65 | 0.212874000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 66 | 0.212934000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 67 | 0.213331000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 68 | 0.213391000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 69 | 0.213743000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 70 | 0.213803000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 71 | 0.214137000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 72 | 0.214197000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 73 | 0.214558000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 74 | 0.214618000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 75 | 0.214943000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 76 | 0.215003000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 77 | 0.215337000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 78 | 0.215397000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 79 | 0.215722000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 80 | 0.215782000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 81 | 0.216098000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 82 | 0.216158000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 83 | 0.216474000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 84 | 0.216534000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 85 | 0.216931000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 86 | 0.216991000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 87 | 0.217262000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 88 | 0.217322000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 89 | 0.217611000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 90 | 0.217671000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 91 | 0.218032000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 92 | 0.218092000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 93 | 0.218453000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 94 | 0.218513000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 95 | 0.218820000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 96 | 0.218880000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 97 | 0.219259000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 98 | 0.219319000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 99 | 0.219680000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 100 | 0.219740000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

</small>

Frame numbers are local to capture `VhtMcs8_256QAM-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `VhtMcs8_256QAM_40Mhz`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1584**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | - | 792 | 50.00% | 1471.4 B | 13.5 B | 112.7 us | 0.7 us | 5020 MHz | - | 13.0 dBm | 82.04% | 8.92% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 792 | 50.00% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -56.4 dBm | - | 17.96% | 1.95% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 1 | 0.200116000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200176000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 3 | 0.200335000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200395000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 5 | 0.200863000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200923000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 7 | 0.201364000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.201424000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 9 | 0.201802000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.201862000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 11 | 0.202312000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.202372000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 13 | 0.202804000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.202864000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 15 | 0.203305000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.203365000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 17 | 0.203743000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.203803000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 19 | 0.204289000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.204349000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 21 | 0.204826000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.204886000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 23 | 0.205282000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 24 | 0.205342000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 25 | 0.205756000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 26 | 0.205816000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 27 | 0.206176000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 28 | 0.206236000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 29 | 0.206659000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 30 | 0.206719000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 31 | 0.207151000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 32 | 0.207211000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 33 | 0.207697000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 34 | 0.207757000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 35 | 0.208171000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 36 | 0.208231000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 37 | 0.208627000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 38 | 0.208687000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 39 | 0.209065000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 40 | 0.209125000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 41 | 0.209566000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 42 | 0.209626000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 43 | 0.210085000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 44 | 0.210145000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 45 | 0.210505000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 46 | 0.210565000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 47 | 0.211015000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 48 | 0.211075000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 49 | 0.211471000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 50 | 0.211531000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 51 | 0.211999000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 52 | 0.212059000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 53 | 0.212491000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 54 | 0.212551000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 55 | 0.212956000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 56 | 0.213016000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 57 | 0.213385000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 58 | 0.213445000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 59 | 0.213877000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 60 | 0.213937000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 61 | 0.214378000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 62 | 0.214438000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 63 | 0.214888000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 64 | 0.214948000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 65 | 0.215326000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 66 | 0.215386000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 67 | 0.215755000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 68 | 0.215815000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 69 | 0.216202000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 70 | 0.216262000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 71 | 0.216613000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 72 | 0.216673000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 73 | 0.217060000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 74 | 0.217120000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 75 | 0.217588000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 76 | 0.217648000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 77 | 0.218098000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 78 | 0.218158000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 79 | 0.218581000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 80 | 0.218641000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 81 | 0.219091000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 82 | 0.219151000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 83 | 0.219619000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 84 | 0.219679000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 85 | 0.220093000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 86 | 0.220153000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 87 | 0.220621000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 88 | 0.220681000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 89 | 0.221050000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 90 | 0.221110000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 91 | 0.221497000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 92 | 0.221557000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 93 | 0.221989000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 94 | 0.222049000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 95 | 0.222481000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 96 | 0.222541000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 97 | 0.223018000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 98 | 0.223078000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19e123" /></svg> | 99 | 0.223438000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 40 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 100 | 0.223498000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

</small>

Frame numbers are local to capture `VhtMcs8_256QAM_40Mhz-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Analysis of Packet Distribution
Across these configurations, **QoS Data** frames constitute the primary payload delivery mechanism, while **Block Ack (BA)** and **Block Ack Request (BAR)** control frames ensure reliable transport via the MAC-level acknowledgment protocol. Management frames, specifically **Beacons**, are transmitted periodically by the Access Point to maintain BSS time synchronization and broadcast network capabilities. The ratio of control/management overhead to actual data frames indicates the relative MAC efficiency of the chosen configurations.
<!-- END GENERATED: ieee80211-pcap-statistics -->
