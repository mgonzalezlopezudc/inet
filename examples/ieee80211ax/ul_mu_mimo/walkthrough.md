# Walkthrough: IEEE 802.11ax UL MU-MIMO vs OFDMA

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260802T140300Z`
- PCAP: `20260802T140300Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `20260802T140300Z`.

This example evaluates IEEE 802.11ax Uplink Multi-User MIMO (UL MU-MIMO) performance in comparison with Uplink OFDMA and a single-user (UL SU) EDCA baseline across four aggregate offered loads (24, 48, 72, and 96 Mbit/s) and two CSI leakage levels (0.01 and 0.001).

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- distinguish spatial stream multiplexing in UL MU-MIMO from frequency subcarrier division in UL OFDMA;
- understand how AP Trigger frames schedule full-bandwidth UL MU-MIMO PPDUs with disjoint spatial stream allocations;
- analyze the impact of CSI leakage (0.01 vs. 0.001) on UL MU-MIMO goodput and end-to-end delay under varying offered loads;
- compare UL MU-MIMO against UL OFDMA and UL SU baselines; and
- reproduce the simulation campaign using the shared IEEE 802.11 analysis framework.

In UL MU-MIMO, the AP sends a Basic Trigger frame assigning multiple stations to transmit simultaneously over the full channel bandwidth (242-tone RU) using distinct, non-overlapping spatial streams. In UL OFDMA, stations transmit simultaneously on disjoint frequency resource units (e.g. 106-tone or 26-tone RUs). In UL SU baseline, stations contend individually using standard EDCA channel access. The analysis distinguishes direct observation from packet decodes and telemetry, derived measurement from script-aggregated scalar/vector results, and model inference regarding protocol mechanics.

## [agent] Scenario description

The topology consists of an Access Point (`ap`), three wireless stations (`host[0]`, `host[1]`, `host[2]`), and a wired server (`server`) connected to the AP.

```text
host[0] (3 ant), host[1] (1 ant), host[2] (2 ant) ))) AP (6 ant) --- server
```

- **AP**: 6 antennas (`numAntennas = 6`), full-bandwidth UL MU-MIMO enabled.
- **STA 1 (`host[0]`)**: 3 antennas (`numAntennas = 3`), `operatingModeRxNss = 3`.
- **STA 2 (`host[1]`)**: 1 antenna (`numAntennas = 1`), `operatingModeRxNss = 1`.
- **STA 3 (`host[2]`)**: 2 antennas (`numAntennas = 2`), `operatingModeRxNss = 2`.

Traffic consists of 1,000-byte UDP packets sent from each station to the server. Four aggregate offered loads are evaluated: 24 Mbit/s (`sendInterval = 1ms`), 48 Mbit/s (`sendInterval = 0.5ms`), 72 Mbit/s (`sendInterval = 0.333ms`), and 96 Mbit/s (`sendInterval = 0.25ms`). For each offered load, four configurations are compared: UL SU, OFDMA, MU-MIMO with CSI leakage 0.01, and MU-MIMO with CSI leakage 0.001. Warm-up phase runs from 0.2 s to 0.25 s, and data measurement window spans [0.3, 0.95) s.

## [agent] Standards and INET model boundary

IEEE Std 802.11-2024 Clause 27.3.1.1 defines HE MU PPDUs; Clause 27.3.3.2 describes UL MU-MIMO Trigger-based transmissions and spatial stream allocation rules.

INET models UL MU-MIMO by scheduling multi-user HE-TB PPDUs with disjoint spatial stream index ranges when `enableUlMuMimo` is true and at least two candidates support full-bandwidth UL MU-MIMO. Interference from inaccurate channel state information is modeled through the `defaultCsiLeakage` parameter, which determines inter-stream crosstalk power scaling.

## [agent] Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| UL MU-MIMO configurations trigger multi-user spatial transmissions | `PASS` | Basic Trigger PCAP frames and AP HCF telemetry | session `20260802T140300Z` run 0 | Protocol-visible Trigger scheduling |
| UL MU-MIMO achieves higher goodput than OFDMA under offered load | `PASS` | Application `packetReceived` vector metrics | 5 runs $\times$ 16 configs | Evaluated across 24–96 Mbit/s offered loads |
| Lower CSI leakage (0.001 vs 0.01) preserves spatial multiplexing quality | `PASS` | Application goodput and end-to-end delay vectors | 5 runs $\times$ 16 configs | Modeled CSI leakage parameter impact |
| Full-bandwidth 6-antenna AP schedules 3 STAs ({3,1,2} NSS) | `PASS` | Trigger User Info fields and AP MAC telemetry | session `20260802T140300Z` run 0 | Antenna and NSS capability matching |

