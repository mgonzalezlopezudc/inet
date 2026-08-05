# Walkthrough: 802.11n Block Acknowledgement

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260805T215149Z`
- PCAP: `20260805T215149Z`
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

![block_ack scalar/vector analysis](results/20260805T215149Z/block-ack-delivery-delay.png)

Figure provenance: [`results/20260805T215149Z/block-ack-delivery-delay.png.json`](results/20260805T215149Z/block-ack-delivery-delay.png.json). Run-level metric source: [`../../ieee80211ax/analysis/metrics.json`](../../ieee80211ax/analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)<br>vector / **.app[*] / endToEndDelay:vector
- Window / per-run aggregation / exclusions: [0.3, 0.5) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| Compressed Block ACK / goodput mbps | 10.96 | 0.222809 |
| HT Implicit Block ACK / goodput mbps | 10.96 | 0.222809 |
| Immediate Block ACK / goodput mbps | 9.956 | 0.183666 |
| Standard ACK / goodput mbps | 6.12 | 0.0582392 |

> [!NOTE]
> **Pre-ADDBA Sequence Synchronization (Solution 1)**
> While an ADDBA Request is in flight (`isAddbaRequestInProgress = true`), transmission of new QoS Data frames for that `<receiverAddress, TID>` tuple is held in `pendingQueue`. Once the ADDBA Response (`SUCCESS`) arrives, post-ADDBA data frame transmission begins cleanly at sequence number 1 under the active Block ACK agreement. This eliminates sequence gaps before Block ACK setup and restores `HtImplicitBlockAck` throughput to 10.96 Mbps (matching `CompressedBlockAck`).

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **INCONCLUSIVE** | Compare application delivered bytes across Block ACK policies | No manifest acceptance threshold defined for Block ACK policy comparison |
<!-- END GENERATED: ieee80211-scalar-vector-block_ack -->

<!-- BEGIN GENERATED: ieee80211-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260805T215149Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260805T215149Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260805T215149Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/n/capture_manifest.json` (SHA-256 `5b7009b9a9fb6a029d90c2a9c133c9a5ab2bafa3cdb6969a77a6a6dffdca8be4`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `StandardAck` | `none (all decoded frames)` | 970 | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC] (485), Control: Ack (485) | 22.56% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `ImmediateBlockAck` | `none (all decoded frames)` | 1847 | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC, A-MPDU] (1702), Control: Block Ack Request (BAR) (58), Control: Block Ack (BA) (58) | 38.07% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `CompressedBlockAck` | `none (all decoded frames)` | 1008 | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC, A-MPDU] (896), Control: Block Ack Request (BAR) (46), Control: Block Ack (BA) (46) | 34.92% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `HtImplicitBlockAck` | `none (all decoded frames)` | 967 | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC, A-MPDU] (895), Control: Block Ack (BA) (50), Control: Ack (11) | 34.81% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | CompressedBlockAck produced protocol-visible wireless observations | 1008 AP/global transmission observations |
| **PASS** | HtImplicitBlockAck produced protocol-visible wireless observations | 967 AP/global transmission observations |
| **PASS** | ImmediateBlockAck produced protocol-visible wireless observations | 1847 AP/global transmission observations |
| **PASS** | StandardAck produced protocol-visible wireless observations | 970 AP/global transmission observations |

