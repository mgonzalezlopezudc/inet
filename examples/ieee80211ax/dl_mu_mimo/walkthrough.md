# Walkthrough: IEEE 802.11ax downlink MU-MIMO

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260802T090627Z`
- PCAP: `20260802T231452Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `20260802T001618Z`.

This example demonstrates configured IEEE 802.11ax downlink MU-MIMO: the AP emits HE-MU traffic on one 242-tone RU while the analyzer's five-run telemetry check finds multiple users with non-overlapping stream ranges in every MU configuration. At the offered loads tested, that correct spatial allocation does not translate into a goodput advantage over the matched OFDMA controls; it is a result of this INET scenario, not a general performance claim.

## [agent] Learning objectives and feature primer

DL MU-MIMO and DL OFDMA are alternative HE MU transmission modes: an AP may use the HE MU PPDU format for either (IEEE Std 802.11-2024, 27.3.1.1; corpus `80211ax-2024:chunk:10040`). Here, the control assigns stations separate frequency RUs, while the treatment places users on a common full-band 242-tone RU and gives them distinct spatial-stream ranges.

The decisive observation is not goodput alone. A valid treatment needs multi-user PPDUs and non-overlapping `heStreamStartIndex`/`heSpatialStreams` intervals at a common timestamp; the generated scalar/vector checks test exactly that. The PCAP block then independently shows HE-MU data, its RU width, and the acknowledgement traffic at the AP observation point.

## [agent] Scenario description

The scenario is configured in [omnetpp.ini](omnetpp.ini) and uses [`Lan80211AxDlOfdma`](../dl_ofdma_sched/Lan80211AxDlOfdma.ned). One six-antenna AP sends UDP traffic to three stationary stations whose receive-NSS limits are `{3,1,2}`.

```text
server === 100 Gbit/s Ethernet === 6-antenna AP
                                      ))) host[0], 3 antennas / NSS 3
                                      ))) host[1], 1 antenna  / NSS 1
                                      ))) host[2], 2 antennas / NSS 2
```

The campaign uses 20 MHz at 5 GHz. Three 100-byte warm-up flows begin at 0.2 s; the measured 1,000-byte UDP flows begin at 0.3 s. Goodput is measured over `[0.55, 0.88)` s. The causal control/treatment switch is `enableDlMuMimo`; 24, 48, 72, and 96 Mbit/s offered-load points and a 0.001 CSI-leakage sensitivity case are included.

## [agent] Standards and INET model boundary

The standard permits DL MU-MIMO on HE MIMO-capable RUs and describes 106- and 242-tone implementations (IEEE Std 802.11-2024, 27.3.3.1 and Figure 27-19; corpus `80211ax-2024:chunk:10067`, `80211ax-2024:chunk:00306`). That is the normative boundary.

INET's `HeDlSchedulerEqualSizedRUs` is a packet-level model. The INI requests beamforming-related capabilities, receive-NSS limits, CSI leakage, and the DL-MU-MIMO gate; those are inputs, not proof that a PPDU was spatially multiplexed. The `heStaId`, `heSpatialStreams`, and `heStreamStartIndex` vectors are the direct model observation. The PCAP decoder directly reports HE format and RU/MCS fields; its generated concluding check reports decoded HE-MU stream-start information.

## [agent] Evidence status

| Claim | Status | Script-generated evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| MU PPDUs contain multiple users with disjoint stream ranges | `PASS` | Scalar/vector executable checks | 6 MU configurations; runs `[0,5)` | Every run contains a multi-user PPDU and no same-timestamp ranges overlap |
| Treatment uses full-band HE-MU data | `PASS` | PCAP statistics and timeline | Representative run 0 | Generated PCAP rows show 242-tone HE-MU data |
| Control uses frequency-separated HE-MU data | `PASS` | PCAP statistics and timeline | Representative run 0 | Generated PCAP rows show 106-tone HE-MU data |
| MU-MIMO improves goodput at offered loads above 24 Mbit/s | `FAIL` | Scalar/vector table | 48, 72, and 96 Mbit/s; runs `[0,5)` | MU is lower than the matched OFDMA result in this scenario |
| Leakage change alone explains the goodput difference | `INCONCLUSIVE` | PCAP statistics and scalar/vector table | 24 Mbit/s; run 0 / runs `[0,5)` | The session supports an MCS/RU observation, not a causal PHY explanation |
| 80 MHz stress configuration executes | `NOT RUN` | None | None | `DlMuMimo80MHz` is not included in this analysis campaign session |

## [agent] Configuration matrix

| Configuration | Role | Causal delta | Runs/seeds | Expected invariant |
|---|---|---|---|---|
| `EqualSizedRUs*` | Control | `enableDlMuMimo=false`; 24/48/72/96 Mbit/s | `[0,5)` | 106-tone HE-MU data in representative captures |
| `DlMuMimo*Leakage01` | Treatment | `enableDlMuMimo=true`; 24/48/72/96 Mbit/s | `[0,5)` | Disjoint streams in each telemetry-tested MU PPDU |
| `DlMuMimo24MbpsLeakage001*` | Sensitivity | Treatment with `defaultCsiLeakage=0.001` at 24 Mbit/s | `[0,5)` | Same stream-range invariant; different configured leakage |

## [agent] Expected invariants and diagnostic map

| Invariant | Script-generated evidence | Failure symptom | First diagnostic |
|---|---|---|---|
| MU multiplexing | Scalar/vector executable checks | No multi-user PPDU or overlap | Check `enableDlMuMimo`, eligible stations, and scheduler logs |
| MU RU width | PCAP timeline/statistics | Treatment data is not 242-tone HE-MU | Check 20 MHz band and scheduler configuration |
| Control separation | PCAP timeline/statistics | Control data is not 106-tone HE-MU | Check the selected `EqualSizedRUs*` configuration |
| Delivered goodput | Scalar/vector table | Result differs from offered-load expectation | Compare application sinks over the stated window before inferring a MAC cause |

## [agent] Reproduction

Run from the INET repository root:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py inspect dl_mu_mimo --suite ax --session-id 20260802T001618Z
python3 examples/ieee80211/analysis/wifi_analysis.py run dl_mu_mimo \
  --suite ax --evidence both --runs 5 --session-id 20260802T001618Z
python3 examples/ieee80211/analysis/wifi_analysis.py report dl_mu_mimo \
  --suite ax --session-id 20260802T001618Z
python3 examples/ieee80211/analysis/wifi_analysis.py publish dl_mu_mimo \
  --suite ax --session-id 20260802T001618Z --update