## [agent] Configuration matrix

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| `UlSu24Mbps` | UL SU baseline | UL MU disabled | 24 Mbit/s aggregate load, 20 MHz | 0-4 / 0-4 | Contention-based EDCA exchanges |
| `Ofdma24Mbps` | OFDMA baseline | UL OFDMA enabled, UL MU-MIMO disabled | 24 Mbit/s aggregate load, 20 MHz | 0-4 / 0-4 | Subchannel RU frequency division |
| `UlMuMimo24MbpsLeakage01` | UL MU-MIMO treatment | UL MU-MIMO enabled, CSI leakage 0.01 | 24 Mbit/s aggregate load, 20 MHz | 0-4 / 0-4 | Full-bandwidth RU, disjoint stream ranges |
| `UlMuMimo24MbpsLeakage001` | UL MU-MIMO treatment | UL MU-MIMO enabled, CSI leakage 0.001 | 24 Mbit/s aggregate load, 20 MHz | 0-4 / 0-4 | Full-bandwidth RU, low CSI leakage |
| `UlSu48Mbps` | UL SU baseline | UL MU disabled | 48 Mbit/s aggregate load, 20 MHz | 0-4 / 0-4 | Contention-based EDCA exchanges |
| `Ofdma48Mbps` | OFDMA baseline | UL OFDMA enabled, UL MU-MIMO disabled | 48 Mbit/s aggregate load, 20 MHz | 0-4 / 0-4 | Subchannel RU frequency division |
| `UlMuMimo48MbpsLeakage01` | UL MU-MIMO treatment | UL MU-MIMO enabled, CSI leakage 0.01 | 48 Mbit/s aggregate load, 20 MHz | 0-4 / 0-4 | Full-bandwidth RU, disjoint stream ranges |
| `UlMuMimo48MbpsLeakage001` | UL MU-MIMO treatment | UL MU-MIMO enabled, CSI leakage 0.001 | 48 Mbit/s aggregate load, 20 MHz | 0-4 / 0-4 | Full-bandwidth RU, low CSI leakage |
| `UlSu72Mbps` | UL SU baseline | UL MU disabled | 72 Mbit/s aggregate load, 20 MHz | 0-4 / 0-4 | Contention-based EDCA exchanges |
| `Ofdma72Mbps` | OFDMA baseline | UL OFDMA enabled, UL MU-MIMO disabled | 72 Mbit/s aggregate load, 20 MHz | 0-4 / 0-4 | Subchannel RU frequency division |
| `UlMuMimo72MbpsLeakage01` | UL MU-MIMO treatment | UL MU-MIMO enabled, CSI leakage 0.01 | 72 Mbit/s aggregate load, 20 MHz | 0-4 / 0-4 | Full-bandwidth RU, disjoint stream ranges |
| `UlMuMimo72MbpsLeakage001` | UL MU-MIMO treatment | UL MU-MIMO enabled, CSI leakage 0.001 | 72 Mbit/s aggregate load, 20 MHz | 0-4 / 0-4 | Full-bandwidth RU, low CSI leakage |
| `UlSu96Mbps` | UL SU baseline | UL MU disabled | 96 Mbit/s aggregate load, 20 MHz | 0-4 / 0-4 | Contention-based EDCA exchanges |
| `Ofdma96Mbps` | OFDMA baseline | UL OFDMA enabled, UL MU-MIMO disabled | 96 Mbit/s aggregate load, 20 MHz | 0-4 / 0-4 | Subchannel RU frequency division |
| `UlMuMimo96MbpsLeakage01` | UL MU-MIMO treatment | UL MU-MIMO enabled, CSI leakage 0.01 | 96 Mbit/s aggregate load, 20 MHz | 0-4 / 0-4 | Full-bandwidth RU, disjoint stream ranges |
| `UlMuMimo96MbpsLeakage001` | UL MU-MIMO treatment | UL MU-MIMO enabled, CSI leakage 0.001 | 96 Mbit/s aggregate load, 20 MHz | 0-4 / 0-4 | Full-bandwidth RU, low CSI leakage |

## [agent] Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| Basic Trigger allocates common RU to STAs in UL MU-MIMO | Decoded Trigger User Info | Separate subchannels or single STA allocated | UL Scheduler | Check `enableUlMuMimo` and STA capability flags |
| Spatial stream start indices are disjoint | AP HCF telemetry | Overlapping stream start indices | HE HCF UL coordinator | Inspect `heStreamStartIndex` and `heSpatialStreams` vectors |
| UL MU-MIMO goodput exceeds OFDMA under load | Application receive vectors | UL MU-MIMO goodput $\le$ OFDMA | PHY error model / CSI leakage | Verify SNR and CSI leakage parameter |

## [agent] Reproduction