### [script] Configuration: `StandardAck`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **970**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35e01f" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC] | - | 485 | 50.00% | 657.4 B | 244.2 B | 440.6 us | 150.3 us | 2412 MHz | - | 13.0 dBm | 94.70% | 21.37% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 485 | 50.00% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -51.4 dBm | - | 5.30% | 1.20% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 1 | 0.200388000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200448000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 3 | 0.200846000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200906000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 5 | 0.201304000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.201364000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 7 | 0.201862000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.201922000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 9 | 0.202320000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.202380000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 11 | 0.202778000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.202838000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 13 | 0.203774000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.203834000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 15 | 0.204232000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.204292000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 17 | 0.204690000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.204751000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 19 | 0.205686000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.205746000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 21 | 0.206144000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.206205000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 23 | 0.206603000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 24 | 0.206663000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 25 | 0.207579000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 26 | 0.207639000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 27 | 0.208037000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 28 | 0.208097000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 29 | 0.208495000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 30 | 0.208555000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 31 | 0.209511000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 32 | 0.209571000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 33 | 0.209969000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 34 | 0.210029000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 35 | 0.210427000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 36 | 0.210487000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 37 | 0.211383000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 38 | 0.211443000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 39 | 0.211841000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 40 | 0.211901000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 41 | 0.212299000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 42 | 0.212359000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 43 | 0.213315000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 44 | 0.213375000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 45 | 0.213773000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 46 | 0.213833000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 47 | 0.214231000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 48 | 0.214292000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 49 | 0.215207000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 50 | 0.215267000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 51 | 0.215665000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 52 | 0.215726000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 53 | 0.216124000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 54 | 0.216184000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 55 | 0.217140000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 56 | 0.217200000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 57 | 0.217598000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 58 | 0.217658000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 59 | 0.218056000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 60 | 0.218116000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 61 | 0.219012000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 62 | 0.219072000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 63 | 0.219470000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 64 | 0.219530000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 65 | 0.219928000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 66 | 0.219988000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 67 | 0.220904000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 68 | 0.220964000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 69 | 0.221362000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 70 | 0.221422000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 71 | 0.221820000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 72 | 0.221880000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 73 | 0.222796000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 74 | 0.222856000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 75 | 0.223254000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 76 | 0.223314000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 77 | 0.223712000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 78 | 0.223773000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 79 | 0.224728000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 80 | 0.224788000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 81 | 0.225186000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 82 | 0.225247000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 83 | 0.225645000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 84 | 0.225705000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 85 | 0.226641000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 86 | 0.226701000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 87 | 0.227099000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 88 | 0.227159000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 89 | 0.227557000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 90 | 0.227617000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 91 | 0.228513000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 92 | 0.228573000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 93 | 0.228971000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 94 | 0.229031000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 95 | 0.229429000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 96 | 0.229489000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 97 | 0.230385000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 98 | 0.230445000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 99 | 0.230843000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 100 | 0.230903000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

