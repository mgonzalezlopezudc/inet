---
name: inet-80211-walkthrough-writer
description: Create, revise, or review evidence-backed walkthrough.md files for INET IEEE 802.11 example simulations. Use when Codex must explain a Wi-Fi feature didactically while turning the example into a representative feature test and debugging guide, including scenario description, evidence status, configuration matrix, scalar/vector analysis, PCAP statistics, frame-exchange analysis, reproducible commands, standards/model boundaries, and explicit limitations.
---

# Write evidence-backed IEEE 802.11 walkthroughs

Produce a `walkthrough.md` that teaches the feature and acts as an executable
validation and debugging record. Treat prose, configuration, scalar/vector
results, packet captures, and implementation telemetry as distinct evidence
sources.

## Be in the PRESENT, not looking back the PAST
NEVER compare the current evidence, .ini, or implementation to previous ones, unless explicitely told so by the user. 

We are not interested in the past, only in the current state of the example and its evidence.

If there is already a walkthrough available, ALWAYS try to reuse its content BUT do not copy-paste without understanding the content. The walkthrough must be a coherent, self-contained explanation of the current evidence.


## Load the contract and template

Read [references/walkthrough-contract.md](references/walkthrough-contract.md)
in full before inspecting or drafting a walkthrough. It defines the required
sections, evidence vocabulary, statistical rules, and review criteria.

Use [assets/walkthrough-template.md](assets/walkthrough-template.md) as the
starting structure for a new walkthrough. Preserve the generated-section
markers already used by an example's tooling, but keep generated packet tables
subordinate to the explanatory narrative. Include the recommended
`Implementation plan` section when the walkthrough exposes a model gap,
failed invariant, missing observability, or concrete development follow-up.

Label every section and subsection heading from level 2 through level 6 with
its owner. Use `[script]` for headings inside analysis-script generated
blocks and `[agent]` for headings written or updated by an agent using this
skill. Do not label the level-1 walkthrough title.

Keep the results-session ledger directly below the title. Analysis publishers
own the marker-bounded `[script]` ledger and update their scalar/vector or PCAP
entry whenever they update a walkthrough. Agents must not hand-edit those
entries. The agent owns the separate `[agent]` results-session line and must
list every exact results session used to write or update agent-owned sections;
use `NOT RECORDED` only for legacy unversioned evidence and `NOT RUN` only when
the relevant evidence was not executed.

Read [references/analysis-machinery.md](references/analysis-machinery.md) in
full before creating or regenerating simulation artifacts. Use the shared
IEEE 802.11 suite, campaign, typed-PHY, and PCAP machinery before writing
scenario-specific analysis code.

## Route repository work

Apply the repository workflow skills that match the evidence being created:

- Use `inet-ned-ini-analysis` to prove the configuration inheritance, winning
  parameter assignments, instantiated types, and recording paths.
- Use `omnetpp-result-analysis` to discover and query scalar/vector results.
- Use `inet-pcap-tshark-analysis` and, for Wi-Fi semantics,
  `inet-80211-packet-debugging` for capture setup and frame analysis.
- Use `inet-cmdenv-log-analysis`, `omnetpp-eventlog-analysis`, or
  `inet-lldb-debugging` only when results and captures cannot explain a
  relevant decision or failure.
- Use `ieee80211-standards` for normative claims, exact fields, procedures, and
  standards/model boundaries.
- Use `inet-80211-regression-testing` to select deterministic invariants and
  appropriate seed or parameter coverage.
- Use `inet-agent-orchestration` when the task has independent configuration,
  results, packet, standards, or review lanes.

Do not claim that a standard requirement is implemented merely because the
configuration names it. Do not claim that a configured parameter took effect
without effective-configuration or runtime evidence.

## Follow the authoring workflow

### 1. Establish scope

Identify the example directory, target `walkthrough.md`, NED network, INI
files, configuration chain, feature revision, configurations, run numbers,
seeds, measurement window, and available artifacts. Preserve existing user
changes and generated blocks.

State two outcomes before collecting evidence:

1. The learning outcome: what a reader should understand about the 802.11
   mechanism and exchange.
2. The validation outcome: what observable invariant makes the example pass,
   fail, or remain inconclusive.

### 2. Design the comparison

Prefer a minimal treatment/control pair that differs in the feature gate or
one causal parameter. If the example requires a matrix, identify which rows
are controls, treatments, stress cases, and negative cases. Record confounders
such as bandwidth, MCS, offered load, topology, mobility, channel model, and
association timing.

Use one configuration and run first. Expand to multiple repetitions, seeds,
loads, or parameter points only after the mechanism is observable. A single
run can demonstrate a deterministic exchange but does not establish
statistical robustness.

### 3. Build a claim-to-evidence plan

For every intended feature claim, name:

