# INET agent orchestration: roles, synergies, and emergent behavior

## Purpose

The [INET agent orchestration skill](../.agents/skills/inet-agent-orchestration/SKILL.md)
defines how a root agent coordinates specialist agents for nontrivial
OMNeT++/INET and IEEE 802.11 work. It is a coordination system, not merely a
list of available personas. Its purpose is to preserve correctness while
splitting a difficult task into bounded, evidence-producing assignments.

The root agent remains responsible for requirements, decomposition, routing,
handoff decisions, conflict resolution, and final synthesis. Specialist agents
investigate one well-defined question and return distilled conclusions rather
than an unfiltered exploration transcript.

The skill is especially useful when a task crosses several boundaries:

- IEEE 802.11 meaning and INET implementation;
- NED/INI configuration and instantiated runtime behavior;
- source changes and generated/build artifacts;
- packet, event, log, result, and debugger evidence; or
- implementation, regression testing, and independent review.

For a simple lookup, one-command check, or obvious one-file edit, the
coordination overhead can exceed the benefit. Orchestration is intended for
work where independent evidence lanes or specialist judgment materially reduce
risk or elapsed time.

## The operating model

The orchestration loop is:

```text
root requirements and decomposition
              ↓
       bounded specialist lanes
              ↓
   evidence, artifacts, uncertainty
              ↓
       root synthesis and gate
              ↓
     next lane or final conclusion
```

The system has five structural rules:

1. **One level of delegation.** Specialists do not spawn children. This keeps
   ownership, context, and accountability visible in the root thread.
2. **One production-code writer.** An `inet-implementer` may own a named
   production patch; other agents inspect, diagnose, test, analyze, or review
   without editing the same production surface concurrently.
3. **Judgment-based routing.** The tier is selected by the ambiguity and risk
   of the decision, not by the number of files or the length of the response.
4. **Evidence before interpretation.** A specialist must distinguish direct
   observation from inference and return the paths, commands, statuses, and
   limitations that support the conclusion.
5. **Stop when decisive evidence exists.** The root agent does not keep
   spawning lanes simply to make the process look exhaustive.

## Judgment tiers

The tiers describe the kind of reasoning an assignment requires.

| Tier | Best suited for | Deliberately not suited for |
| --- | --- | --- |
| **Sol** | Ambiguous 802.11 semantics, difficult runtime causality, risky production implementation, and final correctness review | Mechanical inventories and bulk extraction |
| **Terra** | Architecture/configuration tracing, established build and test workflows, deterministic regression, and result analysis | Resolving genuinely ambiguous normative or causal questions alone |
| **Luna** | Exact searches, artifact inventories, fixed-filter extraction, and structured fact summaries | Causal explanations, standards interpretation, fix design, statistical judgment, or approval decisions |

The skill maps these tiers to the available runtime and model bindings. If a
binding is unavailable, work moves upward in capability rather than silently
downgrading a judgment-critical assignment. The actual runtime and effort
used should be reported when it differs from the intended tier.

## Specialist roles

### `inet-navigator` — the repository cartographer

**Tier:** Terra  
**Scope:** Read-only.

The navigator maps ownership and relationships before a change or diagnosis:
C++/NED/MSG dependencies, NED inheritance, INI inheritance and wildcard
precedence, `typename` selection, generated-message inputs, feature gates, and
architecture-aware change surfaces.

**Individual benefit:** It replaces guesses about “where the behavior lives”
with a compact map of files, symbols, module paths, effective configuration,
and likely risks. For changes under `src/inet`, it also prepares the relevant
architecture and sealing information before implementation begins.

**Best handoff:** A pre-change map for an implementer, a configuration map for
a simulation detective, or a dependency map for a reviewer. It does not issue
the final architecture verdict and does not infer runtime behavior from static
structure alone.

### `inet-evidence-miner` — the exact extractor

**Tier:** Luna  
**Scope:** Read-only and mechanically bounded.

The evidence miner searches source and existing artifacts, or applies an exact
filter to logs, PCAPs, event logs, scalars, or vectors. Its assignment must
specify the paths, patterns, fields, modules, time windows, run IDs, and
output schema.

**Individual benefit:** It removes repetitive extraction work from higher-tier
agents and returns counts, identifiers, matching records, and missing data
without smuggling in an unsupported diagnosis.

**Best handoff:** Facts for a detective, results analyst, navigator, or root
agent. The miner must label observations that require higher-tier
interpretation.

### `inet-wifi-specialist` — the protocol and model-fidelity authority

**Tier:** Sol  
**Scope:** Read-only.

The Wi-Fi specialist handles IEEE 802.11 frame exchanges, HE/EHT behavior,
association, retries, aggregation, interference, channel access, PHY
reception, and normative-versus-implemented comparisons.

**Individual benefit:** It prevents a protocol requirement from being confused
with an INET feature, a configured feature with an enabled feature, or a
missing frame with a proven collision. It locates the first divergent layer
and connects the applicable standard evidence, INET source, configuration, and
focused regression invariants.