Frame numbers are local to capture `StandardAck-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `ImmediateBlockAck`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1847**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#259c21" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC, A-MPDU] | - | 1702 | 92.15% | 296.9 B | 16.9 B | 218.7 us | 10.4 us | 2412 MHz | - | 13.0 dBm | 97.78% | 37.22% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35e01f" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC] | - | 9 | 0.49% | 298.2 B | 2.0 B | 219.5 us | 1.2 us | 2412 MHz | - | 13.0 dBm | 0.52% | 0.20% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 58 | 3.14% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.43% | 0.16% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 58 | 3.14% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 2412 MHz | -52.3 dBm | - | 1.08% | 0.41% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 14 | 0.76% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -51.4 dBm | 13.0 dBm | 0.09% | 0.03% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 6 | 0.32% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 2412 MHz | -52.3 dBm | 13.0 dBm | 0.11% | 0.04% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 1 | 0.200224000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200284000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 3 | 0.200514000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200574000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 5 | 0.200666000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200726000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 7 | 0.200960000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.201020000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 9 | 0.201250000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=0, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.201310000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 11 | 0.201402000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.201462000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 13 | 0.201614000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.201674000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 15 | 0.202090000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.202150000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 17 | 0.202380000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=0, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.202440000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 19 | 0.202532000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.202593000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 21 | 0.202827000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.202887000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 23 | 0.203117000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=1, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 24 | 0.203177000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 25 | 0.203329000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 26 | 0.203389000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 27 | 0.204677000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=1037 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 28 | 0.204677000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=1, frag=1, more-frag=0, TID=6, A-MPDU=1037 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 29 | 0.204677000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=1037 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 30 | 0.204677000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=2, frag=1, more-frag=0, TID=6, A-MPDU=1037 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 31 | 0.204677000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=1037 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 32 | 0.204677000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=3, frag=1, more-frag=0, TID=6, A-MPDU=1037 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 33 | 0.204749000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 34 | 0.204993000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1037 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 35 | 0.205327000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 36 | 0.205479000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=1, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 37 | 0.205539000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 38 | 0.207647000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=1, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=1310 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 39 | 0.207647000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=1, frag=1, more-frag=0, TID=6, A-MPDU=1310 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 40 | 0.207647000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=1310 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 41 | 0.207647000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=2, frag=1, more-frag=0, TID=6, A-MPDU=1310 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 42 | 0.207647000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=1310 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 43 | 0.207647000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=3, frag=1, more-frag=0, TID=6, A-MPDU=1310 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 44 | 0.207647000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=1310 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 45 | 0.207647000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=4, frag=1, more-frag=0, TID=6, A-MPDU=1310 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 46 | 0.207647000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=1310 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 47 | 0.207647000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=5, frag=1, more-frag=0, TID=6, A-MPDU=1310 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 48 | 0.208169000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 49 | 0.208413000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1310 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 50 | 0.211063000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=1540 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 51 | 0.211063000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=2, frag=1, more-frag=0, TID=6, A-MPDU=1540 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 52 | 0.211063000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=1540 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 53 | 0.211063000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=3, frag=1, more-frag=0, TID=6, A-MPDU=1540 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 54 | 0.211063000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=1540 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 55 | 0.211063000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=4, frag=1, more-frag=0, TID=6, A-MPDU=1540 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 56 | 0.211063000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=1540 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 57 | 0.211063000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=5, frag=1, more-frag=0, TID=6, A-MPDU=1540 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 58 | 0.211063000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=1540 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 59 | 0.211063000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=6, frag=1, more-frag=0, TID=6, A-MPDU=1540 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 60 | 0.211063000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=1540 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 61 | 0.211063000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=7, frag=1, more-frag=0, TID=6, A-MPDU=1540 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 62 | 0.211063000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6, A-MPDU=1540 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 63 | 0.211063000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=8, frag=1, more-frag=0, TID=6, A-MPDU=1540 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 64 | 0.211235000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 65 | 0.211479000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1540 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 66 | 0.214501000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=1796 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 67 | 0.214501000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=4, frag=1, more-frag=0, TID=6, A-MPDU=1796 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 68 | 0.214501000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=1796 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 69 | 0.214501000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=5, frag=1, more-frag=0, TID=6, A-MPDU=1796 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 70 | 0.214501000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=1796 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 71 | 0.214501000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=6, frag=1, more-frag=0, TID=6, A-MPDU=1796 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 72 | 0.214501000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=1796 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 73 | 0.214501000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=7, frag=1, more-frag=0, TID=6, A-MPDU=1796 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 74 | 0.214501000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6, A-MPDU=1796 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 75 | 0.214501000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=8, frag=1, more-frag=0, TID=6, A-MPDU=1796 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 76 | 0.214501000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6, A-MPDU=1796 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 77 | 0.214501000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=9, frag=1, more-frag=0, TID=6, A-MPDU=1796 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 78 | 0.214501000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=6, A-MPDU=1796 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 79 | 0.214501000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=10, frag=1, more-frag=0, TID=6, A-MPDU=1796 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 80 | 0.214501000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=6, A-MPDU=1796 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 81 | 0.214501000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=11, frag=1, more-frag=0, TID=6, A-MPDU=1796 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 82 | 0.214653000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 83 | 0.214897000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1796 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 84 | 0.218291000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 85 | 0.218291000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=6, frag=1, more-frag=0, TID=6, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 86 | 0.218291000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 87 | 0.218291000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=7, frag=1, more-frag=0, TID=6, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 88 | 0.218291000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 89 | 0.218291000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=8, frag=1, more-frag=0, TID=6, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 90 | 0.218291000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 91 | 0.218291000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=9, frag=1, more-frag=0, TID=6, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 92 | 0.218291000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=6, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 93 | 0.218291000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=10, frag=1, more-frag=0, TID=6, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 94 | 0.218291000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=6, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 95 | 0.218291000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=11, frag=1, more-frag=0, TID=6, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 96 | 0.218291000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=6, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 97 | 0.218291000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=12, frag=1, more-frag=0, TID=6, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 98 | 0.218291000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=6, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 99 | 0.218291000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=13, frag=1, more-frag=0, TID=6, A-MPDU=2075 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 100 | 0.218291000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=6, A-MPDU=2075 |

Frame numbers are local to capture `ImmediateBlockAck-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `CompressedBlockAck`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1008**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#259c21" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC, A-MPDU] | - | 896 | 88.89% | 566.0 B | 0.0 B | 384.3 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 98.60% | 34.43% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35e01f" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC] | - | 4 | 0.40% | 566.0 B | 0.0 B | 384.3 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.44% | 0.15% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 46 | 4.56% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.37% | 0.13% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 46 | 4.56% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 2412 MHz | -52.2 dBm | - | 0.40% | 0.14% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 10 | 0.99% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -51.6 dBm | 13.0 dBm | 0.07% | 0.02% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 6 | 0.60% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 2412 MHz | -52.3 dBm | 13.0 dBm | 0.12% | 0.04% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 1 | 0.200388000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200448000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 3 | 0.200540000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200600000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 5 | 0.200998000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.201058000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 7 | 0.201974000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.202034000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 9 | 0.202126000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.202186000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 11 | 0.202278000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.202338000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 13 | 0.202736000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.202796000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 15 | 0.202988000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.203048000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 17 | 0.203386000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.203446000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 19 | 0.204632000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=781 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 20 | 0.204632000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=781 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 21 | 0.204632000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=781 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 22 | 0.204785000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 23 | 0.204845000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 24 | 0.206383000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=902 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 25 | 0.206383000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=902 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 26 | 0.206383000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=902 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 27 | 0.206383000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=902 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 28 | 0.208253000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=991 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 29 | 0.208253000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=991 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 30 | 0.208253000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=991 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 31 | 0.208253000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=991 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 32 | 0.208253000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=991 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 33 | 0.208385000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 34 | 0.208469000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=991 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 35 | 0.210279000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=1141 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 36 | 0.210279000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=1141 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 37 | 0.210279000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=1141 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 38 | 0.210279000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=1141 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 39 | 0.210279000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6, A-MPDU=1141 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 40 | 0.210391000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 41 | 0.210475000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=781, 1141 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 42 | 0.212637000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=1299 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 43 | 0.212637000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=1299 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 44 | 0.212637000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=1299 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 45 | 0.212637000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6, A-MPDU=1299 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 46 | 0.212637000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6, A-MPDU=1299 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 47 | 0.212637000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=6, A-MPDU=1299 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 48 | 0.212809000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 49 | 0.212893000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1299 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 50 | 0.215055000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=1460 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 51 | 0.215055000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6, A-MPDU=1460 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 52 | 0.215055000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6, A-MPDU=1460 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 53 | 0.215055000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=6, A-MPDU=1460 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 54 | 0.215055000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=6, A-MPDU=1460 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 55 | 0.215055000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=6, A-MPDU=1460 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 56 | 0.215227000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 57 | 0.215311000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1460 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 58 | 0.217825000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6, A-MPDU=1635 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 59 | 0.217825000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=6, A-MPDU=1635 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 60 | 0.217825000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=6, A-MPDU=1635 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 61 | 0.217825000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=6, A-MPDU=1635 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 62 | 0.217825000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=6, A-MPDU=1635 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 63 | 0.217825000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=6, A-MPDU=1635 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 64 | 0.217825000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=6, A-MPDU=1635 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 65 | 0.217977000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 66 | 0.218061000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1635 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 67 | 0.220927000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=6, A-MPDU=1825 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 68 | 0.220927000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=6, A-MPDU=1825 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 69 | 0.220927000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=6, A-MPDU=1825 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 70 | 0.220927000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=6, A-MPDU=1825 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 71 | 0.220927000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=6, A-MPDU=1825 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 72 | 0.220927000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=6, A-MPDU=1825 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 73 | 0.220927000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=6, A-MPDU=1825 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 74 | 0.220927000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=6, A-MPDU=1825 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 75 | 0.221099000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 76 | 0.221183000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1825 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 77 | 0.224401000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=6, A-MPDU=2018 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 78 | 0.224401000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=6, A-MPDU=2018 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 79 | 0.224401000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=6, A-MPDU=2018 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 80 | 0.224401000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=6, A-MPDU=2018 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 81 | 0.224401000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=6, A-MPDU=2018 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 82 | 0.224401000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=6, A-MPDU=2018 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 83 | 0.224401000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=6, A-MPDU=2018 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 84 | 0.224401000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=6, A-MPDU=2018 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 85 | 0.224401000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=6, A-MPDU=2018 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 86 | 0.224553000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 87 | 0.224637000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2018 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 88 | 0.227855000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=6, A-MPDU=2214 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 89 | 0.227855000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=6, A-MPDU=2214 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 90 | 0.227855000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=6, A-MPDU=2214 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 91 | 0.227855000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=6, A-MPDU=2214 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 92 | 0.227855000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=6, A-MPDU=2214 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 93 | 0.227855000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=6, A-MPDU=2214 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 94 | 0.227855000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=6, A-MPDU=2214 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 95 | 0.227855000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=6, A-MPDU=2214 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 96 | 0.227855000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=24, frag=0, more-frag=0, TID=6, A-MPDU=2214 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 97 | 0.227967000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 98 | 0.228052000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2214 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 99 | 0.231622000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=6, A-MPDU=2436 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 100 | 0.231622000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=6, A-MPDU=2436 |