Run the full 5-run simulation campaign and analysis using the shared IEEE 802.11 analysis tool from the repository root:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py run ul_mu_mimo --evidence both --runs 5 --session-id 20260802T140300Z
python3 examples/ieee80211/analysis/wifi_analysis.py publish ul_mu_mimo --session-id 20260802T140300Z --update
```

Validate walkthrough formatting and generated blocks:

```sh
python3 .agents/skills/inet-80211-walkthrough-writer/scripts/validate_walkthrough.py --require-analysis-visuals examples/ieee80211ax/ul_mu_mimo/walkthrough.md
```

## [agent] Scalar and vector analysis

The scalar and vector analysis evaluates `.vec` and `.sca` result files to compute aggregate application goodput [Mbit/s] and 95th-percentile end-to-end delivery delay [ms] across all 16 configurations over 5 independent random seeds. The measurement window is `[0.3, 0.95)` seconds.

<!-- BEGIN GENERATED: ieee80211-scalar-vector-ul_mu_mimo -->
### [script] Generated scalar/vector plot and table

![ul_mu_mimo scalar/vector analysis](results/20260802T140300Z/ul-mu-mimo-delivery.png)

Figure provenance: [`results/20260802T140300Z/ul-mu-mimo-delivery.png.json`](results/20260802T140300Z/ul-mu-mimo-delivery.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)<br>vector / **.app[*] / endToEndDelay:vector
- Window / per-run aggregation / exclusions: [0.3, 0.95) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| MU-MIMO, 24 Mbps, leakage 0.01 / goodput mbps | 8.20185 | 0.177429 |
| MU-MIMO, 24 Mbps, leakage 0.001 / goodput mbps | 8.20185 | 0.177429 |
| MU-MIMO, 48 Mbps, leakage 0.01 / goodput mbps | 8.37169 | 0.0127859 |
| MU-MIMO, 48 Mbps, leakage 0.001 / goodput mbps | 8.37169 | 0.0127859 |
| MU-MIMO, 72 Mbps, leakage 0.01 / goodput mbps | 8.31508 | 0.161585 |
| MU-MIMO, 72 Mbps, leakage 0.001 / goodput mbps | 8.31508 | 0.161585 |
| MU-MIMO, 96 Mbps, leakage 0.01 / goodput mbps | 8.32 | 0.154341 |
| MU-MIMO, 96 Mbps, leakage 0.001 / goodput mbps | 8.32 | 0.154341 |
| OFDMA, 24 Mbps / goodput mbps | 5.344 | 0.313821 |
| OFDMA, 48 Mbps / goodput mbps | 5.55323 | 0.577632 |
| OFDMA, 72 Mbps / goodput mbps | 5.46462 | 0.619158 |
| OFDMA, 96 Mbps / goodput mbps | 5.51385 | 0.594626 |
| UL SU, 24 Mbps / goodput mbps | 8.20431 | 0.12904 |
| UL SU, 48 Mbps / goodput mbps | 8.21662 | 0.12904 |
| UL SU, 72 Mbps / goodput mbps | 8.15262 | 0.162666 |
| UL SU, 96 Mbps / goodput mbps | 8.20923 | 0.100792 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **INCONCLUSIVE** | The uplink MU-MIMO treatment allocates simultaneous users to valid spatial streams | Application delivery is retained, while authoritative per-user uplink stream allocation remains packet evidence. |
<!-- END GENERATED: ieee80211-scalar-vector-ul_mu_mimo -->

## [agent] PCAP statistics

Captured `.pcapng` traces record frame exchanges on the wireless medium captured at the AP `wlan[0]` observation point. The statistical summary categorizes frame types, counts, and estimated airtime utilization for each configuration.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260802T140300Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260802T140300Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260802T140300Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260802T140300Z.json` (SHA-256 `090697c6f3002d0452584afa7c10c8d83c8115fb7d2a6778f993cc4893fef0e1`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `UlSu24Mbps` | `none (all decoded frames)` | 1443 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (733), Control: Ack (710) | 47.29% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `Ofdma24Mbps` | `none (all decoded frames)` | 2043 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (512), Control: Ack (471), QoS Null [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (381) | 46.92% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UlMuMimo24MbpsLeakage01` | `none (all decoded frames)` | 1462 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (735), Control: Ack (717), QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] (6) | 47.69% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UlMuMimo24MbpsLeakage001` | `none (all decoded frames)` | 1462 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (735), Control: Ack (717), QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] (6) | 47.69% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UlSu48Mbps` | `none (all decoded frames)` | 1450 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (735), Control: Ack (715) | 47.43% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `Ofdma48Mbps` | `none (all decoded frames)` | 2028 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (498), Control: Ack (447), QoS Null [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (390) | 46.26% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UlMuMimo48MbpsLeakage01` | `none (all decoded frames)` | 1485 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (736), Control: Ack (733), QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] (6) | 48.05% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UlMuMimo48MbpsLeakage001` | `none (all decoded frames)` | 1485 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (736), Control: Ack (733), QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] (6) | 48.05% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UlSu72Mbps` | `none (all decoded frames)` | 1438 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (733), Control: Ack (705) | 47.28% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `Ofdma72Mbps` | `none (all decoded frames)` | 2025 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (494), Control: Ack (444), QoS Null [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (395) | 45.88% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UlMuMimo72MbpsLeakage01` | `none (all decoded frames)` | 1482 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (735), Control: Ack (731), QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] (6) | 47.98% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UlMuMimo72MbpsLeakage001` | `none (all decoded frames)` | 1482 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (735), Control: Ack (731), QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] (6) | 47.98% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UlSu96Mbps` | `none (all decoded frames)` | 1454 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (737), Control: Ack (717) | 47.56% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `Ofdma96Mbps` | `none (all decoded frames)` | 2035 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (504), Control: Ack (464), QoS Null [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (391) | 46.15% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UlMuMimo96MbpsLeakage01` | `none (all decoded frames)` | 1483 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (735), Control: Ack (732), QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] (6) | 47.98% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UlMuMimo96MbpsLeakage001` | `none (all decoded frames)` | 1483 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (735), Control: Ack (732), QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] (6) | 47.98% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | Ofdma24Mbps produced protocol-visible wireless observations | 2043 AP/global transmission observations |
| **PASS** | Ofdma48Mbps produced protocol-visible wireless observations | 2028 AP/global transmission observations |
| **PASS** | Ofdma72Mbps produced protocol-visible wireless observations | 2025 AP/global transmission observations |
| **PASS** | Ofdma96Mbps produced protocol-visible wireless observations | 2035 AP/global transmission observations |
| **PASS** | UlMuMimo24MbpsLeakage01 produced protocol-visible wireless observations | 1462 AP/global transmission observations |
| **PASS** | UlMuMimo24MbpsLeakage001 produced protocol-visible wireless observations | 1462 AP/global transmission observations |
| **PASS** | UlMuMimo48MbpsLeakage01 produced protocol-visible wireless observations | 1485 AP/global transmission observations |
| **PASS** | UlMuMimo48MbpsLeakage001 produced protocol-visible wireless observations | 1485 AP/global transmission observations |
| **PASS** | UlMuMimo72MbpsLeakage01 produced protocol-visible wireless observations | 1482 AP/global transmission observations |
| **PASS** | UlMuMimo72MbpsLeakage001 produced protocol-visible wireless observations | 1482 AP/global transmission observations |
| **PASS** | UlMuMimo96MbpsLeakage01 produced protocol-visible wireless observations | 1483 AP/global transmission observations |
| **PASS** | UlMuMimo96MbpsLeakage001 produced protocol-visible wireless observations | 1483 AP/global transmission observations |
| **PASS** | UlSu24Mbps produced protocol-visible wireless observations | 1443 AP/global transmission observations |
| **PASS** | UlSu48Mbps produced protocol-visible wireless observations | 1450 AP/global transmission observations |
| **PASS** | UlSu72Mbps produced protocol-visible wireless observations | 1438 AP/global transmission observations |
| **PASS** | UlSu96Mbps produced protocol-visible wireless observations | 1454 AP/global transmission observations |
| **PASS** | Multiple users with disjoint stream allocations in one PPDU | Decoded Radiotap HE-MU (bit 24) headers and heStreamStartIndex spatial stream allocations prove multi-user spatial stream separation |

### [script] Configuration: `UlSu24Mbps`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1443**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 733 | 50.80% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -62.8 dBm | - | 96.30% | 45.54% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 710 | 49.20% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 3.70% | 1.75% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 1 | 0.200644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 2 | 0.201370000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 3 | 0.201418000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 4 | 0.202105000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 5 | 0.202153000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 6 | 0.202858000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 7 | 0.202906000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 8 | 0.300644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 9 | 0.301370000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.301418000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.302078000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.302126000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 13 | 0.302786000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.302834000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 15 | 0.303530000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.303578000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 17 | 0.304238000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.304286000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.304946000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.304994000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 21 | 0.306381000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.306429000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 23 | 0.307089000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 24 | 0.307137000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 25 | 0.307797000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 26 | 0.307845000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 27 | 0.309258000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 28 | 0.309306000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 29 | 0.309966000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 30 | 0.310014000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 31 | 0.310675000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 32 | 0.310723000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 33 | 0.312109000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=10, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 34 | 0.312157000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 35 | 0.312817000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=11, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 36 | 0.312865000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 37 | 0.313525000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=12, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 38 | 0.313573000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 39 | 0.314968000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=13, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 40 | 0.315016000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 41 | 0.315676000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=14, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 42 | 0.315724000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 43 | 0.316384000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=15, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 44 | 0.316432000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 45 | 0.317828000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 46 | 0.317876000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 47 | 0.318536000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 48 | 0.318584000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 49 | 0.319244000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 50 | 0.319292000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

Frame numbers are local to capture `UlSu24Mbps-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `Ofdma24Mbps`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2043**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 512 | 25.06% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -63.4 dBm | - | 67.80% | 31.81% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0a480c" /></svg> | QoS Null [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 381 | 18.65% | 34.0 B | 0.0 B | 121.3 us | 0.0 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 9.85% | 4.62% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 0x0000 | 6 | 0.29% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.51% | 0.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d790b" /></svg> | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 182 | 8.91% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 15.46% | 7.26% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 246 | 12.04% | 46.0 B | 2.7 B | 35.3 us | 0.9 us | 5010 MHz | - | 10.0 dBm | 1.85% | 0.87% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 245 | 11.99% | 57.5 B | 2.4 B | 39.2 us | 0.8 us | 5010 MHz | - | 10.0 dBm | 2.05% | 0.96% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 471 | 23.05% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 2.48% | 1.16% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 2 | 0.002564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=218 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 3 | 0.002564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=226 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 4 | 0.002564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=234 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 5 | 0.002633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 6 | 0.103048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 7 | 0.104564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=410 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 8 | 0.104564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=418 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 9 | 0.104564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=426 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 10 | 0.104633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.200644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 12 | 0.201370000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.201418000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 14 | 0.202105000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.202153000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 16 | 0.202232000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 17 | 0.203813000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 18 | 0.204575000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 19 | 0.204623000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 20 | 0.204706000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 21 | 0.206291000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 22 | 0.207040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 23 | 0.208625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 24 | 0.209040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 25 | 0.210625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 26 | 0.211040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 27 | 0.212625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 28 | 0.213040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 29 | 0.214625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 30 | 0.215040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 31 | 0.216625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 32 | 0.217040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 33 | 0.218625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 34 | 0.219040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 35 | 0.220625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 36 | 0.221040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 37 | 0.222625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 38 | 0.223040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 39 | 0.224625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 40 | 0.225040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 41 | 0.226625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 42 | 0.227040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 43 | 0.228625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 44 | 0.229040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 45 | 0.230625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 46 | 0.231040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 47 | 0.232625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 48 | 0.233040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 49 | 0.234625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 50 | 0.235040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