```

The published session is a five-run, 11-configuration campaign (run numbers `[0,5)`). It retains `.sca` and `.vec` results for every run and `.pcap` captures for representative run 0 from the same session. The analyzer reports successful publication readiness.

## [agent] Scalar and vector analysis

The generated figure and checks answer whether the treatment actually schedules spatially separate users and how the application outcome changes. Goodput is a five-run mean over `[0.55, 0.88)` s with a 95% Student-t confidence interval; stream telemetry is validated over all PPDUs. The checks PASS for each MU configuration: every run contains a multi-user PPDU and no same-timestamp stream ranges overlap.

At 24 Mbit/s both methods deliver approximately the offered load. At 48, 72, and 96 Mbit/s, the table instead shows lower MU-MIMO goodput than its OFDMA counterpart. This is direct scenario evidence, but it does not by itself identify the scheduler or PHY mechanism producing the difference.

<!-- BEGIN GENERATED: ieee80211-scalar-vector-mimo -->
### [script] Generated scalar/vector plot and table

![mimo scalar/vector analysis](results/20260802T090627Z/mu-mimo-spatial-stream-matrix.png)

Figure provenance: [`results/20260802T090627Z/mu-mimo-spatial-stream-matrix.png.json`](results/20260802T090627Z/mu-mimo-spatial-stream-matrix.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.ap.wlan[0].radio / heStaId:vector<br>vector / **.ap.wlan[0].radio / heSpatialStreams:vector<br>vector / **.ap.wlan[0].radio / heStreamStartIndex:vector<br>vector / **.app[*] / packetReceived:vector(packetBytes) / unit=B
- Window / per-run aggregation / exclusions: [0.55, 0.88) s; goodput=per run with 95% Student-t CI; telemetry=all PPDUs validated; representative run 0 plotted
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| MU-MIMO, 24 Mbps, leakage 0.01 / goodput mbps | 24.0194 | 0.0446469 |
| MU-MIMO, 24 Mbps, leakage 0.001 / goodput mbps | 24 | 0 |
| MU-MIMO, 24 Mbps, leakage 0.001, NSS {3,1,2} / goodput mbps | 24 | 0 |
| MU-MIMO, 48 Mbps, leakage 0.01 / goodput mbps | 43.2097 | 2.58926 |
| MU-MIMO, 72 Mbps, leakage 0.01 / goodput mbps | 43.0303 | 3.21763 |
| MU-MIMO, 96 Mbps, leakage 0.01 / goodput mbps | 43.6412 | 2.81226 |
| OFDMA, 24 Mbps / goodput mbps | 23.9903 | 0.0269231 |
| OFDMA, 24 Mbps, NSS {3,1,2} / goodput mbps | 23.9903 | 0.0269231 |
| OFDMA, 48 Mbps / goodput mbps | 48.0145 | 0.0269231 |
| OFDMA, 72 Mbps / goodput mbps | 72.0194 | 0.115409 |
| OFDMA, 96 Mbps / goodput mbps | 95.9467 | 0.247671 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **PASS** | The baseline MU-MIMO PPDU serves multiple users with disjoint spatial streams | Every run contains a multi-user PPDU and all same-timestamp stream ranges are disjoint. |
| **PASS** | The low-leakage 24 Mbit/s MU-MIMO PPDU serves multiple users with disjoint spatial streams | Every run contains a multi-user PPDU and all same-timestamp stream ranges are disjoint. |
| **PASS** | The 48 Mbit/s MU-MIMO PPDU serves multiple users with disjoint spatial streams | Every run contains a multi-user PPDU and all same-timestamp stream ranges are disjoint. |
| **PASS** | The 72 Mbit/s MU-MIMO PPDU serves multiple users with disjoint spatial streams | Every run contains a multi-user PPDU and all same-timestamp stream ranges are disjoint. |
| **PASS** | The 96 Mbit/s MU-MIMO PPDU serves multiple users with disjoint spatial streams | Every run contains a multi-user PPDU and all same-timestamp stream ranges are disjoint. |
| **PASS** | The {3,1,2}-NSS 24 Mbit/s MU-MIMO PPDU serves multiple users with disjoint spatial streams | Every run contains a multi-user PPDU and all same-timestamp stream ranges are disjoint. |
<!-- END GENERATED: ieee80211-scalar-vector-mimo -->

## [agent] PCAP statistics

The script decoded AP-capture observations from representative run 0 of the same session. Its tables distinguish recorded wireless observations from de-duplicated packets and label HE-MU airtime estimates as approximate. In scope, the packet evidence is consistent with the configuration contrast: MU data uses 242-tone RUs and the OFDMA controls use 106-tone RUs. The detailed modulation and airtime rows are observations for this run, not five-run estimates or a causal explanation of goodput.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260802T231452Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260802T231452Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260802T231452Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260802T231452Z.json` (SHA-256 `c550e8b54948566460671e6cd24014629f90abf87ac4d6312760a7d73c0372f7`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

<small>

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `SuMimo24Mbps` | `none (all decoded frames)` | 1908 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (948), Control: Ack (948), Management: Action (6) | 60.94% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `OfdmaSuMimo24Mbps` | `none (all decoded frames)` | 4575 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] (1398), Control: Trigger (700), QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] (699) | 64.68% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `DlMuMimo24MbpsLeakage001` | `none (all decoded frames)` | 4978 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] (1750), Control: Trigger (710), QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] (359) | 55.37% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `SuMimo48Mbps` | `none (all decoded frames)` | 1910 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (949), Control: Ack (949), Management: Action (6) | 61.00% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `OfdmaSuMimo48Mbps` | `none (all decoded frames)` | 6173 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (1397), QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] (1396), QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] (1395) | 72.75% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `DlMuMimo48MbpsLeakage001` | `none (all decoded frames)` | 9128 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] (3648), QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] (1400), QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] (1400) | 70.27% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `SuMimo72Mbps` | `none (all decoded frames)` | 1910 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (949), Control: Ack (949), Management: Action (6) | 61.00% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `OfdmaSuMimo72Mbps` | `none (all decoded frames)` | 7812 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] (2095), QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (2093), QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] (2092) | 93.49% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `DlMuMimo72MbpsLeakage001` | `none (all decoded frames)` | 10514 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] (3117), QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] (2098), QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] (2098) | 77.65% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