- the expected invariant;
- the control or counterfactual;
- the authoritative artifact and observation point;
- the exact result name, decoded field, log event, or model signal;
- the status: `PASS`, `FAIL`, `INCONCLUSIVE`, or `NOT RUN`;
- the scope and limitation.

Prefer cross-layer evidence:

```text
effective configuration
  -> feature-specific model telemetry
  -> protocol-visible frame exchange
  -> application or system outcome
```

Do not replace mechanism evidence with throughput alone. Do not treat frame
subtype totals as proof of field values, scheduler decisions, successful
reception, or application delivery.

### 4. Reproduce and validate artifacts

Discover an existing suite descriptor and generated-section marker before
running custom commands. Prefer extending a suite descriptor or feature plugin
when the shared observation envelope already represents the evidence. Keep
generation-specific PHY decoding in its typed profile; do not fork the common
MAC, provenance, campaign, or PCAP pipeline.

Validate that every cited `.sca`, `.vec`, `.pcap`/`.pcapng`, log, image, and
provenance file exists and belongs to the stated configuration and run. Never
mix numeric results from separate sessions.

### 5. Analyze scalar and vector evidence

Discover result names and module paths before selecting them. Report filters,
modules, units, time windows, warm-up handling, aggregation, and empty or
ambiguous matches.

Aggregate within each run before computing uncertainty across runs. Vector
samples from one run are not independent repetitions. Use paired analysis
when seeds and scenario inputs are intentionally paired. State when a table is
single-run evidence and avoid population-level language.

Explain why each metric is relevant to the feature. Separate outcome metrics
such as goodput, delay, fairness, or energy from mechanism telemetry such as
RU allocation, Trigger counts, retry state, NAV, TWT state, or rate selection.

Publish a compact Markdown table and a deterministic plot as a
provenance-bound presentation bundle inside `## Scalar and vector analysis`.
The table must expose the configuration, metric or invariant, source
result/module/unit, window and per-run aggregation, independent-run count and
exclusions, and estimate or single-run observation. The plot must answer a
stated comparison, distribution, or timeline question. When a plot would not
materially improve the explanation, write an explicit no-plot rationale in
the section. Keep exhaustive machine output outside the explanatory table.

### 6. Analyze PCAP statistics and the frame exchange

Decode PHY facts through the shared typed profiles. Accept a field as
authoritative only when the capture format's presence and known bits support
it. Preserve missing, unsupported, or ambiguous facts as unknown. In
particular, do not fill width, guard interval, NSS, coding, RU, or user
attribution from configuration defaults. Use the raw-radiotap fallback
described in the machinery reference when the installed TShark recognizes an
EHT field but does not export its value.

Record capture point, direction semantics, file format, time precision,
checksum/FCS settings, and TShark version when decoding depends on it. State
whether counts are capture observations, MPDUs, aggregates, or de-duplicated
transmissions.

Provide both:

1. A compact frame/PHY statistics table for each relevant configuration.
2. A packet-composition or count-versus-airtime plot when it distinguishes the
   configurations; otherwise an explicit no-plot rationale.
3. An annotated timeline of a representative exchange with frame numbers,
   simulation timestamps, transmitter/receiver, decoded fields, and the role
   of each frame in the invariant.

Correlate PCAP timestamps with result vectors or targeted logs when the claim
depends on causality or internal classification. State undecoded or missing
fields explicitly. Keep the compact table and plot inside `## PCAP statistics`;
put exhaustive generated packet-type rows in a subordinate marked block.

### 7. Write the verdict and debugging path

Synthesize what configuration, telemetry, frames, and end-to-end results prove
together. Distinguish direct observation, derived measurement, and inference.
List failed, inconclusive, and not-run claims instead of smoothing them into a
success narrative.

For each central invariant, document the expected symptom on failure, the
first artifact to inspect, the likely subsystem, and the next focused
diagnostic. This is what makes the walkthrough a reusable feature test and
debug guide rather than a result report.

### 8. Validate the document

Run:

```sh
python3 .agents/skills/inet-80211-walkthrough-writer/scripts/validate_walkthrough.py \
  --require-analysis-visuals path/to/walkthrough.md
```

Fix every error and assess every warning. Then manually apply the quality
gate in the contract; the validator checks structure, not scientific truth.

## Preserve evidence integrity

- Use relative links and paths in the walkthrough.
- Make commands directly runnable from their stated working directory.
- Keep result-session identifiers, runs, seeds, windows, capture points, and
  provenance next to the claims they support.
- Refresh the `[agent]` results-session line whenever agent-owned sections are
  written or updated. Preserve the script-owned session block unchanged.
- Label configuration text as input evidence, PCAP fields as observed packet
  evidence, result records as model telemetry or outcomes, and causal
  explanations as inference unless correlated evidence establishes them.
- Report negative and boundary cases. A useful walkthrough tells the reader
  what the artifacts cannot prove.
- Never update fingerprint expectations as part of writing a walkthrough
  without explicit user approval.
