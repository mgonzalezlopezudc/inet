# Walkthrough: 802.11ax Downlink OFDMA

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260726T150000Z`
- PCAP: `20260726T150000Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `20260726T150000Z`.

This walkthrough uses one co-recorded publication session to compare the
single-user EDCA control with the two equal-resource-unit (RU) downlink OFDMA
schedulers. Runs 0–4 provide application and scheduler results; run 0 also
provides packet captures at the access point (AP) and all three stations.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- explain how downlink OFDMA lets an access point (AP) transmit to several
  stations in one high-efficiency multi-user (HE-MU) physical-layer protocol
  data unit (PPDU);
- distinguish the equal-RU `fBW` and `fHoL` policies from the single-user
  Enhanced Distributed Channel Access (EDCA) control;
- identify simultaneous HE-MU QoS Data and the following acknowledgment
  exchange in a capture; and
- reproduce the five-run comparison and run-0 packet inspection.

The AP partitions channel tones into RUs and sends a separate payload to each
selected station in one high-efficiency multi-user (HE-MU) physical-layer
protocol data unit (PPDU). In this model, `fBW` chooses the widest equal-RU
layout that fits the selected users, while `fHoL` tries to serve every
backlogged user using the smallest fitting equal-RU layout. The feature-test
invariant is a run-0 HE-MU transmission with multiple same-time recipients,
followed by the scheduled acknowledgment exchange. The SU control is the
counterfactual.

## [agent] Scenario description

The [network](Lan80211AxDlOfdma.ned) extends the common single-BSS HE network
with three fixed stations. The [configuration](omnetpp.ini) places the AP at
`(250,200)` m and the stations nearby, uses a 5 GHz 20 MHz channel, and sends
three equal 100-byte UDP flows from a wired server at 1 ms intervals. One
warm-up packet per station starts at 0.2 s to establish Block Ack agreements;
the measured flows start at 0.3 s. Each run ends at 1 s and analysis uses
`[0.3,0.88)` s.

```text
server === wired LAN === AP  -- HE downlink -->  host[0]
                               |-------------->  host[1]
                               `-------------->  host[2]