Frame numbers are local to capture `CompressedBlockAck-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded HT Compressed Block Ack records

##### Origin address: 0a:aa:00:00:00:01

| Frame | Simulation time (s) | Starting sequence | Bitmap | Acknowledged MPDU sequence numbers |
|---:|---:|---:|---|---|
| 34 | 0.208469000 | 2 | 1f00000000000000 | 2, 3, 4, 5, 6 |
| 57 | 0.215311000 | 7 | 3f00000000000000 | 7, 8, 9, 10, 11, 12 |
| 87 | 0.224637000 | 13 | ff01000000000000 | 13, 14, 15, 16, 17, 18, 19, 20, 21 |
| 122 | 0.235584000 | 22 | ff03000000000000 | 22, 23, 24, 25, 26, 27, 28, 29, 30, 31 |
| 162 | 0.248410000 | 32 | ff0f000000000000 | 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43 |
| 210 | 0.263973000 | 44 | ff7f000000000000 | 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58 |
| 264 | 0.281707000 | 59 | ffff010000000000 | 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75 |
| 327 | 0.302569000 | 76 | ffff0f0000000000 | 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95 |
| 399 | 0.326660000 | 96 | ffff7f0000000000 | 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118 |
| 480 | 0.353898000 | 119 | ffffff0300000000 | 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144 |
| 569 | 0.383933000 | 145 | ffffff0f00000000 | 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172 |
| 659 | 0.414339000 | 173 | ffffff0f00000000 | 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200 |
| 749 | 0.444765000 | 201 | ffffff0f00000000 | 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228 |
| 839 | 0.475172000 | 229 | ffffff0f00000000 | 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 256 |
| 929 | 0.505578000 | 257 | ffffff0f00000000 | 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280, 281, 282, 283, 284 |
| 1002 | 0.529961000 | 285 | ff7f000000000000 | 285, 286, 287, 288, 289, 290, 291, 292, 293, 294, 295, 296, 297, 298, 299 |

##### Origin address: 0a:aa:00:00:00:02

| Frame | Simulation time (s) | Starting sequence | Bitmap | Acknowledged MPDU sequence numbers |
|---:|---:|---:|---|---|
| 41 | 0.210475000 | 1 | ff00000000000000 | 1, 2, 3, 4, 5, 6, 7, 8 |
| 66 | 0.218061000 | 9 | 7f00000000000000 | 9, 10, 11, 12, 13, 14, 15 |
| 98 | 0.228052000 | 16 | ff01000000000000 | 16, 17, 18, 19, 20, 21, 22, 23, 24 |
| 135 | 0.239742000 | 25 | ff07000000000000 | 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35 |
| 177 | 0.253272000 | 36 | ff1f000000000000 | 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48 |
| 227 | 0.269499000 | 49 | ff7f000000000000 | 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63 |
| 284 | 0.288289000 | 64 | ffff030000000000 | 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81 |
| 350 | 0.310268000 | 82 | ffff1f0000000000 | 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102 |
| 425 | 0.335354000 | 103 | ffffff0000000000 | 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126 |
| 509 | 0.363648000 | 127 | ffffff0700000000 | 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153 |
| 599 | 0.394095000 | 154 | ffffff0f00000000 | 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181 |
| 689 | 0.424501000 | 182 | ffffff0f00000000 | 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209 |
| 779 | 0.454928000 | 210 | ffffff0f00000000 | 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237 |
| 869 | 0.485334000 | 238 | ffffff0f00000000 | 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265 |
| 959 | 0.515680000 | 266 | ffffff0f00000000 | 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287, 288, 289, 290, 291, 292, 293 |

##### Origin address: 0a:aa:00:00:00:03

| Frame | Simulation time (s) | Starting sequence | Bitmap | Acknowledged MPDU sequence numbers |
|---:|---:|---:|---|---|
| 49 | 0.212893000 | 5 | 3f00000000000000 | 5, 6, 7, 8, 9, 10 |
| 76 | 0.221183000 | 11 | ff00000000000000 | 11, 12, 13, 14, 15, 16, 17, 18 |
| 110 | 0.231818000 | 19 | ff03000000000000 | 19, 20, 21, 22, 23, 24, 25, 26, 27, 28 |
| 148 | 0.243900000 | 29 | ff07000000000000 | 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39 |
| 193 | 0.258447000 | 40 | ff3f000000000000 | 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53 |
| 245 | 0.275417000 | 54 | ffff000000000000 | 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69 |
| 305 | 0.295283000 | 70 | ffff070000000000 | 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88 |
| 374 | 0.318278000 | 89 | ffff3f0000000000 | 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110 |
| 452 | 0.344460000 | 111 | ffffff0100000000 | 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135 |
| 539 | 0.373791000 | 136 | ffffff0f00000000 | 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163 |
| 629 | 0.404197000 | 164 | ffffff0f00000000 | 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191 |
| 719 | 0.434643000 | 192 | ffffff0f00000000 | 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219 |
| 809 | 0.465050000 | 220 | ffffff0f00000000 | 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247 |
| 899 | 0.495436000 | 248 | ffffff0f00000000 | 248, 249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275 |
| 985 | 0.524415000 | 276 | ffffff0000000000 | 276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287, 288, 289, 290, 291, 292, 293, 294, 295, 296, 297, 298, 299 |

### [script] Configuration: `HtImplicitBlockAck`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **967**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#259c21" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC, A-MPDU] | - | 895 | 92.55% | 566.0 B | 0.0 B | 384.3 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 98.81% | 34.40% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35e01f" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC] | - | 5 | 0.52% | 566.0 B | 0.0 B | 384.3 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.55% | 0.19% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 50 | 5.17% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 2412 MHz | -52.4 dBm | - | 0.44% | 0.15% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 11 | 1.14% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -52.2 dBm | 13.0 dBm | 0.08% | 0.03% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 6 | 0.62% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 2412 MHz | -52.3 dBm | 13.0 dBm | 0.12% | 0.04% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 1 | 0.200388000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200448000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 3 | 0.200540000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200600000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 5 | 0.200998000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.201058000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 7 | 0.201974000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.202034000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 9 | 0.202126000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.202186000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 11 | 0.202584000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.202644000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 13 | 0.202836000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.202896000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 15 | 0.203234000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.203294000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 17 | 0.204500000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=723 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 18 | 0.204500000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=723 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 19 | 0.204500000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=723 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 20 | 0.204584000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=723 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 21 | 0.205062000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.205123000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 23 | 0.205215000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 24 | 0.205275000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 25 | 0.206733000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=952 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 26 | 0.206733000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=952 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 27 | 0.206733000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=952 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 28 | 0.206733000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=952 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 29 | 0.206865000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 30 | 0.206925000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 31 | 0.208795000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=1083 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 32 | 0.208795000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=1083 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 33 | 0.208795000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=1083 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 34 | 0.208795000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=1083 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 35 | 0.208795000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=1083 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 36 | 0.208879000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1083 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 37 | 0.210749000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=1211 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 38 | 0.210749000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=1211 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 39 | 0.210749000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=1211 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 40 | 0.210749000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=1211 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 41 | 0.210749000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6, A-MPDU=1211 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 42 | 0.210833000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1211 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 43 | 0.212683000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=1339 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 44 | 0.212683000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=1339 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 45 | 0.212683000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6, A-MPDU=1339 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 46 | 0.212683000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6, A-MPDU=1339 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 47 | 0.212683000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=6, A-MPDU=1339 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 48 | 0.212767000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1339 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 49 | 0.215029000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=1467 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 50 | 0.215029000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6, A-MPDU=1467 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 51 | 0.215029000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6, A-MPDU=1467 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 52 | 0.215029000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=6, A-MPDU=1467 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 53 | 0.215029000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=6, A-MPDU=1467 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 54 | 0.215029000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=6, A-MPDU=1467 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 55 | 0.215114000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1083, 1467 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 56 | 0.217728000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6, A-MPDU=1620 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 57 | 0.217728000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=6, A-MPDU=1620 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 58 | 0.217728000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=6, A-MPDU=1620 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 59 | 0.217728000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=6, A-MPDU=1620 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 60 | 0.217728000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=6, A-MPDU=1620 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 61 | 0.217728000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=6, A-MPDU=1620 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 62 | 0.217728000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=6, A-MPDU=1620 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 63 | 0.217812000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1620 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 64 | 0.220406000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=6, A-MPDU=1762 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 65 | 0.220406000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=6, A-MPDU=1762 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 66 | 0.220406000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=6, A-MPDU=1762 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 67 | 0.220406000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=6, A-MPDU=1762 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 68 | 0.220406000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=6, A-MPDU=1762 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 69 | 0.220406000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=6, A-MPDU=1762 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 70 | 0.220406000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=6, A-MPDU=1762 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 71 | 0.220490000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1762 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 72 | 0.223456000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=6, A-MPDU=1922 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 73 | 0.223456000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=6, A-MPDU=1922 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 74 | 0.223456000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=6, A-MPDU=1922 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 75 | 0.223456000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=6, A-MPDU=1922 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 76 | 0.223456000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=6, A-MPDU=1922 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 77 | 0.223456000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=6, A-MPDU=1922 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 78 | 0.223456000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=6, A-MPDU=1922 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 79 | 0.223456000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=6, A-MPDU=1922 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 80 | 0.223540000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1083, 1467, 1922 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 81 | 0.226466000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=6, A-MPDU=2089 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 82 | 0.226466000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=6, A-MPDU=2089 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 83 | 0.226466000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=6, A-MPDU=2089 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 84 | 0.226466000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=6, A-MPDU=2089 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 85 | 0.226466000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=6, A-MPDU=2089 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 86 | 0.226466000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=6, A-MPDU=2089 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 87 | 0.226466000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=6, A-MPDU=2089 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 88 | 0.226466000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=6, A-MPDU=2089 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 89 | 0.226550000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2089 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 90 | 0.229848000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=6, A-MPDU=2256 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 91 | 0.229848000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=6, A-MPDU=2256 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 92 | 0.229848000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=6, A-MPDU=2256 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 93 | 0.229848000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=6, A-MPDU=2256 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 94 | 0.229848000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=6, A-MPDU=2256 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 95 | 0.229848000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=6, A-MPDU=2256 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 96 | 0.229848000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=24, frag=0, more-frag=0, TID=6, A-MPDU=2256 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 97 | 0.229848000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=25, frag=0, more-frag=0, TID=6, A-MPDU=2256 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 98 | 0.229848000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=26, frag=0, more-frag=0, TID=6, A-MPDU=2256 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 99 | 0.229932000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2256 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 100 | 0.233190000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=6, A-MPDU=2430 |

Frame numbers are local to capture `HtImplicitBlockAck-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded HT Compressed Block Ack records

