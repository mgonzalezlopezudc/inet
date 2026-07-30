# Walkthrough: HE Buffer Status Reports

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260730T165757Z`
- PCAP: `20260730T165757Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `20260730T165757Z`.

This walkthrough asks whether the AP's BSR report state is joined correctly to
uplink scheduling decisions under four configured reporting/traffic cases.
The fresh five-seed session supports a scoped `PASS` for that accounting join
and for the observed HE trigger exchanges; it does not establish a general
throughput or freshness benefit.

## [agent] Learning objectives and feature primer

BSR is queue information sent by a non-AP station so its AP can choose uplink
resources. The AP acts on the report it has received, not necessarily the
station's instantaneous queue. The learning question is whether the report
state is carried into the AP's trigger decision under implicit reporting,
bursty traffic, and a short report-age limit.

The decisive evidence is the AP-side reported/planned-byte pair keyed by
`heUlTriggerDecisionId`. Reported bytes can exceed planned bytes because one
trigger allocates a transmission opportunity rather than draining the whole
queue. BSR is scheduling state, not application goodput.

## [agent] Scenario description

[`HeBsrNetwork`](HeBsrNetwork.ned) contains one HE AP and three stationary
stations. The [configuration](omnetpp.ini) selects 5 GHz, 20 MHz HE operation,
QoS stations, `HeHcf`, and the AP-side `HeUlSchedulerBacklogBased` scheduler.
Each station offers 700-byte UDP packets every 2 ms during active periods. A
short warm-up precedes the 0.3–0.95 s measurement window.

`FreshBsr` is the default. `LongTriggerCheck` sets
`ulTriggerCheckInterval = 0.25s` and `reportMaxAge = 150ms`, so the AP checks
the cached implicit reports less often while still requiring reasonably fresh
reports. `BurstyTraffic` uses ON periods 0.3–0.5 s and 0.65–0.95 s.
`StaleBsr` sets `reportMaxAge = 10ms` while keeping the topology and offered
traffic unchanged.

## [agent] Standards and INET model boundary

IEEE Std 802.11-2024 Clause 26.5.5 describes buffer status reporting and
identifies BSRP as Trigger Type 4. The generated Trigger tables show decoded
capture fields, but a Trigger frame alone does not prove that the AP used a
particular report correctly.

INET exposes the mechanism through AP-side report/planning telemetry and the
HE uplink scheduler. INI parameters are requested behavior, vectors are
script-derived measurements, and the AP PCAP directly observes the surrounding
MAC exchange. QoS Data counts alone do not prove BSR freshness or delivery of
reported bytes.

## [agent] Evidence status

| Claim | Status | Script-generated evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| Reported backlog is joined to a scheduler decision. | `PASS` | Scalar/vector executable check and joined decision tables | Five runs, seeds 0–4; scheduled configurations | The check covers retained decisions, not application delivery. |
| Implicit reporting is visible at the AP. | `PASS` | Scalar/vector plot and representative PCAP | Five runs; `LongTriggerCheck` | The AP reports implicit BSR activity and the representative run contains scheduled UL activity. |
| The exchange contains decoded HE trigger/TB activity. | `PASS` | PCAP statistics, trigger allocations, and frame timeline | Representative run 0; all four configurations | This is surrounding MAC evidence, not a standalone BSR packet. |

## [agent] Configuration matrix

| Configuration | Role | Causal delta | Runs/seeds | Expected invariant |
|---|---|---|---|---|
| `LongTriggerCheck` | Longer trigger-check interval and bounded report age | `ulTriggerCheckInterval = 0.25s`; `reportMaxAge = 150ms` | 0–4 | AP report telemetry is present and eligible reports can feed scheduled decisions. |
| `FreshBsr` | Fresh-report treatment | No assignment beyond `General` | 0–4 | Trigger decisions contain aligned reported and planned bytes. |
| `BurstyTraffic` | Bursty workload | Two ON/OFF traffic bursts | 0–4 | Trigger decisions contain aligned reported and planned bytes under bursty load. |
| `StaleBsr` | Freshness control | `reportMaxAge = 10ms` | 0–4 | Trigger decisions remain aligned while the AP applies the shorter freshness window. |

## [agent] Expected invariants and diagnostic map

