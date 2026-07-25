# Walkthrough: 802.11ax Target Wake Time

This walkthrough validates Target Wake Time (TWT) with two complementary
evidence sets: a five-seed energy/delivery comparison and a separate run-0
packet session covering individual announced, individual unannounced,
broadcast, and no-TWT modes.

## Learning objectives and feature primer

After completing this walkthrough, the reader can:

- explain how an access point (AP) and station agree on periodic TWT service
  periods so the station can sleep between them;
- distinguish individual announced, individual unannounced, and broadcast TWT;
- identify radio-mode/power evidence and the announced-mode PS-Poll sequence;
  and
- reproduce the delivery-gated energy calculation and packet timeline.

A TWT agreement defines when a station is expected to be awake. An announced
station signals its presence before the AP sends buffered traffic; this model
uses Power-Save Poll (PS-Poll). An unannounced station may receive service
without that announcement. The outcome invariant is not simply “lower power”:
the TWT treatment must retain at least 95% of its paired baseline delivery.

## Scenario description

The [network](TwtRegression.ned) and [configuration](omnetpp.ini) contain one
fixed AP, two fixed wireless stations, and a wired server. Stations send
200-byte UDP payloads every 2.011 s, offset by 5 ms. Traffic starts near 10 s,
after setup, and stops at 90 s; analysis uses `[10,100)` s so queued traffic
can drain. There is no OMNeT++ warm-up period; the explicit analysis window
excludes setup. Nodes are stationary on a strong 5 GHz, 20 MHz link with no
external interference.

```text
sta[0] --\
          +--- 5 GHz BSS --- AP === 100 Gbit/s === server
sta[1] --/
```

The 100 ms wake interval and 10 ms service period deliberately expose repeated
wake/sleep behavior. Energy configurations use identical application inputs,
seeds, and state-based radio power levels.

## Standards and INET model boundary

IEEE Std 802.11-2024 Clause 26.8.1 says TWT concentrates exchanges into
predefined service periods to reduce contention and required awake time
(`80211ax-2024:chunk:09855`). Individual agreement setup is specified through
Clause 10.46 and the Clause 26.8 procedures; broadcast and individual parameter
sets are distinct (`80211ax-2024:chunk:03387`, Table 9-346).

INET models agreements with `Ieee80211TwtManager` and example management/agent
modules, and power with `StateBasedEpEnergyConsumer`. The integrated power
vectors directly expose modeled energy; they do not measure physical hardware.
PS-Poll is direct frame evidence for this announced-mode implementation, but
the capture does not prove all standard negotiation fields or real-device
interoperability.

## Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| TWT preserves the measured workload | `PASS` | delivered-byte vectors and paired ratio | energy session `20260725T120411Z`, runs/seeds 0–4 | ratio 1.0, above 0.95 gate |
| TWT reduces modeled energy per delivered bit | `PASS` | time-integrated power vectors | same five paired runs | 44.89% reduction |
| Announced mode signals wake presence | `PASS` | PS-Poll frames in AP capture | packet session `20260724T175025Z`, run 0 | 1,992 observations |
| Other modes omit PS-Poll in retained captures | `PASS` | generated subtype tables | run 0 | absence is bounded to these captures |
| Frame-level wake event caused a power-vector transition | `INCONCLUSIVE` | sessions are separate | — | no co-recorded packet/power correlation |

## Configuration matrix

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| `BaselineEnergy` | Control | TWT disabled | matched two-STA uplink, 20 MHz | 0–4 | delivery reference; no TWT sleep |
| `TwtEnergySaving` | Treatment | individual unannounced, 100/10 ms | matched | 0–4 | ≥95% paired delivery and lower J/bit |
| `Baseline` | Packet control | TWT disabled | matched packet session | run 0 | no PS-Poll TWT sequence |
| `IndividualUnannounced` | Treatment | individual, unannounced | matched | run 0 | no announced-presence PS-Poll |
| `IndividualAnnounced` | Treatment | same agreement, `announced=true` | matched | run 0 | PS-Poll at service periods |
| `Broadcast` | Treatment | shared broadcast ID 1 schedule | matched | run 0 | broadcast schedule without individual-announced PS-Poll |

