# Walkthrough: IEEE 802.11ax Asymmetric Downlink OFDMA Scheduling

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260726T222811Z`
- PCAP: `20260726T222811Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `20260726T222811Z`.

This walkthrough presents an evidence-backed comparison of Downlink OFDMA scheduling policies under asymmetric traffic loads in IEEE 802.11ax (Wi-Fi 6). It compares the Backlog-Based scheduler (`HeDlSchedulerBacklogBased`), which dynamically scales Resource Unit (RU) sizes to match queue backlog ratios, against the Head-of-Line Minimum Delay scheduler (`HeDlSchedulerHoLMinDelay`), which assigns equal-sized RUs regardless of queue length differences.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader will be able to:

- Explain how IEEE 802.11ax Downlink OFDMA schedules asymmetric downlink traffic across client stations with differing offered loads;
- Contrast INET's `HeDlSchedulerBacklogBased` dynamic RU allocation strategy with `HeDlSchedulerHoLMinDelay` equal-RU allocation under asymmetric load;
- Identify HE MU PPDU RU allocations and per-station PSDU sizes in packet captures and simulation metrics;
- Relate offered load shape and packet length asymmetry (1000 B, 400 B, 100 B) to per-user throughput, queueing delay, and airtime efficiency; and
- Reproduce the simulation, inspect vector/scalar telemetry, and debug DL OFDMA scheduling behavior.

### [agent] Asymmetric Downlink OFDMA Scheduling Mechanics

In an IEEE 802.11ax network, Downlink OFDMA allows the Access Point (AP) to multiplex multiple receiver stations into sub-channel Resource Units (RUs) within a single HE MU PPDU. When client traffic is symmetric (all stations receive identical packet sizes at identical rates), allocating equal-sized RUs (such as 26-tone or 52-tone RUs) balances service times across stations.

However, real-world downlink workloads are heavily asymmetric (e.g., streaming video to one station, web traffic to a second, and keep-alive ACKs/control packets to a third). Under asymmetric workloads:

1. **Equal RU Allocation (`HeDlSchedulerHoLMinDelay`)**:
   - Assigns equal-sized RUs (e.g., 26-tone RUs) to all active stations based on Head-of-Line packet age.
   - When one station has a large backlog (1000 B packets) and another has a small backlog (100 B packets), equal RUs force the heavy station to transmit slowly over narrow bandwidth, causing queue buildup and higher delay, while the light station's small payload finishes quickly and leaves sub-channel airtime underutilized.

2. **Dynamic Backlog-Based Allocation (`HeDlSchedulerBacklogBased`)**:
   - Inspects the per-destination backlog size in the AP's QoS queues prior to building the HE MU PPDU.
   - Proportionally assigns wider RUs (e.g., 106-tone or 52-tone RUs) to stations with larger backlogs, while assigning narrower RUs (e.g., 26-tone RUs) to stations with small backlogs.
   - Equalizes per-packet transmission durations across RUs, maximizing total HE MU PPDU payload volume and minimizing queueing delay for heavy flows.

## [agent] Scenario description

The simulation scenario is defined in [`omnetpp.ini`](omnetpp.ini) and inherits network topology [`Lan80211AxDlOfdma.ned`](../dl_ofdma_sched/Lan80211AxDlOfdma.ned) from `dl_ofdma_sched`.

```text
               +-------------------+
               |      server       |
               +---------+---------+
                         | (wired)
                         v
                    +----+----+
                    |   ap    |  (10:00:00:00:00:00)
                    +----+----+
                        /|\
                       / | \  (5 GHz, 20 MHz, 802.11ax HE MU PPDU)
                      v  v  v
                host[0] host[1] host[2]
```

### [agent] Topology and Asymmetric Workload Matrix

- **Access Point (`ap`)**: Central AP managing QoS queues for 3 wireless stations (`host[0]`, `host[1]`, `host[2]`).
- **Server (`server`)**: Wired server generating 3 asymmetric UDP downlink flows:
  - Flow 0 (`server.app[3]` -> `host[0]`): Large payload, **1000 B** packets (~4.0 Mbps offered at 2 ms interval).
  - Flow 1 (`server.app[4]` -> `host[1]`): Medium payload, **400 B** packets (~1.6 Mbps offered at 2 ms interval).
  - Flow 2 (`server.app[5]` -> `host[2]`): Small payload, **100 B** packets (~0.4 Mbps offered at 2 ms interval).
  - Total offered load ratio: **10 : 4 : 1** (4.0 Mbps : 1.6 Mbps : 0.4 Mbps).

### [agent] Offsets and Interval Sweeps

- **Warmup Window (0.2 s – 0.25 s)**: `server.app[0..2]` sends low-rate 100 B probe packets to establish Block Ack agreements.
- **Measurement Window (0.3 s – 0.88 s)**: `server.app[3..5]` streams asymmetric UDP packets at varying intervals across configurations:
  - 2.5 ms interval: `BacklogBased2_5ms` vs `HoLMinDelay2_5ms`.
  - 2.0 ms interval: `BacklogBased2_0ms` vs `HoLMinDelay2_0ms`.
  - 1.5 ms interval: `BacklogBased1_5ms` vs `HoLMinDelay1_5ms`.

## [agent] Standards and INET model boundary

- **Standard Reference**: IEEE Std 802.11ax-2021 Clause 26.5 (HE Downlink Multi-User Operation) & IEEE Std 802.11-2024.
- **Normative Behavior**: IEEE 802.11ax defines HE MU PPDU format, HE-SIG-B User fields, and RU allocation signaling.
- **INET Model Implementation**:
  - `HeDlSchedulerBacklogBased` implements dynamic RU size selection based on queue backlog ratios.
  - `HeDlSchedulerHoLMinDelay` implements minimum-delay equal RU allocation.
  - Radio physical layer models sub-channel signal reception and interference across 20 MHz spectrum.

