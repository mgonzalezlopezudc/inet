# INET Architectural Requirements: Rules, Synergies, and Emergent Behavior

## Purpose

The [`inet-architectural-requirements` skill](../.agents/skills/inet-architectural-requirements/SKILL.md)
is INET's architecture-and-change-control system. It translates broad goals—composability,
fidelity, extensibility, reproducibility, and maintainability—into named requirements, review
questions, enforcement checks, exception ledgers, and a sealing policy.

The skill answers a different question from the user-facing requirements in
[`requirements.md`](../.agents/skills/inet-architectural-requirements/references/requirements.md):

- The `R-*` requirements describe what INET should let a simulation user do: model, compose, run,
  analyze, visualize, emulate, learn from, distribute, and reproduce simulations.
- The `AR-*` requirements describe how the framework must be structured so those user-facing
  capabilities remain possible as the code base grows.

The important idea is that these rules are not independent style preferences. They form a network
of mutually reinforcing constraints. A typed contract enables a replaceable module; registration
and dispatch make that module discoverable; packet chunks, tags, serializers, and signals make its
behavior interoperable and observable; deterministic tests and build rules make the result
repeatable. The aggregate outcome is a framework in which local changes are easier to make, easier
to inspect, and less likely to damage unrelated behavior.

## What the skill does when a change is proposed

For a design, implementation, refactor, audit, naming review, or sealing request, the skill applies
the following control loop:

1. Establish the scope and affected paths.
2. Resolve the sealing status before changing anything under `src/inet/`.
3. Map the work to the applicable `R-*`, `AR-*`, naming, and—where relevant—`AR-WLAN-*` rules.
4. Inspect the actual C++, NED, MSG, configuration, registration, build, and test artifacts.
5. Check the architecture and naming exception ledgers so existing sanctioned deviations are not
   reported as new findings.
6. Prefer the smallest change that satisfies the contracts, ownership, observability,
   configuration, determinism, and testing requirements.
7. Validate in proportion to the risk and report commands, statuses, artifacts, findings, and
   unresolved approvals.

The skill therefore changes the unit of review from “does this patch look reasonable?” to “which
architectural contracts does this patch touch, what evidence establishes compliance, and what will
prevent a regression later?”

## The individual general requirements

Each requirement below has two parts: its architectural purpose and the benefit it provides on its
own. The final sections explain why the benefits become substantially stronger when the requirements
are applied together.

### Code organization — `AR-ORG`

- **`AR-ORG-DOMAINS` — layered, domain-partitioned source tree with acyclic dependencies.**
  Purpose: keep protocol and functional domains discoverable and make dependencies point toward
  shared infrastructure rather than back upward into protocols. Benefit: contributors can locate
  code and reason about a subsystem without loading the entire framework, while unrelated layers do
  not become accidental dependencies.
- **`AR-ORG-CONTRACTS` — every extensible role has a separate C++ and NED contract.** Purpose:
  separate the substitutable interface, reusable base machinery, and concrete implementation.
  Benefit: a conforming implementation can replace another implementation with a known gate and API
  shape, without coupling consumers to a concrete class.
- **`AR-ORG-VIS-SPLIT` — model logic, visualization, and instrumentation are separate.** Purpose:
  keep protocol code concerned only with protocol behavior and expose observations outward.
  Benefit: visualization and recording can be enabled, disabled, or replaced without changing the
  simulated behavior, preserving observation neutrality.
- **`AR-ORG-KERNEL` — use OMNeT++ kernel facilities instead of reimplementing them in INET.**
  Purpose: keep the boundary between the general simulator kernel and the network model clean.
  Benefit: scheduling, RNG, lifecycle, and debugging semantics have one authoritative
  implementation instead of diverging INET-specific copies.

### Module design — `AR-MOD`

- **`AR-MOD-COMPOSITION` — compose small single-purpose modules; keep structure in compound modules.**
  Purpose: make the module graph, rather than a deep class hierarchy, the main unit of reuse.
  Benefit: behavior is independently testable and can usually be changed by rewiring or replacing a
  part instead of growing a multipurpose “god module.”