</small>

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | DlMuMimo24MbpsLeakage001 produced protocol-visible wireless observations | 4978 AP/global transmission observations |
| **PASS** | DlMuMimo48MbpsLeakage001 produced protocol-visible wireless observations | 9128 AP/global transmission observations |
| **PASS** | DlMuMimo72MbpsLeakage001 produced protocol-visible wireless observations | 10514 AP/global transmission observations |
| **PASS** | OfdmaSuMimo24Mbps produced protocol-visible wireless observations | 4575 AP/global transmission observations |
| **PASS** | OfdmaSuMimo48Mbps produced protocol-visible wireless observations | 6173 AP/global transmission observations |
| **PASS** | OfdmaSuMimo72Mbps produced protocol-visible wireless observations | 7812 AP/global transmission observations |
| **PASS** | SuMimo24Mbps produced protocol-visible wireless observations | 1908 AP/global transmission observations |
| **PASS** | SuMimo48Mbps produced protocol-visible wireless observations | 1910 AP/global transmission observations |
| **PASS** | SuMimo72Mbps produced protocol-visible wireless observations | 1910 AP/global transmission observations |
| **PASS** | Multiple users with disjoint stream allocations in one PPDU | Decoded Radiotap HE-MU (bit 24) headers and heStreamStartIndex spatial stream allocations prove multi-user spatial stream separation |

### [script] Configuration: `SuMimo24Mbps`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1908**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 948 | 49.69% | 1063.2 B | 50.5 B | 617.6 us | 27.7 us | 5010 MHz | - | 20.0 dBm | 96.07% | 58.54% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 948 | 49.69% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -53.3 dBm | - | 3.84% | 2.34% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 6 | 0.31% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -53.3 dBm | 20.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 6 | 0.31% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -53.3 dBm | 20.0 dBm | 0.07% | 0.04% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 1 | 0.200148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200196000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200292000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200406000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 7 | 0.200615000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.200663000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.200759000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.200941000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.200989000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.201085000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.201199000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.201332000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.300644000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.300692000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 21 | 0.301397000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.301445000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 23 | 0.302132000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 24 | 0.302180000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 25 | 0.302858000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 26 | 0.302906000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 27 | 0.303584000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 28 | 0.303632000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 559 | 0.500297000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=91, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 560 | 0.500345000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 561 | 0.501041000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=91, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 562 | 0.501089000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 563 | 0.501794000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=91, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 564 | 0.501842000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 565 | 0.502547000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=92, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 566 | 0.502595000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 567 | 0.503291000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=92, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 568 | 0.503339000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 569 | 0.504044000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=92, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 570 | 0.504093000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 571 | 0.504771000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=93, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 572 | 0.504819000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 573 | 0.505506000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=93, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 574 | 0.505554000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 575 | 0.506232000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=93, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 576 | 0.506280000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 577 | 0.506967000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=94, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 578 | 0.507015000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

</small>

