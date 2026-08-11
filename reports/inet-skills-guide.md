# INET Skills Guide

This document explains the **18 project-scoped skills** in
[`.agents/skills/`](../.agents/skills/). They are instruction packages for
working on OMNeT++/INET, especially IEEE 802.11. They do not replace source
code, tests, or engineering judgment; they make the path from a question to
**reproducible evidence and a defensible conclusion** more consistent.

## The aggregate purpose

Taken together, the skills form an **evidence-governed engineering system**:

```text
meaning and constraints
        ↓
standards · architecture · configuration · build
        ↓
controlled execution
        ↓
logs · packets · events · results · debugger state
        ↓
tests · regressions · fingerprints · plots · walkthroughs · review
```

Their central benefit is separation of questions that are easy to conflate:

- **What does IEEE 802.11 require?** The standards skill answers this.
- **What is INET designed to permit?** The architecture skill answers this.
- **What did this configuration actually instantiate?** The NED/INI skill answers
  this.
- **What did the simulator actually do?** The execution and evidence skills answer
  this.
- **Is the result repeatable, correctly measured, and safe to keep?** The testing,
  regression, analysis, documentation, and review skills answer this.

**This separation prevents category errors:** a configured parameter being
mistaken for runtime behavior, a standard requirement for an implementation, a
capture for a causal explanation, or a single passing run for coverage.

## Skill families at a glance

| Family | Skills | Main value |
| --- | --- | --- |
| Meaning and design | [IEEE 802.11 standards](../.agents/skills/ieee80211-standards/SKILL.md), [architectural requirements](../.agents/skills/inet-architectural-requirements/SKILL.md) | Establish normative behavior and repository design constraints. |
| Configuration and execution | [NED/INI analysis](../.agents/skills/inet-ned-ini-analysis/SKILL.md), [build/debug modes](../.agents/skills/inet-build-debug-modes/SKILL.md), [simulation run](../.agents/skills/inet-simulation-run/SKILL.md) | Make the instantiated model and executable artifacts trustworthy. |
| Runtime evidence | [802.11 packet debugging](../.agents/skills/inet-80211-packet-debugging/SKILL.md), [Cmdenv logs](../.agents/skills/inet-cmdenv-log-analysis/SKILL.md), [PCAP/TShark](../.agents/skills/inet-pcap-tshark-analysis/SKILL.md), [event logs](../.agents/skills/omnetpp-eventlog-analysis/SKILL.md), [LLDB](../.agents/skills/inet-lldb-debugging/SKILL.md) | Locate the first divergence and distinguish observation from inference. |
| Data and representation | [packet tags](../.agents/skills/inet-packet-tag-debugging/SKILL.md), [result analysis](../.agents/skills/omnetpp-result-analysis/SKILL.md), [result plotting](../.agents/skills/omnetpp-result-plotting/SKILL.md) | Explain internal packet state and turn recorded data into valid measurements. |
| Confidence and communication | [unit tests](../.agents/skills/inet-unit-tests/SKILL.md), [802.11 regression](../.agents/skills/inet-80211-regression-testing/SKILL.md), [fingerprint regression](../.agents/skills/inet-fingerprint-regression/SKILL.md), [walkthrough writer](../.agents/skills/inet-80211-walkthrough-writer/SKILL.md) | Demonstrate that a claim is protected, reproducible, and understandable. |
| Coordination | [agent orchestration](../.agents/skills/inet-agent-orchestration/SKILL.md) | Route independent work to bounded specialist roles and gate handoffs. |

## The skills individually

### 1. IEEE 802.11 standards

[Source](../.agents/skills/ieee80211-standards/SKILL.md)

**Purpose:** This skill searches the repository’s generated standards corpus by clause,
table, figure, field, or procedure. It defines when to rebuild the corpus,
when a PDF must be consulted, and how to preserve the revision and chunk
identifiers behind a finding.