- **`AR-MOD-PLUGGABLE` — use interface-typed, defaulted, optionally omitted submodules.** Purpose:
  make substitution and optionality explicit in NED and configuration. Benefit: one node type can
  support different radios, queues, clocks, energy models, or protocol stacks without source edits.
- **`AR-MOD-FIDELITY` — expose multiple configuration-selectable levels of detail.** Purpose: treat
  accuracy versus computational cost as a user-selected modeling dimension. Benefit: a study can
  use a coarse model for scale or a detailed model for physical analysis while preserving the same
  surrounding composition contract.
- **`AR-MOD-NODEBASE` — assemble nodes from shared per-layer bases and locate services by lookup.**
  Purpose: reuse common node scaffolding without hardcoding collaborator paths. Benefit: protocol
  modules survive changes in node composition, interface count, and internal layout.

### Packet representation — `AR-PKT`

- **`AR-PKT-CHUNKS` — represent packet content as typed, immutable, shared chunks and views.**
  Purpose: make packet structure first-class while avoiding unnecessary copies or destructive
  mutation. Benefit: encapsulation, buffering, duplication, logging, and reinspection remain cheap
  and safe across multiple consumers.
- **`AR-PKT-DUAL` — provide both field-based and raw-byte header forms through serializers.**
  Purpose: separate convenient model-level fields from exact wire representation. Benefit: the same
  packet model supports readable simulation logic, real-byte interoperability, PCAP exchange,
  emulation, and content-aware regression fingerprints.
- **`AR-PKT-TAGS` — carry local metadata in tags, never in wire content, and strip it at transmission.**
  Purpose: distinguish in-node service information from information actually transmitted over the
  network. Benefit: models can communicate requests and indications without accidentally giving a
  receiver knowledge that could not have crossed the medium.
- **`AR-PKT-ERRORS` — represent transmission errors at selectable fidelity levels.** Purpose: allow
  whole-packet, chunk-level, or bit-level corruption models. Benefit: a MAC study need not pay for
  bit-level detail, while a PHY study can model partial corruption and interference when necessary.
- **`AR-PKT-SIGNAL` — represent physical transmissions as immutable `Signal` objects distinct from packets.**
  Purpose: separate digital content from the physical phenomenon carrying it and make each PHY-stage
  result auditable. Benefit: shared-medium calculations remain consistent, physical processing can
  be composed, and later stages cannot silently mutate earlier transmission facts.

### Protocol interaction — `AR-COM`

- **`AR-COM-REGISTRY` — register protocols and services rather than hardcoding them into wiring.**
  Purpose: make protocol identity and capability discoverable at initialization. Benefit: dispatch
  and extension work through registration points instead of central switches or gate-number
  assumptions.
- **`AR-COM-DISPATCH` — address peers by protocol and service, not topology.** Purpose: route by
  semantic identity using dispatchers and packet tags. Benefit: protocol components can be inserted,
  removed, or rearranged without rewiring every neighbor.
- **`AR-COM-SOCKETS` — expose transport use through socket-style callback APIs.** Purpose: give
  applications a stable bind/connect/send/close interface instead of duplicating raw command and
  indication handling. Benefit: applications are simpler and generic socket infrastructure can
  manage different transports uniformly.
- **`AR-COM-DIRECT` — use typed direct C++ calls for same-instant, same-node coordination.** Purpose:
  reserve simulation messages and events for communication that actually has temporal or topological
  meaning. Benefit: causality is clearer, event counts are lower, and fingerprints reflect model
  behavior instead of internal zero-time plumbing.

### Initialization and lifecycle — `AR-LIFE`

- **`AR-LIFE-STAGES` — use one global, documented, multi-stage initialization order.** Purpose:
  give independently authored modules a shared contract for setup dependencies such as interfaces,
  addresses, routes, and applications. Benefit: new modules fit into existing bring-up semantics
  instead of inventing fragile local ordering schemes.
- **`AR-LIFE-OPERATIONS` — use a common, scriptable lifecycle protocol for shutdown, restart, and crash.**
  Purpose: model failure and recovery as explicit operations with module opt-in and defined semantics.
  Benefit: resilience, power-failure, reboot, and graceful-versus-crash studies become reproducible
  scenarios rather than ad-hoc message hacks.