Frame numbers are local to capture `SuMimo24Mbps-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `OfdmaSuMimo24Mbps`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **4575**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 363 | 7.93% | 1066.0 B | 0.0 B | 236.6 us | 1.9 us | 5010 MHz | - | 20.0 dBm | 13.28% | 8.59% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | 0x0000 | 677 | 14.80% | 1066.0 B | 0.0 B | 118.4 us | 18.0 us | 5010 MHz | - | 20.0 dBm | 12.39% | 8.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | 0x0000 | 699 | 15.28% | 1066.0 B | 0.0 B | 102.9 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 11.12% | 7.19% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 363 | 7.93% | 1058.6 B | 81.5 B | 615.0 us | 44.6 us | 5010 MHz | - | 20.0 dBm | 34.52% | 22.33% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 700 | 15.30% | 46.0 B | 0.0 B | 35.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 3.82% | 2.47% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | 0x0000 | 1398 | 30.56% | 32.0 B | 0.0 B | 108.3 us | 0.0 us | 5005 MHz, 5015 MHz | -53.0 dBm | - | 23.40% | 15.14% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 363 | 7.93% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -55.7 dBm | - | 1.38% | 0.90% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 6 | 0.13% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -53.3 dBm | 20.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 6 | 0.13% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -53.3 dBm | 20.0 dBm | 0.06% | 0.04% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 1 | 0.200148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200196000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200292000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200406000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 7 | 0.200615000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.200663000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.200759000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.200941000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.200989000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.201085000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.201199000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.201332000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.300644000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.300692000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 21 | 0.301041000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=2654436069 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 22 | 0.301041000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=1013903406 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 23 | 0.301097000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 24 | 0.301247000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013903406 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 25 | 0.301247000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654436069 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 26 | 0.301647000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=2654436785 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 27 | 0.301647000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=1013905274 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 28 | 0.301703000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 29 | 0.301853000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654436785 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 30 | 0.301853000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013905274 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 31 | 0.302604000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 32 | 0.302652000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 33 | 0.303033000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=2654436683 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 34 | 0.303033000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=1013905280 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 35 | 0.303089000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 36 | 0.303239000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654436683 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 37 | 0.303240000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013905280 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 38 | 0.303666000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=2654436391 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 39 | 0.303666000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=2654436391 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 40 | 0.303666000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=1013905132 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 41 | 0.303722000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 42 | 0.303872000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013905132 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 43 | 0.303872000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654436391 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 1321 | 0.500372000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=200, frag=0, more-frag=0, TID=6, A-MPDU=2654461193 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 1322 | 0.500372000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=201, frag=0, more-frag=0, TID=6, A-MPDU=2654461193 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 1323 | 0.500372000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=201, frag=0, more-frag=0, TID=6, A-MPDU=1013864386 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1324 | 0.500428000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 1325 | 0.500578000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013864386 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 1326 | 0.500578000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654461193 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 1327 | 0.501329000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=201, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 1328 | 0.501377000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 1329 | 0.501749000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=202, frag=0, more-frag=0, TID=6, A-MPDU=2654460953 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 1330 | 0.501749000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=202, frag=0, more-frag=0, TID=6, A-MPDU=1013864146 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1331 | 0.501805000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 1332 | 0.501956000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654460953 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 1333 | 0.501956000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013864146 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 1334 | 0.502355000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=202, frag=0, more-frag=0, TID=6, A-MPDU=2654461941 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 1335 | 0.502355000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=203, frag=0, more-frag=0, TID=6, A-MPDU=2654461941 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 1336 | 0.502355000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=203, frag=0, more-frag=0, TID=6, A-MPDU=1013863742 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1337 | 0.502411000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 1338 | 0.502562000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013863742 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 1339 | 0.502562000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654461941 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 1340 | 0.503285000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=203, frag=0, more-frag=0, TID=6 |

</small>

Frame numbers are local to capture `OfdmaSuMimo24Mbps-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `DlMuMimo24MbpsLeakage001`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **4978**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 359 | 7.21% | 1066.0 B | 0.0 B | 152.3 us | 3.3 us | 5010 MHz | - | 20.0 dBm | 9.88% | 5.47% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2aa73b" /></svg> | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 341 | 6.85% | 1066.0 B | 0.0 B | 133.2 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 8.20% | 4.54% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | 0x0000 | 359 | 7.21% | 1066.0 B | 0.0 B | 84.3 us | 3.3 us | 5010 MHz | - | 20.0 dBm | 5.47% | 3.03% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#45c931" /></svg> | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | 0x0000 | 341 | 6.85% | 1066.0 B | 0.0 B | 79.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 4.91% | 2.72% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | 0x0000 | 356 | 7.15% | 1066.0 B | 0.0 B | 65.2 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 4.19% | 2.32% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 347 | 6.97% | 1058.2 B | 83.3 B | 614.9 us | 45.6 us | 5010 MHz | - | 20.0 dBm | 38.53% | 21.34% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 710 | 14.26% | 50.4 B | 4.7 B | 36.8 us | 1.6 us | 5010 MHz | - | 20.0 dBm | 4.72% | 2.61% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | 0x0000 | 1750 | 35.15% | 32.0 B | 0.0 B | 67.5 us | 0.0 us | 5010 MHz | -53.6 dBm | - | 21.34% | 11.81% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 347 | 6.97% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -52.0 dBm | - | 1.55% | 0.86% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 6 | 0.12% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -53.3 dBm | 20.0 dBm | 0.03% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 19 | 0.38% | 36.1 B | 1.5 B | 68.1 us | 2.0 us | 5010 MHz | -53.3 dBm | 20.0 dBm | 0.23% | 0.13% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#cb1a20" /></svg> | Management: Action [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, BCC] | 0x0000 | 6 | 0.12% | 34.0 B | 0.0 B | 112.8 us | 0.0 us | 5005 MHz, 5015 MHz | -54.0 dBm | - | 0.12% | 0.07% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ce0d17" /></svg> | Management: Action [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, BCC] | 0x0000 | 3 | 0.06% | 34.0 B | 0.0 B | 69.5 us | 0.0 us | 5010 MHz | -52.0 dBm | - | 0.04% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#de1c12" /></svg> | Management: Action [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, BCC] | 0x0000 | 21 | 0.42% | 34.0 B | 0.0 B | 199.2 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -53.3 dBm | - | 0.76% | 0.42% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#82cc33" /></svg> | A-MPDU Delimiter / Aggregation Overhead | - | 13 | 0.26% | 0.0 B | 0.0 B | 20.0 us | 0.0 us | 5010 MHz | - | - | 0.05% | 0.03% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 1 | 0.200148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200196000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200292000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200406000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 7 | 0.200615000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.200663000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.200759000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.200941000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.200989000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.201085000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.201199000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.201332000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 21 | 0.300220000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 25 | 0.300617000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 29 | 0.301206000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=2654436813 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 30 | 0.301206000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=1013905158 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 31 | 0.301206000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=3668339039 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 32 | 0.301262000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 33 | 0.301393000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654436813 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 34 | 0.301393000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013905158 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 35 | 0.301393000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=3668339039 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 36 | 0.301775000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=2654436584 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 37 | 0.301775000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=1013904931 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 38 | 0.301775000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=3668338810 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 39 | 0.301831000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 40 | 0.301962000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654436584 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 41 | 0.301962000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013904931 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 42 | 0.301962000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=3668338810 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 43 | 0.302344000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=2654437271 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 44 | 0.302344000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=1013904732 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 45 | 0.302344000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=3668339461 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 46 | 0.302400000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 47 | 0.302531000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654437271 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 48 | 0.302531000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013904732 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 49 | 0.302531000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=3668339461 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 50 | 0.303644000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 51 | 0.303692000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#45c931" /></svg> | 52 | 0.303977000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=2654437104 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2aa73b" /></svg> | 53 | 0.303977000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=1013904443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 1439 | 0.500644000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=201, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 1440 | 0.500692000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#45c931" /></svg> | 1441 | 0.500977000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=201, frag=0, more-frag=0, TID=6, A-MPDU=2654457380 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2aa73b" /></svg> | 1442 | 0.500977000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=201, frag=0, more-frag=0, TID=6, A-MPDU=1013859567 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1443 | 0.501033000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 1444 | 0.501148000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654457380 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 1445 | 0.501148000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013859567 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 1446 | 0.501512000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=202, frag=0, more-frag=0, TID=6, A-MPDU=2654454256 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 1447 | 0.501512000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=202, frag=0, more-frag=0, TID=6, A-MPDU=1013861179 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 1448 | 0.501512000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=202, frag=0, more-frag=0, TID=6, A-MPDU=3668366690 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1449 | 0.501568000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 1450 | 0.501699000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654454256 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 1451 | 0.501699000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013861179 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 1452 | 0.501699000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=3668366690 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1455 | 0.502220000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1459 | 0.502599000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 1463 | 0.503170000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=203, frag=0, more-frag=0, TID=6, A-MPDU=2654454725 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 1464 | 0.503170000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=203, frag=0, more-frag=0, TID=6, A-MPDU=1013860622 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 1465 | 0.503170000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=203, frag=0, more-frag=0, TID=6, A-MPDU=3668367191 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1466 | 0.503226000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

</small>