##### Origin address: 0a:aa:00:00:00:01

| Frame | Simulation time (s) | Starting sequence | Bitmap | Acknowledged MPDU sequence numbers |
|---:|---:|---:|---|---|
| 48 | 0.212767000 | 6 | 1f00000000000000 | 6, 7, 8, 9, 10 |
| 71 | 0.220490000 | 11 | 7f00000000000000 | 11, 12, 13, 14, 15, 16, 17 |
| 99 | 0.229932000 | 18 | ff01000000000000 | 18, 19, 20, 21, 22, 23, 24, 25, 26 |
| 132 | 0.241075000 | 27 | ff07000000000000 | 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37 |
| 172 | 0.254701000 | 38 | ff1f000000000000 | 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50 |
| 216 | 0.269755000 | 51 | ff3f000000000000 | 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64 |
| 267 | 0.287254000 | 65 | ffff010000000000 | 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81 |
| 325 | 0.307236000 | 82 | ffff070000000000 | 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100 |
| 391 | 0.330035000 | 101 | ffff3f0000000000 | 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122 |
| 466 | 0.355961000 | 123 | ffffff0100000000 | 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147 |
| 550 | 0.385095000 | 148 | ffffff0f00000000 | 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175 |
| 637 | 0.415246000 | 176 | ffffff0f00000000 | 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203 |
| 724 | 0.445416000 | 204 | ffffff0f00000000 | 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231 |
| 811 | 0.475587000 | 232 | ffffff0f00000000 | 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259 |
| 898 | 0.505737000 | 260 | ffffff0f00000000 | 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287 |
| 963 | 0.528203000 | 288 | ff0f000000000000 | 288, 289, 290, 291, 292, 293, 294, 295, 296, 297, 298, 299 |

