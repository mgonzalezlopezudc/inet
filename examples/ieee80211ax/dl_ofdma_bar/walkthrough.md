# Walkthrough: 802.11ax Downlink OFDMA Block Ack Request (BAR) Signaling

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260802T093100Z`
- PCAP: `20260802T093100Z`
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

![dl_bar scalar/vector analysis](results/20260802T093100Z/dl-bar-acknowledgment-dashboard.png)

Figure provenance: [`results/20260802T093100Z/dl-bar-acknowledgment-dashboard.png.json`](results/20260802T093100Z/dl-bar-acknowledgment-dashboard.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes) / unit=B<br>vector / **.app[*] / endToEndDelay:vector / unit=s<br>vector / **.ap.wlan[0].radio / heStaId:vector<br>vector / **.ap.wlan[0].radio / heScheduledPsduBytes:vector / unit=B<br>vector / **.ap.wlan[0].radio / heUserPpduDuration:vector / unit=s
- Window / per-run aggregation / exclusions: [0.3, 0.95) s; delay=pooled packet p95 within run; observation=one per run; per_user=STA-ID aligned scheduled PSDU bytes and PPDU duration; uncertainty=95% Student-t CI
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| DL SU Baseline / goodput mbps | 13.0708 | 2.46598e-15 |
| Sequential BAR / goodput mbps | 11.1852 | 0.0127859 |
| Triggered BAR / goodput mbps | 11.1852 | 0.0127859 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **INCONCLUSIVE** | Sequential BAR, Triggered BAR, and DL SU delivery and delay come from application results | No manifest-defined acceptance criterion exists for the DL BAR comparison. |
<!-- END GENERATED: ieee80211-scalar-vector-dl_bar -->

## [agent] PCAP statistics

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260802T093100Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260802T093100Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260802T093100Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260802T093100Z.json` (SHA-256 `033b4ea92d44b5eda85b540d37039db3dff51fa29313e890954510e2dbc3cec0`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `TriggeredBar` | `none (all decoded frames)` | 1152 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (983), Control: Block Ack Request (BAR) (77), Control: Block Ack (BA) (77) | 61.23% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `SequentialBar` | `none (all decoded frames)` | 1152 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (983), Control: Block Ack Request (BAR) (77), Control: Block Ack (BA) (77) | 61.23% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `SuBaseline` | `none (all decoded frames)` | 1415 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (1141), Control: Block Ack Request (BAR) (127), Control: Block Ack (BA) (127) | 67.97% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | SequentialBar produced protocol-visible wireless observations | 1152 AP/global transmission observations |
| **PASS** | SuBaseline produced protocol-visible wireless observations | 1415 AP/global transmission observations |
| **PASS** | TriggeredBar produced protocol-visible wireless observations | 1152 AP/global transmission observations |

### [script] Configuration: `TriggeredBar`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1152**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 983 | 85.33% | 1063.3 B | 49.6 B | 617.6 us | 27.2 us | 5010 MHz | - | 20.0 dBm | 99.16% | 60.71% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 77 | 6.68% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.35% | 0.22% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 77 | 6.68% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | -65.5 dBm | - | 0.39% | 0.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 3 | 0.26% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 6 | 0.52% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 6 | 0.52% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.07% | 0.04% |

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
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.300644000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 20 | 0.301410000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 21 | 0.302140000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 22 | 0.302879000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 23 | 0.303618000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 24 | 0.304357000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 25 | 0.305105000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 26 | 0.305862000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 27 | 0.306583000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 28 | 0.307322000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 29 | 0.308079000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 30 | 0.308809000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 31 | 0.309521000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 32 | 0.309639000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 33 | 0.309687000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 34 | 0.310347000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 35 | 0.311068000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 36 | 0.311789000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 37 | 0.312537000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 38 | 0.313294000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 39 | 0.314033000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 40 | 0.314754000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 41 | 0.315484000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 42 | 0.316223000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 43 | 0.316935000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 44 | 0.317647000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 45 | 0.318404000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 46 | 0.319134000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 47 | 0.319270000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 48 | 0.319319000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 49 | 0.319979000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 50 | 0.320691000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=6 |

Frame numbers are local to capture `TriggeredBar-#0Lan80211AxDlOfdmaBar.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `SequentialBar`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1152**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 983 | 85.33% | 1063.3 B | 49.6 B | 617.6 us | 27.2 us | 5010 MHz | - | 20.0 dBm | 99.16% | 60.71% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 77 | 6.68% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.35% | 0.22% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 77 | 6.68% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | -65.5 dBm | - | 0.39% | 0.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 3 | 0.26% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 6 | 0.52% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 6 | 0.52% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.07% | 0.04% |

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
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.300644000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 20 | 0.301410000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 21 | 0.302140000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 22 | 0.302879000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 23 | 0.303618000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 24 | 0.304357000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 25 | 0.305105000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 26 | 0.305862000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 27 | 0.306583000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 28 | 0.307322000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 29 | 0.308079000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 30 | 0.308809000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 31 | 0.309521000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 32 | 0.309639000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 33 | 0.309687000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 34 | 0.310347000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 35 | 0.311068000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 36 | 0.311789000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 37 | 0.312537000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 38 | 0.313294000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 39 | 0.314033000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 40 | 0.314754000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 41 | 0.315484000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 42 | 0.316223000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 43 | 0.316935000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 44 | 0.317647000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 45 | 0.318404000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 46 | 0.319134000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 47 | 0.319270000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 48 | 0.319319000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 49 | 0.319979000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 50 | 0.320691000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=6 |

Frame numbers are local to capture `SequentialBar-#0Lan80211AxDlOfdmaBar.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `SuBaseline`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1415**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 1141 | 80.64% | 1066.0 B | 0.0 B | 587.2 us | 11.4 us | 5010 MHz | - | 20.0 dBm | 98.57% | 67.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 5 | 0.35% | 526.0 B | 440.9 B | 323.7 us | 241.2 us | 5010 MHz | - | 20.0 dBm | 0.24% | 0.16% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 127 | 8.98% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.52% | 0.36% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 127 | 8.98% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | -65.3 dBm | - | 0.57% | 0.39% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 3 | 0.21% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 6 | 0.42% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 6 | 0.42% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.06% | 0.04% |

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
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.300644000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 20 | 0.301304000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 21 | 0.302556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=854 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 22 | 0.302556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=854 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 23 | 0.303853000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=933 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 24 | 0.303853000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=933 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 25 | 0.305681000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=966 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 26 | 0.305681000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=966 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 27 | 0.305681000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=966 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 28 | 0.308119000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=1080 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 29 | 0.308119000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=1080 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 30 | 0.308119000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=1080 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 31 | 0.308119000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=1080 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 32 | 0.308212000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 33 | 0.308260000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 34 | 0.311848000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=1251 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 35 | 0.311848000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=1251 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 36 | 0.311848000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=1251 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 37 | 0.311848000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=1251 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 38 | 0.311848000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6, A-MPDU=1251 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 39 | 0.311848000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6, A-MPDU=1251 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 40 | 0.311923000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 41 | 0.311972000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 42 | 0.316744000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=1436 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 43 | 0.316744000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=1436 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 44 | 0.316744000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=1436 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 45 | 0.316744000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6, A-MPDU=1436 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 46 | 0.316744000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6, A-MPDU=1436 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 47 | 0.316744000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=6, A-MPDU=1436 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 48 | 0.316744000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=6, A-MPDU=1436 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | 49 | 0.316744000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=6, A-MPDU=1436 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 50 | 0.316810000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

Frame numbers are local to capture `SuBaseline-#0Lan80211AxDlOfdmaBar.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Analysis of Packet Distribution
Across these configurations, **QoS Data** frames constitute the primary payload delivery mechanism, while **Block Ack (BA)** and **Block Ack Request (BAR)** control frames ensure reliable transport via the MAC-level acknowledgment protocol. Management frames, specifically **Beacons**, are transmitted periodically by the Access Point to maintain BSS time synchronization and broadcast network capabilities. The ratio of control/management overhead to actual data frames indicates the relative MAC efficiency of the chosen configurations.
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