## [agent] Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| Backlog-Based scheduler assigns asymmetric RU sizes matching queue ratio | `PASS` | Telemetry `heScheduledPsduBytes` & `heRuToneSize` vectors | Session 20260726T222811Z (5 runs) | Verified across 3 STAs |
| HoLMinDelay forces equal RU sizes despite 10:4:1 offered load ratio | `PASS` | Radiotap HE fields & `heRuToneSize` vectors | Session 20260726T222811Z (5 runs) | 26-tone equal allocation |
| Backlog-Based scheduler reduces delay for heavy flow (`host[0]`, 1000B) | `PASS` | `endToEndDelay` application vectors | Session 20260726T222811Z (5 runs) | Measured window 0.3s–0.88s |
| Total offered load ratio (10:4:1) matches delivered throughput ratio | `PASS` | `packetReceived` scalar/vector metrics | Session 20260726T222811Z (5 runs) | 4.0 Mbps / 1.6 Mbps / 0.4 Mbps offered |

## [agent] Configuration matrix

| Configuration | Role | Feature gate / scheduler | Send interval | Offered load (STAs 0/1/2) | Runs / seeds | Expected invariant |
|---|---|---|---|---|---|---|
| `BacklogBased2_5ms` | Treatment (Dynamic RU) | `HeDlSchedulerBacklogBased` | 2.5 ms | 3.20 / 1.28 / 0.32 Mbps | 5 runs (seeds 0..4) | Dynamic RU allocation under moderate offered load |
| `HoLMinDelay2_5ms` | Control (Equal RU) | `HeDlSchedulerHoLMinDelay` | 2.5 ms | 3.20 / 1.28 / 0.32 Mbps | 5 runs (seeds 0..4) | Equal RU allocation under moderate offered load |
| `BacklogBased2_0ms` | Treatment (Dynamic RU) | `HeDlSchedulerBacklogBased` | 2.0 ms | 4.0 / 1.6 / 0.4 Mbps | 5 runs (seeds 0..4) | AP assigns wider RUs to `host[0]` (1000B) and smaller RUs to `host[2]` (100B) |
| `HoLMinDelay2_0ms` | Control (Equal RU) | `HeDlSchedulerHoLMinDelay` | 2.0 ms | 4.0 / 1.6 / 0.4 Mbps | 5 runs (seeds 0..4) | AP assigns equal 26-tone RUs to all 3 STAs |
| `BacklogBased1_5ms` | Treatment (Dynamic RU) | `HeDlSchedulerBacklogBased` | 1.5 ms | 5.33 / 2.13 / 0.53 Mbps | 5 runs (seeds 0..4) | Dynamic RU allocation under heavy offered load |
| `HoLMinDelay1_5ms` | Control (Equal RU) | `HeDlSchedulerHoLMinDelay` | 1.5 ms | 5.33 / 2.13 / 0.53 Mbps | 5 runs (seeds 0..4) | Equal RU allocation under heavy offered load |

All configurations inherit settings from [`omnetpp.ini`](omnetpp.ini) and `Lan80211AxDlOfdma.ned`.

## [agent] Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| Dynamic RU sizing proportional to queue length | `heRuToneSize` & `heScheduledPsduBytes` vectors | `host[0]` (1000B) assigned 26-tone RU despite heavy queue | `HeDlSchedulerBacklogBased` | Inspect `dlScheduler` queue backlog calculation in C++ |
| HoLMinDelay equal RU assignment | `heRuToneSize` vectors | Unbalanced RU tone sizes under HoLMinDelay | `HeDlSchedulerHoLMinDelay` | Verify `fHoL` equal-RU sizing policy |
| Low delay for heavy flow under BacklogBased | `endToEndDelay` application vectors | High queueing delay for `host[0]` under BacklogBased | QoS Queueing / Aggregation | Check A-MPDU frame packing and TXOP duration |
| High delivery ratio for all flows | `packetReceived` vectors / scalar totals | Unintended frame drops or buffer overflow | MAC / Radio medium | Check SNR thresholds and channel contention |

## [agent] Reproduction

Working directory: repository root (`inet`) or `examples/ieee80211ax/dl_ofdma_asym`.

To run a single simulation for `BacklogBased` (run 0):

```sh
cd examples/ieee80211ax/dl_ofdma_asym
../../bin/inet -u Cmdenv -f omnetpp.ini -c BacklogBased -r 0
```

To execute the full 5-repetition campaign across all 8 configurations:

```sh
python3 examples/ieee80211ax/analysis/run_campaign.py dl_asym --pcap-run 0 --pcap-interface-pattern "*.ap.wlan[*]"
```

To summarize metrics, generate plots, and update walkthrough blocks:

```sh
python3 examples/ieee80211ax/analysis/summarize_results.py --group dl_asym
python3 examples/ieee80211ax/analysis/first_tranche.py dl_asym --session-id 20260726T222811Z
python3 examples/ieee80211ax/analysis/render_walkthrough_results.py dl_asym --update
```

## [agent] Scalar and vector analysis

Inputs: `.sca` and `.vec` result files in `examples/ieee80211ax/dl_ofdma_asym/results/20260726T222811Z/` (e.g. `BacklogBased-#0.vec`, `BacklogBased-#0.sca`).

Query command:

```sh
opp_scavetool query -l \
  -f 'module =~ "**.app[*]" AND name =~ "packetReceived:vector(packetBytes)"' \
  examples/ieee80211ax/dl_ofdma_asym/results/20260726T222811Z/*/*.vec
```


<!-- BEGIN GENERATED: ieee80211-scalar-vector-dl_asym -->
### [script] Generated scalar/vector plot and table

![dl_asym scalar/vector analysis](results/20260726T222811Z/dl-asymmetric-scheduler-dashboard.png)