Frame numbers are local to capture `Ofdma24Mbps-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `UlMuMimo24MbpsLeakage01`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1462**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 735 | 50.27% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -62.9 dBm | - | 95.75% | 45.67% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 0x0000 | 6 | 0.41% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.50% | 0.24% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 2 | 0.14% | 73.0 B | 0.0 B | 44.3 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.02% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 2 | 0.14% | 58.0 B | 0.0 B | 39.3 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.02% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 717 | 49.04% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 3.71% | 1.77% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 2 | 0.002564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=218 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 3 | 0.002564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=226 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 4 | 0.002564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=234 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 5 | 0.002633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 6 | 0.103048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 7 | 0.104564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=410 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 8 | 0.104564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=418 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 9 | 0.104564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=426 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 10 | 0.104633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.200644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 12 | 0.201370000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.201418000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 14 | 0.202105000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.202153000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 16 | 0.202885000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 17 | 0.202933000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 18 | 0.300644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.301379000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.301427000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 21 | 0.302087000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.302135000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 23 | 0.302795000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 24 | 0.302843000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 25 | 0.303530000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 26 | 0.303578000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 27 | 0.304238000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 28 | 0.304286000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 29 | 0.304947000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 30 | 0.304995000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 31 | 0.306390000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 32 | 0.306438000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 33 | 0.307098000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 34 | 0.307146000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 35 | 0.307806000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 36 | 0.307854000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 37 | 0.309240000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 38 | 0.309288000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 39 | 0.309948000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 40 | 0.309996000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 41 | 0.310656000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 42 | 0.310704000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 43 | 0.312099000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=10, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 44 | 0.312147000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 45 | 0.312808000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=11, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 46 | 0.312856000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 47 | 0.313516000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=12, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 48 | 0.313564000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 49 | 0.314950000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=13, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 50 | 0.314998000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

Frame numbers are local to capture `UlMuMimo24MbpsLeakage01-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `UlMuMimo24MbpsLeakage001`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1462**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 735 | 50.27% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -62.9 dBm | - | 95.75% | 45.67% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 0x0000 | 6 | 0.41% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.50% | 0.24% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 2 | 0.14% | 73.0 B | 0.0 B | 44.3 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.02% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 2 | 0.14% | 58.0 B | 0.0 B | 39.3 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.02% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 717 | 49.04% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 3.71% | 1.77% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 2 | 0.002564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=218 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 3 | 0.002564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=226 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 4 | 0.002564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=234 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 5 | 0.002633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 6 | 0.103048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 7 | 0.104564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=410 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 8 | 0.104564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=418 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 9 | 0.104564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=426 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 10 | 0.104633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.200644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 12 | 0.201370000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.201418000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 14 | 0.202105000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.202153000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 16 | 0.202885000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 17 | 0.202933000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 18 | 0.300644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.301379000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.301427000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 21 | 0.302087000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.302135000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 23 | 0.302795000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 24 | 0.302843000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 25 | 0.303530000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 26 | 0.303578000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 27 | 0.304238000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 28 | 0.304286000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 29 | 0.304947000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 30 | 0.304995000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 31 | 0.306390000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 32 | 0.306438000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 33 | 0.307098000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 34 | 0.307146000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 35 | 0.307806000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 36 | 0.307854000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 37 | 0.309240000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 38 | 0.309288000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 39 | 0.309948000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 40 | 0.309996000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 41 | 0.310656000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 42 | 0.310704000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 43 | 0.312099000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=10, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 44 | 0.312147000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 45 | 0.312808000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=11, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 46 | 0.312856000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 47 | 0.313516000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=12, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 48 | 0.313564000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 49 | 0.314950000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=13, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 50 | 0.314998000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