### Composable packet processing — `AR-QUEUE`

- **`AR-QUEUE-ROLES` — use standard push/pull source and sink contracts.** Purpose: give queues,
  filters, schedulers, shapers, and classifiers a common processing algebra. Benefit: datapaths can
  be assembled in arbitrary useful chains from reusable components.
- **`AR-QUEUE-STREAMING` — support progressive transfer such as preemption and cut-through.**
  Purpose: make partial transfer a property of the processing contract, not a special case in one
  protocol. Benefit: modern link behavior can be expressed using the same composition mechanisms as
  ordinary packet processing.

### Observability — `AR-OBS`

- **`AR-OBS-SIGNALS` — expose behavior through declared signals to external subscribers.** Purpose:
  separate producing protocol behavior from recording and visualization consumers. Benefit: new
  statistics, traces, and visualizers can be added without editing or perturbing the model.
- **`AR-OBS-NED-TRUTH` — make NED the single source of truth for the external module interface.**
  Purpose: keep parameters, gates, signals, and statistics machine-readable in one authoritative
  declaration. Benefit: generated references, IDEs, configuration, and documentation consume the
  same current contract instead of drifting prose copies.
- **`AR-OBS-INTROSPECTION` — register dissection, printing, and filtering support with each protocol.**
  Purpose: make generic packet tooling protocol-aware without a central protocol-specific switch.
  Benefit: new packet types become inspectable, printable, serializable, and filterable as part of
  the same contribution.
- **`AR-OBS-FLOWS` — preserve end-to-end flow membership with region tags.** Purpose: let flow
  identity follow data through fragmentation, aggregation, reordering, and re-encapsulation.
  Benefit: latency, queueing, and end-to-end measurements can cross module boundaries without every
  protocol implementing bespoke cooperation.

### Configuration and parameterization — `AR-CFG`

- **`AR-CFG-INFER` — infer derivable structure and keep configuration DRY.** Purpose: define facts
  once and calculate or propagate consequences. Benefit: fewer contradictory settings and a smaller,
  more understandable configuration surface.
- **`AR-CFG-PARAMS` — use typed, unit-annotated, defaulted, single-meaning parameters.** Purpose:
  make configuration values dimensionally checkable and unambiguous, separating user overrides from
  resolved values. Benefit: invalid units, missing defaults, and overloaded parameter meanings fail
  earlier or become obvious instead of producing silent model changes.

### Extensibility — `AR-EXT`

- **`AR-EXT-NOCORE` — add protocols through existing contracts and registration points.** Purpose:
  keep the core closed to protocol-specific dependencies while remaining open to new protocols.
  Benefit: extensions have a bounded change surface and do not make unrelated core code know about
  every optional feature.
- **`AR-EXT-ATTACH` — attach protocol-specific data to shared core structures.** Purpose: extend
  common objects without editing them to depend on higher-layer protocols. Benefit: optional
  functionality can be compiled out and the core remains reusable and dependency-clean.
- **`AR-EXT-FEATURES` — partition optional functionality into independently disableable features.**
  Purpose: declare feature dependencies and isolate optional symbols. Benefit: users and CI can
  build focused subsets, and the framework avoids becoming a monolithic must-build-everything unit.

### Build and project structure — `AR-BUILD`

- **`AR-BUILD-OUTOFTREE` — keep generated and build artifacts relocatable and out of the source tree.**
  Purpose: make a build a function of pinned source plus configuration, with isolated outputs.
  Benefit: debug/release variants, CI jobs, worktrees, and toolchain combinations do not clobber one
  another or accidentally reuse stale generated code.
- **`AR-BUILD-DECLARATIVE` — describe build configuration in one authoritative, introspectable source.**
  Purpose: eliminate duplicated and machine-specific build facts. Benefit: IDEs, CI, and external
  drivers can discover the same feature, toolchain, flag, and output configuration.

### Quality and conventions — `AR-QUAL`

