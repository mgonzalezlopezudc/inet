# Walkthrough: IEEE 802.11ax UL MU-MIMO vs OFDMA

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260802T190557Z`
- PCAP: `20260802T190557Z`
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

Traffic consists of 100-byte UDP packets sent from each station to the server. Four aggregate offered loads are evaluated: 24 Mbit/s (`sendInterval = 0.1ms`), 48 Mbit/s (`sendInterval = 0.05ms`), 72 Mbit/s (`sendInterval = 0.0333ms`), and 96 Mbit/s (`sendInterval = 0.025ms`). For each offered load, four configurations are compared: UL SU, OFDMA, MU-MIMO with CSI leakage 0.01, and MU-MIMO with CSI leakage 0.001. Warm-up phase runs from 0.2 s to 0.25 s, and data measurement window spans [0.3, 0.95) s.

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

![ul_mu_mimo scalar/vector analysis](results/20260802T190557Z/ul-mu-mimo-delivery.png)

Figure provenance: [`results/20260802T190557Z/ul-mu-mimo-delivery.png.json`](results/20260802T190557Z/ul-mu-mimo-delivery.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)<br>vector / **.app[*] / endToEndDelay:vector
- Window / per-run aggregation / exclusions: [0.3, 0.95) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| MU-MIMO, 24 Mbps, leakage 0.001 / goodput mbps | 11.8966 | 0.0426804 |
| MU-MIMO, 48 Mbps, leakage 0.001 / goodput mbps | 11.9335 | 0.020503 |
| MU-MIMO, 72 Mbps, leakage 0.001 / goodput mbps | 11.9434 | 0.0398506 |
| OFDMA, 24 Mbps / goodput mbps | 10.0431 | 0.295738 |
| OFDMA, 48 Mbps / goodput mbps | 10.016 | 0.349387 |
| OFDMA, 72 Mbps / goodput mbps | 10.0505 | 0.292682 |
| UL SU, 24 Mbps / goodput mbps | 8.18215 | 0.196123 |
| UL SU, 48 Mbps / goodput mbps | 8.18708 | 0.130748 |
| UL SU, 72 Mbps / goodput mbps | 8.20185 | 0.0688539 |

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
![802.11 Packet Type Statistics](results/20260802T190557Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260802T190557Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260802T190557Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260802T190557Z.json` (SHA-256 `35d5853c229067f2993b818bf1fe5a9957e4ad5971cb855a5cf1a4fcfe5738a0`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `UlSu24Mbps` | `none (all decoded frames)` | 1429 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (729), Control: Ack (700) | 47.02% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `Ofdma24Mbps` | `none (all decoded frames)` | 2192 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (526), Control: Ack (524), Control: Trigger (253) | 61.67% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UlMuMimo24MbpsLeakage001` | `none (all decoded frames)` | 1995 | Control: Trigger (379), Control: Block Ack (BA) (378), QoS Data [HE-TB, HE-MCS 2, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] (311) | 63.19% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UlSu48Mbps` | `none (all decoded frames)` | 1440 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (731), Control: Ack (709) | 47.17% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `Ofdma48Mbps` | `none (all decoded frames)` | 2191 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (528), Control: Ack (525), Control: Trigger (253) | 61.74% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UlMuMimo48MbpsLeakage001` | `none (all decoded frames)` | 1992 | Control: Trigger (384), Control: Block Ack (BA) (384), QoS Data [HE-TB, HE-MCS 2, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] (318) | 63.27% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UlSu72Mbps` | `none (all decoded frames)` | 1457 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (737), Control: Ack (720) | 47.57% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `Ofdma72Mbps` | `none (all decoded frames)` | 2194 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (524), Control: Ack (522), Control: Trigger (255) | 61.75% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UlMuMimo72MbpsLeakage001` | `none (all decoded frames)` | 1996 | Control: Trigger (381), Control: Block Ack (BA) (381), QoS Data [HE-TB, HE-MCS 2, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] (315) | 63.35% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | Ofdma24Mbps produced protocol-visible wireless observations | 2192 AP/global transmission observations |
| **PASS** | Ofdma48Mbps produced protocol-visible wireless observations | 2191 AP/global transmission observations |
| **PASS** | Ofdma72Mbps produced protocol-visible wireless observations | 2194 AP/global transmission observations |
| **PASS** | UlMuMimo24MbpsLeakage001 produced protocol-visible wireless observations | 1995 AP/global transmission observations |
| **PASS** | UlMuMimo48MbpsLeakage001 produced protocol-visible wireless observations | 1992 AP/global transmission observations |
| **PASS** | UlMuMimo72MbpsLeakage001 produced protocol-visible wireless observations | 1996 AP/global transmission observations |
| **PASS** | UlSu24Mbps produced protocol-visible wireless observations | 1429 AP/global transmission observations |
| **PASS** | UlSu48Mbps produced protocol-visible wireless observations | 1440 AP/global transmission observations |
| **PASS** | UlSu72Mbps produced protocol-visible wireless observations | 1457 AP/global transmission observations |
| **PASS** | Multiple users with disjoint stream allocations in one PPDU | Decoded Radiotap HE-MU (bit 24) headers and heStreamStartIndex spatial stream allocations prove multi-user spatial stream separation |

### [script] Configuration: `UlSu24Mbps`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1429**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 729 | 51.01% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -74.9 dBm | - | 96.33% | 45.29% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 700 | 48.99% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 3.67% | 1.73% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 1 | 0.200644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 2 | 0.201370000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 3 | 0.201418000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 4 | 0.202108000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 5 | 0.202156000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 6 | 0.202863000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 7 | 0.202911000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 8 | 0.300644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 9 | 0.301371000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.301419000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.302082000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.302130000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 13 | 0.302793000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.302841000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 15 | 0.303538000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.303586000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 17 | 0.304248000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.304296000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.304958000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.305006000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 21 | 0.306395000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.306443000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 23 | 0.307106000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 24 | 0.307154000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 25 | 0.307816000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 26 | 0.307864000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 27 | 0.309270000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 28 | 0.309318000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 29 | 0.309980000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 30 | 0.310028000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 31 | 0.310690000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 32 | 0.310738000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 33 | 0.312124000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 34 | 0.312172000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 35 | 0.312833000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 36 | 0.312881000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 37 | 0.313543000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 38 | 0.313591000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 39 | 0.314986000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=10, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 40 | 0.315034000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 41 | 0.315696000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=11, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 42 | 0.315744000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 43 | 0.316406000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=12, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 44 | 0.316454000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 45 | 0.317851000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 46 | 0.317899000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 47 | 0.318562000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 48 | 0.318610000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 49 | 0.319273000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 50 | 0.319321000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

Frame numbers are local to capture `UlSu24Mbps-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `Ofdma24Mbps`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2192**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 526 | 24.00% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -78.8 dBm | - | 52.99% | 32.68% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1ddd30" /></svg> | QoS Data [HE-TB, HE-MCS 2, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | 0x0000 | 186 | 8.49% | 1070.0 B | 0.0 B | 483.6 us | 0.0 us | 5015 MHz | -75.0 dBm | - | 14.59% | 8.99% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22d334" /></svg> | QoS Data [HE-TB, HE-MCS 2, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | 0x0000 | 183 | 8.35% | 1070.0 B | 0.0 B | 334.4 us | 0.0 us | 5005 MHz | -75.0 dBm | - | 9.92% | 6.12% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c933" /></svg> | QoS Data [HE-TB, HE-MCS 2, 52-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | 0x0000 | 1 | 0.05% | 1070.0 B | 0.0 B | 987.1 us | 0.0 us | 5007 MHz | -75.0 dBm | - | 0.16% | 0.10% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#27be36" /></svg> | QoS Data [HE-TB, HE-MCS 2, 52-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | 0x0000 | 3 | 0.14% | 1070.0 B | 0.0 B | 670.1 us | 0.0 us | 5003 MHz | -75.0 dBm | - | 0.33% | 0.20% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0a480c" /></svg> | QoS Null [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 1 | 0.05% | 34.0 B | 0.0 B | 121.3 us | 0.0 us | 5015 MHz | -79.0 dBm | - | 0.02% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 0x0000 | 75 | 3.42% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -76.3 dBm | - | 4.85% | 2.99% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d790b" /></svg> | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 183 | 8.35% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5010 MHz | -79.0 dBm | - | 11.83% | 7.30% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#147b14" /></svg> | QoS Null [HE-TB, HE-MCS 0, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 3 | 0.14% | 34.0 B | 0.0 B | 217.3 us | 0.0 us | 5007 MHz | -79.0 dBm | - | 0.11% | 0.07% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1e7416" /></svg> | QoS Null [HE-TB, HE-MCS 2, 26-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | 0x0000 | 1 | 0.05% | 34.0 B | 0.0 B | 76.3 us | 0.0 us | 5002 MHz | -75.0 dBm | - | 0.01% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 253 | 11.54% | 48.6 B | 8.1 B | 36.2 us | 2.7 us | 5010 MHz | - | 20.0 dBm | 1.49% | 0.92% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 253 | 11.54% | 57.9 B | 1.7 B | 39.3 us | 0.6 us | 5010 MHz | - | 20.0 dBm | 1.61% | 0.99% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 524 | 23.91% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 2.10% | 1.29% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 2 | 0.002565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=218 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 3 | 0.002565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=226 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 4 | 0.002566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=234 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 5 | 0.002633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 6 | 0.013048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 7 | 0.014565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=410 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 8 | 0.014565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=418 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 9 | 0.014566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=426 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 10 | 0.014633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 11 | 0.025048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 12 | 0.026565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=602 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 13 | 0.026565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=610 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 14 | 0.026566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=618 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 15 | 0.026633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 16 | 0.037048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 17 | 0.038565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=794 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 18 | 0.038565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=802 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 19 | 0.038566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=810 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 20 | 0.038633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 21 | 0.049048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 22 | 0.050565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=986 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 23 | 0.050565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=994 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 24 | 0.050566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1002 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 25 | 0.050633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 26 | 0.061048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 27 | 0.062565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1178 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 28 | 0.062565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1186 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 29 | 0.062566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1194 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 30 | 0.062633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 31 | 0.073048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 32 | 0.074565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1370 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 33 | 0.074565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1378 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 34 | 0.074566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1386 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 35 | 0.074633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 36 | 0.085048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 37 | 0.086565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1562 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 38 | 0.086565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1570 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 39 | 0.086566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1578 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 40 | 0.086633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 41 | 0.097048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 42 | 0.098565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1754 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 43 | 0.098565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1762 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 44 | 0.098566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1770 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 45 | 0.098633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 46 | 0.109048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 47 | 0.110565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1946 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 48 | 0.110565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1954 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 49 | 0.110566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1962 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 50 | 0.110633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

Frame numbers are local to capture `Ofdma24Mbps-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `UlMuMimo24MbpsLeakage001`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1995**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 116 | 5.81% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -75.5 dBm | - | 11.41% | 7.21% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36c723" /></svg> | QoS Data [HE-TB, HE-MCS 0, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 311 | 15.59% | 1070.0 B | 0.0 B | 1206.6 us | 0.0 us | 5010 MHz | -79.0 dBm | - | 59.39% | 37.53% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2dbe36" /></svg> | QoS Data [HE-TB, HE-MCS 2, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | 0x0000 | 311 | 15.59% | 1070.0 B | 0.0 B | 231.1 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 11.37% | 7.19% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19cc25" /></svg> | QoS Data [HE-TB, HE-MCS 2, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | 0x0000 | 310 | 15.54% | 1070.0 B | 0.0 B | 166.1 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 8.15% | 5.15% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 0x0000 | 75 | 3.76% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -76.3 dBm | - | 4.73% | 2.99% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0a6710" /></svg> | QoS Null [HE-TB, HE-MCS 2, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | 0x0000 | 1 | 0.05% | 34.0 B | 0.0 B | 40.1 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 0.01% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 379 | 19.00% | 47.7 B | 6.7 B | 35.9 us | 2.2 us | 5010 MHz | - | 20.0 dBm | 2.15% | 1.36% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 378 | 18.95% | 57.9 B | 1.4 B | 39.3 us | 0.5 us | 5010 MHz | - | 20.0 dBm | 2.35% | 1.49% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 114 | 5.71% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.45% | 0.28% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 2 | 0.002565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=218 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 3 | 0.002565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=226 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 4 | 0.002566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=234 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 5 | 0.002633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 6 | 0.013048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 7 | 0.014565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=410 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 8 | 0.014565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=418 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 9 | 0.014566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=426 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 10 | 0.014633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 11 | 0.025048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 12 | 0.026565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=602 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 13 | 0.026565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=610 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 14 | 0.026566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=618 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 15 | 0.026633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 16 | 0.037048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 17 | 0.038565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=794 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 18 | 0.038565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=802 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 19 | 0.038566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=810 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 20 | 0.038633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 21 | 0.049048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 22 | 0.050565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=986 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 23 | 0.050565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=994 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 24 | 0.050566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1002 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 25 | 0.050633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 26 | 0.061048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 27 | 0.062565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1178 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 28 | 0.062565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1186 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 29 | 0.062566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1194 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 30 | 0.062633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 31 | 0.073048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 32 | 0.074565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1370 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 33 | 0.074565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1378 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 34 | 0.074566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1386 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 35 | 0.074633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 36 | 0.085048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 37 | 0.086565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1562 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 38 | 0.086565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1570 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 39 | 0.086566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1578 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 40 | 0.086633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 41 | 0.097048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 42 | 0.098565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1754 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 43 | 0.098565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1762 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 44 | 0.098566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1770 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 45 | 0.098633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 46 | 0.109048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 47 | 0.110565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1946 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 48 | 0.110565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1954 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 49 | 0.110566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1962 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 50 | 0.110633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

Frame numbers are local to capture `UlMuMimo24MbpsLeakage001-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `UlSu48Mbps`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1440**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 731 | 50.76% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -75.2 dBm | - | 96.29% | 45.42% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 709 | 49.24% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 3.71% | 1.75% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 1 | 0.200644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 2 | 0.201370000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 3 | 0.201418000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 4 | 0.202108000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 5 | 0.202156000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 6 | 0.202863000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 7 | 0.202911000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 8 | 0.300644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 9 | 0.301371000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 10 | 0.301419000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 11 | 0.302082000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 12 | 0.302130000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 13 | 0.302793000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 14 | 0.302841000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 15 | 0.304237000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 16 | 0.304285000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 17 | 0.304947000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 18 | 0.304995000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 19 | 0.305657000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 20 | 0.305705000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 21 | 0.307100000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 22 | 0.307148000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 23 | 0.307810000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 24 | 0.307858000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 25 | 0.308519000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 26 | 0.308567000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 27 | 0.309973000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 28 | 0.311416000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 29 | 0.311464000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 30 | 0.312126000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 31 | 0.312174000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 32 | 0.312836000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 33 | 0.312884000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 34 | 0.314279000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=10, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 35 | 0.314327000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 36 | 0.314988000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=11, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 37 | 0.315036000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 38 | 0.315698000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=12, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 39 | 0.315746000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 40 | 0.317159000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=13, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 41 | 0.317207000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 42 | 0.317869000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=14, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 43 | 0.317917000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 44 | 0.318578000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=15, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 45 | 0.318626000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 46 | 0.320024000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 47 | 0.320072000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 48 | 0.320735000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 49 | 0.320783000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 50 | 0.321446000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |

Frame numbers are local to capture `UlSu48Mbps-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `Ofdma48Mbps`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2191**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 528 | 24.10% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -79.0 dBm | - | 53.13% | 32.80% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1ddd30" /></svg> | QoS Data [HE-TB, HE-MCS 2, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | 0x0000 | 185 | 8.44% | 1070.0 B | 0.0 B | 483.6 us | 0.0 us | 5015 MHz | -75.0 dBm | - | 14.49% | 8.95% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22d334" /></svg> | QoS Data [HE-TB, HE-MCS 2, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | 0x0000 | 185 | 8.44% | 1070.0 B | 0.0 B | 334.4 us | 0.0 us | 5005 MHz | -75.0 dBm | - | 10.02% | 6.19% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3dd02f" /></svg> | QoS Data [HE-TB, HE-MCS 2, 26-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | 0x0000 | 1 | 0.05% | 1070.0 B | 0.0 B | 1304.1 us | 0.0 us | 5002 MHz | -75.0 dBm | - | 0.21% | 0.13% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c933" /></svg> | QoS Data [HE-TB, HE-MCS 2, 52-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | 0x0000 | 1 | 0.05% | 1070.0 B | 0.0 B | 987.1 us | 0.0 us | 5007 MHz | -75.0 dBm | - | 0.16% | 0.10% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0a480c" /></svg> | QoS Null [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 1 | 0.05% | 34.0 B | 0.0 B | 121.3 us | 0.0 us | 5015 MHz | -79.0 dBm | - | 0.02% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 0x0000 | 75 | 3.42% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -76.3 dBm | - | 4.84% | 2.99% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d790b" /></svg> | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 185 | 8.44% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5010 MHz | -79.0 dBm | - | 11.94% | 7.38% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 253 | 11.55% | 48.6 B | 8.1 B | 36.2 us | 2.7 us | 5010 MHz | - | 20.0 dBm | 1.48% | 0.92% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 252 | 11.50% | 57.9 B | 1.7 B | 39.3 us | 0.6 us | 5010 MHz | - | 20.0 dBm | 1.60% | 0.99% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 525 | 23.96% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 2.10% | 1.30% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 2 | 0.002565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=218 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 3 | 0.002565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=226 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 4 | 0.002566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=234 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 5 | 0.002633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 6 | 0.013048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 7 | 0.014565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=410 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 8 | 0.014565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=418 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 9 | 0.014566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=426 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 10 | 0.014633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 11 | 0.025048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 12 | 0.026565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=602 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 13 | 0.026565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=610 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 14 | 0.026566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=618 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 15 | 0.026633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 16 | 0.037048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 17 | 0.038565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=794 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 18 | 0.038565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=802 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 19 | 0.038566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=810 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 20 | 0.038633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 21 | 0.049048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 22 | 0.050565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=986 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 23 | 0.050565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=994 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 24 | 0.050566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1002 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 25 | 0.050633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 26 | 0.061048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 27 | 0.062565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1178 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 28 | 0.062565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1186 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 29 | 0.062566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1194 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 30 | 0.062633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 31 | 0.073048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 32 | 0.074565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1370 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 33 | 0.074565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1378 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 34 | 0.074566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1386 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 35 | 0.074633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 36 | 0.085048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 37 | 0.086565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1562 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 38 | 0.086565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1570 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 39 | 0.086566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1578 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 40 | 0.086633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 41 | 0.097048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 42 | 0.098565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1754 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 43 | 0.098565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1762 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 44 | 0.098566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1770 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 45 | 0.098633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 46 | 0.109048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 47 | 0.110565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1946 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 48 | 0.110565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1954 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 49 | 0.110566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1962 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 50 | 0.110633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

Frame numbers are local to capture `Ofdma48Mbps-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `UlMuMimo48MbpsLeakage001`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1992**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 99 | 4.97% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -76.0 dBm | - | 9.72% | 6.15% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36c723" /></svg> | QoS Data [HE-TB, HE-MCS 0, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 318 | 15.96% | 1070.0 B | 0.0 B | 1206.6 us | 0.0 us | 5010 MHz | -79.0 dBm | - | 60.65% | 38.37% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2dbe36" /></svg> | QoS Data [HE-TB, HE-MCS 2, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | 0x0000 | 318 | 15.96% | 1070.0 B | 0.0 B | 231.1 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 11.62% | 7.35% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19cc25" /></svg> | QoS Data [HE-TB, HE-MCS 2, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | 0x0000 | 318 | 15.96% | 1070.0 B | 0.0 B | 166.1 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 8.35% | 5.28% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 0x0000 | 75 | 3.77% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -76.3 dBm | - | 4.73% | 2.99% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 384 | 19.28% | 47.7 B | 6.7 B | 35.9 us | 2.2 us | 5010 MHz | - | 20.0 dBm | 2.18% | 1.38% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 384 | 19.28% | 57.9 B | 1.4 B | 39.3 us | 0.5 us | 5010 MHz | - | 20.0 dBm | 2.39% | 1.51% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 96 | 4.82% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.37% | 0.24% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 2 | 0.002565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=218 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 3 | 0.002565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=226 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 4 | 0.002566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=234 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 5 | 0.002633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 6 | 0.013048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 7 | 0.014565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=410 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 8 | 0.014565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=418 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 9 | 0.014566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=426 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 10 | 0.014633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 11 | 0.025048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 12 | 0.026565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=602 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 13 | 0.026565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=610 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 14 | 0.026566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=618 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 15 | 0.026633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 16 | 0.037048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 17 | 0.038565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=794 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 18 | 0.038565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=802 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 19 | 0.038566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=810 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 20 | 0.038633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 21 | 0.049048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 22 | 0.050565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=986 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 23 | 0.050565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=994 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 24 | 0.050566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1002 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 25 | 0.050633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 26 | 0.061048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 27 | 0.062565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1178 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 28 | 0.062565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1186 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 29 | 0.062566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1194 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 30 | 0.062633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 31 | 0.073048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 32 | 0.074565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1370 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 33 | 0.074565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1378 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 34 | 0.074566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1386 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 35 | 0.074633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 36 | 0.085048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 37 | 0.086565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1562 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 38 | 0.086565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1570 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 39 | 0.086566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1578 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 40 | 0.086633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 41 | 0.097048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 42 | 0.098565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1754 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 43 | 0.098565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1762 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 44 | 0.098566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1770 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 45 | 0.098633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 46 | 0.109048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 47 | 0.110565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1946 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 48 | 0.110565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1954 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 49 | 0.110566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1962 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 50 | 0.110633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

Frame numbers are local to capture `UlMuMimo48MbpsLeakage001-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `UlSu72Mbps`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1457**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 737 | 50.58% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -75.6 dBm | - | 96.27% | 45.79% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 720 | 49.42% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 3.73% | 1.78% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 1 | 0.200644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 2 | 0.201370000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 3 | 0.201418000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 4 | 0.202108000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 5 | 0.202156000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 6 | 0.202863000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 7 | 0.202911000 | ? → 0a:aa:00:00:00:03 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 8 | 0.300644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 9 | 0.301397000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 10 | 0.302155000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 11 | 0.302203000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 12 | 0.302865000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.302913000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 14 | 0.303576000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.303624000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 16 | 0.305010000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 17 | 0.305058000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 18 | 0.305721000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 19 | 0.305769000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 20 | 0.306432000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 21 | 0.306480000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 22 | 0.307876000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 23 | 0.307924000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 24 | 0.308586000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 25 | 0.308634000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 26 | 0.309295000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 27 | 0.309343000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 28 | 0.310741000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 29 | 0.310789000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 30 | 0.311452000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 31 | 0.311500000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 32 | 0.312163000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 33 | 0.312211000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 34 | 0.313615000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=10, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 35 | 0.313663000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 36 | 0.314326000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=11, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 37 | 0.314374000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 38 | 0.315037000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=12, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 39 | 0.315085000 | ? → 0a:aa:00:00:00:02 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 40 | 0.316481000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 41 | 0.316529000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 42 | 0.317191000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 43 | 0.317239000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 44 | 0.317900000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 45 | 0.317948000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 46 | 0.319334000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 47 | 0.319382000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 48 | 0.320044000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 49 | 0.320092000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | 50 | 0.320753000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=6 |

Frame numbers are local to capture `UlSu72Mbps-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `Ofdma72Mbps`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2194**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 524 | 23.88% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -78.9 dBm | - | 52.73% | 32.56% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1ddd30" /></svg> | QoS Data [HE-TB, HE-MCS 2, 106-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | 0x0000 | 187 | 8.52% | 1070.0 B | 0.0 B | 483.6 us | 0.0 us | 5015 MHz | -75.0 dBm | - | 14.65% | 9.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22d334" /></svg> | QoS Data [HE-TB, HE-MCS 2, 106-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | 0x0000 | 187 | 8.52% | 1070.0 B | 0.0 B | 334.4 us | 0.0 us | 5005 MHz | -75.0 dBm | - | 10.13% | 6.25% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3dd02f" /></svg> | QoS Data [HE-TB, HE-MCS 2, 26-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | 0x0000 | 1 | 0.05% | 1070.0 B | 0.0 B | 1304.1 us | 0.0 us | 5002 MHz | -75.0 dBm | - | 0.21% | 0.13% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c933" /></svg> | QoS Data [HE-TB, HE-MCS 2, 52-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | 0x0000 | 1 | 0.05% | 1070.0 B | 0.0 B | 987.1 us | 0.0 us | 5007 MHz | -75.0 dBm | - | 0.16% | 0.10% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0a480c" /></svg> | QoS Null [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 1 | 0.05% | 34.0 B | 0.0 B | 121.3 us | 0.0 us | 5015 MHz | -79.0 dBm | - | 0.02% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 0x0000 | 75 | 3.42% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -76.3 dBm | - | 4.84% | 2.99% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d790b" /></svg> | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 187 | 8.52% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5010 MHz | -79.0 dBm | - | 12.07% | 7.46% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 255 | 11.62% | 48.6 B | 8.1 B | 36.2 us | 2.7 us | 5010 MHz | - | 20.0 dBm | 1.49% | 0.92% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 254 | 11.58% | 57.9 B | 1.7 B | 39.3 us | 0.6 us | 5010 MHz | - | 20.0 dBm | 1.62% | 1.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 522 | 23.79% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 2.09% | 1.29% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 2 | 0.002565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=218 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 3 | 0.002565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=226 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 4 | 0.002566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=234 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 5 | 0.002633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 6 | 0.013048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 7 | 0.014565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=410 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 8 | 0.014565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=418 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 9 | 0.014566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=426 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 10 | 0.014633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 11 | 0.025048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 12 | 0.026565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=602 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 13 | 0.026565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=610 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 14 | 0.026566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=618 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 15 | 0.026633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 16 | 0.037048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 17 | 0.038565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=794 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 18 | 0.038565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=802 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 19 | 0.038566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=810 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 20 | 0.038633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 21 | 0.049048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 22 | 0.050565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=986 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 23 | 0.050565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=994 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 24 | 0.050566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1002 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 25 | 0.050633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 26 | 0.061048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 27 | 0.062565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1178 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 28 | 0.062565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1186 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 29 | 0.062566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1194 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 30 | 0.062633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 31 | 0.073048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 32 | 0.074565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1370 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 33 | 0.074565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1378 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 34 | 0.074566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1386 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 35 | 0.074633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 36 | 0.085048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 37 | 0.086565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1562 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 38 | 0.086565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1570 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 39 | 0.086566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1578 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 40 | 0.086633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 41 | 0.097048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 42 | 0.098565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1754 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 43 | 0.098565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1762 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 44 | 0.098566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1770 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 45 | 0.098633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 46 | 0.109048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 47 | 0.110565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1946 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 48 | 0.110565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1954 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 49 | 0.110566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1962 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 50 | 0.110633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

Frame numbers are local to capture `Ofdma72Mbps-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `UlMuMimo72MbpsLeakage001`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1996**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 0x0000 | 108 | 5.41% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -76.2 dBm | - | 10.59% | 6.71% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36c723" /></svg> | QoS Data [HE-TB, HE-MCS 0, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 0x0000 | 315 | 15.78% | 1070.0 B | 0.0 B | 1206.6 us | 0.0 us | 5010 MHz | -79.0 dBm | - | 60.00% | 38.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2dbe36" /></svg> | QoS Data [HE-TB, HE-MCS 2, 242-tone RU, GI 3.2 us, NSS 2, LDPC, A-MPDU] | 0x0000 | 315 | 15.78% | 1070.0 B | 0.0 B | 231.1 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 11.49% | 7.28% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19cc25" /></svg> | QoS Data [HE-TB, HE-MCS 2, 242-tone RU, GI 3.2 us, NSS 3, LDPC, A-MPDU] | 0x0000 | 315 | 15.78% | 1070.0 B | 0.0 B | 166.1 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 8.26% | 5.23% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 0x0000 | 75 | 3.76% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -76.3 dBm | - | 4.72% | 2.99% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | - | 381 | 19.09% | 47.7 B | 6.7 B | 35.9 us | 2.2 us | 5010 MHz | - | 20.0 dBm | 2.16% | 1.37% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 381 | 19.09% | 57.9 B | 1.4 B | 39.3 us | 0.5 us | 5010 MHz | - | 20.0 dBm | 2.36% | 1.50% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 106 | 5.31% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.41% | 0.26% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 2 | 0.002565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=218 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 3 | 0.002565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=226 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 4 | 0.002566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=234 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 5 | 0.002633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 6 | 0.013048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 7 | 0.014565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=410 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 8 | 0.014565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=418 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 9 | 0.014566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=426 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 10 | 0.014633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 11 | 0.025048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 12 | 0.026565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=602 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 13 | 0.026565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=610 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 14 | 0.026566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=618 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 15 | 0.026633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 16 | 0.037048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 17 | 0.038565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=794 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 18 | 0.038565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=802 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 19 | 0.038566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=810 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 20 | 0.038633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 21 | 0.049048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 22 | 0.050565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=986 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 23 | 0.050565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=994 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 24 | 0.050566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1002 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 25 | 0.050633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 26 | 0.061048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 27 | 0.062565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1178 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 28 | 0.062565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1186 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 29 | 0.062566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1194 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 30 | 0.062633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 31 | 0.073048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 32 | 0.074565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1370 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 33 | 0.074565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1378 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 34 | 0.074566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1386 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 35 | 0.074633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 36 | 0.085048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 37 | 0.086565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1562 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 38 | 0.086565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1570 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 39 | 0.086566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1578 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 40 | 0.086633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 41 | 0.097048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 42 | 0.098565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1754 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 43 | 0.098565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1762 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 44 | 0.098566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1770 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 45 | 0.098633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | 46 | 0.109048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 47 | 0.110565000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1946 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 48 | 0.110565000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1954 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | 49 | 0.110566000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | direction=to DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1962 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 50 | 0.110633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

Frame numbers are local to capture `UlMuMimo72MbpsLeakage001-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

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