```

The geometry is stationary, close range, and has no external interferer. It
isolates downlink scheduling and queueing; it is not a coverage, mobility, or
coexistence experiment.

## [agent] Standards and INET model boundary

IEEE Std 802.11-2024 Clause 26.5.1.1 permits an HE AP to transmit
simultaneously to one or more non-AP stations using downlink OFDMA, downlink
MU-MIMO, or both; user PSDUs are padded to end together
(`80211ax-2024:chunk:09783`). Clause 26.5.1.2 assigns at least one STA ID per
RU (`80211ax-2024:chunk:09784`). HE-SIG-B carries the RU allocation and
per-user decoding information (Clause 27.3.11.8.2 and Table 27-25,
`80211ax-2024:chunk:10175` and `:10177`).

For the observed acknowledgment sequence, Clause 26.4.5 allows an AP to
solicit multiple Block Ack responses with an MU-BAR Trigger
(`80211ax-2024:chunk:09780`); Clauses 26.5.2.3.3 and 26.5.2.4 define the
trigger-derived HE-TB response and its BlockAck content
(`80211ax-2024:chunk:09802` and `:09805`).

INET's `HeHcf`, destination queues, and `fBW`/`fHoL` policies are model
choices, not IEEE scheduling rules. The model attempts DL MU only with at
least two eligible destinations and active originator Block Ack agreements;
otherwise it falls back to SU service. AP-radio vectors expose STA ID,
scheduled PSDU bytes, and user PPDU duration. Radiotap directly exposes the
observed HE format and RU facts, but not the scheduler objective. The SU
control also changes queue organization, so outcome differences cannot be
attributed to frequency partitioning alone.

## [agent] Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| Both OFDMA treatments record HE-MU transmissions | `PASS` | run-0 AP PCAP, decoded HE PPDU format and RU | Session `20260726T150000Z`, run 0 | Direct packet observation |
| Multiple recipients share one transmission time | `PASS` | `fBW` AP capture, frames 20–21 at 0.300605 s | Same session, run 0 | Two recipients, 106-tone RUs |
| The SU control remains HE-SU | `PASS` | run-0 AP capture has HE-SU QoS Data and no HE-MU QoS Data | Same session, run 0 | Direct control observation |
| Equal-RU outcomes exceed the SU control in this workload | `PASS` | application goodput and pooled p95 delay | Same session, runs 0–4 | Five-run estimate; queue topology confounds attribution |
| Per-user scheduler attribution satisfies an automated acceptance rule | `INCONCLUSIVE` | evidence ledger | Same session, runs 0–4 | Telemetry exists, but the evaluator has no STA-ID correlation criterion |
| Asymmetric per-flow HE-MU attribution | `NOT RUN` | OFDMA report check | Same session, run 0 | No asymmetric Backlog/HoL configuration was selected |

The dashboard computes one observation per run. Goodput sums
`packetReceived:vector(packetBytes)` at the application sinks. Delay is the
nearest-rank p95 after pooling `endToEndDelay:vector` samples within a run.
Reported uncertainty is a two-sided 95% Student-t confidence interval over runs
0–4. The session contains the timestamp-aligned AP-radio vectors
`heStaId`, `heScheduledPsduBytes`, and `heUserPpduDuration`, but the current
evaluator deliberately leaves per-user attribution `INCONCLUSIVE`.

## [agent] Configuration matrix

The configuration facts in this table come from [the INI file](omnetpp.ini).
They describe inputs, not measured outcomes.

| Configuration | Role | Feature gate/delta | Workload/channel | Runs | Expected invariant |
|---|---|---|---|---:|---|
| `SuEdcaBaseline` | Control | replaces `HeHcf` with `Hcf`; one shared 300-packet AC_BE queue | three 100 B/1 ms flows, 20 MHz | 5 | HE-SU QoS Data; no HE-MU QoS Data |
| `EqualSizedRUs_fBW` | Treatment | `HeHcf`, equal-RU scheduler, `fBW`, at most three MU stations | matched | 5 | same-time HE-MU users on wide equal RUs |
| `EqualSizedRUs_fHoL` | Treatment | same scheduler with `schedulingFunction="fHoL"` | matched | 5 | HE-MU service for all backlogged users when feasible |

All three configurations receive `[General]`; neither treatment extends the
other. `SuEdcaBaseline` changes the coordination function and uses one shared
300-packet AC_BE queue, while `HeHcf` uses destination queues. The comparison
matches offered load, channel, aggregate configured buffer capacity, and seed
policy, but does not isolate OFDMA from queue organization.

## [agent] Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| AP schedules at least two users together | aligned AP-radio telemetry and same-time HE-MU frames | one STA ID or only HE-SU | DL scheduler/rate selection | inspect scheduler vectors, then AP PCAP |
| Captured users occupy valid RUs | radiotap HE format/RU fields | unknown or overlapping allocation | transmitter/recorder/typed decoder | typed-HE decode and transmitter logs |
| HE-MU data receives a response | QoS Data → MU-BAR/Block Ack timeline | missing response/retry | acknowledgment policy or reception | receiver PCAP and Block Ack logs |
| Outcome comparison uses matched inputs | session manifest, hashes, result metadata, window | mismatched seed/window/load | campaign/analysis | inspect session JSON and figure sidecars |

## [agent] Reproduction

The checked-in configuration uses a total simulation time of 1 s. The
scalar/vector measurement window remains `[0.3,0.88)` s.

Run from the INET repository root. The shared facade expands this into one
Cmdenv invocation per configuration and run, using release-mode INET and
`seed-set = run number`.

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py run dl_ofdma_sched \
  --evidence both --runs 5 \
  --config SuEdcaBaseline \
  --config EqualSizedRUs_fBW \
  --config EqualSizedRUs_fHoL \
  --jobs 1 \
  --session-id 20260726T150000Z
```