- **`AR-QUAL-FINGERPRINT` — guard behavior with simulation-trajectory fingerprints.** Purpose: detect
  changes in event, timing, module, and packet behavior, with intentional baseline changes separated
  for review. Benefit: subtle regressions become visible even when ordinary assertions still pass.
- **`AR-QUAL-TESTS` — ship tests matching the nature of the contribution.** Purpose: use unit,
  module-behavior, statistical, and validation tests where each is appropriate, rather than relying
  on fingerprints alone. Benefit: the suite tests correctness and not merely that behavior changed.
- **`AR-QUAL-DETERMINISM` — make identical inputs produce identical results.** Purpose: remove
  dependence on memory layout, unordered iteration, allocation order, thread timing, or implicit
  RNG behavior. Benefit: simulation science, fingerprints, debugging, and parallel execution become
  trustworthy.
- **`AR-QUAL-NAMING` — make roles legible through framework-wide names.** Purpose: encode categories
  such as interface, base, table, header, tag, serializer, and statistic in names. Benefit: humans,
  search tools, and reviewers can infer how an artifact participates in the architecture.
- **`AR-QUAL-DISPLAY` — maintain complete, distinguishing visual conventions.** Purpose: give module
  categories a consistent icon vocabulary. Benefit: graphical composition and visual model review
  become more informative and less dependent on opening every module definition.
- **`AR-QUAL-LOGGING` — standardize log levels and throw on programming errors.** Purpose: distinguish
  public behavior, internal detail, diagnostics, and violated invariants. Benefit: logs remain
  comparable and useful while impossible states fail loudly instead of being hidden as warnings.
- **`AR-QUAL-TRACEABILITY` — map tests to model structure and baseline changes to causes.** Purpose:
  make the blast radius of a source change and the provenance of a changed expectation discoverable.
  Benefit: contributors can select focused validation and reviewers can understand why a baseline
  moved.
- **`AR-QUAL-ENFORCED` — machine-check every rule that can be mechanically checked.** Purpose: move
  architecture from advice into compiler, test, lint, architecture-script, or agent-review gates.
  Benefit: the desired design becomes the path of least resistance and violations fail near their
  introduction.

## IEEE 802.11 extensions

The IEEE 802.11 requirements apply in addition to all general INET requirements under
`src/inet/linklayer/ieee80211/` and `src/inet/physicallayer/wireless/ieee80211/`. They specialize
the general architecture where Wi-Fi's standards, timing, shared-medium behavior, and many PHY/MAC
variants create extra risk.

### Standard semantics and component boundaries

- **`AR-WLAN-STD-TRACE` — trace normative behavior to an IEEE revision and clause.** Purpose: anchor
  state transitions, fields, timing, and validity rules in the standard. Benefit: implementation
  decisions are defensible and are not accidentally invented from intuition or another simulator.
- **`AR-WLAN-STD-GATING` — gate amendment-specific behavior by mode and capabilities.** Purpose:
  ensure HE/EHT or other newer behavior runs only when local and peer capabilities allow it. Benefit:
  legacy operation remains stable when newer code is present.
- **`AR-WLAN-ARCH-BOUNDARIES` — keep MAC, management, rate selection, and PHY responsibilities distinct.**
  Purpose: give each layer ownership of its own decision space and communicate through typed
  contracts and immutable results. Benefit: changes in one responsibility do not require downcasts
  into another component's implementation state.
- **`AR-WLAN-ARCH-OWNERSHIP` — give every mutable protocol state exactly one owner.** Purpose: make
  association, sequence, retry, NAV, backoff, TXOP, Block Ack, aggregation, power-save, and PHY
  selection state authoritative in one place. Benefit: stale synchronized copies cannot disagree and
  recovery paths become explainable.
- **`AR-WLAN-ARCH-VARIANTS` — isolate substantial variations behind replaceable policies.** Purpose:
  keep amendment, role, and algorithm differences out of scattered conditionals. Benefit: new
  variants are localized, testable, and less likely to change legacy behavior accidentally.

### Frames, PHY modes, and timing