`IndividualAnnounced extends = IndividualUnannounced`, so the causal input
delta is `agent.announced=true`. `TwtEnergySaving extends =
IndividualUnannounced, EnergyBase`, matching the baseline energy model.

## Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| TWT delivery ratio ≥0.95 | server byte vectors, paired by run | lower paired ratio | TWT schedule/queue/application | align release, service period, and receive vectors |
| TWT lowers integrated J/bit | station power vectors `[10,100)` | no reduction or missing sleep levels | radio mode/energy consumer | inspect `radioMode:vector` transitions |
| Announced STA signals presence | AP PS-Poll frames | missing/wrong-period PS-Poll | TWT agent/MAC | AP+STA PCAP and TWT logs |
| Data follows presence signal | PS-Poll → QoS Data → Ack timeline | missing data/Ack or retry | buffering/reception/ack policy | correlate both capture points |

## Reproduction

Run from the INET repository root. This command was **not executed during this
rewrite**; status: `NOT RUN`.

```sh
bin/inet -u Cmdenv -f examples/ieee80211ax/twt/omnetpp.ini \
  -c TwtEnergySaving -r 0 \
  --result-dir=/tmp/inet-twt-energy-r0
```

Historical scalar/vector and packet sessions completed and retained their
artifacts; no new exit status is claimed.

## Scalar and vector analysis

Inputs are `.sca`/`.vec` pairs under
`results/scalar-vector/20260725T120411Z/{BaselineEnergy,TwtEnergySaving}`.

```sh
opp_scavetool query -l \
  -f 'name =~ "radioMode:vector" OR name =~ "powerConsumption:vector" OR name =~ "packetReceived:vector(packetBytes)"' \
  examples/ieee80211ax/twt/results/scalar-vector/20260725T120411Z/BaselineEnergy/*.{sca,vec} \
  examples/ieee80211ax/twt/results/scalar-vector/20260725T120411Z/TwtEnergySaving/*.{sca,vec}
```

Each station's piecewise-constant `powerConsumption:vector` is integrated over
`[10,100)` s, station integrals are summed per run, and the sum is divided by
server-delivered bits. Statistics are computed per run before the five-run
mean and two-sided 95% Student-t interval.

| Configuration | Energy per delivered bit | Delivered bytes | Goodput |
|---|---:|---:|---:|
| `BaselineEnergy` | `2.826326e-6 J/bit` (CI effectively 0) | `16,000 B` in every run | `0.001422222 Mbit/s` |
| `TwtEnergySaving` | `1.557605e-6 ± 4.045265e-11 J/bit` | `16,000 B` in every run | `0.001422222 Mbit/s` |

The derived reduction is 44.89%, with paired delivery ratio 1.0. This is a
five-run model result, not a hardware-energy claim.

## PCAP statistics

Capture points: AP and both station WLAN interfaces.
Session: `results/packet-statistics/20260724T175025Z`.
Format/decode: retained PCAP captures produced through the PCAPng campaign
path, computed checksum/FCS, TShark 4.6.4. Generated rows count captured MPDU
transmission observations.

```sh
tshark -n -r \
  'examples/ieee80211ax/twt/results/packet-statistics/20260724T175025Z/IndividualAnnounced/IndividualAnnounced-#0TwtRegression.ap.wlan[0].pcap' \
  -q -z io,stat,0,'wlan.fc.type_subtype == 0x1a','wlan.fc.retry == 1'
```

| Configuration | Data identities / retries | PS-Poll observations | Interpretation |
|---|---:|---:|---|
| `Baseline` | 80 / 0 | 0 | no-TWT control |
| `IndividualUnannounced` | 80 / 0 | 0 | no announcement frame |
| `IndividualAnnounced` | 80 / 0 | 1,992 | direct announced-presence behavior |
| `Broadcast` | 80 / 0 | 0 | shared schedule, not individual announced mode |

## Frame exchange analysis

```sh
tshark -n -r \
  'examples/ieee80211ax/twt/results/packet-statistics/20260724T175025Z/IndividualAnnounced/IndividualAnnounced-#0TwtRegression.ap.wlan[0].pcap' \
  -Y 'frame.number >= 331 && frame.number <= 345' \
  -T fields -E header=y -E separator='|' \
  -e frame.number -e frame.time_epoch -e wlan.fc.type_subtype \
  -e wlan.ta -e wlan.ra -e wlan.fc.retry -e _ws.col.Info
```