**Best handoff:** A standards-to-source discrepancy, a MAC/PHY interpretation,
or Wi-Fi-specific constraints for an implementer, detective, regression guard,
or reviewer. Normative claims require the applicable IEEE revision and clause;
architecture identifiers such as `AR-WLAN-*` are model constraints, not IEEE
evidence.

### `inet-simulation-detective` — the runtime investigator

**Tier:** Sol  
**Scope:** May run simulations and create named diagnostic artifacts; never
edits production source.

The detective investigates runtime divergence, packet or timing mysteries,
crashes, hangs, module decisions, captures, event causality, and LLDB-level
state. It starts with one configuration and one run, then escalates from
effective configuration and Cmdenv logs through PCAP, results, event logs, and
the debugger only as needed.

**Individual benefit:** It turns “the simulation behaves strangely” into a
first demonstrated divergence, a causal timeline, and a narrow change surface
without patching a merely suspected defect.

**Best handoff:** A proven failure mechanism and bounded implementation target
for `inet-implementer`, or a precise next experiment when the evidence is not
yet decisive.

### `inet-implementer` — the focused change owner

**Tier:** Sol  
**Scope:** Owns only the named files and permitted tests in its assignment.

The implementer makes a small, coherent production C++/NED/MSG change after
the behavior and change surface are established. It keeps interfaces,
callers, generated code, feature declarations, and tests consistent, and
follows architecture, naming, sealing, build-mode, and fingerprint rules.

**Individual benefit:** It gives implementation a single accountable owner,
reducing merge ambiguity and preventing several agents from editing an
unstable production surface at once.

**Best handoff:** A stable diff with an exact behavior claim, build/test
requirements, and known residual risks for the regression guard and reviewer.

### `inet-regression-guard` — the repeatability and coverage owner

**Tier:** Terra  
**Scope:** May add or refine narrowly scoped tests when explicitly assigned;
never changes production source.

The regression guard designs deterministic checks, reproduces failures,
compares before and after behavior, and assesses whether tests actually cover
the intended contract. For Wi-Fi changes it considers focused, boundary,
capability, role, traffic, aggregation, multi-user, determinism, and
unaffected legacy-mode coverage as applicable.

**Individual benefit:** It turns “the patch passes” into a more precise claim:
which behavior is protected, under which configuration and seed, and what
remains untested. It also prevents a fingerprint from being updated merely to
make a test green.

**Best handoff:** Exact commands and statuses, artifacts, first failures,
coverage gaps, and a pass/fail recommendation for the root agent or reviewer.

### `inet-results-analyst` — the measurement authority

**Tier:** Terra  
**Scope:** May create analysis scripts and figures without overwriting raw or
existing analysis artifacts.

The results analyst queries `.sca` and `.vec` data, checks result semantics,
selects valid aggregation and uncertainty methods, compares runs, and creates
deterministic plots when requested.

**Individual benefit:** It protects against treating vector samples as
independent repetitions, mixing incompatible runs, ignoring units or warm-up,
or presenting a visually persuasive but statistically invalid result.

**Best handoff:** A reproducible measurement with inputs, filters, run IDs,
assumptions, validation counts, figures or scripts, and remaining ambiguity.

### `inet-reviewer` — the independent correctness and compliance gate

**Tier:** Sol  
**Scope:** Read-only.

The reviewer examines the implementation in context: declarations and callers,
NED/INI wiring, generated code, feature gates, packet ownership, build mode,
tests, runtime evidence, architecture requirements, naming, and sealing. For
802.11 production changes it applies both the general and WLAN-specific
semantic checklists and reports their required footers.

**Individual benefit:** It supplies an independent challenge to the
implementer's assumptions and catches correctness, compatibility, missing-test,
architecture, evidence, and sealing problems before conclusion.

**Best handoff:** Concrete findings with severity, file/line evidence, failure
mechanism, smallest verification, checklist results, unresolved approvals,
and a compliance verdict. It proposes fixes but does not edit the worktree.

## Aggregate purpose and benefits

Individually, the roles answer different questions. Together, they form a
controlled evidence pipeline:

```text
meaning and constraints
  standards · architecture · navigator · Wi-Fi specialist
                    ↓
configuration and mechanism
  evidence miner · simulation detective
                    ↓
implementation
  one implementer with a bounded change surface
                    ↓
verification and measurement
  regression guard · results analyst
                    ↓
independent acceptance
  reviewer · root synthesis
```

The aggregate benefits are:

- **Correct decomposition:** distinct questions go to agents with the right
  judgment level instead of one generalist being asked to do everything.
- **Evidence composability:** each return has paths, commands, statuses,
  uncertainty, and a next handoff, so facts can be combined without losing
  provenance.
- **Reduced duplicated work:** the root assigns one lane per question and
  reuses a specialist for related follow-ups when its context is valuable.
- **Safer changes:** diagnosis precedes implementation, implementation precedes
  verification, and architecture-sensitive work receives an independent audit.
