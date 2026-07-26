# Walkthrough: 802.11ax Downlink OFDMA with Asymmetric Traffic

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `NOT RUN`
- PCAP: `NOT RUN`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RUN`.

This example isolates how two INET downlink schedulers respond when three
stations offer different amounts of traffic. It is a configuration and
analysis companion to [`dl_ofdma_sched`](../dl_ofdma_sched/walkthrough.md).

## [agent] Learning objectives and feature primer

The reader should be able to identify the heavy, medium, and light queues,
compare backlog-based and head-of-line minimum-delay scheduling, and connect
per-station HE-MU observations to application delivery. Downlink OFDMA lets an
AP serve several stations in one HE-MU PPDU using separate resource units
(RUs); the scheduler policy is an INET model choice, not an IEEE-mandated
backlog or delay objective.

## [agent] Scenario description

[`Lan80211AxDlOfdmaAsym.ned`](Lan80211AxDlOfdmaAsym.ned) extends the three-STA
network used by [`dl_ofdma_sched`](../dl_ofdma_sched/Lan80211AxDlOfdma.ned).
The shared defaults use a stationary 5 GHz, 20 MHz HE BSS, a 0.2 s warm-up,
and a measured phase beginning at 0.3 s. During that phase, `host[0]`,
`host[1]`, and `host[2]` receive 1000 B, 400 B, and 100 B packets every 2 ms.
The five interval variants change only the common interval to 4, 3, 3.5,
2.5, or 1.5 ms.

```text
server === wired LAN === AP -- HE-MU OFDMA --> host[0] (1000 B)
                               |--------------> host[1] (400 B)
                               `--------------> host[2] (100 B)
```

## [agent] Standards and INET model boundary

IEEE Std 802.11-2024 Clause 26.5 describes HE downlink multi-user operation
and RU/user signaling. It does not define INET's `HeDlSchedulerBacklogBased`
or `HeDlSchedulerHoLMinDelay` policy. The INI file is configuration input;
scheduler vectors and decoded AP captures are the authoritative evidence for
what occurred. PCAP frame counts alone do not prove queue backlog, RU choice,
or application delivery.

## [agent] Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| Asymmetric offered load is configured | `PASS` | `omnetpp.ini` input | All selected runs | Configuration input only |
| Backlog and HoL policies produce HE-MU service | `NOT RUN` | AP PCAP and scheduler vectors | Not run | Requires co-recorded evidence |
| Per-STA scheduling attribution | `NOT RUN` | `heStaId`, scheduled bytes, PPDU duration | Not run | Requires aligned telemetry |
| Per-STA application delivery and delay | `NOT RUN` | sink `packetReceived` and `endToEndDelay` | Not run | No acceptance threshold yet |

## [agent] Configuration matrix

| Configuration family | Role | Causal delta | Expected invariant |
|---|---|---|---|
| `BacklogBased` + interval variants | Treatment | `HeDlSchedulerBacklogBased` | HE-MU allocation reflects queued demand |
| `HoLMinDelay` + interval variants | Treatment | `HeDlSchedulerHoLMinDelay` | HE-MU service prioritizes head-of-line delay |

All configurations inherit radio, topology, aggregation, and application
defaults from [`../dl_ofdma_sched/omnetpp.ini`](../dl_ofdma_sched/omnetpp.ini).
The scheduler policy and offered-load interval are the intended deltas.

## [agent] Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| At least two eligible STAs share an HE-MU PPDU | AP PCAP plus `heStaId` vectors | HE-SU only | queue eligibility or scheduler | inspect effective INI and AP scheduler vectors |
| Heavy/medium/light traffic remains attributable by STA | receiver address and `heStaId` | mixed or missing flow attribution | packet metadata or analysis filter | inspect AP capture with per-RA filters |
| Delivery is measured per station | sink scalars/vectors | frame counts disagree with delivery | application/result query | query `packetReceived` and delay by sink |

## [agent] Reproduction

Run from the repository root. The command below is illustrative and was not
run while authoring this walkthrough.

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py run dl_ofdma_asym \
  --suite ax --evidence both --runs 5 --config BacklogBased \
  --config HoLMinDelay --jobs 1 --session-id 20260726T000000Z
```

Use the same session for reporting:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py report dl_ofdma_asym \
  --suite ax --session-id 20260726T000000Z
```

## [agent] Scalar and vector analysis

No plot is published because this example has not yet been run with retained
results. The planned analysis must aggregate within
each run first, then compare per-STA delivered bytes, delay, and scheduler
telemetry across independent seeds.

Expected result artifacts (all `NOT RUN`):
`examples/ieee80211ax/dl_ofdma_asym/results/SESSION/scalars.sca` and
`examples/ieee80211ax/dl_ofdma_asym/results/SESSION/vectors.vec`.

| Configuration | Metric | Source artifact | Status |
|---|---|---|---|
| Backlog/HoL variants | per-STA delivery and delay | `.sca`/`.vec` sink results | `NOT RUN` |

<!-- BEGIN GENERATED: ieee80211-scalar-vector-dl-asym -->
### [script] Generated scalar/vector plot and table

Not generated: no results session is recorded.
<!-- END GENERATED: ieee80211-scalar-vector-dl-asym -->

## [agent] PCAP statistics

No plot is published because no capture session is recorded. The shared
analyzer should use the AP capture for cross-user HE-MU
observations and retain unknown PHY fields as unknown.

Expected capture artifact (`NOT RUN`):
`examples/ieee80211ax/dl_ofdma_asym/results/SESSION/ap.pcapng`.
The intended decoder command is `tshark -n -r ap.pcapng`.

The configuration rows are input evidence; result and capture rows are direct
observation evidence only after the listed session is run.

| Capture point | Counting unit | Decisive fields | Status |
|---|---|---|---|
| AP wireless interface | decoded frame observations | HE-MU, RU, receiver/STA attribution | `NOT RUN` |

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables

Not generated: no PCAP session is recorded.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->

## [agent] Frame exchange analysis

`NOT RUN`. A completed evidence session should show AP HE-MU QoS Data, the
corresponding MU acknowledgment exchange, and receiver addresses or STA IDs
for the three asymmetric flows.

## [agent] Cross-layer findings and verdict

`NOT RUN`. The configuration establishes the intended asymmetric input, but
it does not establish that either scheduler selected a particular RU layout
or satisfied a particular station.

## [agent] Limitations and inconclusive claims

- No retained scalar/vector or PCAP session currently supports a runtime
  verdict.
- Scheduler policy names do not prove their decisions.
- A follow-up campaign should use matched seeds for both scheduler families
  and correlate AP telemetry, captures, and sink results from each run.

## [agent] Further experiments

Run the five interval pairs, then add a symmetric control from
`dl_ofdma_sched` and one high-load case. The expected comparison is a change
in queue backlog and per-STA service while topology and PHY settings remain
fixed.

## [agent] Artifact provenance

No evidence artifacts were generated for this walkthrough. The configuration
source is [`omnetpp.ini`](omnetpp.ini); its shared defaults are in
[`../dl_ofdma_sched/omnetpp.ini`](../dl_ofdma_sched/omnetpp.ini).