##### Origin address: 0a:aa:00:00:00:02

| Frame | Simulation time (s) | Starting sequence | Bitmap | Acknowledged MPDU sequence numbers |
|---:|---:|---:|---|---|
| 20 | 0.204584000 | 1 | 0700000000000000 | 1, 2, 3 |
| 42 | 0.210833000 | 4 | 1f00000000000000 | 4, 5, 6, 7, 8 |
| 63 | 0.217812000 | 9 | 7f00000000000000 | 9, 10, 11, 12, 13, 14, 15 |
| 89 | 0.226550000 | 16 | ff00000000000000 | 16, 17, 18, 19, 20, 21, 22, 23 |
| 120 | 0.237008000 | 24 | ff03000000000000 | 24, 25, 26, 27, 28, 29, 30, 31, 32, 33 |
| 158 | 0.249911000 | 34 | ff0f000000000000 | 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 |
| 201 | 0.264633000 | 46 | ff3f000000000000 | 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59 |
| 249 | 0.281056000 | 60 | ffff000000000000 | 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75 |
| 305 | 0.300314000 | 76 | ffff030000000000 | 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93 |
| 368 | 0.322076000 | 94 | ffff1f0000000000 | 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114 |
| 440 | 0.346927000 | 115 | ffffff0000000000 | 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138 |
| 521 | 0.375045000 | 139 | ffffff0700000000 | 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165 |
| 608 | 0.405196000 | 166 | ffffff0f00000000 | 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193 |
| 695 | 0.435366000 | 194 | ffffff0f00000000 | 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221 |
| 782 | 0.465556000 | 222 | ffffff0f00000000 | 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249 |
| 869 | 0.495707000 | 250 | ffffff0f00000000 | 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277 |
| 950 | 0.523745000 | 278 | ffff3f0000000000 | 278, 279, 280, 281, 282, 283, 284, 285, 286, 287, 288, 289, 290, 291, 292, 293, 294, 295, 296, 297, 298, 299 |