| Invariant | Script-generated evidence | Failure symptom | First diagnostic |
|---|---|---|---|
| Each retained decision has report and plan values joined by decision ID. | Joined decision evidence | Missing or misaligned fields | Check the `heUlTriggerDecision*` vectors at the AP coordinator. |
| The configured bursts produce report activity in the window. | Reported-backlog plot and AP telemetry | No report activity during an ON period | Check application start/stop times and `heUlBufferStatusReportedBytes`. |

## [agent] Reproduction

Run from the INET repository root:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py inspect bsr
MPLCONFIGDIR=/tmp/matplotlib python3 examples/ieee80211/analysis/wifi_analysis.py run bsr \
  --evidence both --runs 5 --session-id 20260730T165757Z
MPLCONFIGDIR=/tmp/matplotlib python3 examples/ieee80211/analysis/wifi_analysis.py report bsr \
  --session-id 20260730T165757Z
MPLCONFIGDIR=/tmp/matplotlib python3 examples/ieee80211/analysis/wifi_analysis.py publish bsr \
  --session-id 20260730T165757Z --update
```

Session `20260730T165757Z` contains five runs for each configuration, scalar/
vector evidence for all runs, and a run-0 AP-MAC PCAP for each configuration.
Results are under `examples/ieee80211ax/bsr/results/20260730T165757Z`.
For example, the retained artifacts include
`results/20260730T165757Z/LongTriggerCheck/LongTriggerCheck-#0.sca` and
`results/20260730T165757Z/LongTriggerCheck/LongTriggerCheck-#0.vec`.

## [agent] Scalar and vector analysis

<!-- BEGIN GENERATED ANALYSIS: scalar-vector -->
<!-- END GENERATED ANALYSIS: scalar-vector -->

The generated comparison answers what backlog the AP observes and whether
reported/planned bytes are aligned for retained scheduled decisions. The
run-level summaries quantify this workload and seed set; they do not measure
application goodput or establish a universal benefit for a 10 ms window.

<!-- BEGIN GENERATED: ieee80211-scalar-vector-bsr -->
### [script] Generated scalar/vector plot and table

![bsr scalar/vector analysis](results/20260730T165757Z/bsr-reported-vs-scheduled.png)