**Benefit: normative precision.** It helps distinguish requirements from
informative text, resolve revision ambiguity, and keep an implementation claim
tied to the correct IEEE source. It is the reference point for questions such
as “should this frame, field, or procedure exist?”, but it never proves that
INET implements or enables that behavior.

### 2. IEEE 802.11 packet debugging

[Source](../.agents/skills/inet-80211-packet-debugging/SKILL.md)

**Purpose:** This skill investigates the complete Wi-Fi path from application or network
packet, through MAC queues and coordination, frame exchange and PHY
construction, radio-medium effects, reception, and upper-layer delivery. It
requires finding the first layer where observed behavior diverges and choosing
the cheapest evidence that can prove it.

**Benefit: end-to-end Wi-Fi discipline.** It prevents common shortcuts such
as inferring collision from a missing ACK, inferring PHY reception from a MAC
capture, or assuming that a configured feature is implemented. It also routes
generic work to the simulation, PCAP, results, logs, event-log, LLDB, and
standards skills while preserving the distinction between direct evidence and
inference.

### 3. INET architectural requirements

[Source](../.agents/skills/inet-architectural-requirements/SKILL.md)

**Purpose:** This skill applies INET’s requirements, dependency direction, contracts,
composition rules, observability, determinism, naming conventions, exception
ledgers, enforcement checks, and source-file sealing policy. Before a change
under `src/inet/`, it requires the target’s sealing status to be resolved.

**Benefit: architectural safety.** It turns broad design preferences into
identifiable `R-*`, `AR-*`, naming, ledger, checklist, and sealing decisions.
For 802.11 changes it adds the WLAN-specific requirements and review
checklists. This prevents a locally successful fix from quietly weakening the
model’s long-term structure.

### 4. INET agent orchestration

[Source](../.agents/skills/inet-agent-orchestration/SKILL.md)

**Purpose:** This skill decomposes nontrivial work into bounded evidence lanes and routes
them to roles such as navigator, evidence miner, Wi-Fi specialist, simulation
detective, implementer, regression guard, results analyst, and reviewer. It
limits delegation depth, permits only one production-code writer, assigns
model tiers by judgment required, and defines handoff gates.

**Benefit: coordinated specialization.** The root
thread retains requirements and synthesis; specialists return concise facts,
commands, artifacts, uncertainty, and next handoffs. The skill also enforces a
useful evidence priority: reproducible runtime/debugger evidence first, then
packet/event/result evidence, effective configuration, source behavior, and
finally hypothesis.

### 5. NED and INI analysis

[Source](../.agents/skills/inet-ned-ini-analysis/SKILL.md)

**Purpose:** This skill proves the actual network type, module path, inherited type,
`typename`, parameter value, configuration chain, and wildcard precedence. It
requires the winning assignment to be demonstrated instead of guessed from
file order or apparent specificity.

**Benefit: configuration truth.** It catches the common situation where a
parameter exists but is overridden, inherited, unmatched, attached to the
wrong module, or incompatible with another model component. It is therefore a
necessary prerequisite for interpreting simulation, capture, and result
evidence.

### 6. Build and debug modes

[Source](../.agents/skills/inet-build-debug-modes/SKILL.md)

**Purpose:** This skill keeps runners, libraries, generated code, project libraries, and
debug symbols consistent. It distinguishes release and debug artifacts,
checks whether message or NED-generated code is fresh, and explains how to
inspect the loaded library when LLDB behavior is suspicious.

**Benefit: artifact integrity.** It prevents an agent from attributing
behavior to new source while the simulator is loading an old library, or from
trying to debug optimized code with mismatched symbols. This is the bridge
between a source diff and the executable that actually ran.

### 7. Simulation run

[Source](../.agents/skills/inet-simulation-run/SKILL.md)

**Purpose:** This skill provides the standard launch path through `inet`, uses Cmdenv for
reproducible automation, reserves Qtenv for interactive needs, and records
configuration, run, environment, mode, and relevant artifacts. It also routes
diagnostics to the more specialized log, PCAP, event-log, result, and LLDB
skills.

