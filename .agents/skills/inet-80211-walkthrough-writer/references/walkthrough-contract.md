# IEEE 802.11 walkthrough contract

## Purpose

A walkthrough has two equal goals:

1. Teach the selected IEEE 802.11 feature and its protocol role.
2. Serve as a representative, reproducible feature test and debugging guide.

A document that only describes configuration is not complete. A document that
only reports statistics is not didactic. A packet-count appendix without an
annotated exchange does not demonstrate a protocol mechanism.

## Evidence vocabulary

Use these statuses consistently:

| Status | Meaning |
|---|---|
| `PASS` | Direct, relevant evidence satisfies the stated invariant in the stated scope. |
| `FAIL` | Direct evidence contradicts the invariant or a required artifact/check failed. |
| `INCONCLUSIVE` | Artifacts exist, but the decisive field, control, correlation, or coverage is missing. |
| `NOT RUN` | The scenario or check has not been executed with retained evidence. |

Describe the basis of each claim:

| Basis | Use |
|---|---|
| Configuration input | Shows what was requested, not what occurred. |
| Direct observation | A decoded field, result record, log decision, or event directly exposes the fact. |
| Derived measurement | A documented calculation from named artifacts, filters, units, and windows. |
| Inference | A reasoned explanation consistent with evidence but not directly exposed. |

Do not promote `INCONCLUSIVE` to `PASS` because a simulation completed or a
capture is nonempty.

## Required document sections

Use these canonical headings. When revising a legacy walkthrough, normalize
equivalent headings to this contract so the structural validator and readers
see the same predictable organization.

### Learning objectives and feature primer

- Explain the feature in plain language before using INET-specific terms.
- Identify the problem the feature solves, participating roles, important
  frames/fields/state, and the expected exchange.
- State what the reader will be able to identify in configuration, results,
  and PCAP evidence.
- Expand acronyms on first use and connect each metric to the mechanism.

### Scenario description

- Describe topology, node roles, wireless links, wired backhaul, traffic
  direction, workload, timing, mobility, channel, and interference.
- Link the NED and INI sources.
- Include a compact diagram when node relationships are not obvious.
- Explain why the scenario isolates or stresses the feature.

### Standards and INET model boundary

- Name the applicable IEEE 802.11 revision and clause/table/field when making a
  normative claim.
- Separate normative behavior, INET's implemented abstraction, configured
  behavior, and observed behavior.
- State important fidelity limits, idealizations, missing fields, or
  unimplemented behaviors.

### Evidence status

Start with an artifact and claim inventory:

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|

Include missing or partial artifact families. Identify exact scalar/vector and
packet-capture sessions. A reader should know what is and is not supported
before reading conclusions.

### Configuration matrix

Include:

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|

Show the causal delta, not a dump of every INI assignment. State inherited
configuration and effective values where wildcard precedence or `typename`
selection matters. Identify controls, treatments, negative cases, and stress
cases. Call out confounders that prevent a clean comparison.

### Expected invariants and diagnostic map

For each central mechanism, provide:

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|

An invariant must be specific enough to check. Prefer fields, state
transitions, counters, timestamp relations, or allocation constraints over
generic claims such as "performance improves."

Map the first failure to a focused next step: effective NED/INI configuration,
Cmdenv module logs, PCAP fields, result telemetry, event-log ordering, or LLDB.

### Reproduction

- State the working directory.
- Give the exact single-configuration, single-run Cmdenv command first.
- Record configuration, run, seed, build mode, overrides, exit status, and
  result directory.
- Record an observed exit status only for a command executed during the
  evidence-producing run. For an illustrative command that was not executed
  during authoring, say `NOT RUN`; never imply a historical success status.
- Give campaign and regeneration commands only after the minimal run.
- Use `-j$(nproc)` for repository build or campaign commands that support
  parallel jobs.
- Keep temporary logging, recording, and capture overrides on the command
  line.

### Scalar and vector analysis

- List input `.sca` and `.vec` artifacts and the exact `opp_scavetool` filter
  or native result-API query.
- Verify module path, result name, type, units, run metadata, and measurement
  window.
- Explain warm-up removal and any time weighting, pooling, percentile,
  fairness, goodput, or energy formula.
- Aggregate per run before calculating means, dispersion, or confidence
  intervals across repetitions.
- Do not treat samples within one vector as repetitions.
- Pair outcome metrics with feature-specific mechanism telemetry.
- State whether results are single-run observations or multi-run estimates.
- Use a plot only when it improves understanding; keep its provenance with the
  input files, hashes, filters, runs, seeds, and window.

### PCAP statistics

- Identify every cited capture, node/interface observation point, direction,
  format, precision, and relevant recorder settings.
- Verify the capture is nonempty and decodes.
- Summarize relevant frame types, PHY formats, MCS/NSS, channel width or RU,
  coding/GI, sizes, durations, retries, and airtime when available.