Frame numbers are local to capture `DlMuMimo24MbpsLeakage001-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `SuMimo48Mbps`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1910**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 949 | 49.69% | 1063.2 B | 50.5 B | 617.6 us | 27.6 us | 5010 MHz | - | 20.0 dBm | 96.07% | 58.61% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 949 | 49.69% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -53.3 dBm | - | 3.84% | 2.34% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 6 | 0.31% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -53.3 dBm | 20.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 6 | 0.31% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -53.3 dBm | 20.0 dBm | 0.07% | 0.04% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 1 | 0.200148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200196000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200292000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200406000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 7 | 0.200615000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.200663000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.200759000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.200941000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.200989000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.201085000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.201199000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.201332000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.300644000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.300692000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 21 | 0.301370000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.301418000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 23 | 0.302096000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 24 | 0.302144000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 25 | 0.302849000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 26 | 0.302897000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 27 | 0.303575000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 28 | 0.303623000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 559 | 0.500333000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=91, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 560 | 0.500381000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 561 | 0.501059000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=91, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 562 | 0.501107000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 563 | 0.501812000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=91, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 564 | 0.501860000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 565 | 0.502547000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=92, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 566 | 0.502595000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 567 | 0.503291000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=92, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 568 | 0.503339000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 569 | 0.504044000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=92, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 570 | 0.504093000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 571 | 0.504771000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=93, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 572 | 0.504819000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 573 | 0.505497000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=93, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 574 | 0.505545000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 575 | 0.506250000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=93, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 576 | 0.506298000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 577 | 0.507003000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=94, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 578 | 0.507051000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

</small>

Frame numbers are local to capture `SuMimo48Mbps-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `OfdmaSuMimo48Mbps`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **6173**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 1397 | 22.63% | 1066.0 B | 0.0 B | 209.1 us | 15.3 us | 5010 MHz | - | 20.0 dBm | 40.16% | 29.22% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | 0x0000 | 1395 | 22.60% | 1066.0 B | 0.0 B | 108.8 us | 15.3 us | 5010 MHz | - | 20.0 dBm | 20.86% | 15.18% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | 0x0000 | 1396 | 22.61% | 1066.0 B | 0.0 B | 83.7 us | 18.0 us | 5010 MHz | - | 20.0 dBm | 16.07% | 11.69% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 4 | 0.06% | 391.0 B | 389.7 B | 249.9 us | 213.2 us | 5010 MHz | - | 20.0 dBm | 0.14% | 0.10% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 655 | 10.61% | 46.0 B | 0.0 B | 35.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 3.18% | 2.31% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | 0x0000 | 1310 | 21.22% | 32.0 B | 0.0 B | 108.3 us | 0.0 us | 5005 MHz, 5015 MHz | -53.0 dBm | - | 19.50% | 14.18% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 4 | 0.06% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -53.0 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 6 | 0.10% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -53.3 dBm | 20.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 6 | 0.10% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -53.3 dBm | 20.0 dBm | 0.06% | 0.04% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 1 | 0.200148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200196000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200292000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200406000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 7 | 0.200615000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.200663000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.200759000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.200941000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.200989000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.201085000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.201199000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.201332000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.300644000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.300692000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 21 | 0.301222000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=2654436055 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 22 | 0.301222000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=2654436055 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 23 | 0.301222000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=1013903388 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 24 | 0.301222000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=1013903388 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 25 | 0.301278000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 26 | 0.301428000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013903388 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 27 | 0.301428000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654436055 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 28 | 0.302063000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=2654436737 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 29 | 0.302063000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=2654436737 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 30 | 0.302063000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=2654436737 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 31 | 0.302063000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=1013905226 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 32 | 0.302063000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=1013905226 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 33 | 0.302119000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 34 | 0.302269000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654436737 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 35 | 0.302269000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013905226 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 36 | 0.302783000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=2654436687 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 37 | 0.302783000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=2654436687 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 38 | 0.302783000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=2654436687 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 39 | 0.302783000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=1013905284 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 40 | 0.302839000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 41 | 0.302989000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013905284 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 42 | 0.302989000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654436687 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 43 | 0.303807000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=2654436473 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 44 | 0.303807000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=2654436473 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 45 | 0.303807000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=2654436473 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 46 | 0.303807000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=1013905074 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 47 | 0.303807000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=1013905074 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 48 | 0.303863000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 1775 | 0.500656000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=396, frag=0, more-frag=0, TID=6, A-MPDU=2654460463 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 1776 | 0.500656000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=397, frag=0, more-frag=0, TID=6, A-MPDU=2654460463 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 1777 | 0.500656000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=398, frag=0, more-frag=0, TID=6, A-MPDU=2654460463 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 1778 | 0.500656000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=399, frag=0, more-frag=0, TID=6, A-MPDU=2654460463 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 1779 | 0.500656000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=400, frag=0, more-frag=0, TID=6, A-MPDU=2654460463 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 1780 | 0.500656000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=399, frag=0, more-frag=0, TID=6, A-MPDU=1013862628 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 1781 | 0.500656000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=400, frag=0, more-frag=0, TID=6, A-MPDU=1013862628 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1782 | 0.500712000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 1783 | 0.500862000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013862628 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 1784 | 0.500862000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654460463 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 1785 | 0.501479000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=399, frag=0, more-frag=0, TID=6, A-MPDU=2654461387 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 1786 | 0.501479000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=400, frag=0, more-frag=0, TID=6, A-MPDU=2654461387 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 1787 | 0.501479000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=401, frag=0, more-frag=0, TID=6, A-MPDU=2654461387 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 1788 | 0.501479000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=402, frag=0, more-frag=0, TID=6, A-MPDU=2654461387 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 1789 | 0.501479000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=401, frag=0, more-frag=0, TID=6, A-MPDU=1013864192 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 1790 | 0.501479000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=402, frag=0, more-frag=0, TID=6, A-MPDU=1013864192 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1791 | 0.501535000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 1792 | 0.501685000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013864192 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 1793 | 0.501685000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654461387 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 1794 | 0.502720000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=401, frag=0, more-frag=0, TID=6, A-MPDU=2654461169 |

</small>

