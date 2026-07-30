# Walkthrough: HE Buffer Status Reports

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260730T105223Z`
- PCAP: `20260730T105223Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This example shows how an AP can use uplink Buffer Status Reports (BSRs) to allocate HE uplink OFDMA resources. With fresh reports, the AP's scheduled bytes follow the reported backlog across two traffic bursts; limiting report age to 10 ms changes that scheduling state and is therefore a useful stale-report control. The result is scoped to the five-seed campaign and the configured three-STA topology.

## [agent] Learning objectives and feature primer

BSR is queue information sent by a non-AP station to help its AP decide how much uplink capacity to grant. The AP schedules from the report state it has received, which may differ from the station's current queue. Three stations send UDP traffic through one HE AP while the workload turns on, off, and on again.

The decisive evidence is the AP-side reported/planned-byte pair keyed by `heUlTriggerDecisionId`. A report may exceed the bytes planned for one trigger because the scheduler is allocating one transmission opportunity, not copying the entire queue. BSR is scheduling state, not application goodput.

## [agent] Scenario description

[`HeBsrNetwork`](HeBsrNetwork.ned) extends the common single-BSS HE network with three stationary hosts and one AP. The [configuration](omnetpp.ini) selects 5 GHz, 20 MHz HE operation, QoS stations, `HeHcf`, and the AP-side `HeUlSchedulerBacklogBased` scheduler. Hosts generate 700-byte UDP packets every 0.35 ms.

`BurstyTraffic` enables ON periods 0.3–0.5 s and 0.65–0.95 s and extends `FullBsrAccounting`. `StaleBsr` keeps the same topology and traffic but sets `reportMaxAge = 10ms`. The analyzer's measurement window is 0.3–0.95 s, so it includes both bursts and the idle interval between them.

## [agent] Standards and INET model boundary

IEEE 802.11ax-2024 clause 26.5.5 describes buffer status report operation. Table 9-25 identifies the BSR control value, and Table 9-47 identifies BSRP as Trigger Type 4. These references define the standard mechanism; they do not prove the behavior of this INET run.

INET exposes the mechanism through AP-side report/planning telemetry and an HE uplink scheduler. The INI is requested behavior; the generated `.vec` results are derived measurements, and the PCAP is a direct observation of the MAC exchange. QoS Data counts alone do not prove BSR freshness or application delivery of the reported bytes.

## [agent] Evidence status

| Claim | Status | Script-generated evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| Reported backlog is joined to scheduler-consumed bytes by trigger decision. | `PASS` | Published scalar/vector executable check and joined decision table | Five runs, seeds 0–4, both configurations | Every retained decision has aligned trigger ID, reported bytes, and planned bytes. |
| The fresh/stale configurations expose different report and planning trajectories. | `PASS` | Published scalar/vector figure and joined decision evidence | Five runs, seeds 0–4, `StaleBsr` vs `BurstyTraffic` | This is a bounded model comparison, not a universal freshness-performance claim. |
| The captured AP-MAC exchange contains the relevant HE trigger/data activity. | `PASS` | Published PCAP statistics and frame exchange | Representative run 0 of each publication configuration | BSR is an A-Control scheduling input, not a standalone frame subtype. |

## [agent] Configuration matrix

| Configuration | Role | Causal delta | Runs/seeds | Expected invariant |
|---|---|---|---|---|
| `BurstyTraffic` | Fresh-report treatment | Extends `FullBsrAccounting`; two traffic bursts | 0–4 | Trigger decisions contain aligned reported and planned bytes. |
| `StaleBsr` | Freshness control | `reportMaxAge = 10ms` | 0–4 | Trigger decisions remain aligned while the AP applies the shorter freshness window. |

## [agent] Expected invariants and diagnostic map

| Invariant | Script-generated evidence | Failure symptom | First diagnostic |
|---|---|---|---|
| Each retained trigger decision has correlated report and plan values. | Joined decision evidence | Missing, misaligned, or non-finite decision fields | Check the six `heUlTriggerDecision*` vectors at the AP coordinator. |
| The traffic bursts are present in the analysis window. | Reported-backlog plot and AP telemetry | No report activity during an ON period | Check application start/stop times and `heUlBufferStatusReportedBytes`. |

