# Walkthrough: 802.11ac VHT Rate Adaptation (AARF and Minstrel)

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260809T174942Z`
- PCAP: `20260809T174942Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough compares fixed VHT MCS 8 with VHT Minstrel under the same 5 GHz/80 MHz traffic and mobility setup. The AARF configuration remains in the INI as an optional model probe, but is not part of the reproducible suite because its generic rate ladder selects a VHT mode rejected by the peer capability state.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- Explain how fixed and peer-aware Minstrel controllers select legal VHT rates as the moving station's path loss changes.
- Compare fixed MCS 8 against Minstrel using selected-rate and retry telemetry.
- Treat the rate-control trajectory as model evidence; it is not a calibrated hardware SNR curve.

## [agent] Scenario description

The topology uses [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned). `host[0]` moves away from `ap` at 15 m/s while receiving UDP traffic.

Two configurations are evaluated:

1. `FixedVhtMcs`: Fixed VHT MCS 8 in 80 MHz.
2. `VhtMinstrelAdaptation`: Peer-aware `VhtMinstrelRateControl` adaptation.

## [agent] Standards and INET model boundary

- **VHT rate control**: AARF and Minstrel select among the VHT modes exposed by the negotiated peer capabilities.
- **INET Model Boundary**: Fixed selection is expressed through `mac.hcf.rateSelection`; dynamic selection is provided by `AarfRateControl` or `VhtMinstrelRateControl`. No transmitter `mcs` parameter is used.

## [agent] Scalar and vector analysis

<!-- BEGIN GENERATED: ieee80211-scalar-vector-vht_rate_adaptation -->
### [script] Generated scalar/vector plot and table

![vht_rate_adaptation scalar/vector analysis](results/20260809T174942Z/vht-rate-adaptation-delivery-delay.png)

Figure provenance: [`results/20260809T174942Z/vht-rate-adaptation-delivery-delay.png.json`](results/20260809T174942Z/vht-rate-adaptation-delivery-delay.png.json). Run-level metric source: [`../../ieee80211ax/analysis/metrics.json`](../../ieee80211ax/analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)<br>vector / **.app[*] / endToEndDelay:vector
- Window / per-run aggregation / exclusions: [0.3, 1.9) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| Fixed VHT MCS 8 / goodput mbps | 56.971 | 0.0600958 |
| VHT Minstrel Adaptation / goodput mbps | 53.462 | 0.231571 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **PASS** | Compare application delivered bytes between fixed VHT MCS 8 and Minstrel under the same 5 GHz/80 MHz traffic and mobility setup | Every matched run preserves at least 0.500 of baseline delivery. |
<!-- END GENERATED: ieee80211-scalar-vector-vht_rate_adaptation -->

