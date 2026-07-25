# HE Buffer Status Report scheduling walkthrough

This walkthrough teaches how an access point (AP) obtains high-efficiency
(HE) station queue information and uses it for uplink orthogonal
frequency-division multiple access (OFDMA) scheduling. It validates only the
retained BSR-poll exchange, AP backlog vectors, and the stated five-run
comparison; the missing report-to-decision join remains explicit.

## Learning objectives and feature primer

After completing this walkthrough, the reader can:

- distinguish a Buffer Status Report Poll (BSRP) Trigger from its HE
  trigger-based (HE-TB) responses;
- find the AP's reported- and scheduled-backlog telemetry;
- explain why a queue-state average is not proof that a particular report
  caused a particular resource-unit (RU) allocation; and
- reproduce one run and the first-line result and packet diagnostics.

A station (STA) can report queued traffic in a QoS frame. The AP may solicit
reports by broadcasting a BSRP Trigger; selected STAs respond simultaneously
on assigned RUs. INET's backlog-based uplink scheduler then has a queue view
from which it can construct later Trigger allocations. The validation outcome
is: a retained run passes the protocol-visible BSRP/HE-TB exchange check and
the campaign passes the backlog-state comparison, but the causal
report-to-scheduling invariant is `INCONCLUSIVE`.

## Scenario description

[HeBsrNetwork.ned](HeBsrNetwork.ned) extends the common single-BSS network
with one wired UDP server, one stationary AP, and three stationary STAs.
Each STA is 60 m from the AP. Traffic is uplink to UDP port 5000 over a
20 MHz, 5 GHz channel with no configured mobility or external interferer.
[omnetpp.ini](omnetpp.ini) defines a 0.2 s warm-up and either saturated,
stale-report, implicit-report, or two-burst traffic over a 2 s run.

```text
host[0..2] -- 802.11ax uplink --> AP -- Ethernet --> server
             BSRP / HE-TB          scheduler
```

`BurstyTraffic` isolates queue-state changes with bursts at 0.3--0.5 s and
1.0--1.3 s. `StaleBsr` instead uses continuous offered load and a 10 ms
report maximum age, so it is a stress case rather than a clean
single-parameter control for burstiness.

## Standards and INET model boundary

IEEE Std 802.11-2024 Table 9-47 assigns Trigger Type 4 to BSRP, and
Clause 26.5.5 defines solicited BSR operation and queue-size reporting
(`80211ax-2024:chunk:01660`, `80211ax-2024:chunk:09817`). INET configures
`HeHcf`, `HeUlSchedulerBacklogBased`, and AP report freshness as model
abstractions. Configuration proves what was requested; the AP vectors expose
model state; the radiotap/802.11 capture exposes Trigger and HE-TB frames.
The capture does not expose the scheduler's consumed-backlog decision key,
and the model vectors do not retain one, so exact standards-to-decision
causality is not claimed.

## Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| BSRP followed by HE-TB responses | `PASS` | AP PCAP session `20260724T175025Z`, frames 1--4 | `StaleBsr`, run 0 | Direct packet observation at AP |
| Reported backlog differs between bursty and stale stress cases | `PASS` | `heUlBufferStatusReportedBytes:vector`; figure provenance | runs 0--4 per config | Derived time-weighted means over 0.3--1.9 s |
| Reported backlog is joined to scheduler-consumed backlog | `INCONCLUSIVE` | experiment contract and the two AP vectors | retained campaigns | No stable scheduling-decision identifier |
| Application delivery consequence | `NOT RUN` | no matched analysis retained | — | No stated acceptance criterion |

## Configuration matrix

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| `FullBsrAccounting` | treatment basis | default report lifetime | saturated uplink; 20 MHz | packet run 0 | explicit BSR activity |
| `StaleBsr` | freshness stress | `reportMaxAge=10ms` | saturated uplink; matched PHY | packet run 0; scalar runs 0--4 | frequent polling and larger recorded AP backlog |
| `ImplicitBsr` | alternate path | `ulTriggerCheckInterval=0.5s` | saturated uplink; matched PHY | packet run 0 | reports can arrive with SU QoS data |
| `BurstyTraffic` | workload treatment | extends full accounting; two bursts | bursty uplink; matched PHY | scalar runs 0--4 | AP backlog falls and rises with offered load |

