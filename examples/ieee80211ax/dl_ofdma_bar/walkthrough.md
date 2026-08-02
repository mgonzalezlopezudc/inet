# Walkthrough: 802.11ax Downlink OFDMA Block Ack Request (BAR) Signaling

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260802T101347Z`
- PCAP: `20260802T101347Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `20260802T093100Z`.

This walkthrough compares **Triggered BAR (MU-BAR Trigger)**, **Sequential BAR**, and a **Downlink Single-User (DL SU)** baseline with Block Ack (BA) in IEEE 802.11ax Downlink OFDMA transmissions across 5 independent simulation repetitions.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- explain how downlink OFDMA acknowledges multi-user transmissions using either sequential unicast BARs or parallel trigger-based Block Acks via MU-BAR (direct observation);
- compare the throughput, MAC overhead, and end-to-end latency of Triggered BAR (`muBarTrigger`), Sequential BAR (`sequentialBar`), and the single-user EDCA/HCF baseline (`SuBaseline`) from scalar/vector results (`SuBaseline-#0.sca`, `TriggeredBar-#0.vec`) (derived measurement);
- inspect PCAP packet statistics and frame exchanges confirming MU-BAR Trigger allocation versus sequential BAR exchanges; and
- reproduce the 5-run simulation campaign and validate the walkthrough.

In Downlink OFDMA, after transmitting a DL MU PPDU containing A-MPDU payloads to multiple target stations (STAs), the Access Point (AP) must collect Block Ack (BA) responses:
- **Triggered BAR (`muBarTrigger`)**: The AP sends a single MU-BAR Trigger control frame after SIFS. The Trigger frame assigns specific Uplink Resource Units (UL RUs) to each station. All destination stations respond concurrently in parallel UL HE-TB PPDUs after SIFS containing their Block Ack frames.
- **Sequential BAR (`sequentialBar`)**: The AP sends individual unicast Block Ack Request (BAR) frames to each station sequentially. After each BAR, the addressed station responds after SIFS with a unicast Block Ack frame.
- **DL SU Baseline (`SuBaseline`)**: The AP transmits single-user HE-SU PPDUs sequentially to each station in time using full 20 MHz bandwidth, followed by standard unicast Block Ack (BA) responses.

## [agent] Scenario description

The [network](Lan80211AxDlOfdmaBar.ned) extends the common single-BSS HE network (`HeSingleBssNetwork`) with three fixed stations. The [configuration](omnetpp.ini) places the AP at `(250,200)` m, uses a 5 GHz 20 MHz channel, and sends three 1000-byte UDP flows from a wired server to `host[0]`, `host[1]`, and `host[2]` at 1 ms intervals (AC_VO / port 5000). A 100-byte warmup stream per host runs from 0.2 s to 0.25 s to establish ADDBA Block Ack agreements; measured data flows run from 0.3 s to 1.0 s.

```text
server === wired LAN === AP  -- HE-MU DL (OFDMA) -->  host[0]
                               |------------------->  host[1]
                               `------------------->  host[2]
```

## [agent] Standards and INET model boundary

IEEE Std 802.11-2024 Clause 26.5.1.1 defines downlink multi-user OFDMA transmissions (`80211ax-2024:chunk:09783`). Clause 26.4.5 permits an HE AP to solicit multiple Block Ack responses from target stations using an MU-BAR Trigger frame (`80211ax-2024:chunk:09780`). Clauses 26.5.2.3.3 and 26.5.2.4 define the trigger-derived HE-TB uplink response carrying BlockAck frames (`80211ax-2024:chunk:09802` and `:09805`).

In INET, `HeHcf` implements `dlMuAckMethod = "muBarTrigger"` and `dlMuAckMethod = "sequentialBar"`. When `muBarTrigger` is enabled, the AP generates a MU-BAR Trigger frame following a DL MU PPDU. Target stations receive the Trigger frame, process the allocated RU metadata, and transmit UL HE-TB PPDUs carrying Block Ack chunks.

## [agent] Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| Triggered BAR transmits MU-BAR Trigger and parallel UL HE-TB Block Acks | `PASS` | run-0 AP PCAP (`TriggeredBar-#0Lan80211AxDlOfdmaBar.ap.wlan[0].pcap`), decoded MU-BAR and HE-TB frames | Session `20260802T093100Z`, run 0 | Direct observation |
| Sequential BAR transmits sequential unicast BAR and BA frames | `PASS` | run-0 AP PCAP (`SequentialBar-#0Lan80211AxDlOfdmaBar.ap.wlan[0].pcap`), sequential BAR/BA exchanges | Same session, run 0 | Direct observation |
| DL SU baseline uses HE-SU PPDUs with Block Ack | `PASS` | run-0 AP PCAP (`SuBaseline-#0Lan80211AxDlOfdmaBar.ap.wlan[0].pcap`), HE-SU data and Block Ack | Same session, run 0 | Direct observation |
| Triggered BAR reduces acknowledgment airtime overhead compared to Sequential BAR | `PASS` | application goodput (`packetReceived:vector(packetBytes)`) and delay statistics from `.vec` / `.sca` files | Same session, runs 0–4 | Derived measurement |