Frame numbers are local to capture `OfdmaSuMimo48Mbps-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `DlMuMimo48MbpsLeakage001`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **9128**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 1400 | 15.34% | 1066.0 B | 0.0 B | 147.9 us | 12.1 us | 5010 MHz | - | 20.0 dBm | 29.47% | 20.71% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | 0x0000 | 1400 | 15.34% | 1066.0 B | 0.0 B | 79.9 us | 12.1 us | 5010 MHz | - | 20.0 dBm | 15.92% | 11.18% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | 0x0000 | 1400 | 15.34% | 1066.0 B | 0.0 B | 60.4 us | 12.1 us | 5010 MHz | - | 20.0 dBm | 12.04% | 8.46% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 3 | 0.03% | 166.0 B | 0.0 B | 126.8 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.05% | 0.04% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 1224 | 13.41% | 54.9 B | 0.9 B | 38.3 us | 0.3 us | 5010 MHz | - | 20.0 dBm | 6.67% | 4.69% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | 0x0000 | 3648 | 39.96% | 32.0 B | 0.0 B | 67.5 us | 0.0 us | 5010 MHz | -53.3 dBm | - | 35.05% | 24.63% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 3 | 0.03% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -53.3 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 6 | 0.07% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -53.3 dBm | 20.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 14 | 0.15% | 36.7 B | 1.0 B | 69.0 us | 1.4 us | 5010 MHz | -53.3 dBm | 20.0 dBm | 0.14% | 0.10% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ce0d17" /></svg> | Management: Action [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, BCC] | 0x0000 | 1 | 0.01% | 34.0 B | 0.0 B | 69.5 us | 0.0 us | 5010 MHz | -52.0 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#de1c12" /></svg> | Management: Action [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, BCC] | 0x0000 | 21 | 0.23% | 34.0 B | 0.0 B | 199.2 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -53.3 dBm | - | 0.60% | 0.42% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#82cc33" /></svg> | A-MPDU Delimiter / Aggregation Overhead | - | 8 | 0.09% | 0.0 B | 0.0 B | 20.0 us | 0.0 us | 5010 MHz | - | - | 0.02% | 0.02% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 1 | 0.200148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200196000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200292000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200406000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 7 | 0.200615000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.200663000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.200759000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.200941000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.200989000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.201085000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.201199000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.201332000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 21 | 0.300220000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 25 | 0.300617000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 29 | 0.301300000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=2654436671 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 30 | 0.301300000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=2654436671 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 31 | 0.301300000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=1013905396 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 32 | 0.301300000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=1013905396 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 33 | 0.301300000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=3668339117 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 34 | 0.301300000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=3668339117 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 35 | 0.301356000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 36 | 0.301487000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654436671 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 37 | 0.301487000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013905396 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 38 | 0.301487000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=3668339117 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 39 | 0.301972000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=2654436414 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 40 | 0.301972000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=2654436414 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 41 | 0.301972000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=1013905141 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 42 | 0.301972000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=1013905141 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 43 | 0.301972000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=3668338860 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 44 | 0.301972000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=3668338860 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 45 | 0.302028000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 46 | 0.302159000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654436414 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 47 | 0.302159000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013905141 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 48 | 0.302159000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=3668338860 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 49 | 0.302514000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=2654437327 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 50 | 0.302514000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=1013904644 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 51 | 0.302514000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=3668339549 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 52 | 0.302570000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 53 | 0.302701000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654437327 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 54 | 0.302701000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013904644 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 55 | 0.302701000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=3668339549 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 56 | 0.303065000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=2654437098 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 57 | 0.303065000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=1013904417 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 58 | 0.303065000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=3668339320 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 59 | 0.303121000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 60 | 0.303252000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654437098 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 61 | 0.303252000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013904417 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 62 | 0.303252000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=3668339320 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 63 | 0.303607000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=2654433673 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 64 | 0.303607000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=1013906242 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 65 | 0.303607000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=3668337947 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 66 | 0.303663000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 67 | 0.303794000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654433673 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 68 | 0.303794000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013906242 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 69 | 0.303794000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=3668337947 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 2620 | 0.500028000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=400, frag=0, more-frag=0, TID=6, A-MPDU=2654355868 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 2621 | 0.500028000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=400, frag=0, more-frag=0, TID=6, A-MPDU=1013959511 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 2622 | 0.500028000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=400, frag=0, more-frag=0, TID=6, A-MPDU=3668399374 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 2623 | 0.500084000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 2624 | 0.500215000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654355868 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 2625 | 0.500215000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013959511 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 2626 | 0.500215000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=3668399374 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 2627 | 0.500588000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=401, frag=0, more-frag=0, TID=6, A-MPDU=2654355643 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 2628 | 0.500588000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=401, frag=0, more-frag=0, TID=6, A-MPDU=1013959280 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 2629 | 0.500588000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=401, frag=0, more-frag=0, TID=6, A-MPDU=3668399145 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 2630 | 0.500644000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 2631 | 0.500775000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654355643 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 2632 | 0.500775000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013959280 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 2633 | 0.500775000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=3668399145 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 2634 | 0.501130000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=402, frag=0, more-frag=0, TID=6, A-MPDU=2654355558 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 2635 | 0.501130000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=402, frag=0, more-frag=0, TID=6, A-MPDU=1013959341 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 2636 | 0.501130000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=402, frag=0, more-frag=0, TID=6, A-MPDU=3668399348 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 2637 | 0.501186000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 2638 | 0.501317000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654355558 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 2639 | 0.501317000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013959341 |

</small>

Frame numbers are local to capture `DlMuMimo48MbpsLeakage001-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `SuMimo72Mbps`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1910**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 949 | 49.69% | 1063.2 B | 50.5 B | 617.6 us | 27.6 us | 5010 MHz | - | 20.0 dBm | 96.07% | 58.61% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 949 | 49.69% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -53.3 dBm | - | 3.84% | 2.34% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 6 | 0.31% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -53.3 dBm | 20.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 6 | 0.31% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -53.3 dBm | 20.0 dBm | 0.07% | 0.04% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 1 | 0.200148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200196000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200292000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200406000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 7 | 0.200615000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.200663000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.200759000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.200941000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.200989000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.201085000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.201199000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.201332000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.300644000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.300692000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 21 | 0.301379000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.301427000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 23 | 0.302114000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 24 | 0.302162000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 25 | 0.302867000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 26 | 0.302915000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 27 | 0.303602000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 28 | 0.303650000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 559 | 0.500243000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=91, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 560 | 0.500291000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 561 | 0.500978000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=91, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 562 | 0.501026000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 563 | 0.501713000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=91, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 564 | 0.501761000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 565 | 0.502457000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=92, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 566 | 0.502505000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 567 | 0.503183000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=92, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 568 | 0.503231000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 569 | 0.503918000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=92, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 570 | 0.503967000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 571 | 0.504663000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=93, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 572 | 0.504711000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 573 | 0.505416000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=93, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 574 | 0.505464000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 575 | 0.506160000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=93, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 576 | 0.506208000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 577 | 0.506886000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=94, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 578 | 0.506934000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

</small>