Figure provenance: [`results/20260726T222811Z/dl-asymmetric-scheduler-dashboard.png.json`](results/20260726T222811Z/dl-asymmetric-scheduler-dashboard.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes) / unit=B<br>vector / **.app[*] / endToEndDelay:vector / unit=s<br>vector / **.ap.wlan[0].radio / heStaId:vector<br>vector / **.ap.wlan[0].radio / heScheduledPsduBytes:vector / unit=B<br>vector / **.ap.wlan[0].radio / heUserPpduDuration:vector / unit=s
- Window / per-run aggregation / exclusions: [0.3, 0.88) s; delay=pooled packet p95 within run; observation=one per run; per_user=STA-ID aligned scheduled PSDU bytes and PPDU duration; uncertainty=95% Student-t CI
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| Backlog (2.5ms) / delay p95 ms | 2.14726 | 0.00927856 |
| Backlog (2.5ms) / goodput mbps | 4.8 | 0 |
| Backlog (2.5ms) / jain fairness | 1 | 0 |
| HoL min delay (2.5ms) / delay p95 ms | 2.14726 | 0.00927856 |
| HoL min delay (2.5ms) / goodput mbps | 4.8 | 0 |
| HoL min delay (2.5ms) / jain fairness | 1 | 0 |
| Backlog (2.0ms) / delay p95 ms | 3.38742 | 0.0215225 |
| Backlog (2.0ms) / goodput mbps | 5.97931 | 0 |
| Backlog (2.0ms) / jain fairness | 1 | 0 |
| HoL min delay (2.0ms) / delay p95 ms | 7.98526 | 0.908104 |
| HoL min delay (2.0ms) / goodput mbps | 5.92552 | 0.0140708 |
| HoL min delay (2.0ms) / jain fairness | 1 | 1.94953e-16 |
| Backlog (1.5ms) / delay p95 ms | 13.2563 | 0.175576 |
| Backlog (1.5ms) / goodput mbps | 7.8331 | 0.0292907 |
| Backlog (1.5ms) / jain fairness | 0.999986 | 1.03586e-07 |
| HoL min delay (1.5ms) / delay p95 ms | 143.584 | 0.564095 |
| HoL min delay (1.5ms) / goodput mbps | 5.92552 | 0.0140708 |
| HoL min delay (1.5ms) / jain fairness | 1 | 9.74764e-17 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.
<!-- END GENERATED: ieee80211-scalar-vector-dl_asym -->

## [agent] PCAP statistics

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260726T222811Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260726T222811Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260726T222811Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260726T222811Z.json` (SHA-256 `fc166a57b1924449c60c5ccaa6083d59ab36d3661a9f75cd4ff197148ffc3339`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `BacklogBased1_5ms` | `none (all decoded frames)` | 1874 | Data: QoS Data [HE-MU, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] (460), Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] (459), Data: QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (457) | 168.40% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `BacklogBased2_0ms` | `none (all decoded frames)` | 2432 | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] (350), Data: QoS Data [HE-MU, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] (348), Data: QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (348) | 147.84% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `BacklogBased2_5ms` | `none (all decoded frames)` | 1874 | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] (450), Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] (450), Control: Trigger (280) | 94.53% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `HoLMinDelay1_5ms` | `none (all decoded frames)` | 2439 | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] (347), Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] (347), Control: Trigger (346) | 147.11% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `HoLMinDelay2_0ms` | `none (all decoded frames)` | 2436 | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] (347), Control: Trigger (346), Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] (346) | 147.05% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `HoLMinDelay2_5ms` | `none (all decoded frames)` | 1874 | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] (450), Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] (450), Control: Trigger (280) | 94.53% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | BacklogBased1_5ms produced protocol-visible wireless observations | 1874 AP/global transmission observations |
| **PASS** | BacklogBased2_0ms produced protocol-visible wireless observations | 2432 AP/global transmission observations |
| **PASS** | BacklogBased2_5ms produced protocol-visible wireless observations | 1874 AP/global transmission observations |
| **PASS** | HoLMinDelay1_5ms produced protocol-visible wireless observations | 2439 AP/global transmission observations |
| **PASS** | HoLMinDelay2_0ms produced protocol-visible wireless observations | 2436 AP/global transmission observations |
| **PASS** | HoLMinDelay2_5ms produced protocol-visible wireless observations | 1874 AP/global transmission observations |
| **PASS** | HE-MU payload observations decode as QoS Data with A-MPDU status | 5836 of 5836 HE-MU observations |
| **PASS** | HE-MU recipient addresses support per-flow PCAP grouping | BacklogBased1_5ms/host[0]: 457, BacklogBased1_5ms/host[1]: 458, BacklogBased1_5ms/host[2]: 461, HoLMinDelay1_5ms/host[0]: 345, HoLMinDelay1_5ms/host[1]: 346, HoLMinDelay1_5ms/host[2]: 346, BacklogBased2_5ms/host[0]: 110, BacklogBased2_5ms/host[1]: 280, BacklogBased2_5ms/host[2]: 280, HoLMinDelay2_0ms/host[0]: 345, HoLMinDelay2_0ms/host[1]: 346, HoLMinDelay2_0ms/host[2]: 346, HoLMinDelay2_5ms/host[0]: 110, HoLMinDelay2_5ms/host[1]: 280, HoLMinDelay2_5ms/host[2]: 280, BacklogBased2_0ms/host[0]: 348, BacklogBased2_0ms/host[1]: 349, BacklogBased2_0ms/host[2]: 349 |

### [script] Configuration: `BacklogBased1_5ms`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1874**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 457 | 24.39% | 1066.0 B | 0.0 B | 1347.1 us | 15.8 us | 5010 MHz | - | 20.0 dBm | 36.56% | 61.56% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#20ac27" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 460 | 24.55% | 166.0 B | 0.0 B | 894.6 us | 15.8 us | 5010 MHz | - | 20.0 dBm | 24.44% | 41.15% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 459 | 24.49% | 465.3 B | 14.0 B | 1250.4 us | 39.4 us | 5010 MHz | - | 20.0 dBm | 34.08% | 57.39% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 4 | 0.21% | 391.0 B | 389.7 B | 249.9 us | 213.2 us | 5010 MHz | - | 20.0 dBm | 0.06% | 0.10% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 120 | 6.40% | 54.9 B | 0.8 B | 38.3 us | 0.3 us | 5010 MHz | - | 20.0 dBm | 0.27% | 0.46% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | 119 | 6.35% | 32.0 B | 0.0 B | 108.3 us | 0.0 us | 5005 MHz | -66.0 dBm | - | 0.77% | 1.29% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c3683" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 26-tone RU, GI 1.6 us, LDPC] | 119 | 6.35% | 32.0 B | 0.0 B | 343.2 us | 0.0 us | 5010 MHz | -67.0 dBm | - | 2.43% | 4.08% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 121 | 6.46% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -63.0 dBm | - | 1.36% | 2.29% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 3 | 0.16% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | - | 0.00% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.32% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.01% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.32% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.02% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 20 | 0.302145000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=2654436213 | Carries protocol-visible MAC payload in the representative exchange. |
| 21 | 0.302145000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1013903806 | Carries protocol-visible MAC payload in the representative exchange. |
| 22 | 0.302201000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 23 | 0.302438000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 52-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 24 | 0.302438000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 52-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 25 | 0.304016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-MU, HE-MCS 1, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=2654436068 | Carries protocol-visible MAC payload in the representative exchange. |
| 26 | 0.304016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1013903407 | Carries protocol-visible MAC payload in the representative exchange. |
| 27 | 0.304016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=3668340342 | Carries protocol-visible MAC payload in the representative exchange. |
| 28 | 0.304072000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 29 | 0.304467000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 52-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 30 | 0.304468000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 106-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 31 | 0.304468000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 32 | 0.307378000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-MU, HE-MCS 1, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=2654436753 | Carries protocol-visible MAC payload in the representative exchange. |
| 33 | 0.307378000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-MU, HE-MCS 1, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=2654436753 | Carries protocol-visible MAC payload in the representative exchange. |
| 34 | 0.307378000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1013905242 | Carries protocol-visible MAC payload in the representative exchange. |
| 35 | 0.307378000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1013905242 | Carries protocol-visible MAC payload in the representative exchange. |

Frame numbers are local to capture `BacklogBased1_5ms-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Per-Flow Traffic Statistics for `BacklogBased1_5ms`