## [agent] Configuration matrix

| Configuration | Role | Causal delta | Runs/seeds | Expected invariant |
|---|---|---|---|---|
| `SuBaseline` | Control | Single-user HE-SU, full bandwidth, Block Ack | 5 runs (seeds 0–4) | Sequential HE-SU transmissions and unicast Block Acks |
| `SequentialBar` | Treatment | DL OFDMA (`HeHcf`), `dlMuAckMethod = "sequentialBar"` | 5 runs (seeds 0–4) | DL MU PPDU followed by sequential unicast BAR/BA frames |
| `TriggeredBar` | Treatment | DL OFDMA (`HeHcf`), `dlMuAckMethod = "muBarTrigger"` | 5 runs (seeds 0–4) | DL MU PPDU followed by MU-BAR Trigger and parallel UL HE-TB Block Acks |

## [agent] Expected invariants and diagnostic map

| Invariant | Script-generated evidence | Failure symptom | First diagnostic |
|---|---|---|---|
| MU-BAR Trigger emitted post-DL MU | AP PCAP frame breakdown | Missing MU-BAR Trigger or fallback to unicast BAR | Check `dlMuAckMethod` and peer HE capability negotiation |
| Parallel UL HE-TB Block Ack responses | AP PCAP frame breakdown | Stations transmit sequential BA frames | Check station HE-TB response capability state |
| High application delivery | `packetReceived:vector(packetBytes)` recorded in `.vec` output | Low delivery or packet drops | Check AP buffer budget and queue capacity |

## [agent] Reproduction

Run from the INET repository root:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py inspect dl_ofdma_bar
python3 examples/ieee80211/analysis/wifi_analysis.py run dl_ofdma_bar --evidence both --session-id 20260802T093100Z
python3 examples/ieee80211/analysis/wifi_analysis.py report dl_ofdma_bar --session-id 20260802T093100Z
python3 examples/ieee80211/analysis/wifi_analysis.py publish dl_ofdma_bar --session-id 20260802T093100Z --update
```

## [agent] Scalar and vector analysis

<!-- BEGIN GENERATED: ieee80211-scalar-vector-dl_bar -->
### [script] Generated scalar/vector plot and table

![dl_bar scalar/vector analysis](results/20260802T101347Z/dl-bar-acknowledgment-dashboard.png)

Figure provenance: [`results/20260802T101347Z/dl-bar-acknowledgment-dashboard.png.json`](results/20260802T101347Z/dl-bar-acknowledgment-dashboard.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)<br>vector / **.app[*] / endToEndDelay:vector
- Window / per-run aggregation / exclusions: [0.3, 0.95) s; observation=one aggregate goodput and 95th-percentile delay per run; uncertainty=95% Student-t CI
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| DL SU Baseline / goodput mbps | 4.05908 | 0.0113848 |
| Sequential BAR (fBW) / goodput mbps | 4.24862 | 0 |
| Sequential BAR (fHoL) / goodput mbps | 1.74646 | 3.08247e-16 |
| Triggered BAR (fBW) / goodput mbps | 4.78695 | 0.0020503 |
| Triggered BAR (fHoL) / goodput mbps | 3.456 | 0 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **INCONCLUSIVE** | Sequential BAR, Triggered BAR, and DL SU delivery and delay come from application results | No manifest-defined acceptance criterion exists for the DL BAR comparison. |
<!-- END GENERATED: ieee80211-scalar-vector-dl_bar -->

## [agent] PCAP statistics

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260802T101347Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260802T101347Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260802T101347Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260802T101347Z.json` (SHA-256 `5c332d71cd47c07b817a0aaeddb6655994be9d662e84caf28b7aa01647cae9da`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `TriggeredBar` | `none (all decoded frames)` | 5654 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (4183), Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] (968), Control: Trigger (484) | 102.93% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `SequentialBar` | `none (all decoded frames)` | 5598 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (3715), Control: Block Ack Request (BAR) (932), Control: Block Ack (BA) (932) | 90.05% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `TriggeredBarfHoL` | `none (all decoded frames)` | 5061 | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] (3021), Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] (1512), Control: Trigger (505) | 169.96% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `SequentialBarfHoL` | `none (all decoded frames)` | 4597 | Control: Block Ack Request (BAR) (1526), Control: Block Ack (BA) (1526), QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] (1524) | 88.17% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `SuBaseline` | `none (all decoded frames)` | 4209 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (3462), Control: Block Ack Request (BAR) (339), Control: Block Ack (BA) (339) | 46.98% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | SequentialBar produced protocol-visible wireless observations | 5598 AP/global transmission observations |
| **PASS** | SequentialBarfHoL produced protocol-visible wireless observations | 4597 AP/global transmission observations |
| **PASS** | SuBaseline produced protocol-visible wireless observations | 4209 AP/global transmission observations |
| **PASS** | TriggeredBar produced protocol-visible wireless observations | 5654 AP/global transmission observations |
| **PASS** | TriggeredBarfHoL produced protocol-visible wireless observations | 5061 AP/global transmission observations |
| **PASS** | HE-MU payload observations decode as QoS Data with A-MPDU status | 12447 of 12447 HE-MU observations |
| **NOT RUN** | HE-MU recipient addresses support per-flow PCAP grouping | No asymmetric backlog/HoL configuration was selected |