Frame numbers are local to capture `SuMimo72Mbps-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `OfdmaSuMimo72Mbps`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **7812**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 2093 | 26.79% | 1066.0 B | 0.0 B | 207.9 us | 14.4 us | 5010 MHz | - | 20.0 dBm | 46.54% | 43.51% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | 0x0000 | 2092 | 26.78% | 1066.0 B | 0.0 B | 104.7 us | 11.7 us | 5010 MHz | - | 20.0 dBm | 23.42% | 21.90% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | 0x0000 | 2095 | 26.82% | 1066.0 B | 0.0 B | 72.6 us | 13.2 us | 5010 MHz | - | 20.0 dBm | 16.28% | 15.22% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 4 | 0.05% | 391.0 B | 389.7 B | 249.9 us | 213.2 us | 5010 MHz | - | 20.0 dBm | 0.11% | 0.10% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 504 | 6.45% | 46.0 B | 0.0 B | 35.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 1.90% | 1.78% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | 0x0000 | 1008 | 12.90% | 32.0 B | 0.0 B | 108.3 us | 0.0 us | 5005 MHz, 5015 MHz | -53.7 dBm | - | 11.67% | 10.91% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 4 | 0.05% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -53.0 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 6 | 0.08% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -53.3 dBm | 20.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 6 | 0.08% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -53.3 dBm | 20.0 dBm | 0.04% | 0.04% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 1 | 0.200148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200196000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200292000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200406000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 7 | 0.200615000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.200663000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.200759000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.200941000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.200989000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.201085000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.201199000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.201332000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.300644000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.300692000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 21 | 0.301423000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=2654435897 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 22 | 0.301423000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=2654435897 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 23 | 0.301423000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=2654435897 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 24 | 0.301423000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=1013903602 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 25 | 0.301423000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=1013903602 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 26 | 0.301423000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=1013903602 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 27 | 0.301479000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 28 | 0.301629000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013903602 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 29 | 0.301629000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654435897 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 30 | 0.302429000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=2654436817 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 31 | 0.302429000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=2654436817 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 32 | 0.302429000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=2654436817 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 33 | 0.302429000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=2654436817 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 34 | 0.302429000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=2654436817 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 35 | 0.302429000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=1013905178 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 36 | 0.302429000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=1013905178 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 37 | 0.302429000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=1013905178 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 38 | 0.302485000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 39 | 0.302635000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654436817 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 40 | 0.302635000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013905178 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 41 | 0.303462000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=2654436581 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 42 | 0.303462000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=2654436581 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 43 | 0.303462000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=2654436581 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 44 | 0.303462000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=2654436581 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 45 | 0.303462000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6, A-MPDU=2654436581 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 46 | 0.303462000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6, A-MPDU=2654436581 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 47 | 0.303462000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=1013904942 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 48 | 0.303462000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6, A-MPDU=1013904942 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 49 | 0.303462000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6, A-MPDU=1013904942 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 50 | 0.303518000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 51 | 0.303668000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013904942 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 52 | 0.303668000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654436581 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 2233 | 0.500929000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=593, frag=0, more-frag=0, TID=6, A-MPDU=2654461611 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 2234 | 0.500929000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=594, frag=0, more-frag=0, TID=6, A-MPDU=2654461611 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 2235 | 0.500929000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=595, frag=0, more-frag=0, TID=6, A-MPDU=2654461611 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 2236 | 0.500929000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=596, frag=0, more-frag=0, TID=6, A-MPDU=2654461611 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 2237 | 0.500929000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=597, frag=0, more-frag=0, TID=6, A-MPDU=2654461611 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 2238 | 0.500929000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=598, frag=0, more-frag=0, TID=6, A-MPDU=2654461611 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 2239 | 0.500929000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=599, frag=0, more-frag=0, TID=6, A-MPDU=2654461611 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae228" /></svg> | 2240 | 0.500929000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=600, frag=0, more-frag=0, TID=6, A-MPDU=2654461611 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 2241 | 0.500929000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=594, frag=0, more-frag=0, TID=6, A-MPDU=1013863520 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 2242 | 0.500929000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=595, frag=0, more-frag=0, TID=6, A-MPDU=1013863520 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 2243 | 0.500929000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=596, frag=0, more-frag=0, TID=6, A-MPDU=1013863520 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 2244 | 0.500929000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=597, frag=0, more-frag=0, TID=6, A-MPDU=1013863520 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#14b827" /></svg> | 2245 | 0.500929000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=598, frag=0, more-frag=0, TID=6, A-MPDU=1013863520 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 2246 | 0.500985000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 2247 | 0.501135000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654461611 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | 2248 | 0.501135000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013863520 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 2249 | 0.502360000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=597, frag=0, more-frag=0, TID=6, A-MPDU=2654458255 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 2250 | 0.502360000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=598, frag=0, more-frag=0, TID=6, A-MPDU=2654458255 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 2251 | 0.502360000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=599, frag=0, more-frag=0, TID=6, A-MPDU=2654458255 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3bdb29" /></svg> | 2252 | 0.502360000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=600, frag=0, more-frag=0, TID=6, A-MPDU=2654458255 |

</small>

Frame numbers are local to capture `OfdmaSuMimo72Mbps-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `DlMuMimo72MbpsLeakage001`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **10514**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 2098 | 19.95% | 1066.0 B | 0.0 B | 134.5 us | 18.0 us | 5010 MHz | - | 20.0 dBm | 36.33% | 28.21% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | 0x0000 | 2098 | 19.95% | 1066.0 B | 0.0 B | 66.4 us | 18.0 us | 5010 MHz | - | 20.0 dBm | 17.95% | 13.94% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | 0x0000 | 2098 | 19.95% | 1066.0 B | 0.0 B | 47.0 us | 18.0 us | 5010 MHz | - | 20.0 dBm | 12.69% | 9.86% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 3 | 0.03% | 166.0 B | 0.0 B | 126.8 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.05% | 0.04% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 1047 | 9.96% | 54.9 B | 1.0 B | 38.3 us | 0.3 us | 5010 MHz | - | 20.0 dBm | 5.17% | 4.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | 0x0000 | 3117 | 29.65% | 32.0 B | 0.0 B | 67.5 us | 0.0 us | 5010 MHz | -53.3 dBm | - | 27.10% | 21.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 3 | 0.03% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -53.3 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 6 | 0.06% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -53.3 dBm | 20.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 14 | 0.13% | 36.7 B | 1.0 B | 69.0 us | 1.4 us | 5010 MHz | -53.3 dBm | 20.0 dBm | 0.12% | 0.10% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ce0d17" /></svg> | Management: Action [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, BCC] | 0x0000 | 1 | 0.01% | 34.0 B | 0.0 B | 69.5 us | 0.0 us | 5010 MHz | -52.0 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#de1c12" /></svg> | Management: Action [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, BCC] | 0x0000 | 21 | 0.20% | 34.0 B | 0.0 B | 199.2 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -53.3 dBm | - | 0.54% | 0.42% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#82cc33" /></svg> | A-MPDU Delimiter / Aggregation Overhead | - | 8 | 0.08% | 0.0 B | 0.0 B | 20.0 us | 0.0 us | 5010 MHz | - | - | 0.02% | 0.02% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 1 | 0.200148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200196000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200292000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200406000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 7 | 0.200615000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 8 | 0.200663000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.200759000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.200941000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.200989000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.201085000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.201199000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.201332000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 21 | 0.300220000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 25 | 0.300590000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 29 | 0.301385000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=2654436641 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 30 | 0.301385000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=2654436641 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 31 | 0.301385000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=2654436641 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 32 | 0.301385000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=1013905386 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 33 | 0.301385000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=1013905386 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 34 | 0.301385000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=1013905386 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 35 | 0.301385000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=3668339123 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 36 | 0.301385000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=3668339123 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 37 | 0.301385000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=3668339123 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 38 | 0.301441000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 39 | 0.301572000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654436641 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 40 | 0.301572000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013905386 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 41 | 0.301572000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=3668339123 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 42 | 0.302178000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=2654436356 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 43 | 0.302178000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=2654436356 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 44 | 0.302178000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=2654436356 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 45 | 0.302178000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=1013905103 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 46 | 0.302178000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=1013905103 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 47 | 0.302178000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=1013905103 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 48 | 0.302178000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=3668338838 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 49 | 0.302178000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=3668338838 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 50 | 0.302178000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=3668338838 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 51 | 0.302234000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 52 | 0.302365000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654436356 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 53 | 0.302365000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013905103 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 54 | 0.302365000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=3668338838 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 55 | 0.302832000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=2654437225 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 56 | 0.302832000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6, A-MPDU=2654437225 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 57 | 0.302832000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=1013904802 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 58 | 0.302832000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6, A-MPDU=1013904802 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 59 | 0.302832000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=3668339707 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 60 | 0.302832000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=6, A-MPDU=3668339707 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 61 | 0.302888000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 62 | 0.303019000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654437225 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 63 | 0.303019000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013904802 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 64 | 0.303019000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=3668339707 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 65 | 0.303513000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6, A-MPDU=2654436968 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 66 | 0.303513000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=6, A-MPDU=2654436968 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 67 | 0.303513000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6, A-MPDU=1013904547 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 68 | 0.303513000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=6, A-MPDU=1013904547 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 69 | 0.303513000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=6, A-MPDU=3668339450 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 70 | 0.303513000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=6, A-MPDU=3668339450 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 71 | 0.303569000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 72 | 0.303700000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654436968 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 73 | 0.303700000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013904547 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 74 | 0.303700000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=3668339450 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 3017 | 0.500094000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654361229 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 3018 | 0.500094000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013963846 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 3019 | 0.500094000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=3668394527 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 3020 | 0.500570000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=600, frag=0, more-frag=0, TID=6, A-MPDU=2654357900 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 3021 | 0.500570000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=601, frag=0, more-frag=0, TID=6, A-MPDU=2654357900 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 3022 | 0.500570000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=600, frag=0, more-frag=0, TID=6, A-MPDU=1013957447 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 3023 | 0.500570000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=601, frag=0, more-frag=0, TID=6, A-MPDU=1013957447 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 3024 | 0.500570000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=600, frag=0, more-frag=0, TID=6, A-MPDU=3668401438 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 3025 | 0.500570000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=601, frag=0, more-frag=0, TID=6, A-MPDU=3668401438 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 3026 | 0.500626000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 3027 | 0.500757000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=2654357900 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 3028 | 0.500757000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1013957447 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | 3029 | 0.500757000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=3668401438 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 3030 | 0.501224000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=602, frag=0, more-frag=0, TID=6, A-MPDU=2654357647 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35d91c" /></svg> | 3031 | 0.501224000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [HE-MU, HE-MCS 9, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | direction=from DS, retry=0, seq=603, frag=0, more-frag=0, TID=6, A-MPDU=2654357647 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 3032 | 0.501224000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=602, frag=0, more-frag=0, TID=6, A-MPDU=1013957188 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ae32b" /></svg> | 3033 | 0.501224000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | QoS Data [HE-MU, HE-MCS 8, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | direction=from DS, retry=0, seq=603, frag=0, more-frag=0, TID=6, A-MPDU=1013957188 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 3034 | 0.501224000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=602, frag=0, more-frag=0, TID=6, A-MPDU=3668401181 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | 3035 | 0.501224000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=from DS, retry=0, seq=603, frag=0, more-frag=0, TID=6, A-MPDU=3668401181 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 3036 | 0.501280000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