Observed status: exit 0; 15 of 15 simulations succeeded. Results are under
`results/20260726T150000Z/`. Run 0 contains PCAPng captures at the AP and all
stations in the same trajectories as its scalar/vector data.

```sh
MPLCONFIGDIR=/tmp/matplotlib \
python3 examples/ieee80211/analysis/wifi_analysis.py report dl_ofdma_sched \
  --session-id 20260726T150000Z
```

Observed status: exit 0. The asymmetric-only per-flow check is explicitly
`NOT RUN` for this filtered core matrix; it is not converted into a feature
`PASS`.

## [agent] Scalar and vector analysis

The plot and table below are bound to the fresh session. Goodput is the sum of
received application bytes divided by the 0.58 s window. Delay is the pooled
within-run nearest-rank p95; Jain fairness is computed from each station's
window goodput. Each reported mean and confidence interval uses five
independent runs, never vector samples as repetitions.

Both OFDMA treatments delivered about 2.4 Mbit/s, compared with 1.744 Mbit/s
for the SU control. Their pooled p95 delays were also lower. These are bounded
outcome observations for this topology and load, not proof that OFDMA is
universally faster or that frequency partitioning alone caused the difference.

Inspect the exact result names and modules with:

```sh
opp_scavetool query -l \
  -f 'name =~ "packetReceived:vector(packetBytes)" OR name =~ "endToEndDelay:vector" OR name =~ "heStaId:vector" OR name =~ "heScheduledPsduBytes:vector" OR name =~ "heUserPpduDuration:vector"' \
  examples/ieee80211ax/dl_ofdma_sched/results/20260726T150000Z/*/*.sca \
  examples/ieee80211ax/dl_ofdma_sched/results/20260726T150000Z/*/*.vec
```

The evidence ledger leaves both its STA-ID correlation rule and its
per-user-delivery acceptance rule `INCONCLUSIVE`: the underlying records are
present, but the manifest defines no executable acceptance criteria for them.

The aligned AP telemetry supplies an additional mechanism view:

| Treatment | AP DL-MU PPDUs/run | Users/PPDU | Three-user fraction | Scheduled PSDU/user |
|---|---:|---:|---:|---:|
| `fBW` | 572.6 ± 4.7 | 2.000 ± 0.000 | 0% | 170 B |
| `fHoL` | 580.0 ± 0.0 | 2.448 ± 0.050 | 44.79% ± 4.98% | 170 B |

Entries were grouped by equal timestamp within each run before run-level
summarization. This directly records how the model scheduled users, but the
present ledger does not join each STA ID to application delivery.

<!-- BEGIN GENERATED: ieee80211-scalar-vector-dl -->
### [script] Generated scalar/vector plot and table

![dl scalar/vector analysis](results/20260726T150000Z/dl-scheduler-dashboard.png)

Figure provenance: [`results/20260726T150000Z/dl-scheduler-dashboard.png.json`](results/20260726T150000Z/dl-scheduler-dashboard.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes) / unit=B<br>vector / **.app[*] / endToEndDelay:vector / unit=s<br>vector / **.ap.wlan[0].radio / heStaId:vector<br>vector / **.ap.wlan[0].radio / heScheduledPsduBytes:vector / unit=B<br>vector / **.ap.wlan[0].radio / heUserPpduDuration:vector / unit=s
- Window / per-run aggregation / exclusions: [0.3, 0.88) s; delay=pooled packet p95 within run; observation=one per run; per_user=STA-ID aligned scheduled PSDU bytes and PPDU duration; uncertainty=95% Student-t CI
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| EDCA / delay p95 ms | 140.695 | 0.833888 |
| EDCA / goodput mbps | 1.74428 | 0.00549648 |
| EDCA / jain fairness | 0.996821 | 0.000505725 |
| fBW / delay p95 ms | 6.9538 | 4.41329 |
| fBW / goodput mbps | 2.38759 | 0.00938052 |
| fBW / jain fairness | 0.99993 | 6.66467e-05 |
| fHoL / delay p95 ms | 0.758235 | 0.0317215 |
| fHoL / goodput mbps | 2.4 | 0 |
| fHoL / jain fairness | 1 | 0 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.
<!-- END GENERATED: ieee80211-scalar-vector-dl -->