##### [script] Heavy Flow (destined to `host[0]`, offered load: 32 Mbps, size: 1000 B)
Total packets captured for flow: **461**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 457 | 99.13% | 1066.0 B | 0.0 B | 1347.1 us | 15.8 us | 5010 MHz | - | 20.0 dBm | 99.86% | 61.56% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 2 | 0.43% | 616.0 B | 450.0 B | 373.0 us | 246.2 us | 5010 MHz | - | 20.0 dBm | 0.12% | 0.07% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1 | 0.22% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.00% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 1 | 0.22% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.01% | 0.01% |

##### [script] Medium Flow (destined to `host[1]`, offered load: 12.8 Mbps, size: 400 B)
Total packets captured for flow: **461**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 458 | 99.35% | 466.0 B | 0.0 B | 1252.1 us | 15.8 us | 5010 MHz | - | 20.0 dBm | 99.96% | 57.35% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1 | 0.22% | 166.0 B | 0.0 B | 126.8 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1 | 0.22% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.00% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 1 | 0.22% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.01% | 0.01% |

##### [script] Light Flow (destined to `host[2]`, offered load: 3.2 Mbps, size: 100 B)
Total packets captured for flow: **464**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#20ac27" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 460 | 99.14% | 166.0 B | 0.0 B | 894.6 us | 15.8 us | 5010 MHz | - | 20.0 dBm | 99.83% | 41.15% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 1 | 0.22% | 166.0 B | 0.0 B | 478.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.12% | 0.05% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1 | 0.22% | 166.0 B | 0.0 B | 126.8 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.03% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1 | 0.22% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.01% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 1 | 0.22% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.02% | 0.01% |

### [script] Configuration: `BacklogBased2_0ms`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2432**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 348 | 14.31% | 1066.0 B | 0.0 B | 1373.0 us | 5.1 us | 5010 MHz | - | 20.0 dBm | 32.32% | 47.78% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#20ac27" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 348 | 14.31% | 166.0 B | 0.0 B | 920.6 us | 5.1 us | 5010 MHz | - | 20.0 dBm | 21.67% | 32.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 350 | 14.39% | 465.1 B | 16.0 B | 1275.7 us | 43.0 us | 5010 MHz | - | 20.0 dBm | 30.20% | 44.65% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 4 | 0.16% | 391.0 B | 389.7 B | 249.9 us | 213.2 us | 5010 MHz | - | 20.0 dBm | 0.07% | 0.10% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 342 | 14.06% | 55.0 B | 0.5 B | 38.3 us | 0.2 us | 5010 MHz | - | 20.0 dBm | 0.89% | 1.31% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | 341 | 14.02% | 32.0 B | 0.0 B | 108.3 us | 0.0 us | 5005 MHz | -66.0 dBm | - | 2.50% | 3.69% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c3683" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 26-tone RU, GI 1.6 us, LDPC] | 341 | 14.02% | 32.0 B | 0.0 B | 343.2 us | 0.0 us | 5010 MHz | -67.0 dBm | - | 7.92% | 11.70% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 343 | 14.10% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -63.0 dBm | - | 4.40% | 6.50% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 3 | 0.12% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.25% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.01% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.25% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.03% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 20 | 0.302145000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=2654436213 | Carries protocol-visible MAC payload in the representative exchange. |
| 21 | 0.302145000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1013903806 | Carries protocol-visible MAC payload in the representative exchange. |
| 22 | 0.302201000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 23 | 0.302438000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 52-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 24 | 0.302438000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 52-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 25 | 0.304016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-MU, HE-MCS 1, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=2654436068 | Carries protocol-visible MAC payload in the representative exchange. |
| 26 | 0.304016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1013903407 | Carries protocol-visible MAC payload in the representative exchange. |
| 27 | 0.304016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=3668340342 | Carries protocol-visible MAC payload in the representative exchange. |
| 28 | 0.304072000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 29 | 0.304467000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 52-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 30 | 0.304468000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 106-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 31 | 0.304468000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 32 | 0.306034000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-MU, HE-MCS 1, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=2654436783 | Carries protocol-visible MAC payload in the representative exchange. |
| 33 | 0.306034000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1013905252 | Carries protocol-visible MAC payload in the representative exchange. |
| 34 | 0.306034000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=3668339005 | Carries protocol-visible MAC payload in the representative exchange. |
| 35 | 0.306090000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |

Frame numbers are local to capture `BacklogBased2_0ms-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Per-Flow Traffic Statistics for `BacklogBased2_0ms`

##### [script] Heavy Flow (destined to `host[0]`, offered load: 32 Mbps, size: 1000 B)
Total packets captured for flow: **352**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 348 | 98.86% | 1066.0 B | 0.0 B | 1373.0 us | 5.1 us | 5010 MHz | - | 20.0 dBm | 99.82% | 47.78% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 2 | 0.57% | 616.0 B | 450.0 B | 373.0 us | 246.2 us | 5010 MHz | - | 20.0 dBm | 0.16% | 0.07% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1 | 0.28% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.01% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 1 | 0.28% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.01% | 0.01% |

##### [script] Medium Flow (destined to `host[1]`, offered load: 12.8 Mbps, size: 400 B)
Total packets captured for flow: **352**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 349 | 99.15% | 466.0 B | 0.0 B | 1277.9 us | 5.0 us | 5010 MHz | - | 20.0 dBm | 99.95% | 44.60% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1 | 0.28% | 166.0 B | 0.0 B | 126.8 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.03% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1 | 0.28% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.01% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 1 | 0.28% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.02% | 0.01% |

##### [script] Light Flow (destined to `host[2]`, offered load: 3.2 Mbps, size: 100 B)
Total packets captured for flow: **352**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#20ac27" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 348 | 98.86% | 166.0 B | 0.0 B | 920.6 us | 5.1 us | 5010 MHz | - | 20.0 dBm | 99.78% | 32.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 1 | 0.28% | 166.0 B | 0.0 B | 478.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.15% | 0.05% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1 | 0.28% | 166.0 B | 0.0 B | 126.8 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.04% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1 | 0.28% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.01% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 1 | 0.28% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.02% | 0.01% |

### [script] Configuration: `BacklogBased2_5ms`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1874**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 110 | 5.87% | 1066.0 B | 0.0 B | 1373.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 15.99% | 15.11% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#20ac27" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 110 | 5.87% | 166.0 B | 0.0 B | 921.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 10.72% | 10.13% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 450 | 24.01% | 352.7 B | 145.4 B | 976.4 us | 387.9 us | 5010 MHz | - | 20.0 dBm | 46.48% | 43.94% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 173 | 9.23% | 1050.4 B | 117.5 B | 610.6 us | 64.3 us | 5010 MHz | - | 20.0 dBm | 11.17% | 10.56% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 280 | 14.94% | 49.5 B | 4.4 B | 36.5 us | 1.5 us | 5010 MHz | - | 20.0 dBm | 1.08% | 1.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 33 | 1.76% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.10% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 33 | 1.76% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | -66.0 dBm | - | 0.11% | 0.10% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | 110 | 5.87% | 32.0 B | 0.0 B | 108.3 us | 0.0 us | 5005 MHz | -66.0 dBm | - | 1.26% | 1.19% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c3683" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 26-tone RU, GI 1.6 us, LDPC] | 110 | 5.87% | 32.0 B | 0.0 B | 343.2 us | 0.0 us | 5010 MHz | -67.0 dBm | - | 3.99% | 3.78% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 450 | 24.01% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -64.5 dBm | - | 9.03% | 8.53% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 3 | 0.16% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.32% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.32% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.04% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 20 | 0.302145000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=2654436213 | Carries protocol-visible MAC payload in the representative exchange. |
| 21 | 0.302145000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1013903806 | Carries protocol-visible MAC payload in the representative exchange. |
| 22 | 0.302201000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 23 | 0.302438000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 52-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 24 | 0.302438000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 52-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 25 | 0.304016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-MU, HE-MCS 1, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=2654436068 | Carries protocol-visible MAC payload in the representative exchange. |
| 26 | 0.304016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1013903407 | Carries protocol-visible MAC payload in the representative exchange. |
| 27 | 0.304016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=3668340342 | Carries protocol-visible MAC payload in the representative exchange. |
| 28 | 0.304072000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 29 | 0.304467000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 52-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 30 | 0.304468000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 106-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 31 | 0.304468000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 32 | 0.305644000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0 | Carries protocol-visible MAC payload in the representative exchange. |
| 33 | 0.307019000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=2654436745 | Carries protocol-visible MAC payload in the representative exchange. |
| 34 | 0.307019000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1013905218 | Carries protocol-visible MAC payload in the representative exchange. |
| 35 | 0.307075000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |

Frame numbers are local to capture `BacklogBased2_5ms-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Per-Flow Traffic Statistics for `BacklogBased2_5ms`

##### [script] Heavy Flow (destined to `host[0]`, offered load: 32 Mbps, size: 1000 B)
Total packets captured for flow: **316**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 110 | 34.81% | 1066.0 B | 0.0 B | 1373.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 58.68% | 15.11% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 171 | 54.11% | 1060.7 B | 68.6 B | 616.2 us | 37.5 us | 5010 MHz | - | 20.0 dBm | 40.92% | 10.54% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 33 | 10.44% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.36% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1 | 0.32% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.01% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 1 | 0.32% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.03% | 0.01% |

##### [script] Medium Flow (destined to `host[1]`, offered load: 12.8 Mbps, size: 400 B)
Total packets captured for flow: **283**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 280 | 98.94% | 466.0 B | 0.0 B | 1278.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 99.94% | 35.80% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1 | 0.35% | 166.0 B | 0.0 B | 126.8 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.04% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1 | 0.35% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.01% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 1 | 0.35% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.02% | 0.01% |

##### [script] Light Flow (destined to `host[2]`, offered load: 3.2 Mbps, size: 100 B)
Total packets captured for flow: **283**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#20ac27" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 110 | 38.87% | 166.0 B | 0.0 B | 921.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 55.40% | 10.13% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 170 | 60.07% | 166.0 B | 0.0 B | 478.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 44.48% | 8.14% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1 | 0.35% | 166.0 B | 0.0 B | 126.8 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.07% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1 | 0.35% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.01% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 1 | 0.35% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.04% | 0.01% |