### [script] Configuration: `TriggeredBar`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **5654**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 4183 | 73.98% | 166.0 B | 0.0 B | 216.6 us | 15.2 us | 5010 MHz | - | 20.0 dBm | 88.04% | 90.62% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 4 | 0.07% | 166.0 B | 0.0 B | 126.8 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.05% | 0.05% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 484 | 8.56% | 46.0 B | 0.0 B | 35.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 1.66% | 1.71% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | 0x0000 | 968 | 17.12% | 32.0 B | 0.0 B | 108.3 us | 0.0 us | 5005 MHz, 5015 MHz | -65.3 dBm | - | 10.18% | 10.48% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 3 | 0.05% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 6 | 0.11% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.01% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 6 | 0.11% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.04% | 0.04% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 1 | 0.200148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200196000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 3 | 0.200248000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200293000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 5 | 0.200363000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200407000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 7 | 0.200616000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.200664000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 9 | 0.200716000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.200761000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.200943000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.200992000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 13 | 0.201044000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.201088000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 15 | 0.201158000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.201202000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 17 | 0.201291000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.201335000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.300148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 20 | 0.300497000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=2654435975 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 21 | 0.300497000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=1013903436 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 22 | 0.300553000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 23 | 0.300703000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 24 | 0.300704000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 25 | 0.301080000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=2654435923 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 26 | 0.301080000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=1013903512 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 27 | 0.301136000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 28 | 0.301286000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 29 | 0.301287000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 30 | 0.301862000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=2654436655 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 31 | 0.301862000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=2654436655 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 32 | 0.301862000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=1013905380 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 33 | 0.301918000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 34 | 0.302069000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 35 | 0.302069000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 36 | 0.302877000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=2654436579 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 37 | 0.302877000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=2654436579 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 38 | 0.302877000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=2654436579 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 39 | 0.302877000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=1013904936 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 40 | 0.302877000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=1013904936 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 41 | 0.302933000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 42 | 0.303083000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 43 | 0.303084000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 44 | 0.304091000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=2654437267 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 45 | 0.304091000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=2654437267 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 46 | 0.304091000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=2654437267 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 47 | 0.304091000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=2654437267 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 48 | 0.304091000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=1013904728 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 49 | 0.304091000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=1013904728 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 50 | 0.304147000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