## [agent] PCAP statistics

Capture point: `Lan80211AxDlOfdma.ap.wlan[0]`, with per-host captures retained.
Capture session: `results/20260726T150000Z`, run 0.
Decode scope: PCAPng/radiotap capture observations, TShark and Capinfos 4.6.4.
HE fields are reported only when radiotap marks them known.

```sh
tshark -n -r \
  'examples/ieee80211ax/dl_ofdma_sched/results/20260726T150000Z/EqualSizedRUs_fBW/EqualSizedRUs_fBW-#0Lan80211AxDlOfdma.ap.wlan[0].pcap' \
  -Y 'radiotap.he.data_1.ppdu_format == 2 && wlan.fc.type == 2 && wlan.fc.subtype == 8' \
  -T fields -e frame.number
```

| Configuration | AP observations | HE-SU | HE-MU QoS Data | Trigger | HE-TB Block Ack | Authoritative RU facts |
|---|---:|---:|---:|---:|---:|---|
| `SuEdcaBaseline` | 1,790 | 1,515 | 0 | 0 | 0 | no HE-MU RU |
| `EqualSizedRUs_fBW` | 4,414 | 708 | 1,378 | 689 | 1,378 | 1,378 × 106-tone |
| `EqualSizedRUs_fHoL` | 4,668 | 419 | 1,684 | 700 | 1,684 | 852 × 52-tone; 832 × 106-tone |

