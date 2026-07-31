# Walkthrough: HE BSS coloring and spatial reuse

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260731T214358Z`
- PCAP: `20260731T214358Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough evaluates HE BSS coloring and OBSS/PD spatial reuse with a
disabled control, three thresholds, and a same-color control. The current
five-run, co-recorded session establishes changed outcomes and packet
distributions, but does not correlate every receiver decision with its
classification and threshold; the mechanism verdict is therefore
`INCONCLUSIVE`.

## [agent] Learning objectives and feature primer

An HE receiver uses BSS color to distinguish its own BSS from an overlapping
BSS (OBSS). When an OBSS PPDU meets the OBSS/PD condition, the receiver may
reuse the channel. A less-negative threshold admits more reuse, but can also
increase interference. This is a receiver/carrier-sense decision: delivery,
airtime, and packet counts cannot prove it without correlated receiver
telemetry.

## [agent] Scenario description

[BssColoringNetwork.ned](BssColoringNetwork.ned) contains two APs, two
stations per AP, and separate wired servers. BSS 1 is stationary; BSS 2 moves
as a rigid group along the x axis. During the `[0.3, 0.95) s` measurement
window, AP separation grows from 120 m to 250 m, sweeping received OBSS power
through the configured thresholds. Both servers send jittered downlink UDP
traffic after a Block Ack warm-up. The material configuration is in
[omnetpp.ini](omnetpp.ini).

```text
server1 -- AP1 ~~ sta1[0..1]     sta2[0..1] ~~ AP2 -- server2
                  stationary       moving BSS 2 →
```

The threshold sweep changes only `obssPdThreshold`; the same-color case is the
negative control for inter-BSS classification.

## [agent] Standards and INET model boundary

IEEE Std 802.11-2024 Clause 26.10 describes HE spatial reuse; Clause 26.10.2
defines OBSS/PD-based operation, and Clause 26.2.3 defines spatial-reuse-group
PPDU identification. These references were verified in corpus chunks
`80211ax-2024:chunk:09886`, `09890`, and `09741`.

INET configures the colors in the MIB and requests the receiver behavior with
`enableSpatialReuse` and `obssPdThreshold`. The movement, traffic, scalar radio
medium, and concurrent-start setting are experiment choices. The simulation is
an INET model demonstration, not a standards-conformance certification.

## [agent] Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| Receiver mechanism is correlated per reception | `INCONCLUSIVE` | generated scalar/vector checks | runs/seeds `0–4` | classification and reuse decision are not correlated |
| Outcomes differ across the threshold sweep | `INCONCLUSIVE` | generated scalar/vector checks | runs/seeds `0–4` | no manifest-defined multi-condition threshold |
| MAC traffic is present in every condition | `PASS` | generated PCAP checks | run/seed `0` | capture observations, not deliveries |
| Packet distributions differ by condition | `PASS` | generated PCAP checks | run/seed `0` | a screening signal, not proof of reuse |

## [agent] Configuration matrix

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| `BssColoringDisabled` | Control | spatial reuse off, ED `-82 dBm` | matched two-BSS downlink | `0–4` | no OBSS/PD ignores |
| `ObssPdConservative` | Treatment | enabled, `-81 dBm` | matched | `0–4` | least treatment reuse |
| `BssColoringEnabled` | Treatment | enabled, `-79 dBm` | matched | `0–4` | intermediate reuse |
| `ObssPdAggressive` | Stress | enabled, `-78 dBm` | matched | `0–4` | greatest reuse |
| `BssColoringCollision` | Negative | enabled, both BSSs use color 1 | matched | `0–4` | resembles disabled control |

Each threshold condition extends `BssColoringEnabled`. The negative control
sets all local colors to 1; all other material settings are matched.

## [agent] Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| Every decision has a matching classification and threshold | receiver decision telemetry | uncorrelated streams | HE receiver observability | publish a joined per-reception record |
| Threshold sweep has a declared pass rule | generated scalar/vector checks | outcome-only comparison | suite manifest | define and test a bounded ordering or effect size |
| Same-color resembles disabled | current result bundle | changed negative-control outcome | color configuration | inspect all AP/STA effective colors |

## [agent] Reproduction

The published session is `20260731T214358Z`, with five independent runs per
condition and PCAPng from representative run 0. From the repository root:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py inspect bss_coloring \
  --session-id 20260731T214358Z