Figure provenance: [`results/20260730T165757Z/bsr-reported-vs-scheduled.png.json`](results/20260730T165757Z/bsr-reported-vs-scheduled.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / heUlBufferStatusReportedBytes:vector<br>vector / heUlBufferStatusScheduledBytes:vector
- Window / per-run aggregation / exclusions: [0.3, 0.95) s; timeline=representative run 0; event-driven step observations
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| Bursty traffic / reported backlog time weighted mean bytes | 1011.35 | 258.153 |
| Fresh BSR / reported backlog time weighted mean bytes | 1474.9 | 333.375 |
| Long trigger check / reported backlog time weighted mean bytes | 1119.15 | 252.849 |
| Stale BSR / reported backlog time weighted mean bytes | 1518.6 | 98.1539 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.
<!-- END GENERATED: ieee80211-scalar-vector-bsr -->

## [agent] PCAP statistics

<!-- BEGIN GENERATED ANALYSIS: pcap -->
<!-- END GENERATED ANALYSIS: pcap -->

The generated packet view uses the AP-MAC capture from representative run 0.
Its compact summary establishes that all four configurations produced recorded
wireless activity; the decoded Trigger allocations and timeline show the
Trigger-to-HE-TB pattern. These are mechanism observations, while AP decision
telemetry remains authoritative for reported and planned BSR bytes.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260730T165757Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260730T165757Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260730T165757Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260730T165757Z.json` (SHA-256 `7b02ad3e96b526262a8e0197e0f3b36c465683aece60d40bc613ed37a3d701b2`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `FreshBsr` | `none (all decoded frames)` | 1620 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (434), Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (256), Control: Ack (230) | 61.52% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `StaleBsr` | `none (all decoded frames)` | 1905 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (402), Data: QoS Data [HE-TB, HE-MCS 2, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (246), Control: Block Ack (BA) (239) | 72.20% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `LongTriggerCheck` | `none (all decoded frames)` | 1494 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (605), Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (353), Control: Ack (332) | 54.45% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `BurstyTraffic` | `none (all decoded frames)` | 1311 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (380), Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (233), Control: Ack (208) | 47.95% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | FreshBsr produced protocol-visible wireless observations | 1620 AP/global transmission observations |
| **PASS** | StaleBsr produced protocol-visible wireless observations | 1905 AP/global transmission observations |
| **PASS** | LongTriggerCheck produced protocol-visible wireless observations | 1494 AP/global transmission observations |
| **PASS** | BurstyTraffic produced protocol-visible wireless observations | 1311 AP/global transmission observations |

### [script] Configuration: `FreshBsr`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1620**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 256 | 15.80% | 652.0 B | 284.9 B | 366.5 us | 153.1 us | 5010 MHz | -72.0 dBm | - | 15.25% | 9.38% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 434 | 26.79% | 1187.9 B | 460.7 B | 685.8 us | 252.0 us | 5010 MHz | -72.0 dBm | - | 48.38% | 29.76% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31d62e" /></svg> | Data: QoS Data [HE-TB, HE-MCS 2, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 104 | 6.42% | 768.2 B | 2.0 B | 662.0 us | 19.6 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 11.19% | 6.89% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24ae34" /></svg> | Data: QoS Data [HE-TB, HE-MCS 2, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 78 | 4.81% | 767.1 B | 1.8 B | 289.9 us | 16.9 us | 5010 MHz | -75.0 dBm | - | 3.68% | 2.26% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#37b52c" /></svg> | Data: QoS Data [HE-TB, HE-MCS 2, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 76 | 4.69% | 770.0 B | 0.0 B | 1404.9 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -75.0 dBm | - | 17.36% | 10.68% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 9 | 0.56% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.58% | 0.36% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#13560b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 2, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 3 | 0.19% | 34.0 B | 0.0 B | 156.9 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 0.08% | 0.05% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 144 | 8.89% | 37.2 B | 6.9 B | 32.4 us | 2.3 us | 5010 MHz | - | 10.0 dBm | 0.76% | 0.47% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 75 | 4.63% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -72.0 dBm | - | 0.34% | 0.21% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 199 | 12.28% | 67.1 B | 49.6 B | 42.4 us | 16.5 us | 5010 MHz | - | 10.0 dBm | 1.37% | 0.84% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 230 | 14.20% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.92% | 0.57% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.37% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.37% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.07% | 0.04% |

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
| 11 | 0.200484000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 12 | 0.201077000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 13 | 0.201682000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 14 | 0.201730000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 16 | 0.201826000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 17 | 0.202345000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |

Frame numbers are local to capture `FreshBsr-#0HeBsrNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded HE Trigger user allocations

| Frame | Simulation time (s) | Trigger type | Ordered user allocations |
|---:|---:|---:|---|
| 1 | 0.001048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 6 | 0.104048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 32 | 0.302048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 35 | 0.305048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 86 | 0.328043000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 100 | 0.332949000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=38, MCS=2, target RSSI=35 |
| 112 | 0.337406000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 133 | 0.346985000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 156 | 0.356624000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 161 | 0.359889000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 168 | 0.363449000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 173 | 0.366678000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 213 | 0.383884000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 221 | 0.387593000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 242 | 0.399059000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 256 | 0.404010000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 269 | 0.409820000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 277 | 0.412973000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35 |
| 280 | 0.415036000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35 |
| 316 | 0.431617000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 334 | 0.439069000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 337 | 0.441036000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 352 | 0.449062000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 364 | 0.453494000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 379 | 0.458848000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 383 | 0.461513000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35 |
| 392 | 0.465672000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 400 | 0.469822000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35 |
| 409 | 0.473121000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 423 | 0.478772000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 455 | 0.492252000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 476 | 0.503077000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 497 | 0.515116000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 505 | 0.519266000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 513 | 0.523882000 | 0 | #0: AID=1, RU=61, MCS=2, target RSSI=35 |
| 525 | 0.528034000 | 0 | #0: AID=1, RU=61, MCS=2, target RSSI=35 |
| 540 | 0.533034000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 552 | 0.537161000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 578 | 0.545354000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 582 | 0.548036000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 590 | 0.552652000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 598 | 0.556792000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 606 | 0.560121000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 612 | 0.563309000 | 0 | #0: AID=3, RU=53, MCS=2, target RSSI=35 |
| 615 | 0.566036000 | 0 | #0: AID=3, RU=53, MCS=2, target RSSI=35 |
| 623 | 0.569655000 | 0 | #0: AID=3, RU=53, MCS=2, target RSSI=35 |
| 637 | 0.577346000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 647 | 0.581536000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 656 | 0.586036000 | 0 | #0: AID=1, RU=61, MCS=2, target RSSI=35 |
| 673 | 0.596454000 | 0 | #0: AID=1, RU=61, MCS=2, target RSSI=35 |
| 689 | 0.602766000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 702 | 0.608402000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=38, MCS=2, target RSSI=35 |
| 706 | 0.610096000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=54, MCS=2, target RSSI=35 |
| 716 | 0.614270000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=38, MCS=2, target RSSI=35 |
| 724 | 0.618038000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=54, MCS=2, target RSSI=35 |
| 733 | 0.621665000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=54, MCS=2, target RSSI=35 |
| 750 | 0.627793000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 761 | 0.632410000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 766 | 0.634113000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 784 | 0.640850000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35 |
| 793 | 0.644593000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 806 | 0.649848000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=54, MCS=2, target RSSI=35 |
| 816 | 0.654023000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=54, MCS=2, target RSSI=35 |
| 830 | 0.660025000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35; #2: AID=2, RU=54, MCS=2, target RSSI=35 |
| 837 | 0.663223000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=38, MCS=2, target RSSI=35; #2: AID=3, RU=54, MCS=2, target RSSI=35 |
| 846 | 0.666611000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 857 | 0.670452000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=54, MCS=2, target RSSI=35 |
| 864 | 0.673258000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 876 | 0.676600000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=38, MCS=2, target RSSI=35; #2: AID=3, RU=39, MCS=2, target RSSI=35 |
| 884 | 0.680256000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=1, RU=4, MCS=2, target RSSI=35; #2: AID=3, RU=54, MCS=2, target RSSI=35 |
| 899 | 0.684301000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 910 | 0.688347000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=2, RU=4, MCS=2, target RSSI=35; #2: AID=3, RU=54, MCS=2, target RSSI=35 |
| 917 | 0.690061000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=4, MCS=2, target RSSI=35; #2: AID=2, RU=54, MCS=2, target RSSI=35 |
| 933 | 0.694924000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 957 | 0.702900000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 973 | 0.709642000 | 0 | #0: AID=1, RU=61, MCS=2, target RSSI=35 |
| 991 | 0.717164000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35 |
| 1007 | 0.724036000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1018 | 0.729565000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35; #2: AID=2, RU=54, MCS=2, target RSSI=35 |
| 1029 | 0.733774000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 1041 | 0.737960000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 1047 | 0.741184000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 1067 | 0.748828000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35 |
| 1070 | 0.751036000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35 |
| 1086 | 0.759043000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 1089 | 0.761548000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 1102 | 0.766089000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 1106 | 0.768036000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 1125 | 0.774845000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 1140 | 0.780123000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1147 | 0.783441000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1150 | 0.786036000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1154 | 0.790899000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1158 | 0.793036000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1162 | 0.795728000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 1165 | 0.799873000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 1179 | 0.803755000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1183 | 0.806429000 | 0 | #0: AID=3, RU=53, MCS=2, target RSSI=35 |
| 1188 | 0.809043000 | 0 | #0: AID=3, RU=53, MCS=2, target RSSI=35 |
| 1197 | 0.813202000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35 |

Showing the first 100 of 144 decoded Trigger frames; the script-owned packet metrics JSON preserves every row.

### [script] Configuration: `StaleBsr`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1905**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 234 | 12.28% | 606.4 B | 310.2 B | 341.9 us | 166.3 us | 5010 MHz | -72.0 dBm | - | 11.08% | 8.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 402 | 21.10% | 1188.1 B | 486.0 B | 685.9 us | 265.9 us | 5010 MHz | -72.0 dBm | - | 38.19% | 27.57% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31d62e" /></svg> | Data: QoS Data [HE-TB, HE-MCS 2, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 246 | 12.91% | 768.0 B | 2.0 B | 661.0 us | 19.7 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 22.52% | 16.26% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24ae34" /></svg> | Data: QoS Data [HE-TB, HE-MCS 2, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 59 | 3.10% | 766.8 B | 1.6 B | 287.0 us | 15.1 us | 5010 MHz | -75.0 dBm | - | 2.34% | 1.69% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#37b52c" /></svg> | Data: QoS Data [HE-TB, HE-MCS 2, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 80 | 4.20% | 770.0 B | 0.0 B | 1404.9 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -75.0 dBm | - | 15.57% | 11.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 102 | 5.35% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 5.63% | 4.07% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#166d09" /></svg> | Data: QoS Null [HE-TB, HE-MCS 2, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 44 | 2.31% | 34.0 B | 0.0 B | 156.9 us | 0.0 us | 5002 MHz | -75.0 dBm | - | 0.96% | 0.69% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#13560b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 2, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 14 | 0.73% | 34.0 B | 0.0 B | 156.9 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 0.30% | 0.22% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 191 | 10.03% | 45.8 B | 13.4 B | 35.3 us | 4.5 us | 5010 MHz | - | 10.0 dBm | 0.93% | 0.67% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 57 | 2.99% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -72.0 dBm | - | 0.22% | 0.16% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#946833" /></svg> | Control: Block Ack Request (BAR) [HE-TB, HE-MCS 2, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 1 | 0.05% | 24.0 B | 0.0 B | 56.1 us | 0.0 us | 5005 MHz | -75.0 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 239 | 12.55% | 66.7 B | 42.6 B | 42.2 us | 14.2 us | 5010 MHz | - | 10.0 dBm | 1.40% | 1.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 224 | 11.76% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.77% | 0.55% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.31% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.31% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.06% | 0.04% |

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

#### [script] Decoded HE Trigger user allocations

| Frame | Simulation time (s) | Trigger type | Ordered user allocations |
|---:|---:|---:|---|
| 1 | 0.001048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 6 | 0.014048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 11 | 0.027048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 16 | 0.040048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 21 | 0.053048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 26 | 0.066048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 31 | 0.079048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 36 | 0.092048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 41 | 0.105048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 46 | 0.118048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 51 | 0.131048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 56 | 0.144048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 61 | 0.157048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 66 | 0.170048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 71 | 0.183048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 76 | 0.196048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 101 | 0.212048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 106 | 0.225048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 111 | 0.238048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 116 | 0.251048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 121 | 0.264048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 126 | 0.277048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 131 | 0.290048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 138 | 0.303048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 177 | 0.321301000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 187 | 0.325221000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=4, MCS=2, target RSSI=35; #2: AID=2, RU=54, MCS=2, target RSSI=35 |
| 197 | 0.330336000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=2, RU=4, MCS=2, target RSSI=35; #2: AID=3, RU=54, MCS=2, target RSSI=35 |
| 209 | 0.334502000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 216 | 0.337821000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35 |
| 225 | 0.341148000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 229 | 0.343763000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35; #1: AID=1, RU=38, MCS=2, target RSSI=35 |
| 236 | 0.347212000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35 |
| 251 | 0.353867000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 259 | 0.357502000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 269 | 0.361138000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 285 | 0.367382000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35; #1: AID=2, RU=38, MCS=2, target RSSI=35; #2: AID=3, RU=39, MCS=2, target RSSI=35 |
| 300 | 0.374380000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 310 | 0.378351000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 315 | 0.380054000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 326 | 0.384229000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 336 | 0.387969000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 351 | 0.392799000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35 |
| 360 | 0.396962000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 373 | 0.400841000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 391 | 0.407325000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 403 | 0.412666000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=38, MCS=2, target RSSI=35; #2: AID=3, RU=54, MCS=2, target RSSI=35 |
| 412 | 0.416339000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=38, MCS=2, target RSSI=35 |
| 427 | 0.422158000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 437 | 0.425368000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35; #1: AID=1, RU=54, MCS=2, target RSSI=35 |
| 441 | 0.427071000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35; #1: AID=1, RU=54, MCS=2, target RSSI=35 |
| 459 | 0.434559000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35 |
| 475 | 0.439707000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=54, MCS=2, target RSSI=35 |
| 496 | 0.450282000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35 |
| 509 | 0.455976000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 525 | 0.461487000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35; #1: AID=1, RU=38, MCS=2, target RSSI=35; #2: AID=2, RU=39, MCS=2, target RSSI=35 |
| 535 | 0.465669000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 553 | 0.471528000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 564 | 0.475301000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 573 | 0.479919000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=0, RU=4, MCS=2, target RSSI=35; #2: AID=3, RU=54, MCS=2, target RSSI=35 |
| 584 | 0.484109000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35 |
| 593 | 0.487422000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35; #1: AID=2, RU=54, MCS=2, target RSSI=35 |
| 604 | 0.491701000 | 0 | #0: AID=1, RU=61, MCS=2, target RSSI=35 |
| 616 | 0.495883000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35; #1: AID=1, RU=54, MCS=2, target RSSI=35 |
| 625 | 0.500058000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 632 | 0.503253000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35 |
| 643 | 0.507136000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35; #1: AID=1, RU=38, MCS=2, target RSSI=35; #2: AID=3, RU=39, MCS=2, target RSSI=35 |
| 653 | 0.511331000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35; #2: AID=2, RU=54, MCS=2, target RSSI=35 |
| 659 | 0.513041000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 685 | 0.522999000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=54, MCS=2, target RSSI=35 |
| 697 | 0.526861000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 707 | 0.530064000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 728 | 0.538743000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 735 | 0.542483000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 749 | 0.548726000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=2, RU=4, MCS=2, target RSSI=35; #2: AID=3, RU=54, MCS=2, target RSSI=35 |
| 763 | 0.553510000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 771 | 0.558014000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 784 | 0.562177000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 791 | 0.564908000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 794 | 0.567036000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 805 | 0.569967000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 813 | 0.572706000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 816 | 0.575036000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 825 | 0.577839000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 836 | 0.581998000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 859 | 0.591141000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 867 | 0.594509000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 873 | 0.597811000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=1, RU=4, MCS=2, target RSSI=35; #2: AID=3, RU=54, MCS=2, target RSSI=35 |
| 881 | 0.601599000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=1, RU=4, MCS=2, target RSSI=35; #2: AID=3, RU=54, MCS=2, target RSSI=35 |
| 891 | 0.605251000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35 |
| 904 | 0.609192000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 917 | 0.615685000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35; #1: AID=1, RU=38, MCS=2, target RSSI=35; #2: AID=3, RU=39, MCS=2, target RSSI=35 |
| 928 | 0.619473000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=54, MCS=2, target RSSI=35 |
| 938 | 0.622787000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 949 | 0.625664000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 966 | 0.632694000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 980 | 0.638534000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35; #1: AID=1, RU=38, MCS=2, target RSSI=35; #2: AID=3, RU=39, MCS=2, target RSSI=35 |
| 989 | 0.642203000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=54, MCS=2, target RSSI=35 |
| 999 | 0.646387000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 1004 | 0.648090000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 1024 | 0.655652000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=38, MCS=2, target RSSI=35; #2: AID=3, RU=39, MCS=2, target RSSI=35 |

Showing the first 100 of 191 decoded Trigger frames; the script-owned packet metrics JSON preserves every row.

### [script] Configuration: `LongTriggerCheck`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1494**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 353 | 23.63% | 635.9 B | 286.0 B | 359.5 us | 150.5 us | 5010 MHz | -72.0 dBm | - | 23.31% | 12.69% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 605 | 40.50% | 1138.6 B | 490.1 B | 658.8 us | 268.1 us | 5010 MHz | -72.0 dBm | - | 73.21% | 39.86% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#37b52c" /></svg> | Data: QoS Data [HE-TB, HE-MCS 2, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 1 | 0.07% | 770.0 B | 0.0 B | 1404.9 us | 0.0 us | 5003 MHz | -75.0 dBm | - | 0.26% | 0.14% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 1 | 0.07% | 34.0 B | 0.0 B | 31.3 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.01% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 99 | 6.63% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -72.0 dBm | - | 0.51% | 0.28% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 91 | 6.09% | 138.8 B | 37.5 B | 66.3 us | 12.5 us | 5010 MHz | - | 10.0 dBm | 1.11% | 0.60% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 332 | 22.22% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 1.50% | 0.82% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.40% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.03% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.40% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.08% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 976 | 0.750603000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=107, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 977 | 0.750651000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 978 | 0.750721000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 979 | 0.752241000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Data / HE-TB, HE-MCS 2, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=103, frag=0, more-frag=0, TID=6, A-MPDU=32800 | Carries protocol-visible MAC payload in the representative exchange. |
| 980 | 0.752302000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 981 | 0.752897000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=107, frag=1, more-frag=0, TID=6, A-MPDU=32883 | Carries protocol-visible MAC payload in the representative exchange. |
| 982 | 0.752897000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=108, frag=0, more-frag=0, TID=6, A-MPDU=32883 | Carries protocol-visible MAC payload in the representative exchange. |
| 983 | 0.753797000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=109, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 984 | 0.754725000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=116, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 985 | 0.754773000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 986 | 0.755305000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=116, frag=1, more-frag=0, TID=6, A-MPDU=33022 | Carries protocol-visible MAC payload in the representative exchange. |
| 987 | 0.755305000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=117, frag=0, more-frag=0, TID=6, A-MPDU=33022 | Carries protocol-visible MAC payload in the representative exchange. |
| 988 | 0.756242000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=109, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 989 | 0.756290000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 990 | 0.756406000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=109, frag=1, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 991 | 0.756454000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack Request (BAR) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |

Frame numbers are local to capture `LongTriggerCheck-#0HeBsrNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded HE Trigger user allocations

| Frame | Simulation time (s) | Trigger type | Ordered user allocations |
|---:|---:|---:|---|
| 978 | 0.750721000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |

### [script] Configuration: `BurstyTraffic`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1311**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 233 | 17.77% | 642.1 B | 283.0 B | 360.5 us | 150.9 us | 5010 MHz | -72.0 dBm | - | 17.52% | 8.40% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 380 | 28.99% | 1132.2 B | 521.2 B | 655.3 us | 285.1 us | 5010 MHz | -72.0 dBm | - | 51.93% | 24.90% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31d62e" /></svg> | Data: QoS Data [HE-TB, HE-MCS 2, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 40 | 3.05% | 768.1 B | 2.0 B | 661.5 us | 19.6 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 5.52% | 2.65% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24ae34" /></svg> | Data: QoS Data [HE-TB, HE-MCS 2, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 63 | 4.81% | 767.0 B | 1.7 B | 288.9 us | 16.3 us | 5010 MHz | -75.0 dBm | - | 3.79% | 1.82% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#37b52c" /></svg> | Data: QoS Data [HE-TB, HE-MCS 2, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 57 | 4.35% | 770.0 B | 0.0 B | 1404.9 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -75.0 dBm | - | 16.70% | 8.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 12 | 0.92% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 1.00% | 0.48% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 100 | 7.63% | 36.6 B | 7.9 B | 32.2 us | 2.6 us | 5010 MHz | - | 10.0 dBm | 0.67% | 0.32% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 59 | 4.50% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -72.0 dBm | - | 0.34% | 0.17% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 147 | 11.21% | 71.4 B | 53.0 B | 43.8 us | 17.7 us | 5010 MHz | - | 10.0 dBm | 1.34% | 0.64% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 208 | 15.87% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 1.07% | 0.51% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.46% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.03% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.46% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.09% | 0.04% |

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

#### [script] Decoded HE Trigger user allocations

| Frame | Simulation time (s) | Trigger type | Ordered user allocations |
|---:|---:|---:|---|
| 1 | 0.001048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 6 | 0.104048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 11 | 0.207048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 100 | 0.335408000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35 |
| 117 | 0.341279000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 120 | 0.343036000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 127 | 0.346637000 | 0 | #0: AID=1, RU=61, MCS=2, target RSSI=35 |
| 156 | 0.356002000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=54, MCS=2, target RSSI=35 |
| 164 | 0.358781000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35 |
| 179 | 0.365725000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 219 | 0.381419000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 233 | 0.385304000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 238 | 0.387653000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 243 | 0.391723000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 259 | 0.400365000 | 0 | #0: AID=3, RU=53, MCS=2, target RSSI=35 |
| 263 | 0.402052000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 274 | 0.406060000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 283 | 0.410020000 | 0 | #0: AID=3, RU=53, MCS=2, target RSSI=35 |
| 298 | 0.415529000 | 0 | #0: AID=3, RU=53, MCS=2, target RSSI=35 |
| 329 | 0.429629000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 342 | 0.436357000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 345 | 0.438044000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 353 | 0.441796000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 361 | 0.445964000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 389 | 0.457726000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 396 | 0.461328000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 404 | 0.465486000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 417 | 0.469416000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 425 | 0.473566000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 443 | 0.483114000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 446 | 0.485036000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 454 | 0.489223000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 467 | 0.495544000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35 |
| 494 | 0.512036000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 497 | 0.514036000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 500 | 0.516036000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 503 | 0.518036000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 506 | 0.520036000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 509 | 0.522036000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 512 | 0.524036000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 556 | 0.646048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 619 | 0.674228000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=38, MCS=2, target RSSI=35; #2: AID=3, RU=39, MCS=2, target RSSI=35 |
| 635 | 0.680210000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35; #2: AID=1, RU=54, MCS=2, target RSSI=35 |
| 641 | 0.682036000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 661 | 0.688781000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 677 | 0.696367000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35; #1: AID=1, RU=54, MCS=2, target RSSI=35 |
| 685 | 0.700079000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35; #1: AID=1, RU=54, MCS=2, target RSSI=35 |
| 693 | 0.703706000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35 |
| 701 | 0.707049000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35 |
| 712 | 0.711924000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 725 | 0.717616000 | 0 | #0: AID=1, RU=61, MCS=2, target RSSI=35 |
| 735 | 0.722702000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 754 | 0.729651000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 767 | 0.735195000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 779 | 0.739370000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 809 | 0.750731000 | 0 | #0: AID=1, RU=61, MCS=2, target RSSI=35 |
| 825 | 0.756585000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 837 | 0.763098000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35; #1: AID=1, RU=54, MCS=2, target RSSI=35 |
| 846 | 0.766441000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 849 | 0.769036000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 857 | 0.773670000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 865 | 0.776850000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 872 | 0.780497000 | 0 | #0: AID=1, RU=61, MCS=2, target RSSI=35 |
| 890 | 0.786654000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35 |
| 898 | 0.790930000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 905 | 0.793612000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 916 | 0.797232000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 927 | 0.800981000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 940 | 0.804858000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 952 | 0.810808000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 975 | 0.821601000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 985 | 0.825355000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 1004 | 0.831976000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 1011 | 0.835587000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 1021 | 0.840323000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 1025 | 0.843881000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 1034 | 0.847203000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 1043 | 0.850951000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 1053 | 0.855134000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1090 | 0.874036000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1098 | 0.878212000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1106 | 0.881964000 | 0 | #0: AID=3, RU=53, MCS=2, target RSSI=35 |
| 1118 | 0.885943000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1149 | 0.900583000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 1161 | 0.903498000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 1173 | 0.907259000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 1182 | 0.911097000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 1190 | 0.915704000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 1202 | 0.919871000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 1214 | 0.924559000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1218 | 0.927225000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1221 | 0.930036000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1228 | 0.933637000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 1239 | 0.937830000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 1249 | 0.941987000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 1260 | 0.945325000 | 0 | #0: AID=3, RU=53, MCS=2, target RSSI=35 |
| 1265 | 0.947539000 | 0 | #0: AID=3, RU=53, MCS=2, target RSSI=35 |
| 1270 | 0.950163000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1273 | 0.953036000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1280 | 0.956664000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |

### [script] Analysis of Packet Distribution
The scheduled conditions contain the expected Trigger/response activity, but a BSR is an A-Control scheduling input rather than a frame subtype. IEEE Std 802.11-2024 Clause 26.5.5 requires the report contents and capability conditions; use the AP-reported and scheduled-backlog telemetry documented above. QoS Data counts are not evidence that a BSR was fresh or that the reported bytes were delivered.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->

## [agent] Frame exchange analysis

<!-- BEGIN GENERATED ANALYSIS: frame-exchange -->
<!-- END GENERATED ANALYSIS: frame-exchange -->

The generated timeline shows an AP Trigger followed by HE-TB station
responses and acknowledgment activity. The decoded allocations expose the
assigned AIDs, RUs, and MCS values. This is capture-local evidence from run 0,
not an event-log reconstruction; the trigger-decision vectors provide the
cross-layer BSR accounting.

## [agent] Cross-layer findings and verdict

The topology is constant; the configurations vary report timing, report age,
and traffic shape. Across seeds 0–4, every retained scheduled decision in
`FreshBsr`, `LongTriggerCheck`, `BurstyTraffic`, and `StaleBsr` has an aligned
decision ID, reported backlog, and planned bytes. `LongTriggerCheck` adds the
0.25 s trigger-check interval and 150 ms report-age limit while retaining
protocol-visible implicit reporting. The scoped verdict is `PASS` for the
accounting join and for observed Trigger/HE-TB activity. It is `INCONCLUSIVE`
for delivery of all reported bytes and for any general advantage of the 150 ms
age limit.

## [agent] Limitations and inconclusive claims

- The comparison covers one topology, workload, five seeds, and one 150 ms
  freshness setting. Test additional age limits and matched workloads before
  making a broader claim.
- BSR is scheduling state, not necessarily a standalone frame subtype. If the
  accounting join fails, inspect the AP `heUlTriggerDecision*` vectors first;
  if the exchange itself is unclear, inspect the generated run-0 PCAP timeline.
- PCAP is representative run-0 mechanism evidence and does not replace the
  five-run scalar/vector evidence.