- **`AR-WLAN-FRAME-REPRESENTATION` — represent every on-air field once as typed packet content.**
  Purpose: distinguish wire information from local tags and centralize classification, address,
  sequence, TID, serialization, dissection, and printing. Benefit: sender, receiver, PHY, PCAP, and
  tests operate on the same authoritative frame representation; generated message code remains
  generated rather than hand-edited.
- **`AR-WLAN-PHY-AUTHORITY` — make PHY mode objects authoritative for legality, rate, and duration.**
  Purpose: centralize validation and calculation for width, MCS, NSS, RU, guard interval, preamble,
  symbol count, rate, and PPDU duration. Benefit: MAC and rate selection cannot silently diverge
  from PHY timing or accept invalid combinations.
- **`AR-WLAN-PHY-TIMING` — derive protocol timing centrally and with units.** Purpose: calculate
  SIFS, slots, interframe spaces, timeouts, NAV, and transmission durations from standard inputs and
  the selected mode. Benefit: boundary behavior is consistent and numeric timing constants do not
  drift across MAC, management, and PHY code.

### MAC operation

- **`AR-WLAN-MAC-EXCHANGE` — give each frame exchange one explicit state machine.** Purpose: make
  transmission, expected response, timeout, retry, completion, timer ownership, and deterministic
  event priority explicit. Benefit: ACK/CTS/Block Ack and management responses cannot acquire
  partially duplicated decisions in unrelated modules.
- **`AR-WLAN-MAC-SEQUENCE` — use shared modulo sequence, aggregation, and Block Ack rules.** Purpose:
  centralize wrap-around comparison, identity retention, window advancement, and ownership. Benefit:
  retransmissions, aggregation, and reordering preserve protocol identity at boundary values.
- **`AR-WLAN-MAC-QOS` — define QoS classification and EDCA state once.** Purpose: map TID/user
  priority to one access category before contention and give each category its own queue and state.
  Benefit: internal collisions, TXOP, retries, and contention are resolved by protocol policy rather
  than incidental queue ordering.
- **`AR-WLAN-MAC-MULTIUSER` — separate MU scheduling from PPDU construction.** Purpose: make the
  scheduler produce a complete immutable, validated transmission plan while PHY turns that plan into
  a PPDU. Benefit: user/resource selection is reproducible and testable without letting PHY internals
  become an accidental scheduling API.

### 802.11 observability and verification

- **`AR-WLAN-OBS-EVENTS` — emit each semantic event once from its owner.** Purpose: make contention,
  attempts, responses, retries, Block Ack changes, aggregation, association, and MU allocation
  observable without duplicating event meaning. Benefit: statistics, traces, and visualizers can
  subscribe to authoritative events rather than reconstructing private state.
- **`AR-WLAN-QUAL-TESTS` — pair normative behavior with focused tests and legacy regressions.** Purpose:
  exercise state, frame content, event timelines, boundary values, malformed inputs, capability
  combinations, roles, traffic types, aggregation, Block Ack, and legacy coexistence. Benefit:
  fingerprints detect unexpected trajectory changes while focused tests establish whether the new
  behavior is correct.

## The supporting governance mechanisms

The skill contains more than the `AR-*` statements. These mechanisms make the requirements usable
in daily development.

### Naming conventions and naming exceptions

The naming reference turns `AR-QUAL-NAMING` into concrete rules for packages, files, NED types,
gates, parameters, signals, statistics, MSG types and fields, C++ identifiers, configurations,
features, directories, tests, and icons. A name such as `I...`, `...Base`, `...Table`, `...Header`,
`...Tag`, or `...Serializer` communicates a role before a contributor opens the file.

The naming-exception ledger records permanent, deliberate deviations as stable `NS-*` entries and
keeps genuine rename candidates as `NV-*` findings. This prevents a legacy exception from being
re-litigated while also preventing “the code already does this” from weakening the rule for new
work.

### Architecture exceptions and violations

The architecture-exception ledger separates sanctioned couplings (`AS-*`) from genuine violations
(`AV-*`). It is a record of existing reality, not a catalog of patterns to copy. The separation is
important: an audit can acknowledge a known exception without turning it into a precedent, and a
new deviation has a durable identifier and disposition instead of disappearing into review prose.