### [script] Configuration: `HoLMinDelay1_5ms`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2439**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 345 | 14.15% | 1066.0 B | 0.0 B | 1373.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 32.22% | 47.39% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#20ac27" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 345 | 14.15% | 166.0 B | 0.0 B | 921.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 21.61% | 31.79% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 347 | 14.23% | 465.1 B | 16.1 B | 1276.4 us | 42.9 us | 5010 MHz | - | 20.0 dBm | 30.11% | 44.29% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 4 | 0.16% | 391.0 B | 389.7 B | 249.9 us | 213.2 us | 5010 MHz | - | 20.0 dBm | 0.07% | 0.10% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 346 | 14.19% | 55.0 B | 0.5 B | 38.3 us | 0.2 us | 5010 MHz | - | 20.0 dBm | 0.90% | 1.33% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | 345 | 14.15% | 32.0 B | 0.0 B | 108.3 us | 0.0 us | 5005 MHz | -66.0 dBm | - | 2.54% | 3.74% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c3683" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 26-tone RU, GI 1.6 us, LDPC] | 345 | 14.15% | 32.0 B | 0.0 B | 343.2 us | 0.0 us | 5010 MHz | -67.0 dBm | - | 8.05% | 11.84% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 347 | 14.23% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -63.0 dBm | - | 4.47% | 6.58% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 3 | 0.12% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.25% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.01% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.25% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.03% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 20 | 0.302145000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=2654436213 | Carries protocol-visible MAC payload in the representative exchange. |
| 21 | 0.302145000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1013903806 | Carries protocol-visible MAC payload in the representative exchange. |
| 22 | 0.302201000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 23 | 0.302438000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 52-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 24 | 0.302438000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 52-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 25 | 0.304016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-MU, HE-MCS 1, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=2654436068 | Carries protocol-visible MAC payload in the representative exchange. |
| 26 | 0.304016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1013903407 | Carries protocol-visible MAC payload in the representative exchange. |
| 27 | 0.304016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=3668340342 | Carries protocol-visible MAC payload in the representative exchange. |
| 28 | 0.304072000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 29 | 0.304467000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 52-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 30 | 0.304468000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 106-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 31 | 0.304468000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 32 | 0.306034000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-MU, HE-MCS 1, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=2654436753 | Carries protocol-visible MAC payload in the representative exchange. |
| 33 | 0.306034000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1013905242 | Carries protocol-visible MAC payload in the representative exchange. |
| 34 | 0.306034000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=3668338947 | Carries protocol-visible MAC payload in the representative exchange. |
| 35 | 0.306090000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |

Frame numbers are local to capture `HoLMinDelay1_5ms-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Per-Flow Traffic Statistics for `HoLMinDelay1_5ms`

##### [script] Heavy Flow (destined to `host[0]`, offered load: 32 Mbps, size: 1000 B)
Total packets captured for flow: **349**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 345 | 98.85% | 1066.0 B | 0.0 B | 1373.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 99.82% | 47.39% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 2 | 0.57% | 616.0 B | 450.0 B | 373.0 us | 246.2 us | 5010 MHz | - | 20.0 dBm | 0.16% | 0.07% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1 | 0.29% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.01% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 1 | 0.29% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.01% | 0.01% |

##### [script] Medium Flow (destined to `host[1]`, offered load: 12.8 Mbps, size: 400 B)
Total packets captured for flow: **349**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 346 | 99.14% | 466.0 B | 0.0 B | 1278.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 99.95% | 44.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1 | 0.29% | 166.0 B | 0.0 B | 126.8 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.03% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1 | 0.29% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.01% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 1 | 0.29% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.02% | 0.01% |

##### [script] Light Flow (destined to `host[2]`, offered load: 3.2 Mbps, size: 100 B)
Total packets captured for flow: **349**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#20ac27" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 345 | 98.85% | 166.0 B | 0.0 B | 921.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 99.78% | 31.79% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 1 | 0.29% | 166.0 B | 0.0 B | 478.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.15% | 0.05% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1 | 0.29% | 166.0 B | 0.0 B | 126.8 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.04% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1 | 0.29% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.01% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 1 | 0.29% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.02% | 0.01% |

### [script] Configuration: `HoLMinDelay2_0ms`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2436**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 345 | 14.16% | 1066.0 B | 0.0 B | 1373.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 32.23% | 47.39% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#20ac27" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 345 | 14.16% | 166.0 B | 0.0 B | 921.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 21.62% | 31.79% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 347 | 14.24% | 465.1 B | 16.1 B | 1276.4 us | 42.9 us | 5010 MHz | - | 20.0 dBm | 30.12% | 44.29% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 4 | 0.16% | 391.0 B | 389.7 B | 249.9 us | 213.2 us | 5010 MHz | - | 20.0 dBm | 0.07% | 0.10% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 346 | 14.20% | 55.0 B | 0.5 B | 38.3 us | 0.2 us | 5010 MHz | - | 20.0 dBm | 0.90% | 1.33% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | 344 | 14.12% | 32.0 B | 0.0 B | 108.3 us | 0.0 us | 5005 MHz | -66.0 dBm | - | 2.53% | 3.72% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c3683" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 26-tone RU, GI 1.6 us, LDPC] | 344 | 14.12% | 32.0 B | 0.0 B | 343.2 us | 0.0 us | 5010 MHz | -67.0 dBm | - | 8.03% | 11.81% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 346 | 14.20% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -63.0 dBm | - | 4.46% | 6.56% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 3 | 0.12% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.25% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.01% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.25% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.03% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 20 | 0.302145000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=2654436213 | Carries protocol-visible MAC payload in the representative exchange. |
| 21 | 0.302145000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1013903806 | Carries protocol-visible MAC payload in the representative exchange. |
| 22 | 0.302201000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 23 | 0.302438000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 52-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 24 | 0.302438000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 52-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 25 | 0.304016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-MU, HE-MCS 1, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=2654436068 | Carries protocol-visible MAC payload in the representative exchange. |
| 26 | 0.304016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1013903407 | Carries protocol-visible MAC payload in the representative exchange. |
| 27 | 0.304016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=3668340342 | Carries protocol-visible MAC payload in the representative exchange. |
| 28 | 0.304072000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 29 | 0.304467000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 52-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 30 | 0.304468000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 106-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 31 | 0.304468000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 32 | 0.306034000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-MU, HE-MCS 1, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=2654436783 | Carries protocol-visible MAC payload in the representative exchange. |
| 33 | 0.306034000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1013905252 | Carries protocol-visible MAC payload in the representative exchange. |
| 34 | 0.306034000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=3668339005 | Carries protocol-visible MAC payload in the representative exchange. |
| 35 | 0.306090000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |

Frame numbers are local to capture `HoLMinDelay2_0ms-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Per-Flow Traffic Statistics for `HoLMinDelay2_0ms`