python3 examples/ieee80211/analysis/wifi_analysis.py report bss_coloring \
  --session-id 20260731T214358Z
python3 examples/ieee80211/analysis/wifi_analysis.py publish bss_coloring \
  --session-id 20260731T214358Z --update
```

## [agent] Scalar and vector analysis

The generated bundle reports goodput, Jain fairness, and concurrent AP airtime
over `[0.3, 0.95) s`, with run-level 95% Student-t CIs. It also selects
receiver signals intended to observe BSS type, eligibility, OBSS/PD threshold,
power limit, and reason. The values change across the sweep and the same-color
control matches the disabled condition, but the executable checks correctly
mark the evidence `INCONCLUSIVE`: there is no per-reception correlation from
classification through the reuse decision, and the manifest has no explicit
multi-condition outcome rule. The direct observations are the session's
`.vec` artifacts, including
`results/20260731T214358Z/BssColoringEnabled/BssColoringEnabled-#0.vec`; the
published metrics and CIs are derived measurements, and mechanism attribution
without a per-reception join remains an inference.

<!-- BEGIN GENERATED: ieee80211-scalar-vector-bss -->
### [script] Generated scalar/vector plot and table

![bss scalar/vector analysis](results/20260731T214358Z/bss-coloring-comparison.png)

Figure provenance: [`results/20260731T214358Z/bss-coloring-comparison.png.json`](results/20260731T214358Z/bss-coloring-comparison.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes) / unit=B<br>vector / **.ap*.wlan[0].radio / transmissionState:vector<br>vector / **.receiver / heSpatialReuseReason:vector<br>vector / **.receiver / heSpatialReuseBssType:vector<br>vector / **.receiver / heSpatialReuseEligible:vector<br>vector / **.receiver / heSpatialReuseObssPdThreshold:vector / unit=dBm<br>vector / **.receiver / heSpatialReuseTransmitPowerLimit:vector / unit=dBm
- Window / per-run aggregation / exclusions: [0.3, 0.95) s; observation=per-run measurement-window aggregate; uncertainty=95% Student-t CI; validation=requires inter-BSS OBSS/PD decisions and validates the 21 dBm/-82 dBm threshold-to-power relation
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
| **INCONCLUSIVE** | OBSS classification, OBSS/PD threshold, CCA, and power-limit decisions are observable | The retained telemetry does not expose a correlated per-reception OBSS classification and reuse decision. |
| **INCONCLUSIVE** | Reuse comparison reports delivery, fairness, and concurrent airtime | The multi-condition delivery, fairness, and airtime ordering lacks a manifest-defined executable threshold. |
<!-- END GENERATED: ieee80211-scalar-vector-bss -->

## [agent] PCAP statistics