### Enforcement ladder and review checklists

Requirements are enforced at the strongest available tier:

1. **T1:** the compiler or NED toolchain rejects the violation.
2. **T2:** a unit, module, statistical, validation, or fingerprint test fails.
3. **T3:** a deterministic linter, feature check, build matrix, or architecture script flags it.
4. **T4:** an agent reviewer evaluates semantic rules that static tools cannot express.
5. **T5:** a human decides genuine design questions, such as whether a new fidelity level is worth
   its complexity.

The general agent-review checklist checks semantic leakage such as visualization in model code,
zero-time messages used as calls, duplicated NED truth, raw-message applications, missing
introspection, DRY violations, bad parameters, core changes for extensions, hardcoded build values,
and missing tests. The IEEE 802.11 checklist adds standard traceability, ownership, PHY authority,
MAC exchange, sequence rules, MU separation, semantic event ownership, and focused verification.

The goal is not to pretend every design decision is mechanical. It is to ensure that anything that
can be made objective is checked objectively, anything semantic is reviewed systematically, and only
the irreducibly judgment-based choices remain for human design review.

### Architecture checks and sealing

The include-graph fitness function is run from the repository root for architecture-sensitive
changes:

```sh
bash .agents/skills/inet-architectural-requirements/references/enforcement/check-architecture.sh
bash .agents/skills/inet-architectural-requirements/references/enforcement/check-architecture.sh src/inet/<focused-subtree>
```

Its output must be reconciled with the architecture ledger; a known non-allowlisted violation is
not automatically a newly introduced violation, and a nonzero exit is not by itself a diagnosis.

Sealing is the terminal state of the pipeline, not a shortcut around it. A file or recursive
directory is sealed only after a complete audit against architecture, naming, applicable checks, and
review items, with every deviation fixed or explicitly accepted in a ledger. Once sealed, it cannot
be modified by an AI without explicit permission for that exact file in the current conversation.
This turns scarce review attention toward unsettled code while preserving a clear re-audit path for
sealed code that later needs an authorized change.

## Aggregate purpose and benefits

Applied as a system, the requirements create several architectural properties that no single rule
could provide.

### 1. A contract graph instead of a dependency tangle

`AR-ORG-DOMAINS`, `AR-ORG-CONTRACTS`, `AR-MOD-COMPOSITION`, `AR-MOD-PLUGGABLE`, `AR-COM-REGISTRY`,
`AR-COM-DISPATCH`, `AR-EXT-NOCORE`, and `AR-EXT-ATTACH` form a contract graph:

```text
contract → implementation → registration → semantic dispatch
    ↑             ↓                ↓              ↓
 optionality   composition      introspection   alternate topology
```

The combined effect is structural substitutability. A component is not merely “replaceable” because
an interface exists; it is replaceable because the slot is interface-typed, the service is
discoverable, the packet identity is explicit, and the core does not need to know the concrete
implementation. This is what makes adding a protocol an extension rather than a central-core edit.

### 2. A truthful data path from model to wire to evidence

`AR-PKT-CHUNKS`, `AR-PKT-DUAL`, `AR-PKT-TAGS`, `AR-PKT-ERRORS`, `AR-PKT-SIGNAL`,
`AR-OBS-INTROSPECTION`, and `AR-OBS-FLOWS` divide information according to what it is:

- typed content is what the packet carries;
- tags are local service metadata;
- signals are physical transmissions and their immutable results;
- serializers are the wire boundary;
- region tags preserve data identity through transformations;
- dissectors, printers, filters, and flow measurements consume the same representations.

The emergent benefit is evidentiary continuity. A field inspected in a packet, serialized into a
capture, processed by the PHY, and attributed to a flow can be traced through one representation
model without inventing a parallel “debug representation.” It becomes much harder for an analysis
tool to report something that the actual model did not represent.

### 3. Behavior that is composable but not causally vague