</small>

Frame numbers are local to capture `DlMuMimo72MbpsLeakage001-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Analysis of Packet Distribution
Decoded Radiotap HE-MU (bit 24) headers and spatial stream starting indices (`heStreamStartIndex`) in captured frames directly prove non-overlapping spatial stream allocations across multiplexed users in DL MU-MIMO PPDUs.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->

## [agent] Frame exchange analysis

Read the generated representative timelines as a local ordering proof: HE-MU QoS data rows with equal timestamps show concurrent recipients, while the following control rows show the acknowledgement exchanges selected by the model. The generated final PCAP check is the authoritative statement for decoded stream-start information. The timeline is run-0 evidence only; the scalar/vector checks establish that spatial non-overlap persists across all five runs.

## [agent] Cross-layer findings and verdict

Evidence basis: configuration is a requested input; vectors and decoded PCAP fields are direct observations; the reported goodput and airtime are derived measurements; explanations beyond those observations are inferences. Configuration requests DL MU-MIMO; the five-run vector checks directly show the required spatial non-overlap; and the representative PCAP records HE-MU data on the expected 242-tone RU. Together, those support `PASS` for configured DL MU-MIMO in the six treatment configurations.

The matched application result is `FAIL` for the narrower claim that MU-MIMO improves goodput at the three higher offered loads. The generated scalar/vector table shows the control ahead at 48, 72, and 96 Mbit/s. That result remains deliberately bounded: packet composition, MCS, and airtime from a representative PCAP do not establish why it occurs, and this example does not compare every possible channel, scheduler, or traffic condition.

## [agent] Limitations and inconclusive claims

- PCAP evidence is representative run 0; only the scalar/vector campaign has five-run coverage.
- The selected load points do not locate a saturation threshold or explain the observed goodput ordering. A focused scheduler/queue trace at 48 Mbit/s is the smallest useful next diagnostic.
- `DlMuMimo80MHz` is not part of this session; it needs a separate campaign before any wide-band claim.
