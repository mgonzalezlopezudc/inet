# Walkthrough: HE BSS coloring and spatial reuse

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260731T222028Z`
- PCAP: `20260731T222028Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This example shows that the INET HE receiver distinguishes own-BSS and
overlapping-BSS PPDUs, then applies the configured OBSS/PD threshold to the
latter. The current five-run session passes that receiver-mechanism check.
It does not establish that any threshold produces a general performance
advantage, so that outcome claim remains `INCONCLUSIVE`.

## [agent] Learning objectives and feature primer

Can a receiver use BSS color and OBSS/PD to reuse the channel only for
eligible overlapping-BSS traffic? HE BSS color identifies a BSS; an HE
receiver can classify a different color as overlapping-BSS (OBSS). When the
received OBSS PPDU satisfies the OBSS/PD condition, the receiver can ignore
it for carrier sensing and continue contention. A less-negative threshold
permits more candidate reuse and can also admit more interference.

This is a receiver decision, not a conclusion that follows from throughput or
packet counts. The decisive evidence is therefore the receiver telemetry that
correlates classification, eligibility, threshold, power limit, reason, and
ignore outcome for each retained reception.

## [agent] Scenario description

[BssColoringNetwork.ned](BssColoringNetwork.ned) has two APs, two stations per
AP, and one wired server per BSS. BSS 1 stays fixed. BSS 2 moves as a group,
increasing AP separation from 120 m to 250 m during the `[0.3, 0.95) s`
measurement window. The movement sweeps received OBSS power across the
configured thresholds while both servers send matched, jittered downlink UDP
traffic. The relevant configuration is in [omnetpp.ini](omnetpp.ini).

```text
server1 -- AP1 ~~ sta1[0..1]     sta2[0..1] ~~ AP2 -- server2
                  stationary       moving BSS 2 →
```

The threshold sweep changes only `obssPdThreshold`. The disabled configuration
keeps ordinary energy detection, while the same-color configuration prevents
inter-BSS classification. Together these controls isolate the modeled BSS
color and OBSS/PD decision.

## [agent] Standards and INET model boundary

IEEE Std 802.11-2024 Clause 26.10 describes HE spatial reuse; Clause 26.10.2
defines OBSS/PD-based operation, and Clause 26.2.3 defines spatial-reuse-group
PPDU identification. These references were verified in corpus chunks
`80211ax-2024:chunk:09886`, `09890`, and `09741`.

INET sets the colors in the MIB and enables the modeled receiver behavior with
`enableSpatialReuse` and `obssPdThreshold`. Mobility, traffic, scalar radio
medium, and concurrent-start behavior are experiment choices. This is an INET
model demonstration, not standards-conformance certification.

## [agent] Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| Receiver applies the modeled BSS-color and OBSS/PD decision | `PASS` | generated scalar/vector checks | runs/seeds `0-4` | every retained decision is joined to its inputs and outcome |
| A threshold has a defined outcome advantage | `INCONCLUSIVE` | generated scalar/vector checks | runs/seeds `0-4` | no manifest-defined multi-condition outcome criterion |
| Every condition has visible MAC traffic | `PASS` | generated PCAP checks | run/seed `0` | capture observations, not deliveries |
| Captured frame distributions differ | `PASS` | generated PCAP checks | run/seed `0` | screening signal only; does not prove reuse |

## [agent] Configuration matrix

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| `BssColoringDisabled` | Control | spatial reuse off; ED `-82 dBm` | matched two-BSS downlink | `0-4` | no OBSS/PD ignores |
| `ObssPdConservative` | Treatment | enabled; `-81 dBm` | matched | `0-4` | fewest treatment ignores |
| `BssColoringEnabled` | Treatment | enabled; `-79 dBm` | matched | `0-4` | intermediate ignores |
| `ObssPdAggressive` | Stress | enabled; `-78 dBm` | matched | `0-4` | most treatment ignores |
| `BssColoringCollision` | Negative control | enabled; both BSSs use color 1 | matched | `0-4` | no inter-BSS reuse |

Each threshold condition extends `BssColoringEnabled`. The negative control
sets all local colors to 1; all other material settings are matched.