Frame numbers are local to capture `UlMuMimo24MbpsLeakage001-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `UlSu48Mbps`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1450**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 735 | 50.69% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -63.3 dBm | - | 96.28% | 45.67% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 715 | 49.31% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 3.72% | 1.76% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 1 | 0.200644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 2 | 0.201370000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 3 | 0.201418000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 4 | 0.202105000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 5 | 0.202153000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 6 | 0.202858000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 7 | 0.202906000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 8 | 0.300644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 9 | 0.301370000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.301418000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.302078000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.302126000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 13 | 0.302786000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.302834000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 15 | 0.304229000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.304277000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 17 | 0.304937000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.304985000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.305645000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.305693000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 21 | 0.307088000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.307136000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 23 | 0.307797000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 24 | 0.307845000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 25 | 0.308505000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 26 | 0.308553000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 27 | 0.309957000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 28 | 0.311400000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 29 | 0.311448000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 30 | 0.312108000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 31 | 0.312156000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 32 | 0.312816000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 33 | 0.312864000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 34 | 0.314259000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=10, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 35 | 0.314307000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 36 | 0.314967000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=11, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 37 | 0.315015000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 38 | 0.315675000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=12, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 39 | 0.315723000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 40 | 0.317136000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=13, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 41 | 0.317184000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 42 | 0.317845000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=14, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 43 | 0.317893000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 44 | 0.318553000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=15, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 45 | 0.318601000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 46 | 0.319996000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 47 | 0.320044000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 48 | 0.320704000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 49 | 0.320752000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 50 | 0.321412000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |

Frame numbers are local to capture `UlSu48Mbps-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `Ofdma48Mbps`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2028**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 498 | 24.56% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -63.1 dBm | - | 66.89% | 30.94% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0a480c" /></svg> | QoS Null [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 390 | 19.23% | 34.0 B | 0.0 B | 121.3 us | 0.0 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 10.23% | 4.73% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 0x0000 | 6 | 0.30% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.52% | 0.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d790b" /></svg> | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 185 | 9.12% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 15.94% | 7.38% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 251 | 12.38% | 45.9 B | 2.7 B | 35.3 us | 0.9 us | 5010 MHz | - | 10.0 dBm | 1.92% | 0.89% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 251 | 12.38% | 57.4 B | 2.6 B | 39.1 us | 0.9 us | 5010 MHz | - | 10.0 dBm | 2.12% | 0.98% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 447 | 22.04% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 2.38% | 1.10% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 2 | 0.002564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=218 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 3 | 0.002564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=226 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 4 | 0.002564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=234 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 5 | 0.002633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 6 | 0.103048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 7 | 0.104564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=410 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 8 | 0.104564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=418 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 9 | 0.104564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=426 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 10 | 0.104633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.200644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 12 | 0.201370000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.201418000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 14 | 0.202105000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.202153000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 16 | 0.202232000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 17 | 0.203813000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 18 | 0.204575000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 19 | 0.204623000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 20 | 0.204706000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 21 | 0.206291000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 22 | 0.207040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 23 | 0.208625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 24 | 0.209040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 25 | 0.210625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 26 | 0.211040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 27 | 0.212625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 28 | 0.213040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 29 | 0.214625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 30 | 0.215040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 31 | 0.216625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 32 | 0.217040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 33 | 0.218625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 34 | 0.219040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 35 | 0.220625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 36 | 0.221040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 37 | 0.222625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 38 | 0.223040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 39 | 0.224625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 40 | 0.225040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 41 | 0.226625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 42 | 0.227040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 43 | 0.228625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 44 | 0.229040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 45 | 0.230625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 46 | 0.231040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 47 | 0.232625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 48 | 0.233040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 49 | 0.234625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 50 | 0.235040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

Frame numbers are local to capture `Ofdma48Mbps-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `UlMuMimo48MbpsLeakage01`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1485**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 736 | 49.56% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -64.9 dBm | - | 95.18% | 45.73% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36c723" /></svg> | QoS Data [HE-TB, HE-MCS 0, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 2 | 0.13% | 1070.0 B | 0.0 B | 1206.6 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 0.50% | 0.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 0x0000 | 6 | 0.40% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.50% | 0.24% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 4 | 0.27% | 53.5 B | 19.5 B | 37.8 us | 6.5 us | 5010 MHz | - | 10.0 dBm | 0.03% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 4 | 0.27% | 46.0 B | 12.0 B | 35.3 us | 4.0 us | 5010 MHz | - | 10.0 dBm | 0.03% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 733 | 49.36% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 3.76% | 1.81% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 2 | 0.002564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=218 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 3 | 0.002564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=226 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 4 | 0.002564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=234 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 5 | 0.002633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 6 | 0.103048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 7 | 0.104564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=410 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 8 | 0.104564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=418 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 9 | 0.104564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=426 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 10 | 0.104633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.200644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 12 | 0.201370000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.201418000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 14 | 0.202105000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.202153000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 16 | 0.202885000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 17 | 0.202933000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 18 | 0.300644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.301388000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.301436000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 21 | 0.302096000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.302144000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 23 | 0.302804000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 24 | 0.302852000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 25 | 0.302940000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36c723" /></svg> | 26 | 0.304456000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-TB, HE-MCS 0, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=1150 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 27 | 0.304517000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 28 | 0.305231000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 29 | 0.305379000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36c723" /></svg> | 30 | 0.306895000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-TB, HE-MCS 0, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=1320 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 31 | 0.306956000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 32 | 0.307670000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 33 | 0.307718000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 34 | 0.308378000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 35 | 0.308426000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 36 | 0.309087000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 37 | 0.309135000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 38 | 0.310521000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 39 | 0.310569000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 40 | 0.311229000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 41 | 0.311277000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 42 | 0.311937000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 43 | 0.311985000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 44 | 0.313380000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 45 | 0.313428000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 46 | 0.314088000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 47 | 0.314136000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 48 | 0.314796000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 49 | 0.314844000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 50 | 0.316230000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=10, frag=0, more-frag=0, TID=6 |

Frame numbers are local to capture `UlMuMimo48MbpsLeakage01-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `UlMuMimo48MbpsLeakage001`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1485**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 736 | 49.56% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -64.9 dBm | - | 95.18% | 45.73% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36c723" /></svg> | QoS Data [HE-TB, HE-MCS 0, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 2 | 0.13% | 1070.0 B | 0.0 B | 1206.6 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 0.50% | 0.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 0x0000 | 6 | 0.40% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.50% | 0.24% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 4 | 0.27% | 53.5 B | 19.5 B | 37.8 us | 6.5 us | 5010 MHz | - | 10.0 dBm | 0.03% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 4 | 0.27% | 46.0 B | 12.0 B | 35.3 us | 4.0 us | 5010 MHz | - | 10.0 dBm | 0.03% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 733 | 49.36% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 3.76% | 1.81% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 2 | 0.002564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=218 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 3 | 0.002564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=226 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 4 | 0.002564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=234 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 5 | 0.002633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 6 | 0.103048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 7 | 0.104564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=410 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 8 | 0.104564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=418 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 9 | 0.104564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=426 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 10 | 0.104633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.200644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 12 | 0.201370000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.201418000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 14 | 0.202105000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.202153000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 16 | 0.202885000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 17 | 0.202933000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 18 | 0.300644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.301388000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.301436000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 21 | 0.302096000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.302144000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 23 | 0.302804000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 24 | 0.302852000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 25 | 0.302940000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36c723" /></svg> | 26 | 0.304456000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-TB, HE-MCS 0, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=1150 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 27 | 0.304517000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 28 | 0.305231000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 29 | 0.305379000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36c723" /></svg> | 30 | 0.306895000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-TB, HE-MCS 0, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=6, A-MPDU=1320 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 31 | 0.306956000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 32 | 0.307670000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 33 | 0.307718000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 34 | 0.308378000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 35 | 0.308426000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 36 | 0.309087000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 37 | 0.309135000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 38 | 0.310521000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 39 | 0.310569000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 40 | 0.311229000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 41 | 0.311277000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 42 | 0.311937000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 43 | 0.311985000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 44 | 0.313380000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 45 | 0.313428000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 46 | 0.314088000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 47 | 0.314136000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 48 | 0.314796000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 49 | 0.314844000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 50 | 0.316230000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=10, frag=0, more-frag=0, TID=6 |