**Benefit: a stable execution baseline.** It reduces failures caused by
wrong working directories, missing NED paths, accidental runner selection, or
release/debug mixing before those failures are misdiagnosed as model behavior.

### 8. Cmdenv log analysis

[Source](../.agents/skills/inet-cmdenv-log-analysis/SKILL.md)

**Purpose:** This skill captures narrow, timestamped, event-numbered module logs and uses
them to trace enqueue, dequeue, transmission, reception, drop, retry,
timeout, and state transitions. It emphasizes saved logs, targeted levels,
and searching for the first relevant warning or error.

**Benefit: low-cost behavioral visibility.** Logs often show the policy
decision that a packet capture cannot show, while event numbers and simulation
times provide a common coordinate system for correlating other evidence.

### 9. PCAP and TShark analysis

[Source](../.agents/skills/inet-pcap-tshark-analysis/SKILL.md)

**Purpose:** This skill configures narrow PCAPng captures, validates that the files exist
and decode, extracts exact protocol fields with TShark, and correlates capture
timestamps with simulation time and logs. It distinguishes capture position,
sender-side observation, and receiver-side evidence.

**Benefit: protocol-visible proof.** It can show the actual frame exchange,
headers, addresses, retries, or streams, while its strict limits prevent
absence from one capture point being overinterpreted as loss or delivery.

### 10. OMNeT++ event-log analysis

[Source](../.agents/skills/omnetpp-eventlog-analysis/SKILL.md)

**Purpose:** This skill reconstructs simulator-level causality: scheduling, self-messages,
delayed sends, cancellations, deliveries, ownership, deletion, and event
ordering. It starts from a narrow reproduction and traces backward from the
wrong event, then forward through the resulting chain.

**Benefit: causal resolution.** A PCAP
can show that a frame appeared, and a log can show a decision, but an event log
can show which timer or message movement caused that decision and whether an
event was cancelled or reordered.

### 11. Packet, chunk, and tag debugging

[Source](../.agents/skills/inet-packet-tag-debugging/SKILL.md)

**Purpose:** This skill separates packet bytes from metadata and traces the first module
where a chunk, tag, region tag, protocol marker, ownership state, or
encapsulation changes unexpectedly. It covers `peek` versus `pop`, duplication,
fragmentation, aggregation, and request/indication direction.

**Benefit: representation-level precision.** It explains failures where the
serialized packet looks plausible but the next module lacks the internal
metadata that drives dispatch, addressing, error handling, or PHY/MAC policy.

### 12. LLDB debugging

[Source](../.agents/skills/inet-lldb-debugging/SKILL.md)

**Purpose:** This skill escalates to source-level debugging only after a narrow reproduction
and lower-cost evidence have identified a suspicious path. It requires matching
debug artifacts, targeted breakpoints or watchpoints, a preserved backtrace,
local-variable inspection, and correlation with simulation time, event number,
module, message, and packet identity.

**Benefit: state-at-the-instruction evidence.** This is useful for crashes, aborts, hangs,
invalid ownership, and otherwise unresolved runtime divergence. The discipline
of inspecting before evaluating and not mutating state from debugger
expressions keeps the investigation from becoming an uncontrolled experiment.

### 13. OMNeT++ result analysis

[Source](../.agents/skills/omnetpp-result-analysis/SKILL.md)

**Purpose:** This skill queries and exports `.sca` and `.vec` files using exact run
metadata, narrow filters, verified module/result names, units, and result
types. It distinguishes scalars, vectors, statistics, and histograms and uses
the repository-supported `CSV-R` or `CSV-S` export formats.

**Benefit: measurement hygiene.** It prevents mixed runs, ambiguous matches,
wrong units, or aggregate values from being treated as evidence for a specific
configuration or transition. It also connects aggregate counters to the
packet, log, and event timelines when a causal interpretation is needed.

### 14. OMNeT++ result plotting