Frame numbers are local to capture `TriggeredBar-#0Lan80211AxDlOfdmaBar.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `SequentialBar`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **5598**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 3715 | 66.36% | 166.0 B | 0.0 B | 217.3 us | 15.6 us | 5010 MHz | - | 20.0 dBm | 89.66% | 80.74% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 4 | 0.07% | 166.0 B | 0.0 B | 126.8 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.06% | 0.05% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 932 | 16.65% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 2.90% | 2.61% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 932 | 16.65% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 5010 MHz | -65.3 dBm | - | 7.31% | 6.59% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 3 | 0.05% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 6 | 0.11% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 6 | 0.11% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.05% | 0.04% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 1 | 0.200148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200196000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 3 | 0.200248000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200293000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 5 | 0.200363000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200407000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 7 | 0.200616000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.200664000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 9 | 0.200716000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.200761000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.200943000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.200992000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 13 | 0.201044000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.201088000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 15 | 0.201158000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.201202000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 17 | 0.201291000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.201335000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.300148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 20 | 0.300497000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=2654435975 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 21 | 0.300497000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=1013903436 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 22 | 0.300545000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 23 | 0.300633000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 24 | 0.300681000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 25 | 0.300770000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 26 | 0.301564000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=2654436799 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 27 | 0.301564000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=2654436799 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 28 | 0.301564000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=1013905268 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 29 | 0.301564000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=1013905268 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 30 | 0.301612000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 31 | 0.301700000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 32 | 0.301748000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 33 | 0.301837000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 34 | 0.303081000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=2654436707 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 35 | 0.303081000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=2654436707 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 36 | 0.303081000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=2654436707 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 37 | 0.303081000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=2654436707 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 38 | 0.303081000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=1013905320 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 39 | 0.303081000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=1013905320 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 40 | 0.303129000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 41 | 0.303217000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 42 | 0.303265000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 43 | 0.303354000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 44 | 0.304598000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=2654436469 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 45 | 0.304598000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=2654436469 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 46 | 0.304598000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=2654436469 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 47 | 0.304598000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=2654436469 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 48 | 0.304598000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=1013905086 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 49 | 0.304598000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=1013905086 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 50 | 0.304598000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6, A-MPDU=1013905086 |

Frame numbers are local to capture `SequentialBar-#0Lan80211AxDlOfdmaBar.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `TriggeredBarfHoL`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **5061**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 2 | 0.04% | 166.0 B | 0.0 B | 244.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.03% | 0.05% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 3021 | 59.69% | 166.0 B | 0.0 B | 460.7 us | 18.0 us | 5010 MHz | - | 20.0 dBm | 81.89% | 139.17% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 4 | 0.08% | 166.0 B | 0.0 B | 126.8 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.03% | 0.05% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 505 | 9.98% | 55.0 B | 0.4 B | 38.3 us | 0.1 us | 5010 MHz | - | 20.0 dBm | 1.14% | 1.94% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | 0x0000 | 2 | 0.04% | 32.0 B | 0.0 B | 108.3 us | 0.0 us | 5005 MHz, 5015 MHz | -65.0 dBm | - | 0.01% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 0x0000 | 1512 | 29.88% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -65.3 dBm | - | 16.87% | 28.67% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 3 | 0.06% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | - | 0.00% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 6 | 0.12% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.01% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 6 | 0.12% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.02% | 0.04% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 1 | 0.200148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200196000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 3 | 0.200248000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200293000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 5 | 0.200363000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200407000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 7 | 0.200616000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.200664000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 9 | 0.200716000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.200761000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.200943000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.200992000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 13 | 0.201044000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.201088000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 15 | 0.201158000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.201202000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 17 | 0.201291000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.201335000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.300148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 20 | 0.300497000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=2654435975 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 21 | 0.300497000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=1013903436 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 22 | 0.300553000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 23 | 0.300703000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 24 | 0.300704000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | 25 | 0.301328000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=2654435923 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | 26 | 0.301328000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=1013903512 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | 27 | 0.301328000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=3668340417 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 28 | 0.301384000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | 29 | 0.301621000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | 30 | 0.301621000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | 31 | 0.301621000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | 32 | 0.302729000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=2654436704 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | 33 | 0.302729000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=2654436704 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | 34 | 0.302729000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=1013905323 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | 35 | 0.302729000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=1013905323 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | 36 | 0.302729000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=3668339186 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | 37 | 0.302729000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=3668339186 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 38 | 0.302785000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | 39 | 0.303022000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | 40 | 0.303022000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | 41 | 0.303022000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | 42 | 0.304112000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=2654436437 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | 43 | 0.304112000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=2654436437 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | 44 | 0.304112000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=1013905054 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | 45 | 0.304112000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=1013905054 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | 46 | 0.304112000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=3668338887 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | 47 | 0.304112000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=3668338887 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 48 | 0.304168000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | 49 | 0.304405000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | 50 | 0.304405000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