<!-- BEGIN GENERATED: ieee80211-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260809T174942Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260809T174942Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260809T174942Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ac/capture_manifests/20260809T174942Z.json` (SHA-256 `c31134eb0e59fb021726b3ccde05c5107d5c49adb1aaa6910c5ae32aecf0a26d`). VHT PPDU format, MCS, coding, bandwidth, GI, and NSTS are decoded directly from standards-compliant radiotap VHT fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. VHT SU and VHT MU use modeled preambles; per-user VHT MU signaling remains approximate because radiotap carries common MU metadata alongside each logical user record.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

<small>

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `FixedVhtMcs` | `none (all decoded frames)` | 24327 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] (12414), Control: Ack (11573), Control: Block Ack Request (BAR) (168) | 54.56% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `VhtMinstrelAdaptation` | `none (all decoded frames)` | 22856 | Control: Ack (10546), QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] (10313), QoS Data [VHT, VHT-MCS 7, 80 MHz, GI 0.8 us, BCC] (432) | 54.59% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

</small>

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | FixedVhtMcs produced protocol-visible wireless observations | 24327 AP/global transmission observations |
| **PASS** | VhtMinstrelAdaptation produced protocol-visible wireless observations | 22856 AP/global transmission observations |

### [script] Configuration: `FixedVhtMcs`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **24327**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | - | 12414 | 51.03% | 1068.1 B | 169.4 B | 63.5 us | 3.7 us | 5040 MHz | - | 13.0 dBm | 72.29% | 39.44% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 168 | 0.69% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.43% | 0.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 168 | 0.69% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 5010 MHz | -52.1 dBm | - | 1.09% | 0.59% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 11573 | 47.57% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -55.0 dBm | 13.0 dBm | 26.16% | 14.27% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Req | - | 2 | 0.01% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Resp | - | 2 | 0.01% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -56.5 dBm | - | 0.01% | 0.01% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 1 | 0.200068000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200112000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 3 | 0.200196000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200240000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 5 | 0.200324000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200368000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 7 | 0.200452000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.200496000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 9 | 0.200580000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.200624000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 11 | 0.200708000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.200752000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 13 | 0.200836000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.200880000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 15 | 0.200964000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.201008000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 17 | 0.201092000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.201136000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 19 | 0.201220000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.201265000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 21 | 0.201349000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.201393000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 23 | 0.201632000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 24 | 0.201676000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 25 | 0.201760000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 26 | 0.201804000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 27 | 0.201888000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 28 | 0.201932000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 29 | 0.202016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 30 | 0.202060000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 31 | 0.202144000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 32 | 0.202188000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 33 | 0.202272000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 34 | 0.202316000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 35 | 0.202400000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 36 | 0.202444000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 37 | 0.202528000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 38 | 0.202572000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 39 | 0.202656000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 40 | 0.202700000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 41 | 0.202784000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 42 | 0.202828000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 43 | 0.202912000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 44 | 0.202957000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 45 | 0.203213000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 46 | 0.203257000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 47 | 0.203341000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 48 | 0.203386000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 49 | 0.203470000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 50 | 0.203514000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 51 | 0.203598000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 52 | 0.203642000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 53 | 0.203726000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 54 | 0.203770000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 55 | 0.203854000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 56 | 0.203898000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 57 | 0.203982000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 58 | 0.204026000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 59 | 0.204110000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 60 | 0.204154000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 61 | 0.204238000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 62 | 0.204282000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 63 | 0.204366000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 64 | 0.204410000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 65 | 0.204494000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 66 | 0.204538000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 67 | 0.204786000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 68 | 0.204830000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 69 | 0.204914000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 70 | 0.204959000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 71 | 0.205043000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 72 | 0.205087000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 73 | 0.205171000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 74 | 0.205215000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 75 | 0.205299000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 76 | 0.205343000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 77 | 0.205427000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 78 | 0.205471000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 79 | 0.205555000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 80 | 0.205599000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 81 | 0.205683000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 82 | 0.205727000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 83 | 0.205811000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 84 | 0.205855000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 85 | 0.205939000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 86 | 0.205983000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 87 | 0.206067000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 88 | 0.206111000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 89 | 0.206358000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 90 | 0.206402000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 91 | 0.206658000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 92 | 0.206702000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 93 | 0.206786000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 94 | 0.206830000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 95 | 0.206914000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 96 | 0.206958000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 97 | 0.207042000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 98 | 0.207087000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 99 | 0.207171000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=24, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 100 | 0.207215000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

</small>

Frame numbers are local to capture `FixedVhtMcs-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `VhtMinstrelAdaptation`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **22856**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#30d936" /></svg> | QoS Data [VHT, VHT-MCS 0, 80 MHz, GI 0.8 us, BCC] | - | 112 | 0.49% | 1064.7 B | 181.2 B | 321.6 us | 47.9 us | 5040 MHz | - | 13.0 dBm | 3.30% | 1.80% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2bc31d" /></svg> | QoS Data [VHT, VHT-MCS 1, 80 MHz, GI 0.8 us, BCC] | - | 129 | 0.56% | 1054.9 B | 171.6 B | 179.5 us | 22.7 us | 5040 MHz | - | 13.0 dBm | 2.12% | 1.16% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28e638" /></svg> | QoS Data [VHT, VHT-MCS 2, 80 MHz, GI 0.8 us, BCC] | - | 110 | 0.48% | 1090.9 B | 169.7 B | 136.2 us | 15.0 us | 5040 MHz | - | 13.0 dBm | 1.37% | 0.75% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2fbb2a" /></svg> | QoS Data [VHT, VHT-MCS 3, 80 MHz, GI 0.8 us, BCC] | - | 147 | 0.64% | 1068.1 B | 176.2 B | 110.6 us | 11.7 us | 5040 MHz | - | 13.0 dBm | 1.49% | 0.81% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1e9f2f" /></svg> | QoS Data [VHT, VHT-MCS 4, 80 MHz, GI 0.8 us, BCC] | - | 129 | 0.56% | 1086.9 B | 128.5 B | 87.9 us | 5.7 us | 5040 MHz | - | 13.0 dBm | 1.04% | 0.57% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#20cb23" /></svg> | QoS Data [VHT, VHT-MCS 5, 80 MHz, GI 0.8 us, BCC] | - | 134 | 0.59% | 1067.9 B | 161.9 B | 75.3 us | 5.4 us | 5040 MHz | - | 13.0 dBm | 0.92% | 0.50% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#25b215" /></svg> | QoS Data [VHT, VHT-MCS 6, 80 MHz, GI 0.8 us, BCC] | - | 150 | 0.66% | 1087.7 B | 157.3 B | 72.0 us | 4.6 us | 5040 MHz | - | 13.0 dBm | 0.99% | 0.54% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2dc838" /></svg> | QoS Data [VHT, VHT-MCS 7, 80 MHz, GI 0.8 us, BCC] | - | 432 | 1.89% | 1072.4 B | 81.5 B | 68.4 us | 2.2 us | 5040 MHz | - | 13.0 dBm | 2.71% | 1.48% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#25bb2d" /></svg> | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC, A-MPDU] | - | 5 | 0.02% | 662.0 B | 0.0 B | 54.6 us | 0.0 us | 5040 MHz | - | 13.0 dBm | 0.03% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | - | 10313 | 45.12% | 1067.4 B | 174.6 B | 63.5 us | 3.8 us | 5040 MHz | - | 13.0 dBm | 60.01% | 32.76% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 157 | 0.69% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.40% | 0.22% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 157 | 0.69% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 5010 MHz | -52.0 dBm | - | 1.02% | 0.55% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 10546 | 46.14% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -55.0 dBm | 13.0 dBm | 23.83% | 13.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 226 | 0.99% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -54.7 dBm | - | 0.51% | 0.28% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 103 | 0.45% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -54.5 dBm | - | 0.23% | 0.13% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d2141d" /></svg> | Management: Action: Block Ack: ADDBA Req [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | - | 3 | 0.01% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5040 MHz | - | 13.0 dBm | 0.02% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d2141d" /></svg> | Management: Action: Block Ack: ADDBA Resp [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | - | 3 | 0.01% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5040 MHz | -57.7 dBm | - | 0.02% | 0.01% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 1 | 0.200068000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200112000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 3 | 0.200196000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200240000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 5 | 0.200324000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200368000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 7 | 0.200452000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.200496000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 9 | 0.200580000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.200624000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 11 | 0.200708000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.200752000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 13 | 0.200836000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.200880000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 15 | 0.200964000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.201008000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 17 | 0.201092000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.201136000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 19 | 0.201220000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.201265000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 21 | 0.201349000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.201393000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 23 | 0.201623000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 24 | 0.201667000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28e638" /></svg> | 25 | 0.201823000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 2, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 26 | 0.201871000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#20cb23" /></svg> | 27 | 0.201967000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 5, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 28 | 0.202011000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 29 | 0.202095000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 30 | 0.202139000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 31 | 0.202223000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 32 | 0.202267000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 33 | 0.202351000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 34 | 0.202395000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#30d936" /></svg> | 35 | 0.202747000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 0, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 36 | 0.202807000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 37 | 0.202891000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 38 | 0.202935000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 39 | 0.203183000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 40 | 0.203227000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 41 | 0.203311000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 42 | 0.203355000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 43 | 0.203439000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 44 | 0.203484000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 45 | 0.203568000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 46 | 0.203612000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 47 | 0.203696000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 48 | 0.203740000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 49 | 0.203824000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 50 | 0.203868000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 51 | 0.203952000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 52 | 0.203996000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 53 | 0.204080000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 54 | 0.204124000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 55 | 0.204208000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 56 | 0.204252000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 57 | 0.204336000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 58 | 0.204380000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 59 | 0.204464000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 60 | 0.204508000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 61 | 0.204756000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 62 | 0.204800000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 63 | 0.204884000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 64 | 0.204928000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 65 | 0.205012000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 66 | 0.205056000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 67 | 0.205140000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 68 | 0.205184000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 69 | 0.205268000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 70 | 0.205313000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 71 | 0.205397000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 72 | 0.205441000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 73 | 0.205525000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 74 | 0.205569000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 75 | 0.205653000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 76 | 0.205697000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 77 | 0.205781000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 78 | 0.205825000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 79 | 0.205909000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 80 | 0.205953000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 81 | 0.206037000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 82 | 0.206081000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 83 | 0.206320000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 84 | 0.206364000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 85 | 0.206448000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 86 | 0.206492000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 87 | 0.206576000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 88 | 0.206620000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 89 | 0.206704000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 90 | 0.206748000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 91 | 0.206832000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 92 | 0.206877000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 93 | 0.206961000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 94 | 0.207005000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 95 | 0.207089000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 96 | 0.207133000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28ab21" /></svg> | 97 | 0.207217000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=24, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 98 | 0.207261000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2fbb2a" /></svg> | 99 | 0.207393000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [VHT, VHT-MCS 3, 80 MHz, GI 0.8 us, BCC] | direction=from DS, retry=0, seq=24, frag=0, more-frag=0, TID=7 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 100 | 0.207437000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

</small>

Frame numbers are local to capture `VhtMinstrelAdaptation-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Analysis of Packet Distribution
IEEE 802.11 constrains negotiated HE modes but does not mandate a Minstrel algorithm. These packet counts therefore cannot establish adaptation, and a control/data ratio is not reliable evidence of retransmission or probing. Use the aligned selected-MCS/NSS, EWMA probability, transmission-outcome, and retry vectors documented above. INET's HE Minstrel remains a simplified implementation without scheduler-context or localized-fading adaptation.
<!-- END GENERATED: ieee80211-pcap-statistics -->