## [agent] Reproduction

Run from the INET repository root:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py inspect bsr
MPLCONFIGDIR=/tmp/matplotlib python3 examples/ieee80211/analysis/wifi_analysis.py run bsr \
  --evidence both --runs 5 --session-id 20260730T130100Z
MPLCONFIGDIR=/tmp/matplotlib python3 examples/ieee80211/analysis/wifi_analysis.py report bsr \
  --session-id 20260730T130100Z
MPLCONFIGDIR=/tmp/matplotlib python3 examples/ieee80211/analysis/wifi_analysis.py publish bsr \
  --session-id 20260730T130100Z --update
```

The retained session is `20260730T130100Z`. It ran `BurstyTraffic` and `StaleBsr`, runs 0–4 with seed sets 0–4, from `examples/ieee80211ax/bsr`, and completed all 10 Cmdenv runs successfully. Every run recorded the trigger-decision projection vectors; run 0 of each configuration also recorded an AP-MAC PCAP. Results are under `examples/ieee80211ax/bsr/results/20260730T130100Z`.

## [agent] Scalar and vector analysis

<!-- BEGIN GENERATED ANALYSIS: scalar-vector -->
<!-- END GENERATED ANALYSIS: scalar-vector -->

The generated comparison shows the AP's reported backlog and planned bytes over the two configurations. The joined decision table keys both values by trigger ID, making each comparison a correlated scheduler-state observation. It does not measure application goodput or establish a universal benefit for a 10 ms freshness window.

<!-- BEGIN GENERATED: ieee80211-scalar-vector-bsr -->
### [script] Generated scalar/vector plot and table

![bsr scalar/vector analysis](results/20260730T105223Z/bsr-reported-vs-scheduled.png)

Figure provenance: [`results/20260730T105223Z/bsr-reported-vs-scheduled.png.json`](results/20260730T105223Z/bsr-reported-vs-scheduled.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / heUlBufferStatusReportedBytes:vector<br>vector / heUlBufferStatusScheduledBytes:vector
- Window / per-run aggregation / exclusions: [0.3, 0.95) s; timeline=representative run 0; event-driven step observations
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| Fresh BSR / reported backlog time weighted mean bytes | 58642.8 | 1061.25 |
| Stale BSR / reported backlog time weighted mean bytes | 68531.9 | 844.686 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **PASS** | Reported backlog is joined to scheduler-consumed backlog | Reported and planned backlog bytes are aligned to a trigger decision ID for every retained decision. |

#### [script] Joined BSR scheduler-decision evidence

| Config | Run | Time (s) / Trigger | Users | Reported bytes | Planned bytes |
|---|---:|---:|---:|---:|---:|
| BurstyTraffic | 0 | 0.10400000000000001 / 2 | 3 | 0 | 0 |
| BurstyTraffic | 0 | 0.20700000000000002 / 3 | 3 | 0 | 0 |
| BurstyTraffic | 0 | 0.3052344009660001 / 4 | 1 | 30640 | 30640 |
| BurstyTraffic | 0 | 0.30896820151800003 / 5 | 1 | 18384 | 18384 |
| BurstyTraffic | 0 | 0.3122950020700001 / 6 | 2 | 24512 | 13608 |
| BurstyTraffic | 0 | 0.31560920289800004 / 7 | 2 | 220608 | 13608 |
| BurstyTraffic | 0 | 0.31824860317400006 / 8 | 2 | 226704 | 13608 |
| BurstyTraffic | 0 | 0.3220076867650001 / 9 | 3 | 410544 | 27216 |
| BurstyTraffic | 0 | 0.32581548731700005 / 10 | 3 | 330880 | 27216 |
| BurstyTraffic | 0 | 0.32917668814500006 / 11 | 3 | 851728 | 30352 |
| BurstyTraffic | 0 | 0.3324804886970001 / 12 | 3 | 1011088 | 30352 |
| BurstyTraffic | 0 | 0.33723785532700007 / 13 | 3 | 1145904 | 30352 |

Showing 12 of 1947 joined decisions; the session-bound evidence ledger retains every observation.
<!-- END GENERATED: ieee80211-scalar-vector-bsr -->

## [agent] PCAP statistics

<!-- BEGIN GENERATED ANALYSIS: pcap -->
<!-- END GENERATED ANALYSIS: pcap -->

The generated packet view uses the AP MAC capture from representative run 0. It confirms the surrounding HE Trigger and HE-TB response activity. BSR is scheduling information carried in the MAC exchange, not necessarily a standalone frame subtype, so packet counts do not replace the AP decision telemetry.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260730T105223Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260730T105223Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260730T105223Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260730T105223Z.json` (SHA-256 `adfbd7122ba2dafbd77d2bead8a2e2ab044c8f633486ce66668b44f692e8aa85`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `BurstyTraffic` | `none (all decoded frames)` | 1998 | Data: QoS Data [HE-TB, HE-MCS 2, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (667), Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (395), Control: Ack (246) | 83.10% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `StaleBsr` | `none (all decoded frames)` | 2148 | Data: QoS Data [HE-TB, HE-MCS 2, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (688), Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (403), Control: Ack (257) | 87.27% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | BurstyTraffic produced protocol-visible wireless observations | 1998 AP/global transmission observations |
| **PASS** | StaleBsr produced protocol-visible wireless observations | 2148 AP/global transmission observations |

### [script] Configuration: `BurstyTraffic`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1998**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 86 | 4.30% | 851.0 B | 9.2 B | 473.9 us | 16.3 us | 5010 MHz | -72.0 dBm | - | 4.90% | 4.08% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 395 | 19.77% | 1312.2 B | 311.9 B | 753.8 us | 170.6 us | 5010 MHz | -72.0 dBm | - | 35.83% | 29.77% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31d62e" /></svg> | Data: QoS Data [HE-TB, HE-MCS 2, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 667 | 33.38% | 768.0 B | 2.0 B | 660.5 us | 19.7 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 53.02% | 44.06% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24ae34" /></svg> | Data: QoS Data [HE-TB, HE-MCS 2, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 10 | 0.50% | 766.8 B | 1.6 B | 286.8 us | 15.0 us | 5010 MHz | -75.0 dBm | - | 0.35% | 0.29% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 9 | 0.45% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.43% | 0.36% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#166d09" /></svg> | Data: QoS Null [HE-TB, HE-MCS 2, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 2 | 0.10% | 34.0 B | 0.0 B | 156.9 us | 0.0 us | 5002 MHz | -75.0 dBm | - | 0.04% | 0.03% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#13560b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 2, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 141 | 7.06% | 34.0 B | 0.0 B | 156.9 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 2.66% | 2.21% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 186 | 9.31% | 45.4 B | 4.3 B | 35.1 us | 1.4 us | 5010 MHz | - | 10.0 dBm | 0.79% | 0.65% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 33 | 1.65% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -72.0 dBm | - | 0.11% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 211 | 10.56% | 67.2 B | 31.5 B | 42.4 us | 10.5 us | 5010 MHz | - | 10.0 dBm | 1.08% | 0.89% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 246 | 12.31% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.73% | 0.61% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.30% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.30% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.05% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.003064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=226 | Responds without MAC payload while preserving QoS control information. |
| 3 | 0.003064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=230 | Responds without MAC payload while preserving QoS control information. |
| 4 | 0.003064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=234 | Responds without MAC payload while preserving QoS control information. |
| 5 | 0.003133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 6 | 0.104048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 7 | 0.106064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=418 | Responds without MAC payload while preserving QoS control information. |
| 8 | 0.106064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=422 | Responds without MAC payload while preserving QoS control information. |
| 9 | 0.106064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=426 | Responds without MAC payload while preserving QoS control information. |
| 10 | 0.106133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 11 | 0.207048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 12 | 0.209064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=610 | Responds without MAC payload while preserving QoS control information. |
| 13 | 0.209064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=614 | Responds without MAC payload while preserving QoS control information. |
| 14 | 0.209064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=618 | Responds without MAC payload while preserving QoS control information. |
| 15 | 0.209133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 16 | 0.300484000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |

Frame numbers are local to capture `BurstyTraffic-#0HeBsrNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `StaleBsr`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2148**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 79 | 3.68% | 848.7 B | 16.4 B | 473.8 us | 19.2 us | 5010 MHz | -72.0 dBm | - | 4.29% | 3.74% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 403 | 18.76% | 1299.8 B | 330.2 B | 747.0 us | 180.6 us | 5010 MHz | -72.0 dBm | - | 34.50% | 30.11% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31d62e" /></svg> | Data: QoS Data [HE-TB, HE-MCS 2, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 688 | 32.03% | 768.0 B | 2.0 B | 660.5 us | 19.7 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 52.07% | 45.44% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 72 | 3.35% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 3.29% | 2.87% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#166d09" /></svg> | Data: QoS Null [HE-TB, HE-MCS 2, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 1 | 0.05% | 34.0 B | 0.0 B | 156.9 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 0.02% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#13560b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 2, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 163 | 7.59% | 34.0 B | 0.0 B | 156.9 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 2.93% | 2.56% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 201 | 9.36% | 49.1 B | 8.9 B | 36.4 us | 3.0 us | 5010 MHz | - | 10.0 dBm | 0.84% | 0.73% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 41 | 1.91% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -72.0 dBm | - | 0.13% | 0.11% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 231 | 10.75% | 69.9 B | 31.8 B | 43.3 us | 10.6 us | 5010 MHz | - | 10.0 dBm | 1.15% | 1.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 257 | 11.96% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.73% | 0.63% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.28% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.28% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.05% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.003064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=226 | Responds without MAC payload while preserving QoS control information. |
| 3 | 0.003064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=230 | Responds without MAC payload while preserving QoS control information. |
| 4 | 0.003064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=234 | Responds without MAC payload while preserving QoS control information. |
| 5 | 0.003133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 6 | 0.014048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 7 | 0.016064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=418 | Responds without MAC payload while preserving QoS control information. |
| 8 | 0.016064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=422 | Responds without MAC payload while preserving QoS control information. |
| 9 | 0.016064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=426 | Responds without MAC payload while preserving QoS control information. |
| 10 | 0.016133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 11 | 0.027048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 12 | 0.029064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=610 | Responds without MAC payload while preserving QoS control information. |
| 13 | 0.029064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=614 | Responds without MAC payload while preserving QoS control information. |
| 14 | 0.029064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=618 | Responds without MAC payload while preserving QoS control information. |
| 15 | 0.029133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 16 | 0.040048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |

Frame numbers are local to capture `StaleBsr-#0HeBsrNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Analysis of Packet Distribution
The scheduled conditions contain the expected Trigger/response activity, but a BSR is an A-Control scheduling input rather than a frame subtype. IEEE Std 802.11-2024 Clause 26.5.5 requires the report contents and capability conditions; use the AP-reported and scheduled-backlog telemetry documented above. QoS Data counts are not evidence that a BSR was fresh or that the reported bytes were delivered.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->

## [agent] Frame exchange analysis

<!-- BEGIN GENERATED ANALYSIS: frame-exchange -->
<!-- END GENERATED ANALYSIS: frame-exchange -->

The generated timeline begins with AP Trigger frames followed by HE-TB station responses and acknowledgments. This is the protocol exchange in which the AP applies its uplink schedule; the trigger-decision vectors provide the corresponding BSR values. The timeline is local to the representative capture and is not an event-log reconstruction.

## [agent] Cross-layer findings and verdict

The two configurations hold topology and offered traffic constant and change only the AP's report freshness policy. Across five seeds, every retained trigger decision has an aligned ID, reported backlog, and planned bytes; this supports a scoped `PASS` for BSR accounting and the configured freshness comparison. The AP-MAC capture independently supports a `PASS` for protocol-visible HE trigger/uplink activity.

## [agent] Limitations and inconclusive claims

- The comparison is limited to this topology, workload, five seeds, and 10 ms freshness setting; it is not a general performance claim. Broader claims require additional freshness windows and matched workloads.
- PCAP does not expose BSR as an application packet or necessarily as a standalone frame subtype. Use AP decision telemetry and the decoded trigger/data exchange together.