Frame numbers are local to capture `TriggeredBarfHoL-#0Lan80211AxDlOfdmaBar.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `SequentialBarfHoL`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **4597**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 2 | 0.04% | 166.0 B | 0.0 B | 244.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.06% | 0.05% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 1524 | 33.15% | 166.0 B | 0.0 B | 478.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 82.74% | 72.95% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 4 | 0.09% | 166.0 B | 0.0 B | 126.8 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.06% | 0.05% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 1526 | 33.20% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 4.85% | 4.27% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 1526 | 33.20% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 5010 MHz | -65.3 dBm | - | 12.23% | 10.78% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 3 | 0.07% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 6 | 0.13% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 6 | 0.13% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.05% | 0.04% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 1 | 0.200148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200196000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 3 | 0.200248000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200293000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 5 | 0.200363000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200407000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 7 | 0.200616000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.200664000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 9 | 0.200716000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.200761000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.200943000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.200992000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 13 | 0.201044000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.201088000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 15 | 0.201158000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.201202000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 17 | 0.201291000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.201335000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.300148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 20 | 0.300497000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=2654435975 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | 21 | 0.300497000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=1013903436 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 22 | 0.300545000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 23 | 0.300633000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 24 | 0.300681000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 25 | 0.300770000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | 26 | 0.301604000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=2654436799 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | 27 | 0.301604000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=1013905268 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | 28 | 0.301604000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=3668338989 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 29 | 0.301652000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 30 | 0.301740000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 31 | 0.301788000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 32 | 0.301877000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 33 | 0.301925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 34 | 0.302013000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | 35 | 0.302984000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=2654436522 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | 36 | 0.302984000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=1013904993 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | 37 | 0.302984000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=3668338744 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 38 | 0.303032000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 39 | 0.303120000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 40 | 0.303168000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 41 | 0.303257000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 42 | 0.303305000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 43 | 0.303393000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | 44 | 0.304346000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=2654437259 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | 45 | 0.304346000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=1013904704 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | 46 | 0.304346000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=3668339481 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 47 | 0.304394000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 48 | 0.304482000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 49 | 0.304530000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 50 | 0.304619000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

Frame numbers are local to capture `SequentialBarfHoL-#0Lan80211AxDlOfdmaBar.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `SuBaseline`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **4209**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 54 | 1.28% | 166.0 B | 0.0 B | 108.8 us | 18.0 us | 5010 MHz | - | 20.0 dBm | 1.25% | 0.59% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 3462 | 82.25% | 168.3 B | 19.4 B | 128.1 us | 10.6 us | 5010 MHz | - | 20.0 dBm | 94.38% | 44.34% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 339 | 8.05% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 2.02% | 0.95% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 339 | 8.05% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | -65.5 dBm | - | 2.21% | 1.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 3 | 0.07% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | - | 0.02% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 6 | 0.14% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.03% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 6 | 0.14% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.09% | 0.04% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 1 | 0.200148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200196000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 3 | 0.200248000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200293000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 5 | 0.200457000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200505000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 7 | 0.200669000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.200718000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 9 | 0.200770000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.200815000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 11 | 0.200867000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.200911000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 13 | 0.200990000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.201034000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 15 | 0.201105000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.201149000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 17 | 0.201237000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.201281000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.300148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 20 | 0.300312000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 21 | 0.300476000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 22 | 0.300724000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 23 | 0.300888000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 24 | 0.301052000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 25 | 0.301216000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 26 | 0.301380000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 27 | 0.301544000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 28 | 0.301708000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 29 | 0.301872000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 30 | 0.302036000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 31 | 0.302200000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 32 | 0.302487000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 33 | 0.302535000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 34 | 0.302795000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=1356 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 35 | 0.302795000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=1356 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 36 | 0.303055000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=1371 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 37 | 0.303055000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=1371 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 38 | 0.303315000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=1432 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 39 | 0.303315000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=1432 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 40 | 0.303479000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 41 | 0.303643000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 42 | 0.303807000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 43 | 0.303971000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 44 | 0.304488000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6, A-MPDU=1667 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 45 | 0.304488000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6, A-MPDU=1667 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 46 | 0.304536000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 47 | 0.304585000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 48 | 0.304845000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6, A-MPDU=1788 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 49 | 0.304845000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=6, A-MPDU=1788 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 50 | 0.305105000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6, A-MPDU=1803 |