| Frame | Simulation time | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 331 | 10.031472 s | AP → broadcast | Beacon | management | precedes STA 0 service |
| 332 | 10.031761 s | STA 0 → AP | PS-Poll | retry false | announced wake presence |
| 333 | 10.031925 s | STA 0 → AP | QoS Data | 200-byte UDP payload | queued traffic release |
| 334 | 10.031969 s | AP → STA 0 | Ack | retry false | successful MAC exchange |
| 339 | 10.061135 s | STA 1 → AP | PS-Poll | retry false | second STA presence |
| 340 | 10.061299 s | STA 1 → AP | QoS Data | 200-byte UDP payload | queued traffic release |
| 341 | 10.061343 s | AP → STA 1 | Ack | retry false | successful MAC exchange |

This is direct packet ordering in the announced run. The energy outcome comes
from a different session, so packet-to-power causality remains inference.

## Cross-layer findings and verdict

| Claim | Verdict | Configuration evidence | Model telemetry | Packet evidence | Outcome evidence |
|---|---|---|---|---|---|
| TWT saves modeled energy without dropping work | `PASS` | matched energy pair | mode and power vectors | adjacent packet session | equal bytes, 44.89% lower J/bit |
| Announced TWT presence signaling occurs | `PASS` | only announced flag differs | no packet-session state vector cited | PS-Poll/data/Ack timeline | 80 identities, no retries |
| One captured wake caused one energy transition | `INCONCLUSIVE` | matched schedules | separate vector session | separate capture session | no event correlation |

The retained evidence directly proves the energy/delivery invariant and the
announced packet sequence in their respective scopes. Their connection is a
standards/model-consistent inference, not a co-recorded causal chain.

## Limitations and inconclusive claims

- Packet and energy sessions are separate.
- Radiotap/802.11 captures do not directly expose radio sleep power.
- Only five seeds and one strong-link topology are covered.
- Negotiation-field completeness and real-device fidelity are not established.
- One co-recorded announced and unannounced run with power, mode, AP/STA PCAP,
  and delivery would resolve the main causal gap.

## Further experiments

- Sweep 1, 5, 10, and 50 ms service periods; predict a monotonic awake-energy
  trend until queue delay/loss becomes binding.
- Compare announced and unannounced with identical energy instrumentation.
- Add a marginal-link condition and require the 95% delivery gate before
  accepting any energy reduction.

## Implementation plan

| Item | Evidence-backed plan |
|---|---|
| Demonstrated gap | packet wake sequence and power transitions are not co-recorded |
| Intended behavior | directly correlate service-period presence, radio mode, energy, and delivery |
| Smallest change surface | campaign/feature-plugin observation envelope; no production edit justified |
| Observability | retain TWT state, radio mode/power, AP/STA PCAP, and sink vectors together |
| Validation | announced/unannounced/control, run 0 first then five seeds |
| Compatibility and risks | instrumentation can alter event trajectory; compare within session |
| Architecture and sealing | required before any future `src/inet` edit |
| Next handoff | result/packet analyst for co-recording |

## Artifact provenance

| Artifact family | Session/path | Configurations/runs | Tool/filter/window | Integrity notes |
|---|---|---|---|---|
| Scalar/vector | `results/scalar-vector/20260725T120411Z` | energy pair, runs 0–4 | `[10,100)` s; figure provenance | SHA-256s retained in JSON |
| PCAP | `results/packet-statistics/20260724T175025Z` | four packet configs, run 0 | TShark 4.6.4, AP/STA points | generated block preserved |
| Figure | `../analysis/figures/twt/twt-state-and-energy.png` | energy pair | paired delivery gate 0.95 | metrics in `../analysis/metrics.json` |

<!-- REWRITE-PREFIX-END -->


<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
## 802.11 Packet Type Statistics
![802.11 Packet Type Statistics](packet_statistics.png)

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260724T175025Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %**: The sum of this frame type's estimated airtimes divided by the simulation time limit. Concurrent transmissions from multiple capture points are counted separately, so this value can exceed 100%; it is not the union of busy channel time.

### Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | Baseline produced protocol-visible wireless observations | 1148 AP/global transmission observations |
| **PASS** | Broadcast produced protocol-visible wireless observations | 1156 AP/global transmission observations |
| **PASS** | IndividualAnnounced produced protocol-visible wireless observations | 3148 AP/global transmission observations |
| **PASS** | IndividualUnannounced produced protocol-visible wireless observations | 1156 AP/global transmission observations |

### Configuration: `Baseline`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1148**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | Data: QoS Data | 80 | 6.97% | 270.0 B | 0.0 B | 110.0 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 5.90% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 14 | 1.22% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 0.26% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 14 | 1.22% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.29% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 20 | 1.74% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -69.0 dBm | 13.0 dBm | 0.33% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#9b0821" /></svg> | Management: Beacon | 1000 | 87.11% | 88.0 B | 0.0 B | 137.3 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 92.04% | 0.14% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f2805a" /></svg> | Management: Probe Request | 2 | 0.17% | 63.0 B | 0.0 B | 104.0 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 0.14% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5a489" /></svg> | Management: Probe Response | 2 | 0.17% | 88.0 B | 0.0 B | 137.3 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.18% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e830c9" /></svg> | Management: Association Request | 2 | 0.17% | 71.0 B | 0.0 B | 114.7 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 0.15% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#cc4ce6" /></svg> | Management: Association Response | 2 | 0.17% | 76.0 B | 0.0 B | 121.3 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.16% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f25aa6" /></svg> | Management: Authentication | 8 | 0.70% | 34.0 B | 0.0 B | 65.3 us | 0.0 us | 5010 MHz | -69.0 dBm | 13.0 dBm | 0.35% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 4 | 0.35% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -69.0 dBm | 13.0 dBm | 0.19% | 0.00% |

#### MPDU observation semantics

| Metric | Value |
|---|---:|
| Total data MPDU transmission observations | 80 |
| Unique `(TA, TID, sequence, fragment)` identities | 80 |
| Repeated identity observations | 0 |
| Observations with Retry bit set | 0 |
| Unique A-MPDU references | 0 |

Repeated observations are retained in airtime totals because every transmission consumes channel time; the unique count is provided only for workload/reliability interpretation.

### Configuration: `Broadcast`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1156**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | Data: QoS Data | 80 | 6.92% | 270.0 B | 0.0 B | 110.0 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 5.07% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 14 | 1.21% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 0.23% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 14 | 1.21% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.25% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 24 | 2.08% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -69.0 dBm | 13.0 dBm | 0.34% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#9b0821" /></svg> | Management: Beacon | 1000 | 86.51% | 106.0 B | 0.0 B | 161.3 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 92.91% | 0.16% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f2805a" /></svg> | Management: Probe Request | 2 | 0.17% | 63.0 B | 0.0 B | 104.0 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 0.12% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5a489" /></svg> | Management: Probe Response | 2 | 0.17% | 88.0 B | 0.0 B | 137.3 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.16% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e830c9" /></svg> | Management: Association Request | 2 | 0.17% | 71.0 B | 0.0 B | 114.7 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 0.13% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#cc4ce6" /></svg> | Management: Association Response | 2 | 0.17% | 76.0 B | 0.0 B | 121.3 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.14% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f25aa6" /></svg> | Management: Authentication | 8 | 0.69% | 34.0 B | 0.0 B | 65.3 us | 0.0 us | 5010 MHz | -69.0 dBm | 13.0 dBm | 0.30% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 8 | 0.69% | 42.5 B | 5.5 B | 76.7 us | 7.3 us | 5010 MHz | -69.0 dBm | 13.0 dBm | 0.35% | 0.00% |

#### MPDU observation semantics

| Metric | Value |
|---|---:|
| Total data MPDU transmission observations | 80 |
| Unique `(TA, TID, sequence, fragment)` identities | 80 |
| Repeated identity observations | 0 |
| Observations with Retry bit set | 0 |
| Unique A-MPDU references | 0 |