All rows inherit `HeHcf`, the backlog scheduler, three STAs, MCS 2 scheduler
default, and a 20 MHz channel. `BurstyTraffic` versus `StaleBsr` changes both
workload and freshness policy, so it is not a causal estimate of either delta
alone. Run numbers are retained; the seed values must be read from each result
file's run attributes rather than inferred from the run number.

## Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| Trigger Type 4 receives HE-TB responses | AP PCAP fields and timestamp order | no BSRP or no responses after it | HE coordinator / reception | inspect `wlan.trigger.he.*`, then matched Cmdenv logs |
| AP records reported backlog | AP `heUlBufferStatusReportedBytes:vector` | empty result query | recorder path / BSR parsing | query result names and AP module paths |
| A consumed decision can be tied to its report | reported and scheduled vectors joined by decision ID | ambiguous many-to-many timestamp join | scheduler observability | add a stable decision identifier before regression |

## Reproduction

Run from the INET repository root:

```sh
bin/inet -u Cmdenv -f examples/ieee80211ax/he_bsr/omnetpp.ini \
  -c StaleBsr -r 0 \
  --result-dir=examples/ieee80211ax/he_bsr/results/manual/StaleBsr
```

This minimal command was **not executed during this rewrite**: status
`NOT RUN`. The retained packet session
`results/packet-statistics/20260724T175025Z/StaleBsr` reached the 2 s limit
and `End.` in `cmdenv.stdout`; its original process exit status and exact
generation command were not retained. Regenerate the suite-owned packet
artifacts with:

```sh
python3 examples/ieee80211/analysis/analyze_pcap.py \
  --suite examples/ieee80211/analysis/suites/ax.json \
  --generate --subdir he_bsr --run 0
```

That regeneration command is also `NOT RUN` in this rewrite.

## Scalar and vector analysis

Inputs are
`results/scalar-vector/20260725T120411Z/{BurstyTraffic,StaleBsr}/*.{sca,vec}`;
the figure and its JSON provenance are
[bsr-reported-vs-scheduled.png](../analysis/figures/bsr/bsr-reported-vs-scheduled.png)
and
[bsr-reported-vs-scheduled.png.json](../analysis/figures/bsr/bsr-reported-vs-scheduled.png.json).

```sh
opp_scavetool query -l \
  -f 'type =~ vector AND (name =~ "heUlBufferStatusReportedBytes:vector" OR name =~ "heUlBufferStatusScheduledBytes:vector")' \
  examples/ieee80211ax/he_bsr/results/scalar-vector/20260725T120411Z/*/*.vec
```

| Metric or invariant | Source | Window/aggregation | `BurstyTraffic` | `StaleBsr` | Interpretation |
|---|---|---|---:|---:|---|
| reported backlog | AP `heUlBufferStatusReportedBytes:vector`, B | per-run time-weighted mean, then mean ± 95% Student-t CI across 5 runs; 0.3--1.9 s | 27,145 ± 704 B | 72,371 ± 151 B | direct model state; nonoverlapping CIs |
| scheduled backlog | `heUlBufferStatusScheduledBytes:vector`, B | same intended window | undefined | not reported here | bursty vector lacks an initial state at 0.3 s |

Vector emissions are event-driven; raw sample means are invalid substitutes
for time weighting. These are five independent run summaries, not confidence
intervals over vector samples. The comparison proves different AP-recorded
queue-state trajectories, not a throughput effect or a report-to-allocation
join.

## PCAP statistics

Capture point: `HeBsrNetwork.ap.wlan[0]`

Capture:
`results/packet-statistics/20260724T175025Z/StaleBsr/StaleBsr-#0HeBsrNetwork.ap.wlan[0].pcap`

Scope: legacy PCAP, simulation timestamps, AP packet-signal observations;
TShark 4.6.4 per retained analyzer output. FCS/checksum settings were not
retained.

```sh
tshark -n -r \
  'examples/ieee80211ax/he_bsr/results/packet-statistics/20260724T175025Z/StaleBsr/StaleBsr-#0HeBsrNetwork.ap.wlan[0].pcap' \
  -Y 'wlan.trigger.he.trigger_type == 4' \
  -T fields -E header=y -E separator='|' \
  -e frame.number -e frame.time_epoch -e wlan.ta -e wlan.ra \
  -e wlan.trigger.he.trigger_type -e wlan.trigger.he.ul_length
```