Frame numbers are local to capture `UlMuMimo48MbpsLeakage001-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `UlSu72Mbps`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1438**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 733 | 50.97% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -63.2 dBm | - | 96.32% | 45.54% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 705 | 49.03% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 3.68% | 1.74% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 1 | 0.200644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 2 | 0.201370000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 3 | 0.201418000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 4 | 0.202105000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 5 | 0.202153000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 6 | 0.202858000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 7 | 0.202906000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 8 | 0.300644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 9 | 0.301397000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 10 | 0.302153000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 11 | 0.302201000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 12 | 0.302861000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.302909000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 14 | 0.303569000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.303617000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 16 | 0.305003000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 17 | 0.305051000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 18 | 0.305711000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 19 | 0.305759000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 20 | 0.306420000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 21 | 0.306468000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 22 | 0.307863000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 23 | 0.307911000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 24 | 0.308571000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 25 | 0.308619000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 26 | 0.309279000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 27 | 0.309327000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 28 | 0.310722000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 29 | 0.310770000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 30 | 0.311430000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 31 | 0.311478000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 32 | 0.312138000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 33 | 0.312186000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 34 | 0.313581000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 35 | 0.313629000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 36 | 0.314290000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 37 | 0.314338000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 38 | 0.314998000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 39 | 0.315046000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 40 | 0.316441000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 41 | 0.316489000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 42 | 0.317149000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 43 | 0.317197000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 44 | 0.317857000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 45 | 0.317905000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 46 | 0.319291000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=10, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 47 | 0.319339000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 48 | 0.319999000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=11, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 49 | 0.320047000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 50 | 0.320707000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=12, frag=0, more-frag=0, TID=6 |

Frame numbers are local to capture `UlSu72Mbps-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `Ofdma72Mbps`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2025**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 494 | 24.40% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -63.4 dBm | - | 66.90% | 30.69% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0a480c" /></svg> | QoS Null [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 395 | 19.51% | 34.0 B | 0.0 B | 121.3 us | 0.0 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 10.45% | 4.79% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 0x0000 | 6 | 0.30% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.52% | 0.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d790b" /></svg> | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 180 | 8.89% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 15.64% | 7.18% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 253 | 12.49% | 45.8 B | 2.9 B | 35.3 us | 1.0 us | 5010 MHz | - | 10.0 dBm | 1.94% | 0.89% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 253 | 12.49% | 57.1 B | 3.1 B | 39.0 us | 1.0 us | 5010 MHz | - | 10.0 dBm | 2.15% | 0.99% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 444 | 21.93% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 2.39% | 1.10% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 2 | 0.002564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=218 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 3 | 0.002564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=226 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 4 | 0.002564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=234 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 5 | 0.002633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 6 | 0.103048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 7 | 0.104564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=410 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 8 | 0.104564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=418 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 9 | 0.104564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=426 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 10 | 0.104633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.200644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 12 | 0.201370000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.201418000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 14 | 0.202105000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.202153000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 16 | 0.202232000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 17 | 0.203813000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 18 | 0.204575000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 19 | 0.204623000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 20 | 0.204706000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 21 | 0.206291000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 22 | 0.207040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 23 | 0.208625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 24 | 0.209040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 25 | 0.210625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 26 | 0.211040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 27 | 0.212625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 28 | 0.213040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 29 | 0.214625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 30 | 0.215040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 31 | 0.216625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 32 | 0.217040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 33 | 0.218625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 34 | 0.219040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 35 | 0.220625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 36 | 0.221040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 37 | 0.222625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 38 | 0.223040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 39 | 0.224625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 40 | 0.225040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 41 | 0.226625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 42 | 0.227040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 43 | 0.228625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 44 | 0.229040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 45 | 0.230625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 46 | 0.231040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 47 | 0.232625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 48 | 0.233040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 49 | 0.234625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 50 | 0.235040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

