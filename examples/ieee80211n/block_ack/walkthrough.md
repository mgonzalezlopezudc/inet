# Walkthrough: 802.11n Block Acknowledgement

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260803T184134Z`
- PCAP: `20260803T184134Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough demonstrates the IEEE 802.11n Block Acknowledgement (Block ACK) mechanisms, comparing legacy per-frame ACK policy against Immediate Block ACK, Compressed Block ACK bitmaps, and HT Implicit Block ACK.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- Trace the ADDBA (Add Block ACK Request/Response) management exchange used to establish Block ACK agreements per TID.
- Understand how Compressed Block ACK bitmaps represent up to 64 MPDU sequence numbers in a compact 8-byte field.
- Explain the SIFS timing and overhead reduction when replacing multiple discrete ACKs with a single Block ACK frame.
- Compare throughput and delay performance across standard ACK and Block ACK configurations.

## [agent] Scenario description

The topology uses the common single-BSS network [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned). Traffic flows from `server` to `host[0]` at 0.5 ms packet intervals.

Four configurations are evaluated:

1. `StandardAck`: Legacy Stop-and-Wait ACK after each transmitted MPDU.
2. `ImmediateBlockAck`: ADDBA negotiated Block ACK with explicit BAR/BA exchange.
3. `CompressedBlockAck`: HT Compressed Block ACK bitmap optimization.
4. `HtImplicitBlockAck`: SIFS implicit Compressed Block ACK automatically returned after an A-MPDU burst without requiring an explicit BAR.

## [agent] Standards and INET model boundary

- **IEEE Std 802.11n-2009 / 802.11-2020 Clause 9.21 & 9.3.1.9**: Defines ADDBA management frames, BlockAckReq/BlockAck control structures, compressed bitmaps, and implicit BA rules.
- **INET Model Boundary**: ADDBA negotiation and Block ACK bitmap accounting are implemented in `inet::ieee80211::Hcf`.

## [agent] Scalar and vector analysis

<!-- BEGIN GENERATED: ieee80211-scalar-vector-block_ack -->
### [script] Generated scalar/vector plot and table

![block_ack scalar/vector analysis](results/20260803T184134Z/block-ack-delivery-delay.png)

Figure provenance: [`results/20260803T184134Z/block-ack-delivery-delay.png.json`](results/20260803T184134Z/block-ack-delivery-delay.png.json). Run-level metric source: [`../../ieee80211ax/analysis/metrics.json`](../../ieee80211ax/analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)<br>vector / **.app[*] / endToEndDelay:vector
- Window / per-run aggregation / exclusions: [0.3, 0.9) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| Compressed Block ACK / goodput mbps | 8 | 1.23299e-15 |
| HT Implicit Block ACK / goodput mbps | 8 | 1.23299e-15 |
| Immediate Block ACK / goodput mbps | 8 | 1.23299e-15 |
| Standard ACK / goodput mbps | 8 | 1.23299e-15 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **INCONCLUSIVE** | Compare application delivered bytes across Block ACK policies | No manifest acceptance threshold defined for Block ACK policy comparison |
<!-- END GENERATED: ieee80211-scalar-vector-block_ack -->