- State whether rows count capture observations, MPDUs, A-MPDU subframes, or
  de-duplicated transmissions.
- Do not infer application delivery or internal scheduling decisions from
  frame totals.
- Keep generated exhaustive tables in a marked appendix or generated block;
  put the explanatory summary before them.

### Frame exchange analysis

Include at least one representative timeline:

| Frame | Simulation time | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|

- Use `tshark -n -T fields` with a reproducible display filter.
- Decode the fields that distinguish the feature, not only type/subtype.
- Explain ordering, response, allocation, acknowledgment, retry, or state
  transition as relevant.
- Correlate with model telemetry or logs when the decisive behavior is
  internal or not represented in radiotap/802.11 headers.
- Treat absent fields and dissector heuristics as limitations.
- Never confuse TShark frame numbers with OMNeT++ event numbers.

### Cross-layer findings and verdict

Trace each central claim from effective configuration through mechanism
telemetry and protocol exchange to the end-to-end outcome. State which links
are direct evidence and which are inference.

Evidence recorded in separate sessions may support adjacent, separately
scoped findings when configuration, run/seed policy, and measurement scope are
matched. It cannot prove frame-to-result event causality or exact count
agreement. For a causal chain, co-record the necessary results, capture, and
logs in one run; otherwise label the connection as inference and explain any
trajectory change caused by instrumentation.

Report `PASS`, `FAIL`, `INCONCLUSIVE`, and `NOT RUN` outcomes explicitly.
Avoid broad real-world claims from one topology, one seed, or a packet-level
model.

### Limitations and inconclusive claims

- List claims that the artifacts do not support.
- Identify missing controls, fields, recorders, seeds, parameter coverage, or
  model fidelity.
- State the smallest additional run or instrumentation needed to resolve each
  important gap.

## Recommended sections

### Artifact provenance

Record artifact-session identifiers, exact inputs, hashes, tool versions when
decode/analysis depends on them, analysis scripts, filters, runs, seeds,
windows, and generated outputs. Do not silently mix sessions.

### Further experiments

Offer a few bounded exercises that vary one parameter, negative case, stress
condition, or seed set. Each exercise should predict which invariant or metric
will change and which artifact will show it.

### Implementation plan

Include this section when the walkthrough exposes a model gap, failed
invariant, missing observability, or concrete development follow-up. Keep it
bounded and evidence-driven:

- State the demonstrated gap and intended behavior without treating a
  hypothesis as an established cause.
- Identify the smallest likely change surface: relevant INET modules, source
  files/symbols, NED/INI parameters, messages, signals, results, and protocol
  structures.
- Name the applicable IEEE revision and clauses for normative behavior, and
  separate standards requirements from INET modeling choices.
- Propose any observability additions needed to make the invariant directly
  testable.
- Define focused validation: control/treatment configurations, deterministic
  invariant, runs/seeds or boundary points, packet/result/log evidence, and
  legacy behavior that must remain unchanged.
- Record unresolved design choices, risks, dependencies, and the next owner or
  specialist handoff.
- Before planning changes under `src/inet/`, apply
  `inet-architectural-requirements`, check sealing status, map the applicable
  architecture rules, and identify any permission gate.

This plan guides a future implementation; it is not evidence that the named
code path executed or authorization to modify production source.

## Statistical and comparison criteria

- Use the same build mode, configuration basis, topology, workload, window,
  and seed policy for matched comparisons.
- State whether repetitions are paired.
- Use confidence intervals only across independent runs, not vector samples.
- Report sample count and variability; do not imply a strict ordering when
  intervals overlap without a justified test.
- Interpret delivery, delay, fairness, energy, and airtime together when one
  metric can improve by sacrificing another.
- Avoid ratios with a zero or ill-defined baseline and delivery ratios whose
  generation window does not match the observation window.
- Keep single-run mechanism evidence separate from multi-run outcome evidence.

## Final quality gate

The walkthrough is ready only if all answers are yes:

### Didactic value

- Can a reader explain the feature, roles, fields, and expected exchange?
- Does every chart or table answer a stated learning question?
- Are acronyms, metrics, and model abstractions explained?

### Feature-test value

- Is there a control or justified counterfactual?
- Are the central invariants observable and status-labeled?
- Can another developer reproduce the minimal run and exact queries?
- Would a regression produce a visible `FAIL` rather than vague degradation?

### Debug value

- Does each invariant identify its observation point and failure symptom?
- Does the document say where to look next when evidence disagrees?
- Are configuration, model telemetry, packet exchange, and outcome kept
  distinguishable?

### Evidence integrity

- Does every numeric or protocol claim cite an exact artifact, filter, run,
  seed, and window as applicable?
- Are observation-point duplication and capture limitations disclosed?
- Are missing evidence and inconclusive claims prominent?
- Are all paths relative and all generated sections clearly marked?