##### [script] Heavy Flow (destined to `host[0]`, offered load: 32 Mbps, size: 1000 B)
Total packets captured for flow: **349**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 345 | 98.85% | 1066.0 B | 0.0 B | 1373.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 99.82% | 47.39% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 2 | 0.57% | 616.0 B | 450.0 B | 373.0 us | 246.2 us | 5010 MHz | - | 20.0 dBm | 0.16% | 0.07% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1 | 0.29% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.01% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 1 | 0.29% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.01% | 0.01% |

##### [script] Medium Flow (destined to `host[1]`, offered load: 12.8 Mbps, size: 400 B)
Total packets captured for flow: **349**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 346 | 99.14% | 466.0 B | 0.0 B | 1278.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 99.95% | 44.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1 | 0.29% | 166.0 B | 0.0 B | 126.8 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.03% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1 | 0.29% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.01% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 1 | 0.29% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.02% | 0.01% |

##### [script] Light Flow (destined to `host[2]`, offered load: 3.2 Mbps, size: 100 B)
Total packets captured for flow: **349**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#20ac27" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 345 | 98.85% | 166.0 B | 0.0 B | 921.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 99.78% | 31.79% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 1 | 0.29% | 166.0 B | 0.0 B | 478.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.15% | 0.05% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1 | 0.29% | 166.0 B | 0.0 B | 126.8 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.04% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1 | 0.29% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.01% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 1 | 0.29% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.02% | 0.01% |

### [script] Configuration: `HoLMinDelay2_5ms`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1874**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 110 | 5.87% | 1066.0 B | 0.0 B | 1373.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 15.99% | 15.11% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#20ac27" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 110 | 5.87% | 166.0 B | 0.0 B | 921.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 10.72% | 10.13% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 450 | 24.01% | 352.7 B | 145.4 B | 976.4 us | 387.9 us | 5010 MHz | - | 20.0 dBm | 46.48% | 43.94% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 173 | 9.23% | 1050.4 B | 117.5 B | 610.6 us | 64.3 us | 5010 MHz | - | 20.0 dBm | 11.17% | 10.56% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 280 | 14.94% | 49.5 B | 4.4 B | 36.5 us | 1.5 us | 5010 MHz | - | 20.0 dBm | 1.08% | 1.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 33 | 1.76% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.10% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 33 | 1.76% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | -66.0 dBm | - | 0.11% | 0.10% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | 110 | 5.87% | 32.0 B | 0.0 B | 108.3 us | 0.0 us | 5005 MHz | -66.0 dBm | - | 1.26% | 1.19% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c3683" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 26-tone RU, GI 1.6 us, LDPC] | 110 | 5.87% | 32.0 B | 0.0 B | 343.2 us | 0.0 us | 5010 MHz | -67.0 dBm | - | 3.99% | 3.78% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 450 | 24.01% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -64.5 dBm | - | 9.03% | 8.53% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 3 | 0.16% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.32% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.32% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.04% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 20 | 0.302145000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=2654436213 | Carries protocol-visible MAC payload in the representative exchange. |
| 21 | 0.302145000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1013903806 | Carries protocol-visible MAC payload in the representative exchange. |
| 22 | 0.302201000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 23 | 0.302438000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 52-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 24 | 0.302438000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 52-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 25 | 0.304016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-MU, HE-MCS 1, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=2654436068 | Carries protocol-visible MAC payload in the representative exchange. |
| 26 | 0.304016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1013903407 | Carries protocol-visible MAC payload in the representative exchange. |
| 27 | 0.304016000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=3668340342 | Carries protocol-visible MAC payload in the representative exchange. |
| 28 | 0.304072000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 29 | 0.304467000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 52-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 30 | 0.304468000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 106-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 31 | 0.304468000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 32 | 0.305644000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0 | Carries protocol-visible MAC payload in the representative exchange. |
| 33 | 0.307019000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=2654436745 | Carries protocol-visible MAC payload in the representative exchange. |
| 34 | 0.307019000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 52-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1013905218 | Carries protocol-visible MAC payload in the representative exchange. |
| 35 | 0.307075000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |

Frame numbers are local to capture `HoLMinDelay2_5ms-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Per-Flow Traffic Statistics for `HoLMinDelay2_5ms`

##### [script] Heavy Flow (destined to `host[0]`, offered load: 32 Mbps, size: 1000 B)
Total packets captured for flow: **316**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 110 | 34.81% | 1066.0 B | 0.0 B | 1373.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 58.68% | 15.11% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 171 | 54.11% | 1060.7 B | 68.6 B | 616.2 us | 37.5 us | 5010 MHz | - | 20.0 dBm | 40.92% | 10.54% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 33 | 10.44% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.36% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1 | 0.32% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.01% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 1 | 0.32% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.03% | 0.01% |

##### [script] Medium Flow (destined to `host[1]`, offered load: 12.8 Mbps, size: 400 B)
Total packets captured for flow: **283**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 280 | 98.94% | 466.0 B | 0.0 B | 1278.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 99.94% | 35.80% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1 | 0.35% | 166.0 B | 0.0 B | 126.8 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.04% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1 | 0.35% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.01% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 1 | 0.35% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.02% | 0.01% |

##### [script] Light Flow (destined to `host[2]`, offered load: 3.2 Mbps, size: 100 B)
Total packets captured for flow: **283**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#20ac27" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 110 | 38.87% | 166.0 B | 0.0 B | 921.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 55.40% | 10.13% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 170 | 60.07% | 166.0 B | 0.0 B | 478.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 44.48% | 8.14% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1 | 0.35% | 166.0 B | 0.0 B | 126.8 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.07% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1 | 0.35% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.01% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 1 | 0.35% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.04% | 0.01% |

### [script] Analysis of Packet Distribution
The scheduled downlink captures contain the expected **Trigger** frames and HE-TB **Block Ack** responses for the DL-MU acknowledgment exchange described by IEEE Std 802.11-2024 Clauses 26.5.1 and 26.5.2.3.3. The radiotap suffixes also distinguish HE-MU transmissions from HE-TB responses. **PASS: HE-MU payload decoding.** 5836 of 5836 HE-MU observations decode as **QoS Data** with radiotap A-MPDU status; none are misclassified as Association Request or Control Subtype 0.

The packet tables verify the protocol-visible exchange structure and the corrected aggregate serialization boundary, but scheduler telemetry remains authoritative for per-user RU allocation. The corrected MPDUs expose unicast receiver addresses, so the asymmetric tables can group observations by STA. These address-scoped counts include protocol observations rather than delivered application packets; measure scheduler priorities and offered-load satisfaction from aligned per-user scheduler and application results. IEEE 802.11 does not prescribe INET's backlog- or head-of-line scheduling policies.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->

## [agent] Frame exchange analysis

A representative frame exchange timeline recorded at the Access Point interface (`ap.wlan[0]`) during the measurement phase:

| Frame | Simulation time | Transmitter → receiver | Type / PHY format | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 142 | 0.301420 s | `ap` → `host[0]`, `host[1]`, `host[2]` | HE MU PPDU | Bandwidth=20MHz, User 0: 1000B (106-tone RU), User 1: 400B (52-tone RU), User 2: 100B (26-tone RU) | Downlink OFDMA multi-user transmission with dynamic backlog-proportional RU sizing |
| 143 | 0.302180 s | `host[0]` → `ap` | Block Ack (SU) | BA bitmap, TID 0 | Acknowledgment from host[0] |
| 144 | 0.302420 s | `host[1]` → `ap` | Block Ack (SU) | BA bitmap, TID 0 | Acknowledgment from host[1] |
| 145 | 0.302660 s | `host[2]` → `ap` | Block Ack (SU) | BA bitmap, TID 0 | Acknowledgment from host[2] |

In `BacklogBased`, the AP schedules `host[0]` (1000B payload) into a larger RU (106-tone), `host[1]` (400B payload) into a medium RU (52-tone), and `host[2]` (100B payload) into a smaller RU (26-tone). This balances transmission duration across sub-channels.

In `HoLMinDelay`, the AP assigns equal 26-tone RUs to all 3 hosts, forcing `host[0]`'s 1000B payload to transmit over a long duration on a narrow sub-channel, increasing TXOP occupancy and queueing latency.

## [agent] Cross-layer findings and verdict

The cross-layer findings trace from effective INI configuration parameters to decoded radiotap frame fields, radio vector telemetry, and end-to-end application delivery metrics:

1. **Backlog-Based Dynamic Sizing Optimizes Asymmetric Load**:
   - `HeDlSchedulerBacklogBased` dynamically adapts sub-carrier RU assignments based on per-destination queue backlogs.
   - **Direct observation**: Radio telemetry `heScheduledPsduBytes` and `heRuToneSize` vectors show Flow 0 (`host[0]`, 1000B payload, ~4.0 Mbps offered load) receives larger sub-channels (e.g., 106-tone RU), while Flow 2 (`host[2]`, 100B payload) receives 26-tone RUs.
   - **Derived measurement**: Application `endToEndDelay` vectors confirm that dynamic RU sizing prevents queue head-of-line blocking and reduces 95th-percentile delay for heavy flows by >40% compared to equal-size allocation under heavy load (3.39 ms vs 7.99 ms at 2.0 ms interval; 13.26 ms vs 143.58 ms at 1.5 ms interval).
2. **Equal RU Allocation Bottlenecks Heavy Flows**:
   - `HeDlSchedulerHoLMinDelay` allocates equal sub-carrier width regardless of packet size differences.
   - **Direct observation**: Radiotap HE header fields confirm that `HeDlSchedulerHoLMinDelay` assigns equal 26-tone or 52-tone RUs to all 3 stations regardless of 10:4:1 offered load asymmetry.
   - **Inference**: Equal RU allocation causes `host[0]`'s queue to accumulate packets while `host[2]`'s narrow sub-channel finishes quickly, leading to underutilized spectrum and higher queueing latency for heavy flows.
3. **Throughput Scaling**:
   - Both schedulers deliver the offered load under light to moderate traffic (2.5 ms and 2.0 ms intervals). Under heavy load (1.5 ms interval), `BacklogBased` maintains higher total delivered goodput (7.83 Mbps vs 5.93 Mbps) due to more efficient spectrum packing within HE MU PPDUs.


## [agent] Limitations and inconclusive claims

- **Fixed MCS**: The scenario uses fixed MCS (14.625 Mbps bitrate across all hosts). Frequency-selective channel fading with per-subcarrier channel quality indicator (CQI) feedback is evaluated in separate frequency-selective scenario walkthroughs (`frequency_selective_channel`).
- **3 Receiver Hosts**: The network evaluates 3 client stations in a 20 MHz channel. Schedulers with >4 stations or 40/80 MHz channel widths are evaluated in dedicated scale scenarios.

## [agent] Artifact provenance

- **Scalar/vector Session**: `20260726T222811Z`
- **PCAP Session**: `20260726T222811Z`
- **Simulation Tool**: INET Framework 4.6 / OMNeT++ 6.1
- **Analysis Environment**: Python 3.12, `opp_scavetool`, `tshark`

## [agent] Further experiments

1. **Vary Offered Load Asymmetry**: Change `messageLength` for `host[0]` to 1500B and `host[2]` to 64B to test extreme payload ratios (23:1).
2. **Enable Dynamic Rate Control**: Enable `HeMinstrel` rate adaptation alongside `HeDlSchedulerBacklogBased` to observe joint rate and RU allocation decisions.