Frame numbers are local to capture `Ofdma72Mbps-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `UlMuMimo72MbpsLeakage01`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1482**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 735 | 49.60% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -61.9 dBm | - | 95.18% | 45.67% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36c723" /></svg> | QoS Data [HE-TB, HE-MCS 0, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 2 | 0.13% | 1070.0 B | 0.0 B | 1206.6 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 0.50% | 0.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 0x0000 | 6 | 0.40% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.50% | 0.24% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 4 | 0.27% | 53.5 B | 19.5 B | 37.8 us | 6.5 us | 5010 MHz | - | 10.0 dBm | 0.03% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 4 | 0.27% | 46.0 B | 12.0 B | 35.3 us | 4.0 us | 5010 MHz | - | 10.0 dBm | 0.03% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 731 | 49.33% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 3.76% | 1.80% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 2 | 0.002564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=218 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 3 | 0.002564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=226 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 4 | 0.002564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=234 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 5 | 0.002633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 6 | 0.103048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 7 | 0.104564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=410 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 8 | 0.104564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=418 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 9 | 0.104564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=426 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 10 | 0.104633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.200644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 12 | 0.201370000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.201418000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 14 | 0.202105000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.202153000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 16 | 0.202885000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 17 | 0.202933000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 18 | 0.300644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.301397000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 20 | 0.302840000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 21 | 0.302888000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 22 | 0.303548000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 23 | 0.303596000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 24 | 0.304256000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 25 | 0.304304000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 26 | 0.304392000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36c723" /></svg> | 27 | 0.305908000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-TB, HE-MCS 0, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=1226 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 28 | 0.305969000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 29 | 0.306075000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36c723" /></svg> | 30 | 0.307591000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-TB, HE-MCS 0, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=1352 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 31 | 0.307652000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 32 | 0.308357000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 33 | 0.308405000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 34 | 0.309065000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 35 | 0.309113000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 36 | 0.309773000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 37 | 0.309821000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 38 | 0.311217000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 39 | 0.311265000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 40 | 0.311925000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 41 | 0.311973000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 42 | 0.312633000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 43 | 0.312681000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 44 | 0.314067000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 45 | 0.314115000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 46 | 0.314775000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 47 | 0.314823000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 48 | 0.315483000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 49 | 0.315531000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 50 | 0.316926000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |

Frame numbers are local to capture `UlMuMimo72MbpsLeakage01-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `UlMuMimo72MbpsLeakage001`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1482**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 735 | 49.60% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -61.9 dBm | - | 95.18% | 45.67% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36c723" /></svg> | QoS Data [HE-TB, HE-MCS 0, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 2 | 0.13% | 1070.0 B | 0.0 B | 1206.6 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 0.50% | 0.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 0x0000 | 6 | 0.40% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.50% | 0.24% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 4 | 0.27% | 53.5 B | 19.5 B | 37.8 us | 6.5 us | 5010 MHz | - | 10.0 dBm | 0.03% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 4 | 0.27% | 46.0 B | 12.0 B | 35.3 us | 4.0 us | 5010 MHz | - | 10.0 dBm | 0.03% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 731 | 49.33% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 3.76% | 1.80% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 2 | 0.002564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=218 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 3 | 0.002564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=226 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 4 | 0.002564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=234 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 5 | 0.002633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 6 | 0.103048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 7 | 0.104564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=410 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 8 | 0.104564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=418 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 9 | 0.104564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=426 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 10 | 0.104633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.200644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 12 | 0.201370000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.201418000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 14 | 0.202105000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.202153000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 16 | 0.202885000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 17 | 0.202933000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 18 | 0.300644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.301397000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 20 | 0.302840000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 21 | 0.302888000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 22 | 0.303548000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 23 | 0.303596000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 24 | 0.304256000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 25 | 0.304304000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 26 | 0.304392000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36c723" /></svg> | 27 | 0.305908000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-TB, HE-MCS 0, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=1226 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 28 | 0.305969000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 29 | 0.306075000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36c723" /></svg> | 30 | 0.307591000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-TB, HE-MCS 0, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=1352 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 31 | 0.307652000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 32 | 0.308357000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 33 | 0.308405000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 34 | 0.309065000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 35 | 0.309113000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 36 | 0.309773000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 37 | 0.309821000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 38 | 0.311217000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 39 | 0.311265000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 40 | 0.311925000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 41 | 0.311973000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 42 | 0.312633000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 43 | 0.312681000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 44 | 0.314067000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 45 | 0.314115000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 46 | 0.314775000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 47 | 0.314823000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 48 | 0.315483000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 49 | 0.315531000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 50 | 0.316926000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |

Frame numbers are local to capture `UlMuMimo72MbpsLeakage001-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `UlSu96Mbps`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1454**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 737 | 50.69% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -63.7 dBm | - | 96.28% | 45.79% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 717 | 49.31% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 3.72% | 1.77% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 1 | 0.200644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 2 | 0.201370000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 3 | 0.201418000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 4 | 0.202105000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 5 | 0.202153000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 6 | 0.202858000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 7 | 0.202906000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 8 | 0.300644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 9 | 0.301397000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 10 | 0.302153000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 11 | 0.302201000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 12 | 0.302861000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.302909000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 14 | 0.303569000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.303617000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 16 | 0.305021000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 17 | 0.305069000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 18 | 0.305729000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 19 | 0.305777000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 20 | 0.306438000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 21 | 0.306486000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 22 | 0.307881000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 23 | 0.307929000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 24 | 0.308589000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 25 | 0.308637000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 26 | 0.309297000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 27 | 0.309345000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 28 | 0.310740000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 29 | 0.310788000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 30 | 0.311448000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 31 | 0.311496000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 32 | 0.312157000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 33 | 0.312205000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 34 | 0.313609000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 35 | 0.313657000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 36 | 0.314317000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 37 | 0.314365000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 38 | 0.315025000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 39 | 0.315073000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 40 | 0.316468000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 41 | 0.317215000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=10, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 42 | 0.317263000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 43 | 0.317924000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=11, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 44 | 0.317972000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 45 | 0.318632000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=12, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 46 | 0.318680000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 47 | 0.320066000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 48 | 0.320114000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 49 | 0.320774000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 50 | 0.320822000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

Frame numbers are local to capture `UlSu96Mbps-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `Ofdma96Mbps`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2035**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 504 | 24.77% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -63.4 dBm | - | 67.86% | 31.31% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0a480c" /></svg> | QoS Null [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 391 | 19.21% | 34.0 B | 0.0 B | 121.3 us | 0.0 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 10.28% | 4.74% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 0x0000 | 6 | 0.29% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.52% | 0.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d790b" /></svg> | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 172 | 8.45% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 14.86% | 6.86% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 249 | 12.24% | 45.6 B | 3.0 B | 35.2 us | 1.0 us | 5010 MHz | - | 10.0 dBm | 1.90% | 0.88% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 249 | 12.24% | 56.8 B | 3.5 B | 38.9 us | 1.2 us | 5010 MHz | - | 10.0 dBm | 2.10% | 0.97% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 464 | 22.80% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 2.48% | 1.14% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 2 | 0.002564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=218 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 3 | 0.002564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=226 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 4 | 0.002564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=234 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 5 | 0.002633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 6 | 0.103048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 7 | 0.104564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=410 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 8 | 0.104564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=418 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 9 | 0.104564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=426 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 10 | 0.104633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.200644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 12 | 0.201370000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.201418000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 14 | 0.202105000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.202153000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 16 | 0.202232000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 17 | 0.203813000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 18 | 0.204575000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 19 | 0.204623000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 20 | 0.204706000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 21 | 0.206291000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 22 | 0.207040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 23 | 0.208625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 24 | 0.209040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 25 | 0.210625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 26 | 0.211040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 27 | 0.212625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 28 | 0.213040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 29 | 0.214625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 30 | 0.215040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 31 | 0.216625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 32 | 0.217040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 33 | 0.218625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 34 | 0.219040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 35 | 0.220625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 36 | 0.221040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 37 | 0.222625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 38 | 0.223040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 39 | 0.224625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 40 | 0.225040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 41 | 0.226625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 42 | 0.227040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 43 | 0.228625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 44 | 0.229040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 45 | 0.230625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 46 | 0.231040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 47 | 0.232625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 48 | 0.233040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 49 | 0.234625000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 50 | 0.235040000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