- **Efficient escalation:** the team uses the cheapest evidence that can prove
  the claim, escalating only when the first layer does not explain the
  divergence.
- **Bounded accountability:** ownership and permissions are explicit, while
  the root retains responsibility for the final claim.

## Synergies and emergent behavior

The most important properties are not held by any single role. They emerge
from the constraints on how roles interact.

### 1. Parallel specialization becomes synthesizable

The navigator can map the source and configuration while the Wi-Fi specialist
checks protocol meaning, or while an evidence miner extracts exact records.
Because assignments exclude interpretation outside their scope and require a
common return shape, parallel results remain comparable. The root can combine
them instead of receiving several broad, overlapping essays.

### 2. The system separates “should,” “is configured,” and “did happen”

The Wi-Fi specialist establishes what the standard requires. The navigator
and configuration analysis establish what the model instantiates. The detective
and evidence lanes establish what the simulator actually did. This separation
prevents a standard requirement from being treated as proof of implementation,
and prevents a parameter or source path from being treated as proof of runtime
behavior.

### 3. Evidence forms a causal ladder

The preferred evidence order is:

```text
runtime/debugger observation
  → packet, event-log, and recorded-result evidence
  → effective NED/INI configuration
  → checked-out source behavior
  → hypothesis
```

This ordering creates a useful emergent stopping rule. The investigation stops
when the first decisive divergence is demonstrated, while still allowing the
detective to descend to event logs or LLDB when higher-level evidence cannot
distinguish competing causes.

### 4. Handoffs act as proof obligations

The gates make each stage prove something specific:

| Transition | Required proof |
| --- | --- |
| Diagnose → implement | Demonstrated mechanism, bounded change surface, architecture/sealing decision, applicable requirements, and required permissions |
| Implement → verify | Stable diff and exact claimed behavior |
| Verify → conclude | Tests that exercise the claim, with applicable boundary and legacy coverage |
| Architecture-sensitive change → conclude | Architecture check reconciled with ledgers and complete required review checklists |
| Fingerprint update → conclude | Explained trajectory change and explicit user approval |

The emergent effect is that “done” means more than “a command returned zero.”
It means the evidence satisfies the next decision's prerequisites.

### 5. Independent review is structurally different from more implementation

The implementer optimizes for a coherent patch; the regression guard optimizes
for repeatability; the reviewer searches for what all of them may have missed.
Keeping the reviewer read-only and post-implementation makes disagreement
visible rather than allowing it to be silently resolved by editing the code.

### 6. Specialization produces a shared project memory

The roles use a common vocabulary: exact paths, symbols, module paths,
configuration, run and seed, build mode, command, artifact, status, evidence
strength, uncertainty, and next handoff. Over time, this makes reports and
diagnoses easier to reproduce even when the particular specialist or runtime
changes.

## Typical coordinated workflows

### A Wi-Fi behavior discrepancy

1. `inet-wifi-specialist` establishes the relevant IEEE behavior and identifies
   the first layer that must be checked.
2. `inet-navigator` resolves the instantiated modules, feature gates, and
   effective configuration.
3. `inet-simulation-detective` reproduces one run and gathers the minimum
   evidence needed to demonstrate the divergence.
4. If a source fix is justified, the root gates a single `inet-implementer`.
5. `inet-regression-guard` verifies the focused behavior and relevant legacy
   coverage.
6. `inet-reviewer` independently checks correctness, architecture, evidence,
   and WLAN review requirements.

### A production change

The pattern is navigator or detective first, implementer second, regression
guard and reviewer afterward. For changes in either IEEE 802.11 production
subtree, the architecture overlay, applicable `AR-WLAN-*` requirements, IEEE
revision/clause, and both review checklists become part of the gate rather than
optional documentation.

### A results or plotting task

`inet-results-analyst` owns semantic interpretation and plotting. An
`inet-evidence-miner` may first inventory run IDs, metric names, modules, units,
and recording attributes, but it does not choose statistical methods or
approve the result. This separation keeps extraction mechanical and analysis
meaningful.

## What orchestration does not guarantee

Orchestration improves the reliability of reasoning; it does not make an
invalid configuration valid, make an incomplete capture complete, or turn a
simulation result into a real-world guarantee. It also cannot resolve missing
authority by averaging opinions. When evidence conflicts, the root should
report the disagreement or missing evidence and route the next focused check.

The final claim remains bounded by its provenance: configuration, build mode,
run and seed, capture point, time window, units, artifacts, validation status,
and the explicit boundary between observation and inference.

## Source of truth

This guide explains the design and practical meaning of the orchestration
system. The live rules, tier bindings, agent contracts, evidence lanes, and
handoff gates remain defined by
[`.agents/skills/inet-agent-orchestration/SKILL.md`](../.agents/skills/inet-agent-orchestration/SKILL.md)
and its role files under
`.agents/skills/inet-agent-orchestration/agents/`.
