# Walkthrough: <IEEE 802.11 feature>

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `NOT RUN`
- PCAP: `NOT RUN`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

<One paragraph naming the feature, the comparison, and the evidence-backed
scope of this walkthrough.>

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- <explain the feature and the problem it solves>;
- <identify the decisive frames, fields, or state>;
- <relate the mechanism to the recorded outcome>; and
- <reproduce the representative validation and first-line diagnostics>.

<Explain the protocol mechanism in plain language. Name the participating
roles, expected exchange, decisive fields, and relevant state.>

## [agent] Scenario description

<Describe topology, node roles, traffic, timing, mobility, channel,
interference, and why the scenario isolates or stresses the feature. Link the
NED and INI files. Add a compact diagram if useful.>

## [agent] Standards and INET model boundary

<Name the IEEE revision and relevant clauses/tables/fields. Separate normative
behavior, INET's abstraction, configured behavior, and observed behavior.>

## [agent] Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| <feature gate took effect> | `NOT RUN` | <result/field/log> | <runs> | <scope> |
| <representative exchange occurred> | `NOT RUN` | <PCAP and fields> | <runs> | <scope> |
| <outcome changed as expected> | `NOT RUN` | <scalar/vector metric> | <runs> | <scope> |

## [agent] Configuration matrix

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| <control> | Control | <disabled/baseline> | <matched inputs> | <runs> | <counterfactual> |
| <treatment> | Treatment | <enabled/delta> | <matched inputs> | <runs> | <feature behavior> |

<Explain inherited settings, effective values, and material confounders.>

## [agent] Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| <specific check> | <artifact/result/field> | <observable mismatch> | <module area> | <focused command or skill> |

## [agent] Reproduction

Run from the INET repository root:

```sh
bin/inet -u Cmdenv -f <example>/omnetpp.ini \
  -c <Configuration> -r <run> --seed-set=<seed> \
  --result-dir="$PWD/<example>/results/<session>/<configuration>"
```

<Record the exact command, observed exit status (or `NOT RUN` if illustrative),
output directory, build mode, and temporary command-line overrides. Add
campaign/regeneration commands after the minimal run. Keep `--result-dir`
absolute: scalar/vector output is resolved relative to the INI file, while
PCAP output may be resolved relative to the process working directory.>

## [agent] Scalar and vector analysis

Inputs: `<file.sca>` and `<file.vec>`.

```sh
opp_scavetool query -l \
  -f '<narrow result filter>' \
  <file.sca> <file.vec>
```

| Metric or invariant | Source result and module | Window/aggregation | Control | Treatment | Interpretation |
|---|---|---|---:|---:|---|
| <metric> | <module/result/unit> | <window/method> | <value> | <value> | <feature relevance> |

![<Scalar/vector comparison, distribution, or timeline>](<relative-result-figure.png>)

Figure provenance: [`<relative-result-figure.png.json>`](<relative-result-figure.png.json>).

<Explain run count, seeds, warm-up, per-run aggregation, uncertainty, ambiguous
or empty matches, and the distinction between mechanism and outcome metrics.
If a plot is not useful, replace the image and provenance lines with
`No plot: <concrete reason>.`>

## [agent] PCAP statistics

Capture point: `<node.interface>`  
Capture: `<file.pcapng>`  
Recorder/decode scope: `<format, precision, FCS/checksum, TShark version>`

```sh
tshark -n -r <file.pcapng> -q \
  -z io,stat,0,'<display filter>'
```

| Configuration | Observation count | Relevant frame/PHY summary | Interpretation limit |
|---|---:|---|---|
| <configuration> | <count> | <types/formats/fields> | <capture semantics> |

![<Packet composition or count-versus-airtime comparison>](<relative-pcap-figure.png>)

Figure provenance: [`<relative-pcap-figure.png.json>`](<relative-pcap-figure.png.json>).

<If a plot is not useful, replace the image and provenance lines with
`No plot: <concrete reason>.` Keep exhaustive generated packet rows in a
subordinate marker-bounded block.>

## [agent] Frame exchange analysis

```sh
tshark -n -r <file.pcapng> \
  -Y '<feature-specific display filter>' \
  -T fields -E header=y -E separator='|' \
  -e frame.number -e frame.time_epoch <decisive fields>
```

| Frame | Simulation time | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| <number> | <seconds> | <source → destination> | <frame/PPDU> | <field=value> | <protocol role> |

<Explain ordering, causal evidence, correlations with result vectors/logs, and
what the capture cannot decode.>

## [agent] Cross-layer findings and verdict

| Claim | Verdict | Configuration evidence | Model telemetry | Packet evidence | Outcome evidence |
|---|---|---|---|---|---|
| <claim> | `NOT RUN` | <effective setting> | <result/log> | <frames/fields> | <metric> |

<State direct observations, derived measurements, inferences, session
alignment, and the bounded conclusion. Do not claim event-level causality
across separately instrumented sessions.>

## [agent] Limitations and inconclusive claims

- <unsupported claim and missing evidence>
- <model-fidelity or dissector limitation>
- <smallest additional run, control, field, or recorder needed>

## [agent] Further experiments

- <Change one parameter or negative case; predict the invariant and artifact.>

## [agent] Implementation plan

<Include when the walkthrough exposes a model gap, failed invariant, missing
observability, or concrete development follow-up. Otherwise state that no
implementation work is proposed.>

| Item | Evidence-backed plan |
|---|---|
| Demonstrated gap | <failed or inconclusive invariant and evidence> |
| Intended behavior | <normative requirement or explicit modeling choice> |
| Smallest change surface | <modules/files/symbols/NED/INI/messages> |
| Observability | <signals/results/fields/logs needed> |
| Validation | <control/treatment, invariant, runs/seeds, evidence> |
| Compatibility and risks | <legacy modes, boundaries, dependencies> |
| Architecture and sealing | <requirements, seal status, permission gate> |
| Next handoff | <owner or specialist and unresolved decisions> |

<Treat this as a proposed development path, not proof that the named code path
executed and not authorization to edit production source.>

## [agent] Artifact provenance

| Artifact family | Session/path | Configurations/runs | Tool/filter/window | Integrity notes |
|---|---|---|---|---|
| Scalar/vector | <relative path> | <configs/runs> | <query/window> | <hash/provenance> |
| PCAP | <relative path> | <configs/runs> | <capture/TShark> | <observation points> |