## [agent] Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| Every retained decision has aligned inputs and outcome | joined receiver-decision telemetry | missing or misaligned samples | HE receiver observability | inspect the failed vector join by run and receiver |
| Same color suppresses OBSS/PD reuse | joined receiver-decision telemetry | ignored PPDUs in the color-collision control | effective color configuration | inspect AP and station MIB colors |
| Outcome comparison has an executable rule | generated scalar/vector checks | an outcome claim remains `INCONCLUSIVE` | suite manifest | define and test a bounded effect or ordering |

## [agent] Reproduction

The published session is `20260731T222028Z`: five independent runs per
condition plus PCAPng from representative run 0. From the repository root:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py inspect bss_coloring \
  --session-id 20260731T222028Z
python3 examples/ieee80211/analysis/wifi_analysis.py report bss_coloring \
  --session-id 20260731T222028Z
python3 examples/ieee80211/analysis/wifi_analysis.py publish bss_coloring \
  --session-id 20260731T222028Z --update
```

## [agent] Scalar and vector analysis

The session's `.vec` artifacts answer the learning question directly. The
generated scalar/vector bundle joins every retained receiver decision with BSS type,
eligibility, OBSS/PD threshold, power limit, reason, and ignore outcome.
The controls record no ignored PPDUs. The conservative, enabled, and
aggressive treatments record `4,435`, `10,188`, and `13,059` ignores,
respectively. This supports a `PASS` for the modeled receiver mechanism.

The bundle also reports goodput, Jain fairness, and concurrent AP airtime over
`[0.3, 0.95) s`, with run-level 95% Student-t confidence intervals. Those
derived measurements vary across the sweep, but the manifest defines no
multi-condition effect or ordering to test. They therefore do not support a
performance ranking.

<!-- BEGIN GENERATED: ieee80211-scalar-vector-bss -->
### [script] Generated scalar/vector plot and table

![bss scalar/vector analysis](results/20260731T222028Z/bss-coloring-comparison.png)

Figure provenance: [`results/20260731T222028Z/bss-coloring-comparison.png.json`](results/20260731T222028Z/bss-coloring-comparison.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes) / unit=B<br>vector / **.ap*.wlan[0].radio / transmissionState:vector<br>vector / **.receiver / heSpatialReuseReason:vector<br>vector / **.receiver / heSpatialReuseBssType:vector<br>vector / **.receiver / heSpatialReuseEligible:vector<br>vector / **.receiver / heSpatialReuseObssPdThreshold:vector / unit=dBm<br>vector / **.receiver / heSpatialReuseTransmitPowerLimit:vector / unit=dBm
- Window / per-run aggregation / exclusions: [0.3, 0.95) s; observation=per-run measurement-window aggregate; uncertainty=95% Student-t CI; validation=joins each receiver decision by run, module, and aligned vector sample; requires inter-BSS OBSS/PD decisions and validates the 21 dBm/-82 dBm threshold-to-power relation
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| Aggressive / concurrent ap airtime percent | 30.674 | 2.64108 |
| Aggressive / goodput mbps | 9.61969 | 0.178741 |
| Aggressive / jain fairness | 0.999648 | 0.000326034 |
| Color collision / concurrent ap airtime percent | 1.428 | 0.157647 |
| Color collision / goodput mbps | 6.66338 | 0.443389 |
| Color collision / jain fairness | 0.976132 | 0.0303152 |
| Conservative / concurrent ap airtime percent | 7.728 | 1.67991 |
| Conservative / goodput mbps | 7.24185 | 0.349588 |
| Conservative / jain fairness | 0.985335 | 0.0317093 |
| Disabled / concurrent ap airtime percent | 1.428 | 0.157647 |
| Disabled / goodput mbps | 6.66338 | 0.443389 |
| Disabled / jain fairness | 0.976132 | 0.0303152 |
| Enabled / concurrent ap airtime percent | 25.706 | 2.2034 |
| Enabled / goodput mbps | 9.13231 | 0.14172 |
| Enabled / jain fairness | 0.997675 | 0.00485498 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **PASS** | OBSS classification, OBSS/PD threshold, CCA, and power-limit decisions are observable | Every retained receiver decision has aligned BSS classification, eligibility, OBSS/PD threshold, power limit, reason, and ignore outcome. |
| **INCONCLUSIVE** | Reuse comparison reports delivery, fairness, and concurrent airtime | The multi-condition delivery, fairness, and airtime ordering lacks a manifest-defined executable threshold. |
<!-- END GENERATED: ieee80211-scalar-vector-bss -->

## [agent] PCAP statistics

The same session provides representative run-0 PCAPng captures at the AP
wireless interfaces. The generated tables now preserve the HE BSS color as a
packet-statistics dimension, so otherwise matching PHY rows retain independent
mean received-signal values for colors `0x0001` and `0x0002`.

The captures confirm protocol-visible traffic in every configuration and
different frame-distribution signatures. They are AP observations, not
application deliveries or de-duplicated medium transmissions; the PCAP result
is useful context for the exchange but cannot itself prove a receiver decision.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260731T222028Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260731T222028Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260731T222028Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260731T222028Z.json` (SHA-256 `63f49aecdfd657b4a634fd2ba0548f6ea4084e85977ff34eb9a7ae82eb476c89`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `BssColoringDisabled` | `none (all decoded frames)` | 3354 | Data: QoS Data [HE-MU, HE-MCS 7, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] (584), Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] (584), Data: QoS Data [HE-MU, HE-MCS 7, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] (396) | 143.59% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `BssColoringEnabled` | `none (all decoded frames)` | 2450 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] (274), Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] (272), Data: QoS Data [HE-MU, HE-MCS 7, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] (268) | 124.02% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `ObssPdConservative` | `none (all decoded frames)` | 3294 | Data: QoS Data [HE-MU, HE-MCS 7, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] (586), Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] (586), Data: QoS Data [HE-MU, HE-MCS 7, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] (374) | 142.98% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `ObssPdAggressive` | `none (all decoded frames)` | 2458 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] (272), Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] (272), Data: QoS Data [HE-MU, HE-MCS 7, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] (270) | 122.59% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `BssColoringCollision` | `none (all decoded frames)` | 3354 | Data: QoS Data [HE-MU, HE-MCS 7, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] (980), Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] (980), Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] (490) | 143.59% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | BssColoringCollision produced protocol-visible wireless observations | 3354 AP/global transmission observations |
| **PASS** | BssColoringDisabled produced protocol-visible wireless observations | 3354 AP/global transmission observations |
| **PASS** | BssColoringEnabled produced protocol-visible wireless observations | 2450 AP/global transmission observations |
| **PASS** | ObssPdAggressive produced protocol-visible wireless observations | 2458 AP/global transmission observations |
| **PASS** | ObssPdConservative produced protocol-visible wireless observations | 3294 AP/global transmission observations |
| **PASS** | The bounded scenario exposes a coloring/OBSS-PD decision difference | At least two frame-distribution signatures differ |