`AR-COM-DIRECT`, `AR-LIFE-STAGES`, `AR-LIFE-OPERATIONS`, `AR-QUEUE-ROLES`, and
`AR-QUEUE-STREAMING` make internal cooperation explicit at the right semantic level. Direct calls
represent same-instant procedure-like coordination; scheduled messages represent events; lifecycle
stages represent initialization ordering; queue contracts represent datapath transfer.

The result is not simply fewer lines of code. The event trajectory becomes more meaningful: event
counts, timestamps, timers, and state transitions correspond more closely to modeled behavior rather
than implementation plumbing. That improves debugging, performance, fingerprint signal quality,
and the ability to explain a run causally.

### 4. Observability without observer effects

`AR-ORG-VIS-SPLIT`, `AR-OBS-SIGNALS`, `AR-OBS-NED-TRUTH`, `AR-OBS-INTROSPECTION`,
`AR-OBS-FLOWS`, and `AR-WLAN-OBS-EVENTS` establish a one-way path:

```text
model owner → declared semantic signal/event → recorder, visualizer, analyzer
```

Because consumers subscribe from outside and events are emitted once by the owner, recording is
additive rather than behavioral. NED remains the machine-readable interface, so documentation and
tools can describe what actually exists. Together, these rules turn observability into an external
capability instead of a hidden second implementation of protocol logic.

### 5. Fidelity becomes a controlled dimension

`AR-MOD-FIDELITY`, `AR-PKT-ERRORS`, `AR-PKT-SIGNAL`, `AR-EXT-FEATURES`, and the typed configuration
rules allow a study to choose detail deliberately. A coarse error model and a detailed analog model
can occupy the same conceptual slot; the choice is visible in configuration and bounded by the same
contracts.

The aggregate benefit is scalable experimentation: large scenarios can use affordable abstractions,
while focused studies can increase detail without replacing the entire node architecture. The
tradeoff is explicit rather than hidden in a one-size-fits-all implementation.

### 6. Correctness becomes reproducible rather than anecdotal

`AR-CFG-INFER`, `AR-CFG-PARAMS`, `AR-BUILD-OUTOFTREE`, `AR-BUILD-DECLARATIVE`,
`AR-QUAL-DETERMINISM`, `AR-QUAL-FINGERPRINT`, `AR-QUAL-TESTS`, and `AR-QUAL-TRACEABILITY` form a
reproducibility chain:

```text
unambiguous configuration
        ↓
isolated, discoverable build
        ↓
deterministic execution
        ↓
focused correctness tests + trajectory fingerprints
        ↓
traceable, reviewable baseline
```

Each link removes a different source of uncertainty. A fingerprint is meaningful only when the
model is deterministic; a deterministic run is useful only when the effective configuration and
loaded artifacts are known; a passing regression is more persuasive when the test type matches the
claim and its baseline change has provenance.

### 7. Complexity is paid once in infrastructure, not repeatedly in features

Registries, dispatchers, serializers, signal APIs, lifecycle protocols, queue contracts, feature
descriptors, and generated NED interfaces add framework structure. Their aggregate purpose is to
make the next protocol or model cheaper to integrate. Each new feature can reuse the same extension,
observation, testing, configuration, and build paths instead of creating a bespoke path through the
core.

This is an important emergent property: the architecture has a rising initial discipline cost but a
lower marginal integration cost. Without it, every new protocol appears locally simple while adding
another special case to dispatch, packet inspection, build selection, visualization, and tests.

## IEEE 802.11 synergies and emergent behavior

The WLAN rules sharpen the general system in places where shared-medium timing and capability
variants make hidden coupling especially dangerous.

### One authoritative exchange

`AR-WLAN-ARCH-OWNERSHIP`, `AR-WLAN-MAC-EXCHANGE`, `AR-WLAN-MAC-SEQUENCE`, and
`AR-WLAN-MAC-QOS` combine state ownership, explicit exchange state machines, modulo sequence rules,
and centralized EDCA state. The emergent behavior is transactional MAC logic: staged frames,
retries, acknowledgments, Block Ack windows, and queue outcomes can be attributed to one owner and
one state transition rather than reconstructed from synchronized shadows.

### One authoritative PHY calculation