[Source](../.agents/skills/omnetpp-result-plotting/SKILL.md)

**Purpose:** This skill creates deterministic, non-interactive plots and derived summaries
through `omnetpp.scave.results`, preserving run metadata and experimental
conditions. It defines the measurement before coding, validates inputs, uses
independent runs as repetitions, and selects plot types appropriate to the
recorded quantity.

**Benefit: statistically honest visualization.** It avoids
pooling packets or vector samples as if they were independent repetitions,
losing condition columns, inventing warm-up intervals, or using an ordinary
sample mean for a piecewise-constant signal. The saved script makes the figure
reproducible rather than merely attractive.

### 15. INET unit tests

[Source](../.agents/skills/inet-unit-tests/SKILL.md)

**Purpose:** This skill defines the repository-supported unit-test entry point, the required
build-before-test sequence for compiled changes, release/debug matching,
ccache handling, regex filtering, and failure classification.

**Benefit: fast, local protection.** The explicit
rebuild rule is especially important: a freshly generated test executable does
not prove that the INET library contains the current source.

### 16. IEEE 802.11 regression testing

[Source](../.agents/skills/inet-80211-regression-testing/SKILL.md)

**Purpose:** This skill designs the smallest deterministic scenario for a specific Wi-Fi
behavior, compares before and after under the same conditions, and expands to
seeds or parameter points only after the narrow behavior is understood. It
requires protocol-visible invariants such as association state, ACK/retry
evolution, Block Ack windows, SNIR/error decisions, forwarding, and enabled
feature gates.

**Benefit: targeted coverage.** It makes a
passing run meaningful by tying it to the exact exchange or invariant the
change is supposed to protect, while exposing remaining fidelity limitations.

### 17. Fingerprint regression

[Source](../.agents/skills/inet-fingerprint-regression/SKILL.md)

**Purpose:** This skill treats a fingerprint mismatch as evidence that the simulation
trajectory changed. It identifies the first mismatch, checks event ordering,
packet contents, timing, random streams, topology, recording, and build
artifacts, and requires explanation plus explicit user approval before updating
fingerprint CSV files.

**Benefit: protection against false alarms and silent acceptance.** It
prevents fingerprints from being updated merely to make a test green, while
also providing a disciplined way to accept an intentional trajectory change.

### 18. IEEE 802.11 walkthrough writer

[Source](../.agents/skills/inet-80211-walkthrough-writer/SKILL.md)

**Purpose:** This skill turns a current example, configuration, and script-generated
evidence into a concise walkthrough. It keeps analysis tables, plots, packet
statistics, and frame timelines owned by the shared analysis machinery,
requires bounded claims and explicit verdicts, and validates the final document.

**Benefit: durable communication.** A walkthrough becomes a reproducible
feature explanation and a useful diagnostic entry point without copying data
into a second, potentially inconsistent presentation.

## Synergies and emergent behavior

The skills are **more valuable in combination** than as a checklist of unrelated
tools. Their interactions create several higher-level properties.

### 1. A layered evidence ladder

The normal escalation path is intentionally economical:

```text
NED/INI → Cmdenv → PCAP/TShark or results → event log → source → LLDB
```

**Configuration** proves what was requested and instantiated. **Logs** show
selected internal decisions. **PCAPs** show protocol-visible reality.
**Results** show aggregates and time series. **Event logs** reconstruct
simulator causality. **Source inspection** explains policy. **LLDB** exposes
the live invalid state. Each layer answers a different question, so later
evidence supplements earlier evidence instead of replacing it.

**Emergent benefit: efficient causal debugging.** The investigation stops as
soon as the first divergence is proven, but can continue to instruction-level
state when necessary.

### 2. Standards-to-runtime traceability

The standards skill supplies the **normative rule**; NED/INI analysis establishes
whether the relevant feature is **instantiated and enabled**; source inspection
establishes the **implemented policy**; a run, capture, event log, or result
shows **what happened**. The four together create a trace from “the standard
says” to “this model did.”