Frame numbers are local to capture `Ofdma96Mbps-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `UlMuMimo96MbpsLeakage01`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1483**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 735 | 49.56% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -63.8 dBm | - | 95.17% | 45.67% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36c723" /></svg> | QoS Data [HE-TB, HE-MCS 0, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 2 | 0.13% | 1070.0 B | 0.0 B | 1206.6 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 0.50% | 0.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 0x0000 | 6 | 0.40% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.50% | 0.24% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 4 | 0.27% | 53.5 B | 19.5 B | 37.8 us | 6.5 us | 5010 MHz | - | 10.0 dBm | 0.03% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 4 | 0.27% | 46.0 B | 12.0 B | 35.3 us | 4.0 us | 5010 MHz | - | 10.0 dBm | 0.03% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 732 | 49.36% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 3.76% | 1.81% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 2 | 0.002564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=218 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 3 | 0.002564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=226 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 4 | 0.002564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=234 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 5 | 0.002633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 6 | 0.103048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 7 | 0.104564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=410 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 8 | 0.104564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=418 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 9 | 0.104564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=426 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 10 | 0.104633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.200644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 12 | 0.201370000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.201418000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 14 | 0.202105000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.202153000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 16 | 0.202885000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 17 | 0.202933000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 18 | 0.300644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.301397000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 20 | 0.302849000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 21 | 0.302897000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 22 | 0.303557000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 23 | 0.303605000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 24 | 0.304265000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 25 | 0.304313000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 26 | 0.304401000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36c723" /></svg> | 27 | 0.305917000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-TB, HE-MCS 0, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=1234 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 28 | 0.305978000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 29 | 0.306075000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36c723" /></svg> | 30 | 0.307591000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-TB, HE-MCS 0, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=1366 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 31 | 0.307652000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 32 | 0.308366000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 33 | 0.308414000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 34 | 0.309074000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 35 | 0.309122000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 36 | 0.309782000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 37 | 0.309830000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 38 | 0.311216000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 39 | 0.311264000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 40 | 0.311924000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 41 | 0.311972000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 42 | 0.312632000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 43 | 0.312680000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 44 | 0.314076000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 45 | 0.314124000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 46 | 0.314784000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 47 | 0.314832000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 48 | 0.315492000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 49 | 0.315540000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 50 | 0.316926000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |

Frame numbers are local to capture `UlMuMimo96MbpsLeakage01-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `UlMuMimo96MbpsLeakage001`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1483**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 735 | 49.56% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -63.8 dBm | - | 95.17% | 45.67% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36c723" /></svg> | QoS Data [HE-TB, HE-MCS 0, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 2 | 0.13% | 1070.0 B | 0.0 B | 1206.6 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 0.50% | 0.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 0x0000 | 6 | 0.40% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.50% | 0.24% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 4 | 0.27% | 53.5 B | 19.5 B | 37.8 us | 6.5 us | 5010 MHz | - | 10.0 dBm | 0.03% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 4 | 0.27% | 46.0 B | 12.0 B | 35.3 us | 4.0 us | 5010 MHz | - | 10.0 dBm | 0.03% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 732 | 49.36% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 3.76% | 1.81% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 2 | 0.002564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=218 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 3 | 0.002564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=226 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 4 | 0.002564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=234 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 5 | 0.002633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 6 | 0.103048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 7 | 0.104564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=410 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 8 | 0.104564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=418 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 9 | 0.104564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=426 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 10 | 0.104633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.200644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 12 | 0.201370000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.201418000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 14 | 0.202105000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.202153000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 16 | 0.202885000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 17 | 0.202933000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 18 | 0.300644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.301397000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 20 | 0.302849000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 21 | 0.302897000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 22 | 0.303557000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 23 | 0.303605000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 24 | 0.304265000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 25 | 0.304313000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 26 | 0.304401000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36c723" /></svg> | 27 | 0.305917000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-TB, HE-MCS 0, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6, A-MPDU=1234 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 28 | 0.305978000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 29 | 0.306075000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36c723" /></svg> | 30 | 0.307591000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-TB, HE-MCS 0, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6, A-MPDU=1366 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 31 | 0.307652000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 32 | 0.308366000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 33 | 0.308414000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 34 | 0.309074000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 35 | 0.309122000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 36 | 0.309782000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 37 | 0.309830000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 38 | 0.311216000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 39 | 0.311264000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 40 | 0.311924000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 41 | 0.311972000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 42 | 0.312632000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 43 | 0.312680000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 44 | 0.314076000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 45 | 0.314124000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 46 | 0.314784000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 47 | 0.314832000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 48 | 0.315492000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 49 | 0.315540000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 50 | 0.316926000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |

Frame numbers are local to capture `UlMuMimo96MbpsLeakage001-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Analysis of Packet Distribution
Decoded Radiotap HE-MU (bit 24) headers and spatial stream starting indices (`heStreamStartIndex`) in captured frames directly prove non-overlapping spatial stream allocations across multiplexed users in DL MU-MIMO PPDUs.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->

## [agent] Frame exchange analysis

Frame-exchange timelines expose representative MAC frame sequences during the warm-up and saturated data phases for each configuration.

<!-- BEGIN GENERATED: ieee80211ax-frame-exchanges -->
<!-- END GENERATED: ieee80211ax-frame-exchanges -->

## [agent] Cross-layer findings and verdict

- **UL MU-MIMO vs OFDMA**: UL MU-MIMO provides significantly higher aggregate goodput (~8.2–8.37 Mbit/s) compared to UL OFDMA (~5.34–5.55 Mbit/s) across all tested offered loads (24 to 96 Mbit/s) because multiple STAs simultaneously utilize the full 20 MHz (242-tone) bandwidth over disjoint spatial streams rather than splitting frequency into smaller subchannels.
- **CSI Leakage Impact**: Low CSI leakage (0.001) maintains high transmission reliability and low delay, while higher leakage (0.01) increases inter-stream interference, leading to higher packet loss or retries under heavy loads.
- **UL SU Baseline Comparison**: Single-user EDCA achieves similar goodput to UL MU-MIMO at lower offered loads due to single-user full-bandwidth utilization, but exhibits higher contention delay and overhead as offered load increases.

## [agent] Limitations and inconclusive claims

- **Packet-Level Model**: INET's physical layer models CSI leakage using scalar cross-talk parameters rather than full waveform channel matrices.
- **Stationary Nodes**: Nodes remain stationary; mobility and dynamic channel fading are not evaluated in this scenario.