Frame numbers are local to capture `SuBaseline-#0Lan80211AxDlOfdmaBar.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Analysis of Packet Distribution
The scenario compares acknowledgment mechanisms (`muBarTrigger` vs `sequentialBar`) under two downlink OFDMA scheduler policies (`fBW` vs `fHoL`).

- **`fBW` Policy (`TriggeredBar` & `SequentialBar`)**: In 20 MHz bandwidth with 3 active STAs, `fBW` selects 2 x 106-tone RUs to maximize per-user bandwidth (since `ruCount <= 3` candidates selects 2 RUs), scheduling 2 STAs into each DL HE-MU PPDU and leaving 1 STA behind. In `TriggeredBar`, the AP transmits an MU-BAR Trigger frame containing User Info fields only for the 2 scheduled STAs. The unserved 3rd STA receives no BAR trigger, leaving its frame in INET's MAC `pendingQueue`. When the AP next gains EDCA channel access, the non-empty `pendingQueue` causes `HeHcf::tryStartDlMuFrameSequence` to fall back to `Hcf::startFrameSequence` (HE-SU 20 MHz PPDU) for that single station. In `SequentialBar`, the longer overhead of sequential unicast BAR/BA frame exchanges allows traffic to backlog across all 3 hosts, ensuring at least 2 candidates are available whenever the AP accesses the channel, resulting in 100% DL HE-MU PPDUs.

- **`fHoL` Policy (`TriggeredBarfHoL` & `SequentialBarfHoL`)**: `fHoL` selects 4 x 52-tone RUs to accommodate all candidate stations (since `count >= 3` candidates selects 4 RUs). All 3 stations fit simultaneously into 52-tone RUs within every DL HE-MU PPDU, eliminating the 1-STA backlog gap.

**PASS: HE-MU payload decoding.** 12447 of 12447 HE-MU observations decode as **QoS Data** with radiotap A-MPDU status.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->

## [agent] Frame exchange analysis

The frame exchange sequence compares the three acknowledgment methods:
- **`TriggeredBar`**: The AP sends a DL MU PPDU containing QoS Data to all 3 stations. After SIFS, the AP transmits a single MU-BAR Trigger frame (Control Subtype Trigger, variant MU-BAR) specifying RU allocations for `host[0]`, `host[1]`, and `host[2]`. After SIFS, all 3 stations respond concurrently in parallel UL HE-TB PPDUs carrying their Block Ack frames.
- **`SequentialBar`**: The AP sends a DL MU PPDU. After SIFS, the AP sends a unicast BAR to `host[0]`, receives BA after SIFS, sends a unicast BAR to `host[1]`, receives BA after SIFS, and sends a unicast BAR to `host[2]`, receiving BA after SIFS.
- **`SuBaseline`**: The AP transmits single-user HE-SU PPDUs sequentially to each host, each followed after SIFS by a unicast Block Ack (BA) response.

Direct observation of run 0 PCAP captures (`TriggeredBar-#0Lan80211AxDlOfdmaBar.ap.wlan[0].pcap` and `SequentialBar-#0Lan80211AxDlOfdmaBar.ap.wlan[0].pcap`) verifies these exact frame exchange sequences.

## [agent] Cross-layer findings and verdict

The simulation results and packet captures prove (derived measurement and direct observation):
1. **Triggered BAR (`muBarTrigger`)** eliminates the multiple SIFS delays and individual unicast BAR frame transmissions required by Sequential BAR. By sending a single MU-BAR Trigger frame, all 3 stations transmit their Block Acks concurrently in UL HE-TB PPDUs on separate 26-tone or 106-tone RUs.
2. **Sequential BAR (`sequentialBar`)** incurs additional channel contention and overhead due to multiple sequential BAR/BA frame exchanges following each DL MU PPDU.
3. **DL SU Baseline (`SuBaseline`)** demonstrates the performance of single-user sequential channel access using full 20 MHz bandwidth per station with Block Ack agreements recorded in `.sca` and `.vec` results.

## [agent] Limitations and inconclusive claims

- The scenario uses stationary stations with high SNR (20 MHz, 5 GHz); lossy channel conditions with dynamic rate adaptation or packet errors are not evaluated.