### [script] Configuration: `BssColoringDisabled`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **3354**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#269c32" /></svg> | Data: QoS Data [HE-MU, HE-MCS 7, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0002 | 584 | 17.41% | 1066.0 B | 0.0 B | 604.5 us | 0.0 us | 5050 MHz | -77.8 dBm | 13.0 dBm | 24.59% | 35.30% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#269c32" /></svg> | Data: QoS Data [HE-MU, HE-MCS 7, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0001 | 396 | 11.81% | 1066.0 B | 0.0 B | 604.5 us | 0.0 us | 5050 MHz | -77.8 dBm | 13.0 dBm | 16.67% | 23.94% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e5c80b" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 0x0001 | 68 | 2.03% | 1070.0 B | 215.7 B | 1179.6 us | 237.5 us | 5050 MHz | -79.4 dBm | 13.0 dBm | 5.59% | 8.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e5c80b" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 0x0002 | 74 | 2.21% | 1068.8 B | 182.3 B | 1178.6 us | 201.1 us | 5050 MHz | -77.9 dBm | 13.0 dBm | 6.07% | 8.72% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 125 | 3.73% | 1071.6 B | 163.7 B | 1208.4 us | 179.1 us | 5050 MHz | -80.4 dBm | 13.0 dBm | 10.52% | 15.10% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0002 | 228 | 6.80% | 1064.7 B | 166.3 B | 1200.8 us | 181.9 us | 5050 MHz | -79.4 dBm | 13.0 dBm | 19.07% | 27.38% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0002 | 292 | 8.71% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -77.8 dBm | 13.0 dBm | 1.76% | 2.52% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 198 | 5.90% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -77.8 dBm | 13.0 dBm | 1.19% | 1.71% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#934a1f" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0002 | 6 | 0.18% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -77.5 dBm | 13.0 dBm | 0.03% | 0.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#934a1f" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 9 | 0.27% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -78.8 dBm | 13.0 dBm | 0.04% | 0.06% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d2b7d" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0002 | 6 | 0.18% | 92.0 B | 60.0 B | 136.6 us | 65.6 us | 5050 MHz | -69.0 dBm | - | 0.06% | 0.08% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d2b7d" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 9 | 0.27% | 125.3 B | 49.9 B | 173.1 us | 54.6 us | 5050 MHz | -71.2 dBm | - | 0.11% | 0.16% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 0x0002 | 584 | 17.41% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5043 MHz, 5047 MHz | -71.2 dBm | - | 7.71% | 11.07% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 0x0001 | 396 | 11.81% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5043 MHz, 5047 MHz | -71.1 dBm | - | 5.23% | 7.51% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 128 | 3.82% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -72.6 dBm | 13.0 dBm | 0.46% | 0.66% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0002 | 237 | 7.07% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -72.2 dBm | 13.0 dBm | 0.85% | 1.22% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 7 | 0.21% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -70.8 dBm | 13.0 dBm | 0.04% | 0.05% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0002 | 7 | 0.21% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -70.8 dBm | 13.0 dBm | 0.04% | 0.05% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.201236000 | 10:00:00:00:00:01 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 1 | 0.201236000 | 10:00:00:00:00:02 → 0a:aa:00:00:00:05 | Data: QoS Data / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 2 | 0.201336000 | ? → 10:00:00:00:00:01 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 2 | 0.201336000 | ? → 10:00:00:00:00:02 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 3 | 0.201452000 | 10:00:00:00:00:01 → 0a:aa:00:00:00:02 | Management: Action / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 3 | 0.201452000 | 10:00:00:00:00:02 → 0a:aa:00:00:00:05 | Management: Action / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 4 | 0.201552000 | ? → 10:00:00:00:00:01 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 4 | 0.201552000 | ? → 10:00:00:00:00:02 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 5 | 0.201696000 | 0a:aa:00:00:00:05 → 10:00:00:00:00:02 | Management: Action / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 6 | 0.201796000 | ? → 0a:aa:00:00:00:05 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 7 | 0.201939000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:01 | Management: Action / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 8 | 0.202039000 | ? → 0a:aa:00:00:00:02 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 9 | 0.204192000 | 10:00:00:00:00:01 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 10 | 0.204292000 | ? → 10:00:00:00:00:01 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 11 | 0.204408000 | 10:00:00:00:00:01 → 0a:aa:00:00:00:03 | Management: Action / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 12 | 0.204508000 | ? → 10:00:00:00:00:01 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |

Frame numbers are local to captures `BssColoringDisabled-#0BssColoringNetwork.ap1.wlan[0].pcap`, `BssColoringDisabled-#0BssColoringNetwork.ap2.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `BssColoringEnabled`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2450**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#269c32" /></svg> | Data: QoS Data [HE-MU, HE-MCS 7, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0002 | 268 | 10.94% | 1066.0 B | 0.0 B | 604.5 us | 0.0 us | 5050 MHz | -76.0 dBm | 13.0 dBm | 13.06% | 16.20% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#269c32" /></svg> | Data: QoS Data [HE-MU, HE-MCS 7, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0001 | 268 | 10.94% | 1066.0 B | 0.0 B | 604.5 us | 0.0 us | 5050 MHz | -76.2 dBm | 13.0 dBm | 13.06% | 16.20% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e5c80b" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 0x0001 | 101 | 4.12% | 1073.0 B | 182.1 B | 1182.8 us | 199.6 us | 5050 MHz | -78.2 dBm | 13.0 dBm | 9.63% | 11.95% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e5c80b" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 0x0002 | 103 | 4.20% | 1068.6 B | 175.3 B | 1178.2 us | 192.3 us | 5050 MHz | -77.5 dBm | 13.0 dBm | 9.79% | 12.14% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 193 | 7.88% | 1069.9 B | 213.4 B | 1206.5 us | 233.4 us | 5050 MHz | -77.9 dBm | 13.0 dBm | 18.78% | 23.28% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0002 | 240 | 9.80% | 1068.6 B | 175.4 B | 1205.1 us | 191.9 us | 5050 MHz | -78.0 dBm | 13.0 dBm | 23.32% | 28.92% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0002 | 134 | 5.47% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -76.0 dBm | 13.0 dBm | 0.93% | 1.16% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 134 | 5.47% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -76.2 dBm | 13.0 dBm | 0.93% | 1.16% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#934a1f" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0002 | 11 | 0.45% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -77.5 dBm | 13.0 dBm | 0.06% | 0.07% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#934a1f" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 11 | 0.45% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -77.0 dBm | 13.0 dBm | 0.06% | 0.07% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d2b7d" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0002 | 10 | 0.41% | 104.0 B | 58.8 B | 149.8 us | 64.3 us | 5050 MHz | -65.5 dBm | - | 0.12% | 0.15% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d2b7d" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 11 | 0.45% | 119.3 B | 53.4 B | 166.5 us | 58.5 us | 5050 MHz | -65.4 dBm | - | 0.15% | 0.18% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 0x0002 | 272 | 11.10% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5043 MHz, 5047 MHz | -70.4 dBm | - | 4.16% | 5.16% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 0x0001 | 274 | 11.18% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5043 MHz, 5047 MHz | -70.4 dBm | - | 4.19% | 5.20% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 187 | 7.63% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -64.5 dBm | 13.0 dBm | 0.77% | 0.96% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0002 | 219 | 8.94% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -64.9 dBm | 13.0 dBm | 0.91% | 1.12% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 7 | 0.29% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -70.8 dBm | 13.0 dBm | 0.04% | 0.05% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0002 | 7 | 0.29% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -70.8 dBm | 13.0 dBm | 0.04% | 0.05% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.201236000 | 10:00:00:00:00:01 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 1 | 0.201236000 | 10:00:00:00:00:02 → 0a:aa:00:00:00:05 | Data: QoS Data / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 2 | 0.201336000 | ? → 10:00:00:00:00:01 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 2 | 0.201336000 | ? → 10:00:00:00:00:02 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 3 | 0.201452000 | 10:00:00:00:00:01 → 0a:aa:00:00:00:02 | Management: Action / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 3 | 0.201452000 | 10:00:00:00:00:02 → 0a:aa:00:00:00:05 | Management: Action / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 4 | 0.201552000 | ? → 10:00:00:00:00:01 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 4 | 0.201552000 | ? → 10:00:00:00:00:02 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 5 | 0.201696000 | 0a:aa:00:00:00:05 → 10:00:00:00:00:02 | Management: Action / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 6 | 0.201796000 | ? → 0a:aa:00:00:00:05 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 7 | 0.201939000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:01 | Management: Action / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 8 | 0.202039000 | ? → 0a:aa:00:00:00:02 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 9 | 0.204192000 | 10:00:00:00:00:01 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 10 | 0.204292000 | ? → 10:00:00:00:00:01 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 11 | 0.204408000 | 10:00:00:00:00:01 → 0a:aa:00:00:00:03 | Management: Action / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 12 | 0.204508000 | ? → 10:00:00:00:00:01 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |

Frame numbers are local to captures `BssColoringEnabled-#0BssColoringNetwork.ap1.wlan[0].pcap`, `BssColoringEnabled-#0BssColoringNetwork.ap2.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `ObssPdConservative`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **3294**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#269c32" /></svg> | Data: QoS Data [HE-MU, HE-MCS 7, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0002 | 586 | 17.79% | 1066.0 B | 0.0 B | 604.5 us | 0.0 us | 5050 MHz | -77.8 dBm | 13.0 dBm | 24.78% | 35.43% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#269c32" /></svg> | Data: QoS Data [HE-MU, HE-MCS 7, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0001 | 374 | 11.35% | 1066.0 B | 0.0 B | 604.5 us | 0.0 us | 5050 MHz | -77.6 dBm | 13.0 dBm | 15.81% | 22.61% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e5c80b" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 0x0001 | 59 | 1.79% | 1077.4 B | 225.2 B | 1187.9 us | 248.9 us | 5050 MHz | -79.0 dBm | 13.0 dBm | 4.90% | 7.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e5c80b" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 0x0002 | 90 | 2.73% | 1068.3 B | 165.3 B | 1178.0 us | 182.5 us | 5050 MHz | -77.9 dBm | 13.0 dBm | 7.41% | 10.60% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 121 | 3.67% | 1072.1 B | 174.9 B | 1208.9 us | 191.3 us | 5050 MHz | -79.2 dBm | 13.0 dBm | 10.23% | 14.63% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0002 | 234 | 7.10% | 1067.0 B | 179.6 B | 1203.3 us | 196.5 us | 5050 MHz | -78.9 dBm | 13.0 dBm | 19.69% | 28.16% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0002 | 293 | 8.89% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -77.8 dBm | 13.0 dBm | 1.77% | 2.53% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 187 | 5.68% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -77.6 dBm | 13.0 dBm | 1.13% | 1.61% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#934a1f" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0002 | 8 | 0.24% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -77.5 dBm | 13.0 dBm | 0.03% | 0.05% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#934a1f" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 8 | 0.24% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -78.3 dBm | 13.0 dBm | 0.03% | 0.05% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d2b7d" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0002 | 8 | 0.24% | 77.0 B | 58.1 B | 120.2 us | 63.6 us | 5050 MHz | -67.8 dBm | - | 0.07% | 0.10% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d2b7d" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 8 | 0.24% | 122.0 B | 52.0 B | 169.5 us | 56.8 us | 5050 MHz | -69.9 dBm | - | 0.09% | 0.14% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 0x0002 | 586 | 17.79% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5043 MHz, 5047 MHz | -71.2 dBm | - | 7.77% | 11.11% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 0x0001 | 374 | 11.35% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5043 MHz, 5047 MHz | -71.0 dBm | - | 4.96% | 7.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 110 | 3.34% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -66.5 dBm | 13.0 dBm | 0.39% | 0.56% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0002 | 234 | 7.10% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -69.7 dBm | 13.0 dBm | 0.84% | 1.20% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 7 | 0.21% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -70.8 dBm | 13.0 dBm | 0.04% | 0.05% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0002 | 7 | 0.21% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -70.8 dBm | 13.0 dBm | 0.04% | 0.05% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.201236000 | 10:00:00:00:00:01 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 1 | 0.201236000 | 10:00:00:00:00:02 → 0a:aa:00:00:00:05 | Data: QoS Data / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 2 | 0.201336000 | ? → 10:00:00:00:00:01 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 2 | 0.201336000 | ? → 10:00:00:00:00:02 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 3 | 0.201452000 | 10:00:00:00:00:01 → 0a:aa:00:00:00:02 | Management: Action / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 3 | 0.201452000 | 10:00:00:00:00:02 → 0a:aa:00:00:00:05 | Management: Action / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 4 | 0.201552000 | ? → 10:00:00:00:00:01 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 4 | 0.201552000 | ? → 10:00:00:00:00:02 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 5 | 0.201696000 | 0a:aa:00:00:00:05 → 10:00:00:00:00:02 | Management: Action / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 6 | 0.201796000 | ? → 0a:aa:00:00:00:05 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 7 | 0.201939000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:01 | Management: Action / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 8 | 0.202039000 | ? → 0a:aa:00:00:00:02 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 9 | 0.204192000 | 10:00:00:00:00:01 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 10 | 0.204292000 | ? → 10:00:00:00:00:01 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 11 | 0.204408000 | 10:00:00:00:00:01 → 0a:aa:00:00:00:03 | Management: Action / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 12 | 0.204508000 | ? → 10:00:00:00:00:01 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |

Frame numbers are local to captures `ObssPdConservative-#0BssColoringNetwork.ap1.wlan[0].pcap`, `ObssPdConservative-#0BssColoringNetwork.ap2.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `ObssPdAggressive`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2458**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#269c32" /></svg> | Data: QoS Data [HE-MU, HE-MCS 7, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0002 | 270 | 10.98% | 1066.0 B | 0.0 B | 604.5 us | 0.0 us | 5050 MHz | -76.0 dBm | 13.0 dBm | 13.31% | 16.32% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#269c32" /></svg> | Data: QoS Data [HE-MU, HE-MCS 7, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0001 | 270 | 10.98% | 1066.0 B | 0.0 B | 604.5 us | 0.0 us | 5050 MHz | -76.2 dBm | 13.0 dBm | 13.31% | 16.32% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e5c80b" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 0x0001 | 83 | 3.38% | 1073.8 B | 178.5 B | 1183.8 us | 196.6 us | 5050 MHz | -77.3 dBm | 13.0 dBm | 8.02% | 9.83% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e5c80b" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 0x0002 | 103 | 4.19% | 1068.3 B | 165.2 B | 1177.9 us | 181.4 us | 5050 MHz | -77.4 dBm | 13.0 dBm | 9.90% | 12.13% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 217 | 8.83% | 1065.5 B | 130.2 B | 1201.7 us | 142.4 us | 5050 MHz | -77.2 dBm | 13.0 dBm | 21.27% | 26.08% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0002 | 221 | 8.99% | 1067.5 B | 132.3 B | 1203.9 us | 144.7 us | 5050 MHz | -77.2 dBm | 13.0 dBm | 21.70% | 26.61% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0002 | 135 | 5.49% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -76.0 dBm | 13.0 dBm | 0.95% | 1.17% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 135 | 5.49% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -76.2 dBm | 13.0 dBm | 0.95% | 1.17% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#934a1f" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0002 | 11 | 0.45% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -77.5 dBm | 13.0 dBm | 0.06% | 0.07% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#934a1f" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 10 | 0.41% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -77.0 dBm | 13.0 dBm | 0.05% | 0.06% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d2b7d" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 9 | 0.37% | 85.3 B | 59.6 B | 129.4 us | 65.2 us | 5050 MHz | -64.0 dBm | - | 0.09% | 0.12% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d2b7d" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0002 | 9 | 0.37% | 85.3 B | 59.6 B | 129.4 us | 65.2 us | 5050 MHz | -64.0 dBm | - | 0.09% | 0.12% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 0x0002 | 272 | 11.07% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5043 MHz, 5047 MHz | -70.3 dBm | - | 4.21% | 5.16% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 0x0001 | 272 | 11.07% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5043 MHz, 5047 MHz | -70.2 dBm | - | 4.21% | 5.16% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 213 | 8.67% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -64.2 dBm | 13.0 dBm | 0.89% | 1.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0002 | 214 | 8.71% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -64.2 dBm | 13.0 dBm | 0.90% | 1.10% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 7 | 0.28% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -70.8 dBm | 13.0 dBm | 0.04% | 0.05% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0002 | 7 | 0.28% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -70.8 dBm | 13.0 dBm | 0.04% | 0.05% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.201236000 | 10:00:00:00:00:01 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 1 | 0.201236000 | 10:00:00:00:00:02 → 0a:aa:00:00:00:05 | Data: QoS Data / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 2 | 0.201336000 | ? → 10:00:00:00:00:01 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 2 | 0.201336000 | ? → 10:00:00:00:00:02 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 3 | 0.201452000 | 10:00:00:00:00:01 → 0a:aa:00:00:00:02 | Management: Action / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 3 | 0.201452000 | 10:00:00:00:00:02 → 0a:aa:00:00:00:05 | Management: Action / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 4 | 0.201552000 | ? → 10:00:00:00:00:01 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 4 | 0.201552000 | ? → 10:00:00:00:00:02 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 5 | 0.201696000 | 0a:aa:00:00:00:05 → 10:00:00:00:00:02 | Management: Action / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 6 | 0.201796000 | ? → 0a:aa:00:00:00:05 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 7 | 0.201939000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:01 | Management: Action / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 8 | 0.202039000 | ? → 0a:aa:00:00:00:02 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 9 | 0.204192000 | 10:00:00:00:00:01 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 10 | 0.204292000 | ? → 10:00:00:00:00:01 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 11 | 0.204408000 | 10:00:00:00:00:01 → 0a:aa:00:00:00:03 | Management: Action / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 12 | 0.204508000 | ? → 10:00:00:00:00:01 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |

Frame numbers are local to captures `ObssPdAggressive-#0BssColoringNetwork.ap1.wlan[0].pcap`, `ObssPdAggressive-#0BssColoringNetwork.ap2.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `BssColoringCollision`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **3354**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#269c32" /></svg> | Data: QoS Data [HE-MU, HE-MCS 7, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0001 | 980 | 29.22% | 1066.0 B | 0.0 B | 604.5 us | 0.0 us | 5050 MHz | -77.8 dBm | 13.0 dBm | 41.26% | 59.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e5c80b" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 0x0001 | 142 | 4.23% | 1069.4 B | 199.0 B | 1179.0 us | 219.3 us | 5050 MHz | -78.6 dBm | 13.0 dBm | 11.66% | 16.74% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 353 | 10.52% | 1067.2 B | 165.4 B | 1203.5 us | 180.9 us | 5050 MHz | -79.8 dBm | 13.0 dBm | 29.59% | 42.48% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 490 | 14.61% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -77.8 dBm | 13.0 dBm | 2.95% | 4.23% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#934a1f" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 15 | 0.45% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -78.3 dBm | 13.0 dBm | 0.07% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d2b7d" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 15 | 0.45% | 112.0 B | 56.6 B | 158.5 us | 61.9 us | 5050 MHz | -70.3 dBm | - | 0.17% | 0.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 0x0001 | 980 | 29.22% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5043 MHz, 5047 MHz | -71.1 dBm | - | 12.94% | 18.58% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 365 | 10.88% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -72.3 dBm | 13.0 dBm | 1.30% | 1.87% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 0x0001 | 14 | 0.42% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -70.8 dBm | 13.0 dBm | 0.07% | 0.11% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.201236000 | 10:00:00:00:00:01 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 1 | 0.201236000 | 10:00:00:00:00:02 → 0a:aa:00:00:00:05 | Data: QoS Data / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 2 | 0.201336000 | ? → 10:00:00:00:00:01 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 2 | 0.201336000 | ? → 10:00:00:00:00:02 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 3 | 0.201452000 | 10:00:00:00:00:01 → 0a:aa:00:00:00:02 | Management: Action / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 3 | 0.201452000 | 10:00:00:00:00:02 → 0a:aa:00:00:00:05 | Management: Action / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 4 | 0.201552000 | ? → 10:00:00:00:00:01 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 4 | 0.201552000 | ? → 10:00:00:00:00:02 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 5 | 0.201696000 | 0a:aa:00:00:00:05 → 10:00:00:00:00:02 | Management: Action / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 6 | 0.201796000 | ? → 0a:aa:00:00:00:05 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 7 | 0.201939000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:01 | Management: Action / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 8 | 0.202039000 | ? → 0a:aa:00:00:00:02 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 9 | 0.204192000 | 10:00:00:00:00:01 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 10 | 0.204292000 | ? → 10:00:00:00:00:01 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 11 | 0.204408000 | 10:00:00:00:00:01 → 0a:aa:00:00:00:03 | Management: Action / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 12 | 0.204508000 | ? → 10:00:00:00:00:01 | Control: Ack / HE-SU, HE-MCS 0, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |

Frame numbers are local to captures `BssColoringCollision-#0BssColoringNetwork.ap1.wlan[0].pcap`, `BssColoringCollision-#0BssColoringNetwork.ap2.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Analysis of Packet Distribution
**PASS: BSS-coloring separation.** At least two frame-distribution signatures differ. IEEE Std 802.11-2024 Clause 26.10 permits eligible inter-BSS reuse after OBSS/PD classification; it does not guarantee a throughput improvement, and a more permissive threshold can increase interference. The differing distribution is only a screening signal; the separate five-seed result campaign validates direct OBSS classification, threshold, CCA, power-limit, and reuse-decision telemetry. The current model reports the standards-defined threshold/power coupling but does not dynamically adapt OBSS/PD or apply that limit to later transmissions.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->

## [agent] Frame exchange analysis

The generated timelines show protocol-visible HE data and acknowledgement
exchanges. They establish that the modeled MAC exchange occurred in the
captured trajectory, but an AP observation point does not establish reception
at another node or show an ignored OBSS PPDU.

## [agent] Cross-layer findings and verdict

| Claim | Verdict | Configuration evidence | Model telemetry | Packet evidence | Outcome evidence |
|---|---|---|---|---|---|
| OBSS/PD changes modeled reuse | `PASS` | threshold sweep | joined decisions show treatment ignores and control zeros | exchange present | performance ordering is out of scope |
| Same color suppresses inter-BSS reuse | `PASS` | both BSSs use color 1 | no inter-BSS candidates or ignored PPDUs | matching packet distribution | outcome comparison is out of scope |
| Every condition exchanges MAC traffic | `PASS` | matched conditions | not required | PCAP checks pass | out of scope |

The bounded verdict is `PASS` for modeled OBSS/PD receiver decisions: the
current session directly correlates classification through the ignore outcome
and separates the controls from the threshold treatments. It remains
`INCONCLUSIVE` whether a more permissive threshold produces a defined outcome
advantage, because the suite has no executable multi-condition criterion.

## [agent] Limitations and inconclusive claims

- One AP capture cannot establish reception at another node.
- The scalar radio model and one movement path do not establish deployment
  performance.
- The suite does not yet define an executable multi-condition delivery,
  fairness, or airtime criterion.

## [agent] Further experiments

- Add a manifest-defined outcome criterion, then repeat the threshold sweep.