Repeated observations are retained in airtime totals because every transmission consumes channel time; the unique count is provided only for workload/reliability interpretation.

### Configuration: `IndividualAnnounced`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **3148**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | Data: QoS Data | 80 | 2.54% | 270.0 B | 0.0 B | 110.0 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 4.34% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#63c7e9" /></svg> | Control: PS-Poll | 1992 | 63.28% | 20.0 B | 0.0 B | 26.7 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 26.20% | 0.05% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 14 | 0.44% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 0.19% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 14 | 0.44% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.21% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 24 | 0.76% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -69.0 dBm | 13.0 dBm | 0.29% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#9b0821" /></svg> | Management: Beacon | 1000 | 31.77% | 88.0 B | 0.0 B | 137.3 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 67.73% | 0.14% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f2805a" /></svg> | Management: Probe Request | 2 | 0.06% | 63.0 B | 0.0 B | 104.0 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 0.10% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5a489" /></svg> | Management: Probe Response | 2 | 0.06% | 88.0 B | 0.0 B | 137.3 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.14% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e830c9" /></svg> | Management: Association Request | 2 | 0.06% | 71.0 B | 0.0 B | 114.7 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 0.11% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#cc4ce6" /></svg> | Management: Association Response | 2 | 0.06% | 76.0 B | 0.0 B | 121.3 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.12% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f25aa6" /></svg> | Management: Authentication | 8 | 0.25% | 34.0 B | 0.0 B | 65.3 us | 0.0 us | 5010 MHz | -69.0 dBm | 13.0 dBm | 0.26% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 8 | 0.25% | 42.5 B | 5.5 B | 76.7 us | 7.3 us | 5010 MHz | -69.0 dBm | 13.0 dBm | 0.30% | 0.00% |

#### MPDU observation semantics

| Metric | Value |
|---|---:|
| Total data MPDU transmission observations | 80 |
| Unique `(TA, TID, sequence, fragment)` identities | 80 |
| Repeated identity observations | 0 |
| Observations with Retry bit set | 0 |
| Unique A-MPDU references | 0 |

Repeated observations are retained in airtime totals because every transmission consumes channel time; the unique count is provided only for workload/reliability interpretation.

### Configuration: `IndividualUnannounced`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1156**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | Data: QoS Data | 80 | 6.92% | 270.0 B | 0.0 B | 110.0 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 5.88% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 14 | 1.21% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 0.26% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 14 | 1.21% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.29% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 24 | 2.08% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -69.0 dBm | 13.0 dBm | 0.40% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#9b0821" /></svg> | Management: Beacon | 1000 | 86.51% | 88.0 B | 0.0 B | 137.3 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 91.78% | 0.14% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f2805a" /></svg> | Management: Probe Request | 2 | 0.17% | 63.0 B | 0.0 B | 104.0 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 0.14% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5a489" /></svg> | Management: Probe Response | 2 | 0.17% | 88.0 B | 0.0 B | 137.3 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.18% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e830c9" /></svg> | Management: Association Request | 2 | 0.17% | 71.0 B | 0.0 B | 114.7 us | 0.0 us | 5010 MHz | -69.0 dBm | - | 0.15% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#cc4ce6" /></svg> | Management: Association Response | 2 | 0.17% | 76.0 B | 0.0 B | 121.3 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.16% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f25aa6" /></svg> | Management: Authentication | 8 | 0.69% | 34.0 B | 0.0 B | 65.3 us | 0.0 us | 5010 MHz | -69.0 dBm | 13.0 dBm | 0.35% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 8 | 0.69% | 42.5 B | 5.5 B | 76.7 us | 7.3 us | 5010 MHz | -69.0 dBm | 13.0 dBm | 0.41% | 0.00% |

#### MPDU observation semantics

| Metric | Value |
|---|---:|
| Total data MPDU transmission observations | 80 |
| Unique `(TA, TID, sequence, fragment)` identities | 80 |
| Repeated identity observations | 0 |
| Observations with Retry bit set | 0 |
| Unique A-MPDU references | 0 |

Repeated observations are retained in airtime totals because every transmission consumes channel time; the unique count is provided only for workload/reliability interpretation.

<!-- END GENERATED: ieee80211ax-pcap-statistics -->