Rows count AP-interface observations, not delivered packets or de-duplicated
medium transmissions. Host captures are additional observation points. The
generated per-flow check reports `NOT RUN` because no asymmetric Backlog/HoL
configuration was selected. It therefore makes no claim about per-flow
attribution for this core matrix.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260726T150000Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260726T150000Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260726T150000Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260726T150000Z.json` (SHA-256 `2009a5e8dad2cbb1258ae6cd90ac8c68c6007dc92acd6af3d87fa065913d6e8c`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `EqualSizedRUs_fBW` | `none (all decoded frames)` | 4414 | Data: QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (1378), Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] (1378), Control: Trigger (689) | 60.54% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `EqualSizedRUs_fHoL` | `none (all decoded frames)` | 4668 | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] (852), Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] (852), Data: QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (832) | 94.69% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `SuEdcaBaseline` | `none (all decoded frames)` | 1790 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (1487), Control: Block Ack Request (BAR) (130), Control: Block Ack (BA) (130) | 20.06% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | EqualSizedRUs_fBW produced protocol-visible wireless observations | 4414 AP/global transmission observations |
| **PASS** | EqualSizedRUs_fHoL produced protocol-visible wireless observations | 4668 AP/global transmission observations |
| **PASS** | SuEdcaBaseline produced protocol-visible wireless observations | 1790 AP/global transmission observations |
| **PASS** | HE-MU payload observations decode as QoS Data with A-MPDU status | 3062 of 3062 HE-MU observations |
| **NOT RUN** | HE-MU recipient addresses support per-flow PCAP grouping | No asymmetric backlog/HoL configuration was selected |

### [script] Configuration: `EqualSizedRUs_fBW`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **4414**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 1378 | 31.22% | 166.0 B | 0.0 B | 244.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 55.61% | 33.67% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 134 | 3.04% | 166.0 B | 0.0 B | 108.8 us | 18.0 us | 5010 MHz | - | 20.0 dBm | 2.41% | 1.46% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 574 | 13.00% | 166.0 B | 0.0 B | 126.8 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 12.02% | 7.28% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 689 | 15.61% | 46.0 B | 0.0 B | 35.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 4.02% | 2.43% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 123 | 2.79% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.57% | 0.34% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 123 | 2.79% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | -66.6 dBm | - | 0.62% | 0.38% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | 1378 | 31.22% | 32.0 B | 0.0 B | 108.3 us | 0.0 us | 5005 MHz, 5015 MHz | -64.7 dBm | - | 24.65% | 14.92% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 3 | 0.07% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.14% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.14% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.07% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 20 | 0.300605000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=2654435975 | Carries protocol-visible MAC payload in the representative exchange. |
| 21 | 0.300605000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1013903436 | Carries protocol-visible MAC payload in the representative exchange. |
| 22 | 0.300661000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 23 | 0.300811000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 106-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 24 | 0.300812000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 106-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 25 | 0.301148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0 | Carries protocol-visible MAC payload in the representative exchange. |
| 26 | 0.301488000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=2654436787 | Carries protocol-visible MAC payload in the representative exchange. |
| 27 | 0.301488000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1013905272 | Carries protocol-visible MAC payload in the representative exchange. |
| 28 | 0.301544000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 29 | 0.301694000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 106-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 30 | 0.301695000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 106-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 31 | 0.302148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0 | Carries protocol-visible MAC payload in the representative exchange. |
| 32 | 0.302479000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=2654436719 | Carries protocol-visible MAC payload in the representative exchange. |
| 33 | 0.302479000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1013905316 | Carries protocol-visible MAC payload in the representative exchange. |
| 34 | 0.302535000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 35 | 0.302685000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 106-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |

Frame numbers are local to capture `EqualSizedRUs_fBW-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `EqualSizedRUs_fHoL`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **4668**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bc021" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 832 | 17.82% | 166.0 B | 0.0 B | 244.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 21.47% | 20.33% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#16c022" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 852 | 18.25% | 166.0 B | 0.0 B | 478.7 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 43.07% | 40.78% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 419 | 8.98% | 166.0 B | 0.0 B | 126.8 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 5.61% | 5.31% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 700 | 15.00% | 49.7 B | 4.4 B | 36.6 us | 1.5 us | 5010 MHz | - | 20.0 dBm | 2.70% | 2.56% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 83 | 1.78% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.25% | 0.23% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 83 | 1.78% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | -66.0 dBm | - | 0.27% | 0.25% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | 832 | 17.82% | 32.0 B | 0.0 B | 108.3 us | 0.0 us | 5005 MHz, 5015 MHz | -65.0 dBm | - | 9.51% | 9.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 852 | 18.25% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -65.3 dBm | - | 17.06% | 16.15% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 3 | 0.06% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.13% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.13% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.04% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 20 | 0.300605000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=2654435975 | Carries protocol-visible MAC payload in the representative exchange. |
| 21 | 0.300605000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1013903436 | Carries protocol-visible MAC payload in the representative exchange. |
| 22 | 0.300661000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 23 | 0.300811000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 106-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 24 | 0.300812000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 106-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 25 | 0.301148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0 | Carries protocol-visible MAC payload in the representative exchange. |
| 26 | 0.301488000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=2654436787 | Carries protocol-visible MAC payload in the representative exchange. |
| 27 | 0.301488000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1013905272 | Carries protocol-visible MAC payload in the representative exchange. |
| 28 | 0.301544000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 29 | 0.301694000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 106-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 30 | 0.301695000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 106-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 31 | 0.302148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0 | Carries protocol-visible MAC payload in the representative exchange. |
| 32 | 0.302479000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=2654436719 | Carries protocol-visible MAC payload in the representative exchange. |
| 33 | 0.302479000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1013905316 | Carries protocol-visible MAC payload in the representative exchange. |
| 34 | 0.302535000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 35 | 0.302685000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) / HE-TB, HE-MCS 0, 106-tone RU, NSS 1, GI 1.6 us, LDPC | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |

Frame numbers are local to capture `EqualSizedRUs_fHoL-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `SuEdcaBaseline`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1790**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 28 | 1.56% | 166.0 B | 0.0 B | 108.8 us | 18.0 us | 5010 MHz | - | 20.0 dBm | 1.52% | 0.30% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1487 | 83.07% | 166.9 B | 12.1 B | 127.3 us | 6.6 us | 5010 MHz | - | 20.0 dBm | 94.36% | 18.93% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 130 | 7.26% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 1.81% | 0.36% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 130 | 7.26% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | -65.2 dBm | - | 1.99% | 0.40% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 3 | 0.17% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | - | 0.04% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.34% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.07% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.34% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.21% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.200148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 | Carries protocol-visible MAC payload in the representative exchange. |
| 2 | 0.200196000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 4 | 0.200338000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 6 | 0.200461000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 7 | 0.200715000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 | Carries protocol-visible MAC payload in the representative exchange. |
| 8 | 0.200763000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 10 | 0.201073000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 12 | 0.201214000 | ? → 0a:aa:00:00:00:02 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 13 | 0.201405000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 | Carries protocol-visible MAC payload in the representative exchange. |
| 14 | 0.201454000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 16 | 0.201587000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 18 | 0.201701000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 19 | 0.300148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0 | Carries protocol-visible MAC payload in the representative exchange. |
| 20 | 0.300393000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0 | Carries protocol-visible MAC payload in the representative exchange. |
| 21 | 0.300843000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0 | Carries protocol-visible MAC payload in the representative exchange. |
| 22 | 0.301148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0 | Carries protocol-visible MAC payload in the representative exchange. |

Frame numbers are local to capture `SuEdcaBaseline-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Analysis of Packet Distribution
The scheduled downlink captures contain the expected **Trigger** frames and HE-TB **Block Ack** responses for the DL-MU acknowledgment exchange described by IEEE Std 802.11-2024 Clauses 26.5.1 and 26.5.2.3.3. The radiotap suffixes also distinguish HE-MU transmissions from HE-TB responses. **PASS: HE-MU payload decoding.** 3062 of 3062 HE-MU observations decode as **QoS Data** with radiotap A-MPDU status; none are misclassified as Association Request or Control Subtype 0.

The packet tables verify the protocol-visible exchange structure and the corrected aggregate serialization boundary, but scheduler telemetry remains authoritative for per-user RU allocation. The corrected MPDUs expose unicast receiver addresses, so the asymmetric tables can group observations by STA. These address-scoped counts include protocol observations rather than delivered application packets; measure scheduler priorities and offered-load satisfaction from aligned per-user scheduler and application results. IEEE 802.11 does not prescribe INET's backlog- or head-of-line scheduling policies.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->

## [agent] Frame exchange analysis

```sh
tshark -n -r \
  'examples/ieee80211ax/dl_ofdma_sched/results/20260726T150000Z/EqualSizedRUs_fBW/EqualSizedRUs_fBW-#0Lan80211AxDlOfdma.ap.wlan[0].pcap' \
  -Y 'frame.number >= 19 && frame.number <= 24' \
  -T fields -E header=y -E separator='|' -E occurrence=a \
  -e frame.number -e frame.time_epoch -e wlan.fc.type_subtype \
  -e wlan.ta -e wlan.ra -e radiotap.he.data_1.ppdu_format \
  -e radiotap.he.data_5.data_bw_ru_allocation -e wlan.qos.tid \
  -e _ws.col.Info
```

| Frame | Simulation time | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 19 | 0.300148 s | AP → STA 1 | QoS Data, HE-SU | format 0, full 20 MHz, TID 0 | preceding SU service |
| 20 | 0.300605 s | AP → STA 2 | QoS Data, HE-MU | format 2, 106-tone RU, TID 0 | simultaneous user 1 |
| 21 | 0.300605 s | AP → STA 3 | QoS Data, HE-MU | format 2, 106-tone RU, TID 0 | simultaneous user 2 |
| 22 | 0.300661 s | AP → broadcast | Trigger, HE MU-BAR | AIDs 2/3; RU allocations 53/54 | solicits two responses |
| 23 | 0.300811 s | STA 2 → AP | HE-TB Block Ack | format 3, 106-tone RU | response 1 |
| 24 | 0.300812 s | STA 3 → AP | HE-TB Block Ack | format 3, 106-tone RU | response 2 |