**Result:** This prevents two opposite errors: demanding behavior that the scenario did
not enable, and declaring conformance because a feature exists in the standard
or appears in a configuration file.

### 3. Safe change flow

For a production change, **architecture and sealing rules** constrain the
change surface; **build-mode rules** ensure fresh and matching artifacts;
**unit tests and focused regression tests** exercise the intended contract;
**fingerprint analysis** explains unexpected trajectory changes; the
orchestration and review roles gate implementation and conclusion.

**Emergent behavior: a controlled change loop.** This is more than “edit, run,
and hope.”
The loop preserves architectural intent, executable freshness, targeted
coverage, and an explanation for changed simulation behavior.

### 4. Three views of a packet

**Packet-tag debugging** covers the internal representation, **PCAP/TShark**
covers the serialized protocol view, and **event logs** cover ownership and
movement through the simulator. **Cmdenv logs** add the module’s decision
context. Together they can
distinguish “the bytes were wrong,” “the bytes were right but metadata was
missing,” “the packet was never scheduled,” and “the packet was observed but
failed later.”

This is especially powerful for IEEE 802.11, where management state, MAC
queues, frame exchange, PHY construction, channel effects, receiver decoding,
and upper-layer delivery form a long chain of possible divergence.

### 5. Evidence becomes a publication contract

**Result analysis** defines what was measured; **plotting** defines valid
reduction and uncertainty; **walkthrough writing** defines how the claim is
presented and validated; **regression testing** defines what must remain true.
This turns an
experiment from a directory of outputs into a documented, repeatable claim
with known limits.

**Benefit: organizational memory.** Later users can reproduce the run,
understand what the figure means, and know the first diagnostic to use if the
claim fails.

### 6. Specialization without fragmentation

The orchestration skill makes specialization productive by assigning **narrow
questions** and requiring a **common return shape**. The other skills provide the
shared vocabulary: exact paths, run/seed, build mode, evidence type, command,
artifact, status, uncertainty, and next handoff.

**Emergent result: parallel work that can be synthesized** instead of a set
of disconnected opinions. It also gives the root thread an explicit stopping
rule: stop when decisive evidence answers the question, not when every possible
tool has been used.

## Practical combinations

| Question or task | Recommended combination |
| --- | --- |
| **“Is this 802.11 behavior required?”** | Standards → NED/INI → source inspection; add Wi-Fi packet debugging for an exchange. |
| **“Why was this frame not delivered?”** | Simulation run → NED/INI → Cmdenv logs → sender/receiver PCAP → results/event log → LLDB if state remains unresolved. |
| **“Did my packet lose its metadata?”** | Packet-tag debugging → targeted logs/event log → LLDB; add PCAP only for serialized consequences. |
| **“Can I safely change `src/inet`?”** | Architectural requirements → build/debug modes → unit tests → focused 802.11 regression → fingerprint analysis if needed → reviewer/orchestration gates. |
| **“What does this experiment prove?”** | NED/INI → simulation run → result analysis → result plotting → walkthrough writer. |
| **“Why did the fingerprint change?”** | Build/debug modes → fingerprint regression → logs, event log, PCAP, or results according to the first mismatch. |
| **“Should this Wi-Fi fix be accepted?”** | Standards and architecture for constraints; packet/debug evidence for mechanism; regression and unit tests for coverage; results/walkthrough for externally visible claims. |

## What the system does not guarantee

*The skills improve reasoning; they do not make an invalid configuration valid,*
make a capture complete, or turn a model result into a real-world guarantee.
They also do not eliminate the need to state scope, seed, run, build mode,
capture point, units, time window, and limitations. A conclusion remains only
as strong as the evidence supporting the particular claim.

The most important discipline shared by all of them is therefore **provenance**:
preserve the exact command, working directory, configuration, run and seed,
build artifacts, evidence paths, validation status, and the boundary between
direct observation and inference.