| Configuration | Observation count | Relevant summary | Interpretation limit |
|---|---:|---|---|
| `FullBsrAccounting` | 3049 | 56 Trigger observations; 165 HE-TB QoS Null observations | AP/global transmission observations, not reports joined to decisions |
| `ImplicitBsr` | 2808 | nonempty protocol population | subtype total does not prove implicit report content |
| `StaleBsr` | 3107 | 69 Trigger observations; 206 HE-TB QoS Null observations | not application delivery or freshness proof |

## Frame exchange analysis

| Frame | Simulation time | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.001048 s | AP → broadcast | Trigger | type=4, UL length=730 | solicits BSRs |
| 2 | 0.002064 s | STA 1 → AP | QoS Null, HE-TB | MCS=0, BW/RU code=4 | first simultaneous response observation |
| 3 | 0.002064 s | STA 2 → AP | QoS Null, HE-TB | MCS=0, BW/RU code=4 | second response observation |
| 4 | 0.002064 s | STA 3 → AP | QoS Null, HE-TB | MCS=0, BW/RU code=4 | third response observation |

The equal response timestamps directly demonstrate a trigger-based concurrent
response exchange. They do not decode queue bytes or prove how a later
scheduler decision consumed them; that link remains `INCONCLUSIVE`.

## Cross-layer findings and verdict

| Claim | Verdict | Configuration evidence | Model telemetry | Packet evidence | Outcome evidence |
|---|---|---|---|---|---|
| BSR exchange is exercised | `PASS` | `HeHcf` plus backlog scheduler | report vectors exist | BSRP + three HE-TB responses | not required |
| stale stress changes AP queue view | `PASS` | 10 ms freshness plus continuous load | 72,371 ± 151 B vs 27,145 ± 704 B | both sessions have Trigger activity | no matched delivery analysis |
| a report caused a specific allocation | `INCONCLUSIVE` | scheduler requested | no join identifier | headers do not expose internal join | not analyzed |

The configuration, vector, and packet sessions support adjacent claims but
do not establish event-level causality across sessions. The bounded verdict is
that BSR exchange and AP backlog observability work in the retained scope;
scheduler accounting correctness is not yet a regression-grade invariant.
Evidence basis: the timeline is a **direct observation**, the confidence
intervals are **derived measurements**, and the proposed scheduler connection
is an **inference** that remains inconclusive.

## Limitations and inconclusive claims

- The retained vectors cannot join one report to one scheduling decision.
- The bursty/stale comparison has two causal deltas and is not a clean control.
- No application delivery, delay, fairness, or retry acceptance criterion was
  evaluated.
- One co-recorded run with report value, decision ID, scheduled bytes, Trigger
  allocation, and delivery would resolve the central causal gap.

## Further experiments

- Compare `FullBsrAccounting` and `StaleBsr` at matched saturated load and five
  paired seeds; predict more expired-report polls in the latter.
- Compare full accounting with `ImplicitBsr` while retaining report-source
  telemetry; predict fewer BSRP Trigger frames for the implicit path.
- Sweep `reportMaxAge` around one exchange duration and record timeout state
  plus delivery to expose the boundary without changing workload.

## Implementation plan

| Item | Evidence-backed plan |
|---|---|
| Demonstrated gap | no stable report-to-scheduling decision join |
| Intended behavior | expose which reported queue state a scheduler decision consumed |
| Smallest change surface | AP HE coordinator/scheduler telemetry and result recording; exact source symbols require architecture review |
| Observability | emit decision ID, STA/AC, report timestamp/bytes, scheduled bytes, and Trigger correlation |
| Validation | full/stale matched pair, one deterministic run first, then five paired seeds; co-record vectors, PCAP, and delivery |
| Compatibility and risks | telemetry must not alter event order or legacy scheduler behavior |
| Architecture and sealing | apply `inet-architectural-requirements` and check seals before any `src/inet` edit |
| Next handoff | HE scheduler implementation owner plus independent Wi-Fi regression review |

This is a proposed development path, not evidence that a named source path
executed and not authorization to modify production code.

## Artifact provenance

| Artifact family | Session/path | Configurations/runs | Tool/filter/window | Integrity notes |
|---|---|---|---|---|
| scalar/vector | `results/scalar-vector/20260725T120411Z` | `BurstyTraffic`, `StaleBsr`; 0--4 | JSON provenance; 0.3--1.9 s | hashes retained in figure JSON |
| PCAP/results/log | `results/packet-statistics/20260724T175025Z` | full, implicit, stale; run 0 | shared AX analyzer; TShark 4.6.4 | separate from five-run campaign |