`AR-WLAN-PHY-AUTHORITY`, `AR-WLAN-PHY-TIMING`, `AR-WLAN-FRAME-REPRESENTATION`, and
`AR-WLAN-MAC-MULTIUSER` make a validated PHY mode and immutable MU plan the handoff between
scheduling and transmission. The scheduler chooses users and resources; the PHY validates and
constructs the PPDU; neither silently duplicates the other's formulas. This produces consistent
rates, durations, legality checks, PCAP fields, and timing evidence.

### New amendments without legacy drift

`AR-WLAN-STD-TRACE`, `AR-WLAN-STD-GATING`, `AR-WLAN-ARCH-VARIANTS`, and `AR-EXT-NOCORE` jointly
favor capability-gated policies registered through existing contracts. The result is a safer path
for HE/EHT and future variations: new behavior can be added without making legacy behavior depend on
new concrete types, paths, or unrelated parameters.

### Semantic evidence for protocol decisions

`AR-WLAN-OBS-EVENTS`, `AR-OBS-SIGNALS`, `AR-QUAL-DETERMINISM`, `AR-QUAL-FINGERPRINT`, and
`AR-WLAN-QUAL-TESTS` make a frame exchange observable at the point of ownership and repeatable at
the trajectory level. This lets a reviewer correlate a standard rule, a state transition, an event,
a frame, a timing boundary, and a regression result instead of treating a missing packet as an
unexplained symptom.

## Practical workflow for contributors

For a normal INET change, use the requirements as a design map:

1. Identify the affected domain, contracts, module composition, packet content, tags, configuration,
   observability, build feature, and tests.
2. If the change is under `src/inet/`, resolve exact and ancestor-directory seals first.
3. Read only the requirement references that apply, plus both exception ledgers and the naming rules
   for every new or renamed artifact.
4. Establish ownership and the smallest change surface before editing. For 802.11, also establish
   the standard clause, capability gate, state owner, PHY authority, and exchange owner.
5. Implement through existing contracts, registries, signals, serializers, lifecycle APIs, and
   feature descriptors where possible.
6. Run focused architecture checks and the tests appropriate to the claim. Preserve the exact
   command, working directory, configuration, run number, mode, exit status, and artifact paths.
7. Review the diff semantically, record only genuinely new deviations in the ledgers, and update a
   fingerprint baseline only with explicit approval and a reviewable explanation.
8. If sealing is requested, perform and report the complete audit before recording the seal.

## Limits and tradeoffs

The skill deliberately makes INET stricter than an ad-hoc simulation script. Contracts, typed
packets, explicit ownership, registration, generated interfaces, focused tests, and reviewable
baselines require up-front design work. That cost buys a model that can be composed, inspected,
reproduced, and extended by people who did not author the original feature.

The requirements also do not eliminate judgment. They cannot decide every useful fidelity level,
every boundary between modules, or whether a legacy exception should eventually be removed. The
ledgers, T4 review, and T5 human review make those decisions visible rather than pretending that
all architecture is reducible to a compiler rule.

## Reference map

- [Skill entry point](../.agents/skills/inet-architectural-requirements/SKILL.md)
- [General architectural requirements](../.agents/skills/inet-architectural-requirements/references/architectural-requirements.md)
- [IEEE 802.11 architectural requirements](../.agents/skills/inet-architectural-requirements/references/ieee80211-architectural-requirements.md)
- [User-facing INET requirements](../.agents/skills/inet-architectural-requirements/references/requirements.md)
- [Naming conventions](../.agents/skills/inet-architectural-requirements/references/naming-conventions.md)
- [Architecture exceptions](../.agents/skills/inet-architectural-requirements/references/architecture-exceptions.md)
- [Naming exceptions](../.agents/skills/inet-architectural-requirements/references/naming-exceptions.md)
- [Sealing policy](../.agents/skills/inet-architectural-requirements/references/sealing.md)
- [General agent-review checklist](../.agents/skills/inet-architectural-requirements/references/enforcement/agent-review-checklist.md)
- [IEEE 802.11 agent-review checklist](../.agents/skills/inet-architectural-requirements/references/enforcement/ieee80211-agent-review-checklist.md)