##### Origin address: 0a:aa:00:00:00:03

| Frame | Simulation time (s) | Starting sequence | Bitmap | Acknowledged MPDU sequence numbers |
|---:|---:|---:|---|---|
| 36 | 0.208879000 | 1 | 3e00000000000000 | 2, 3, 4, 5, 6 |
| 55 | 0.215114000 | 1 | fe0f000000000000 | 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 |
| 80 | 0.223540000 | 1 | feff0f0000000000 | 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20 |
| 109 | 0.233274000 | 1 | feffff1f00000000 | 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29 |
| 145 | 0.245473000 | 1 | feffffffff010000 | 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41 |
| 186 | 0.259491000 | 1 | feffffffffff3f00 | 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54 |
| 232 | 0.275230000 | 6 | ffffffffffffffff | 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69 |
| 286 | 0.293764000 | 24 | ffffffffffffffff | 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87 |
| 346 | 0.314490000 | 44 | ffffffffffffffff | 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107 |
| 415 | 0.338305000 | 67 | ffffffffffffffff | 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130 |
| 493 | 0.365307000 | 93 | ffffffffffffffff | 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156 |
| 579 | 0.395166000 | 121 | ffffffffffffffff | 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184 |
| 666 | 0.425336000 | 149 | ffffffffffffffff | 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212 |
| 753 | 0.455486000 | 177 | ffffffffffffffff | 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240 |
| 840 | 0.485657000 | 205 | ffffffffffffffff | 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268 |
| 927 | 0.515807000 | 233 | ffffffffffffffff | 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287, 288, 289, 290, 291, 292, 293, 294, 295, 296 |
| 967 | 0.529434000 | 236 | ffffffffffffffff | 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287, 288, 289, 290, 291, 292, 293, 294, 295, 296, 297, 298, 299 |

### [script] Analysis of Packet Distribution
Across these configurations, **QoS Data** frames constitute the primary payload delivery mechanism, while **Block Ack (BA)** and **Block Ack Request (BAR)** control frames ensure reliable transport via the MAC-level acknowledgment protocol. Management frames, specifically **Beacons**, are transmitted periodically by the Access Point to maintain BSS time synchronization and broadcast network capabilities. The ratio of control/management overhead to actual data frames indicates the relative MAC efficiency of the chosen configurations.
<!-- END GENERATED: ieee80211-pcap-statistics -->