This is direct packet evidence of a two-user DL-MU exchange and its triggered
HE-TB Block Ack responses. Scheduler vectors and captures belong to the same
run-0 trajectory, but the current evaluator does not correlate this exact PPDU
with its same-timestamp STA-ID/bytes/duration records; that final attribution
remains an inference.

## [agent] Cross-layer findings and verdict

| Claim | Verdict | Configuration evidence | Model telemetry | Packet evidence | Outcome evidence |
|---|---|---|---|---|---|
| Equal-RU DL OFDMA is exercised | `PASS` | `HeHcf` and equal-RU scheduler requested | aligned AP vectors | HE-MU data, MU-BAR, HE-TB BA | matched outcome session |
| SU counterfactual remains single-user | `PASS` | `Hcf` control | no HE scheduler vectors | 1,515 HE-SU and 0 HE-MU QoS observations | 1.744 Mbit/s, 140.695 ms p95 |
| Equal-RU treatments exceed SU here | `PASS` | matched load/channel; different queue topology | 2–3 scheduled users/PPDU | HE-MU absent from control | 2.388/2.400 Mbit/s and lower p95 |
| `fHoL`'s three-user scheduling caused its exact outcome | `INCONCLUSIVE` | policy requested | 44.79% three-user PPDUs | mixed 52/106-tone observations | association is consistent, not an accepted causal join |

The co-recorded evidence directly establishes the configured feature, a
representative exchange, model scheduling activity, and bounded application
outcomes. It does not prove a universal scheduler ranking or isolate frequency
partitioning from the queue-organization confounder.

## [agent] Limitations and inconclusive claims

- The automated ledger has no executable STA-ID-to-same-PPDU correlation or
  per-user delivery/delay acceptance criterion.
- The SU/OFDMA control changes queue organization.
- Pooled p95 is not a per-flow percentile.
- Run-0 packet counts are representative mechanism evidence; five runs support
  the outcome estimates.
- Per-flow HE-MU grouping for asymmetric Backlog/HoL workloads was not run in
  this core comparison.

## [agent] Further experiments

- Match per-destination queues in an SU control.
- Add an executable same-timestamp join between PCAP PPDU identity and the
  STA-ID/scheduled-bytes/duration vectors.
- Sweep offered load until `fBW` changes its selected-user count, then check
  whether the RU distribution and delay change together.

## [agent] Implementation plan

No production implementation work is proposed. The shared reporter now marks
its asymmetric-only per-flow check `NOT RUN` when that matrix is not selected.
The remaining `INCONCLUSIVE` ledger items require analysis acceptance criteria,
not a demonstrated `src/inet` behavior change.

## [agent] Artifact provenance

| Artifact family | Session/path | Configurations/runs | Tool/filter/window | Integrity notes |
|---|---|---|---|---|
| Scalar/vector | `results/20260726T150000Z` | three configs, runs 0–4 | `[0.3,0.88)` s; one observation/run; 95% t CI | hashes in evidence ledger and figure JSON |
| PCAP | `results/20260726T150000Z` | same configs, run 0 | TShark/Capinfos 4.6.4; AP and three STA points | 12 nonempty, decodable PCAPng files; capture-manifest hashes |
| Session | `../../ieee80211/analysis/generated/sessions/20260726T150000Z/session.json` | both evidence families | publication classification; runs `[0,5)` | common trajectory and configuration binding |
| Figures | `results/20260726T150000Z/*.png` | fresh session | deterministic renderer plus JSON sidecars | presentation views, not independent evidence |