<!-- BEGIN GENERATED: ieee80211-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260803T184134Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260803T184134Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260803T184134Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/n/capture_manifests/20260803T184134Z.json` (SHA-256 `aaa63173f660df683a540953f36b616efa93d37c84f6c1d558714e30ae2199d3`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `StandardAck` | `none (all decoded frames)` | 2803 | Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC] (1401), Control: Ack (1401), Data (1) | 18.22% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `ImmediateBlockAck` | `none (all decoded frames)` | 2803 | Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC] (1401), Control: Ack (1401), Data (1) | 18.22% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `CompressedBlockAck` | `none (all decoded frames)` | 2803 | Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC] (1401), Control: Ack (1401), Data (1) | 18.22% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `HtImplicitBlockAck` | `none (all decoded frames)` | 2803 | Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC] (1401), Control: Ack (1401), Data (1) | 18.22% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | CompressedBlockAck produced protocol-visible wireless observations | 2803 AP/global transmission observations |
| **PASS** | HtImplicitBlockAck produced protocol-visible wireless observations | 2803 AP/global transmission observations |
| **PASS** | ImmediateBlockAck produced protocol-visible wireless observations | 2803 AP/global transmission observations |
| **PASS** | StandardAck produced protocol-visible wireless observations | 2803 AP/global transmission observations |

### [script] Configuration: `StandardAck`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2803**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | Data | - | 1 | 0.04% | 82.0 B | 0.0 B | 47.3 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.03% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#baedbc" /></svg> | Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC] | - | 1401 | 49.98% | 563.6 B | 13.4 B | 105.4 us | 1.6 us | 2412 MHz | -47.0 dBm | 13.0 dBm | 81.01% | 14.76% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 1401 | 49.98% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -47.0 dBm | 13.0 dBm | 18.96% | 3.46% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 1 | 0.200058000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 2 | 0.200256000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 3 | 0.200300000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 4 | 0.200458000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 5 | 0.200502000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 6 | 0.200880000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 7 | 0.200924000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 8 | 0.201222000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 9 | 0.201266000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 10 | 0.201608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 11 | 0.201652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 12 | 0.202108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.202152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 14 | 0.202608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.202652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 16 | 0.203108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 17 | 0.203152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 18 | 0.203608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 19 | 0.203652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 20 | 0.204108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 21 | 0.204152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 22 | 0.204608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 23 | 0.204652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 24 | 0.205108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 25 | 0.205152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 26 | 0.205608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 27 | 0.205652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 28 | 0.206108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 29 | 0.206152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 30 | 0.206608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 31 | 0.206652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 32 | 0.207108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 33 | 0.207152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 34 | 0.207608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 35 | 0.207652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 36 | 0.208108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 37 | 0.208152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 38 | 0.208610000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 39 | 0.208654000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 40 | 0.209108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 41 | 0.209152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 42 | 0.209608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 43 | 0.209652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 44 | 0.210108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 45 | 0.210152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 46 | 0.210608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 47 | 0.210652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 48 | 0.211108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=24, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 49 | 0.211152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 50 | 0.211608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=25, frag=0, more-frag=0, TID=- |

Frame numbers are local to capture `StandardAck-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `ImmediateBlockAck`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2803**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | Data | - | 1 | 0.04% | 82.0 B | 0.0 B | 47.3 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.03% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#baedbc" /></svg> | Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC] | - | 1401 | 49.98% | 563.6 B | 13.4 B | 105.4 us | 1.6 us | 2412 MHz | -47.0 dBm | 13.0 dBm | 81.01% | 14.76% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 1401 | 49.98% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -47.0 dBm | 13.0 dBm | 18.96% | 3.46% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 1 | 0.200058000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 2 | 0.200256000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 3 | 0.200300000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 4 | 0.200458000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 5 | 0.200502000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 6 | 0.200880000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 7 | 0.200924000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 8 | 0.201222000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 9 | 0.201266000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 10 | 0.201608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 11 | 0.201652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 12 | 0.202108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.202152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 14 | 0.202608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.202652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 16 | 0.203108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 17 | 0.203152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 18 | 0.203608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 19 | 0.203652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 20 | 0.204108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 21 | 0.204152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 22 | 0.204608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 23 | 0.204652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 24 | 0.205108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 25 | 0.205152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 26 | 0.205608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 27 | 0.205652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 28 | 0.206108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 29 | 0.206152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 30 | 0.206608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 31 | 0.206652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 32 | 0.207108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 33 | 0.207152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 34 | 0.207608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 35 | 0.207652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 36 | 0.208108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 37 | 0.208152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 38 | 0.208610000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 39 | 0.208654000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 40 | 0.209108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 41 | 0.209152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 42 | 0.209608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 43 | 0.209652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 44 | 0.210108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 45 | 0.210152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 46 | 0.210608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 47 | 0.210652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 48 | 0.211108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=24, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 49 | 0.211152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 50 | 0.211608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=25, frag=0, more-frag=0, TID=- |