The same published session provides representative run-0 MAC PCAPng captures.
The generated summary confirms protocol-visible traffic in every condition and
different frame-distribution signatures. Capture observations are neither
application deliveries nor de-duplicated medium transmissions, so they screen
for a changed on-air exchange but do not prove a receiver decision.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260731T214358Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260731T214358Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260731T214358Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260731T214358Z.json` (SHA-256 `09ace43471f384a5af21721a6c39a8fc54db3c0b5b7dbe10ab9e33f57dffdef1`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `BssColoringDisabled` | `none (all decoded frames)` | 3354 | Data: QoS Data [HE-MU, HE-MCS 7, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] (980), Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] (980), Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] (490) | 143.59% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `BssColoringEnabled` | `none (all decoded frames)` | 2450 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] (546), Data: QoS Data [HE-MU, HE-MCS 7, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] (536), Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] (433) | 124.02% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `ObssPdConservative` | `none (all decoded frames)` | 3294 | Data: QoS Data [HE-MU, HE-MCS 7, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] (960), Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] (960), Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] (480) | 142.98% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `ObssPdAggressive` | `none (all decoded frames)` | 2458 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] (544), Data: QoS Data [HE-MU, HE-MCS 7, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] (540), Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] (438) | 122.59% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
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

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#269c32" /></svg> | Data: QoS Data [HE-MU, HE-MCS 7, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 980 | 29.22% | 1066.0 B | 0.0 B | 604.5 us | 0.0 us | 5050 MHz | -77.8 dBm | 13.0 dBm | 41.26% | 59.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e5c80b" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 142 | 4.23% | 1069.4 B | 199.0 B | 1179.0 us | 219.3 us | 5050 MHz | -78.6 dBm | 13.0 dBm | 11.66% | 16.74% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 353 | 10.52% | 1067.2 B | 165.4 B | 1203.5 us | 180.9 us | 5050 MHz | -79.8 dBm | 13.0 dBm | 29.59% | 42.48% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 490 | 14.61% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -77.8 dBm | 13.0 dBm | 2.95% | 4.23% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#934a1f" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 15 | 0.45% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -78.3 dBm | 13.0 dBm | 0.07% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d2b7d" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 15 | 0.45% | 112.0 B | 56.6 B | 158.5 us | 61.9 us | 5050 MHz | -70.3 dBm | - | 0.17% | 0.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 980 | 29.22% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5043 MHz, 5047 MHz | -71.1 dBm | - | 12.94% | 18.58% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 365 | 10.88% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -72.3 dBm | 13.0 dBm | 1.30% | 1.87% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 14 | 0.42% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -70.8 dBm | 13.0 dBm | 0.07% | 0.11% |

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

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#269c32" /></svg> | Data: QoS Data [HE-MU, HE-MCS 7, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 536 | 21.88% | 1066.0 B | 0.0 B | 604.5 us | 0.0 us | 5050 MHz | -76.1 dBm | 13.0 dBm | 26.13% | 32.40% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e5c80b" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 204 | 8.33% | 1070.8 B | 178.7 B | 1180.4 us | 196.0 us | 5050 MHz | -77.8 dBm | 13.0 dBm | 19.42% | 24.08% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 433 | 17.67% | 1069.2 B | 193.2 B | 1205.7 us | 211.4 us | 5050 MHz | -77.9 dBm | 13.0 dBm | 42.10% | 52.21% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 268 | 10.94% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -76.1 dBm | 13.0 dBm | 1.87% | 2.31% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#934a1f" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 22 | 0.90% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -77.3 dBm | 13.0 dBm | 0.11% | 0.14% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d2b7d" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 21 | 0.86% | 112.0 B | 56.6 B | 158.5 us | 61.9 us | 5050 MHz | -65.4 dBm | - | 0.27% | 0.33% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 546 | 22.29% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5043 MHz, 5047 MHz | -70.4 dBm | - | 8.35% | 10.35% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 406 | 16.57% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -64.7 dBm | 13.0 dBm | 1.68% | 2.08% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 14 | 0.57% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -70.8 dBm | 13.0 dBm | 0.09% | 0.11% |

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

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#269c32" /></svg> | Data: QoS Data [HE-MU, HE-MCS 7, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 960 | 29.14% | 1066.0 B | 0.0 B | 604.5 us | 0.0 us | 5050 MHz | -77.7 dBm | 13.0 dBm | 40.59% | 58.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e5c80b" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 149 | 4.52% | 1071.9 B | 191.4 B | 1181.9 us | 211.3 us | 5050 MHz | -78.3 dBm | 13.0 dBm | 12.32% | 17.61% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 355 | 10.78% | 1068.7 B | 178.0 B | 1205.2 us | 194.7 us | 5050 MHz | -79.0 dBm | 13.0 dBm | 29.92% | 42.78% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 480 | 14.57% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -77.7 dBm | 13.0 dBm | 2.90% | 4.14% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#934a1f" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 16 | 0.49% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -78.0 dBm | 13.0 dBm | 0.07% | 0.10% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d2b7d" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 16 | 0.49% | 99.5 B | 59.5 B | 144.9 us | 65.1 us | 5050 MHz | -68.8 dBm | - | 0.16% | 0.23% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 960 | 29.14% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5043 MHz, 5047 MHz | -71.1 dBm | - | 12.73% | 18.20% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 344 | 10.44% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -68.6 dBm | 13.0 dBm | 1.23% | 1.77% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 14 | 0.43% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -70.8 dBm | 13.0 dBm | 0.07% | 0.11% |

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

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#269c32" /></svg> | Data: QoS Data [HE-MU, HE-MCS 7, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 540 | 21.97% | 1066.0 B | 0.0 B | 604.5 us | 0.0 us | 5050 MHz | -76.1 dBm | 13.0 dBm | 26.63% | 32.64% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e5c80b" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 186 | 7.57% | 1070.8 B | 171.3 B | 1180.5 us | 188.4 us | 5050 MHz | -77.4 dBm | 13.0 dBm | 17.91% | 21.96% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 438 | 17.82% | 1066.5 B | 131.3 B | 1202.8 us | 143.6 us | 5050 MHz | -77.2 dBm | 13.0 dBm | 42.97% | 52.68% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 270 | 10.98% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -76.1 dBm | 13.0 dBm | 1.90% | 2.33% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#934a1f" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 21 | 0.85% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -77.3 dBm | 13.0 dBm | 0.11% | 0.13% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d2b7d" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 18 | 0.73% | 85.3 B | 59.6 B | 129.4 us | 65.2 us | 5050 MHz | -64.0 dBm | - | 0.19% | 0.23% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 544 | 22.13% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5043 MHz, 5047 MHz | -70.2 dBm | - | 8.41% | 10.31% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 427 | 17.37% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -64.2 dBm | 13.0 dBm | 1.79% | 2.19% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 14 | 0.57% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -70.8 dBm | 13.0 dBm | 0.09% | 0.11% |

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

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#269c32" /></svg> | Data: QoS Data [HE-MU, HE-MCS 7, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 980 | 29.22% | 1066.0 B | 0.0 B | 604.5 us | 0.0 us | 5050 MHz | -77.8 dBm | 13.0 dBm | 41.26% | 59.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e5c80b" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 142 | 4.23% | 1069.4 B | 199.0 B | 1179.0 us | 219.3 us | 5050 MHz | -78.6 dBm | 13.0 dBm | 11.66% | 16.74% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Data: QoS Data [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 353 | 10.52% | 1067.2 B | 165.4 B | 1203.5 us | 180.9 us | 5050 MHz | -79.8 dBm | 13.0 dBm | 29.59% | 42.48% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Trigger [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 490 | 14.61% | 46.0 B | 0.0 B | 86.3 us | 0.0 us | 5050 MHz | -77.8 dBm | 13.0 dBm | 2.95% | 4.23% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#934a1f" /></svg> | Control: Block Ack Request (BAR) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 15 | 0.45% | 24.0 B | 0.0 B | 62.3 us | 0.0 us | 5050 MHz | -78.3 dBm | 13.0 dBm | 0.07% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d2b7d" /></svg> | Control: Block Ack (BA) [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 15 | 0.45% | 112.0 B | 56.6 B | 158.5 us | 61.9 us | 5050 MHz | -70.3 dBm | - | 0.17% | 0.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 980 | 29.22% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5043 MHz, 5047 MHz | -71.1 dBm | - | 12.94% | 18.58% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Control: Ack [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 365 | 10.88% | 14.0 B | 0.0 B | 51.3 us | 0.0 us | 5050 MHz | -72.3 dBm | 13.0 dBm | 1.30% | 1.87% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#d6a400" /></svg> | Management: Action [HE-SU, HE-MCS 0, 20 MHz, GI 3.2 us, LDPC] | 14 | 0.42% | 37.0 B | 0.0 B | 76.5 us | 0.0 us | 5050 MHz | -70.8 dBm | 13.0 dBm | 0.07% | 0.11% |

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
| OBSS/PD changes modeled reuse | `INCONCLUSIVE` | threshold sweep | telemetry is not correlated per reception | exchange present | outcomes differ without an executable pass rule |
| Same color resembles disabled | `INCONCLUSIVE` | both BSSs use color 1 | matching bundle values | matching packet distribution | no mechanism correlation |
| Every condition exchanges MAC traffic | `PASS` | matched conditions | not required | PCAP checks pass | out of scope |

The bounded verdict is `INCONCLUSIVE` for modeled OBSS/PD spatial reuse. The
current session gives a coherent outcome and packet comparison, but it does
not provide the correlation or executable criterion needed to attribute the
change to the receiver mechanism.

## [agent] Limitations and inconclusive claims

- The PCAP and scalar/vector evidence share a session, but not a per-reception
  join between capture events and receiver decisions.
- One AP capture cannot establish reception at another node.
- The scalar radio model and one movement path do not establish deployment
  performance.

## [agent] Further experiments

- Add a suite-owned joined record for BSS type, threshold, eligibility, reason,
  and ignore decision for each received PPDU.
- Add a manifest-defined outcome criterion, then repeat the threshold sweep.