Frame numbers are local to capture `ImmediateBlockAck-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `CompressedBlockAck`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2803**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | Data | - | 1 | 0.04% | 82.0 B | 0.0 B | 47.3 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.03% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#baedbc" /></svg> | Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC] | - | 1401 | 49.98% | 563.6 B | 13.4 B | 105.4 us | 1.6 us | 2412 MHz | -47.0 dBm | 13.0 dBm | 81.01% | 14.76% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 1401 | 49.98% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -47.0 dBm | 13.0 dBm | 18.96% | 3.46% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 1 | 0.200058000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 2 | 0.200256000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 3 | 0.200300000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 4 | 0.200458000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 5 | 0.200502000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 6 | 0.200880000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 7 | 0.200924000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 8 | 0.201222000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 9 | 0.201266000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 10 | 0.201608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 11 | 0.201652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 12 | 0.202108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.202152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 14 | 0.202608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.202652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 16 | 0.203108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 17 | 0.203152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 18 | 0.203608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 19 | 0.203652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 20 | 0.204108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 21 | 0.204152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 22 | 0.204608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 23 | 0.204652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 24 | 0.205108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 25 | 0.205152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 26 | 0.205608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 27 | 0.205652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 28 | 0.206108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 29 | 0.206152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 30 | 0.206608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 31 | 0.206652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 32 | 0.207108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 33 | 0.207152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 34 | 0.207608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 35 | 0.207652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 36 | 0.208108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 37 | 0.208152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 38 | 0.208610000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 39 | 0.208654000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 40 | 0.209108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 41 | 0.209152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 42 | 0.209608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 43 | 0.209652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 44 | 0.210108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 45 | 0.210152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 46 | 0.210608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 47 | 0.210652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 48 | 0.211108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=24, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 49 | 0.211152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 50 | 0.211608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=25, frag=0, more-frag=0, TID=- |

Frame numbers are local to capture `CompressedBlockAck-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `HtImplicitBlockAck`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2803**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | Data | - | 1 | 0.04% | 82.0 B | 0.0 B | 47.3 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.03% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#baedbc" /></svg> | Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC] | - | 1401 | 49.98% | 563.6 B | 13.4 B | 105.4 us | 1.6 us | 2412 MHz | -47.0 dBm | 13.0 dBm | 81.01% | 14.76% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 1401 | 49.98% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -47.0 dBm | 13.0 dBm | 18.96% | 3.46% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 1 | 0.200058000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 2 | 0.200256000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 3 | 0.200300000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 4 | 0.200458000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 5 | 0.200502000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 6 | 0.200880000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 7 | 0.200924000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 8 | 0.201222000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 9 | 0.201266000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 10 | 0.201608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 11 | 0.201652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 12 | 0.202108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.202152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 14 | 0.202608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.202652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 16 | 0.203108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 17 | 0.203152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 18 | 0.203608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 19 | 0.203652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 20 | 0.204108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 21 | 0.204152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 22 | 0.204608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 23 | 0.204652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 24 | 0.205108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 25 | 0.205152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 26 | 0.205608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 27 | 0.205652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 28 | 0.206108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 29 | 0.206152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 30 | 0.206608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 31 | 0.206652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 32 | 0.207108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 33 | 0.207152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 34 | 0.207608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 35 | 0.207652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 36 | 0.208108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 37 | 0.208152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 38 | 0.208610000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 39 | 0.208654000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 40 | 0.209108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 41 | 0.209152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 42 | 0.209608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 43 | 0.209652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 44 | 0.210108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 45 | 0.210152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 46 | 0.210608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 47 | 0.210652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 48 | 0.211108000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=24, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 49 | 0.211152000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#96e996" /></svg> | 50 | 0.211608000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data | direction=from DS, retry=0, seq=25, frag=0, more-frag=0, TID=- |

Frame numbers are local to capture `HtImplicitBlockAck-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Analysis of Packet Distribution
Across these configurations, **QoS Data** frames constitute the primary payload delivery mechanism, while **Block Ack (BA)** and **Block Ack Request (BAR)** control frames ensure reliable transport via the MAC-level acknowledgment protocol. Management frames, specifically **Beacons**, are transmitted periodically by the Access Point to maintain BSS time synchronization and broadcast network capabilities. The ratio of control/management overhead to actual data frames indicates the relative MAC efficiency of the chosen configurations.
<!-- END GENERATED: ieee80211-pcap-statistics -->
