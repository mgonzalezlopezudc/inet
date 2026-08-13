# IEEE 802.11 HCF, VHT, and HE redesign implementation plan

## Status and intent

This is a new, standalone implementation plan. It does not continue the stages or
assumptions of the earlier HCF split plan. It starts from the current checkout and
redesigns the whole `Hcf` family, including `VhtHcf` and `HeHcf`, to remove the
god-object inheritance model while preserving supported simulation behavior.

The plan was independently reviewed against the current HCF/VHT/HE/EHT source,
architecture rules, sealing status, NED contracts, and shared DCF callback. The final
review verdict was approve with no unresolved findings. This review validates the
plan's design surface, not runtime behavior; builds and simulations belong to Phase 0.

The desired end state is one small HCF/EDCA execution kernel composed with explicit
HT, VHT, and HE exchange providers. Amendment-specific code owns its own state and
communicates with the kernel through typed snapshots, validated plans, and exchange
events. `Hcf`, `VhtHcf`, and `HeHcf` remain available as NED-facing compatibility
types during the migration, but they no longer expose the kernel through a large
protected API.

This is primarily an architectural refactor. A protocol correction discovered while
implementing it must be isolated in a separate patch with its own IEEE evidence and
regression justification. It must not be hidden inside a move or extraction.

## Executive decisions

1. Replace behavioral inheritance with composition. `VhtHcf` and `HeHcf` become thin
   compatibility adapters around the same HCF kernel and amendment-specific feature
   components.
2. Keep one owner for the active frame exchange and its timers. The current
   `FrameSequenceHandler`, `HcfExchangeCoordinator`, and `HcfResponseService` are
   consolidated behind one typed exchange engine instead of being coordinated by
   callers spread across three HCF variants.
3. Keep EDCA/EDCAF as the owner of per-AC contention, backoff, retry counters, and
   TXOP state. HE per-STA queues feed the winning access category; they do not become
   another contention domain.
4. Separate selection from execution. A scheduler proposes users/resources; a
   validator creates an immutable transmission plan; an exchange provider commits
   packet reservations; the exchange engine runs it; the PHY remains authoritative
   for mode legality and duration.
5. Make packet identity and ownership explicit. Queue reservations, in-progress
   MPDUs, temporary aggregates, triggered-UL responses, retry reinsertion, and
   association-epoch retirement each have one documented owner and one terminal path.
6. Preserve the external `IHcf` NED slot, the `Hcf`/`VhtHcf`/`HeHcf` typenames,
   parameters, signals, statistics, and configuration precedence throughout the
   migration.
7. Do not retain protected fields as a compatibility mechanism. Temporary adapter
   methods may forward to typed services, but extracted state becomes private to its
   owner.

## Why the redesign is necessary

The present base class implements six callback interfaces plus `ModeSetListener` and
stores nearly every MAC collaborator in one object
([`Hcf.h`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h)). Its
implementation combines:

- upper/lower ingress and packet ownership;
- QoS classification and shared/per-STA queue routing;
- channel access and internal-collision handling;
- TXOP and frame-sequence selection;
- response timeout and receive-completion ordering;
- originator and recipient ACK, CTS, Block Ack, and management procedures;
- retry, drop, and rate-control reporting;
- A-MPDU selection, construction, and transmission bookkeeping;
- PHY mode selection, Duration-field preparation, and immediate responses;
- HT implicit Block Ack, sounding, CSI feedback, and MFB state;
- semantic signal emission and display diagnostics.

`VhtHcf` adds CSI lifecycle, SU sounding, Group ID membership, DL MU candidate
collection, scheduling, frame-sequence selection, radio projection, and special
TX/RX paths in another subclass
([`VhtHcf.cc`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/VhtHcf.cc)).

HE magnifies the problem. The `HeHcf` family spans
[`HeHcf.cc`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc),
[`HeHcfDl.cc`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfDl.cc),
[`HeHcfUl.cc`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfUl.cc),
and
[`HeHcfTxRx.cc`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfTxRx.cc),
but these are only physical file splits: all methods still mutate inherited protected
state. HE currently owns association epochs, queue-bank retirement, peer OMI, CSI,
DL scheduling, UL Trigger scheduling, UORA, triggered-response packet transactions,
Multi-STA BA correlation, two extra timers, TXOP family selection, and concrete
frame-sequence type dispatch.

The warning signs to eliminate are:

- feature code directly reading `mac`, `edca`, `tx`, `modeSet`, BA handlers,
  `frameSequenceHandler`, `exchangeCoordinator`, and `aggregationService`;
- frame sequences downcasting callbacks to `Hcf` or `HeHcf`;
- HCF variants downcasting the running sequence to `VhtDlMuTxOpFs`,
  `HeDlMuTxOpFs`, or `HeUlMuTxOpFs` to discover what happened;
- one method both inspecting queues, negotiating capabilities, selecting PHY
  resources, changing BA state, constructing packets, scheduling timers, and
  committing ownership;
- amendment behavior being selected through override order and concrete types rather
  than an explicit capability- and role-gated exchange choice.

## Scope

### Included production scope

- HCF base module and NED contract:
  [`Hcf.h`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h),
  [`Hcf.cc`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc),
  [`Hcf.ned`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.ned), and
  [`IHcf.ned`](../src/inet/linklayer/ieee80211/mac/contract/IHcf.ned).
- Existing HCF helpers, frame-sequence handler/context contracts, and callback
  interfaces where required to establish single ownership.
- `VhtHcf`, VHT sounding/CSI, Group ID membership, DL MU scheduling/plans, and VHT
  frame-sequence callback seams.
- The full `HeHcf` feature set: peer and association lifecycle, per-STA queues, DL
  OFDMA/MU-MIMO, UL OFDMA/MU-MIMO, UORA, BSR/BSRP, Trigger and HE-TB response
  transactions, Multi-STA BA, OMI, TWT eligibility, puncturing, sounding/CSI, link/PHY
  projection, TXOP selection, and HE-specific TX/RX paths.
- Compatibility changes in `Ieee80211Mac`, frame sequences, tests, examples, and NED
  configuration that are necessary to remove concrete HCF downcasts or protected
  inheritance.
- EHT frame sequences that reuse HE exchange contracts, specifically the inherited
  HE UL callback signature in `EhtUlMuTxOpFs`. If Phase 0 confirms that the class is
  dormant, this scope is source/build compatibility only; it must not create a new
  EHT runtime path.
- Focused characterization and regression tests required by each ownership transfer.

### Explicit non-goals

- Implementing HCCA, PCF, MCF, or missing non-QoS behavior.
- Adding EHT-specific scheduling or exchanges. Existing EHT configurations that use
  `HeHcf` are compatibility regressions, not an EHT redesign in this workstream.
- Changing on-air frame formats, generated `.msg` code, serializers, or packet-tag
  semantics.
- Replacing EDCA, ACK/Block Ack agreement handlers, recovery procedures, rate-control
  algorithms, or PHY mode tables.
- Changing scheduler algorithms merely because their inputs move behind snapshots.
- Renaming public parameters, signals, statistics, NED typenames, or configuration
  paths during the structural migration.
- Updating fingerprint CSV files. Any expected fingerprint change requires separate
  user approval after the first changed event is explained.
- Broad cleanup, formatting, or unrelated TODO removal.

## Compatibility contract

Until the final compatibility decision, every patch must preserve:

- `Ieee80211Mac.ned`'s `hcf: ... like IHcf` slot and the effective paths below it;
- `typename = "Hcf"`, `"VhtHcf"`, and `"HeHcf"` in existing and downstream INI
  files;
- the current NED parameter defaults, units, gates, submodule override points,
  signals, statistics, and result names;
- current initialization-stage behavior and module lookup paths;
- supported packet bytes, fields, tags, FCS handling, A-MPDU member order, and
  selected PHY vectors;
- timer scheduling/cancellation order, callback order, event trajectory, and RNG
  consumption unless a separately approved correctness patch says otherwise;
- queue order, access-category selection, TXOP admission/release, retry counters,
  sequence numbers, Block Ack windows, and rate-control feedback;
- packet pointer identity wherever ACK/retry/rollback code observes it;
- AP/non-AP role behavior, legacy fallback, and disabled-feature behavior;
- source compatibility for repository tests that derive NED modules from `HeHcf` or
  `VhtHcf`. Protected test hooks must first be replaced by explicit injection
  contracts before removal;
- DCF source and behavior when HCF adapts the shared frame-sequence callback; the
  generic `IFrameSequenceHandler::ICallback` contract remains source-compatible.
- existing EHT configurations that use `HeHcf`, plus source/build compatibility for
  dormant EHT frame sequences that inherit an HE callback contract.

## Architectural and standards constraints

The primary project constraints are `AR-ORG-CONTRACTS`, `AR-MOD-COMPOSITION`,
`AR-MOD-PLUGGABLE`, `AR-COM-DIRECT`, `AR-OBS-SIGNALS`, `AR-OBS-NED-TRUTH`,
`AR-CFG-INFER`, `AR-QUAL-TESTS`, `AR-QUAL-DETERMINISM`,
`AR-QUAL-TRACEABILITY`, and `AR-QUAL-NAMING`.

The complete IEEE 802.11 overlay applies, especially:

- `AR-WLAN-STD-TRACE` and `AR-WLAN-STD-GATING`;
- `AR-WLAN-ARCH-BOUNDARIES`, `AR-WLAN-ARCH-OWNERSHIP`, and
  `AR-WLAN-ARCH-VARIANTS`;
- `AR-WLAN-PHY-AUTHORITY` and `AR-WLAN-PHY-TIMING`;
- `AR-WLAN-MAC-EXCHANGE`, `AR-WLAN-MAC-SEQUENCE`, `AR-WLAN-MAC-QOS`, and
  `AR-WLAN-MAC-MULTIUSER`;
- `AR-WLAN-FRAME-REPRESENTATION`, `AR-WLAN-OBS-EVENTS`, and
  `AR-WLAN-QUAL-TESTS`.

Normative decision points and focused tests must retain or add references to IEEE Std
802.11-2024. The central clauses for this redesign are:

- 10.23.2.1 for the four EDCAFs and internal collision semantics;
- 10.23.2.7 and 10.23.2.9 for TXOP sharing and duration bounds;
- 10.3.2.13.3 for response timing after UL MU transmission;
- 10.25.2 and 10.25.6.6.3.1 for agreement/window ownership and operation;
- 11.39 for acknowledged VHT Group ID state;
- 26.5.1 for HE DL MU, 26.5.2 for HE trigger-based UL, 26.5.4 for UORA,
  and 26.7 for HE sounding;
- 27.3.4 for the distinct HE SU, MU, ER-SU, and TB PPDU families.

These references constrain behavior; they do not imply that this refactor should add
currently unimplemented standard behavior.

## Seal and exception status

At plan creation, only `src/inet/common/packet/` is recursively sealed. The HCF,
VHT, HE, frame-sequence, contract, scheduler, and queue files in this plan are
unsealed. No architecture or naming exception ledger entry covers the target area.

Recheck sealing immediately before every production patch because the status may
change. If any target becomes sealed, request explicit permission for that exact file
before editing it. Never add, remove, or change a seal or exception ledger as an
incidental part of this work.

The focused architecture script currently exits nonzero when pointed at the
coordination-function subtree because its simple domain check treats every protocol
include in that subtree as a violation. Record this as a baseline limitation; do not
misreport it as an HCF-specific finding. Still run the full and focused checks after
architecture-sensitive patches and compare new output against the baseline.

## Target architecture

### One NED-facing façade

The final `Hcf` class is a small OMNeT++ boundary. It owns only responsibilities that
must remain on a module:

- initialization and typed resolution of NED submodules;
- entry points from `Ieee80211Mac` (`processUpperFrame`, `processLowerFrame`, corrupt
  reception, lifecycle notifications);
- timer-message dispatch into the component that owns the timer;
- OMNeT++ packet ownership transfer and semantic signal emission delegated from the
  owning service;
- display/watch adapters that query immutable snapshots.

It must not expose queues, handlers, the active sequence, exchange state, or amendment
state as protected fields. The façade constructs a `HcfRuntime` service bundle and
passes narrow references to feature components.

`VhtHcf` and `HeHcf` remain registered classes initially, but their only task is to
select/configure VHT or HE feature composition and preserve existing NED identity.
No protocol algorithm remains in either adapter.

`Ieee80211Mac` talks to the façade through a C++ `IQosCoordinationFunction` contract
paired with the NED `IHcf` module interface. The contract extends the common
`ICoordinationFunction` operations with the MAC-facing QoS/HCF operations that are
currently concrete calls: management-result handler installation, intact-HT-A-MPDU
admission, legacy-preamble notification, TWT eligibility change, and peer-derived
state invalidation. It must not expose amendment services. `Ieee80211Mac` stores this
interface, and initialization rejects a NED `like IHcf` implementation that does not
implement it. This removes both the `Hcf *` member and the `dynamic_cast<HeHcf *>`
legacy-preamble branch.

### Common runtime components

| Component | Sole responsibility | Explicit exclusions |
| --- | --- | --- |
| `HcfRuntime` | Composition root and read-only access to typed common services | No protocol decisions, timers, queues, or mutable amendment state |
| `HcfIngressService` | Checked frame classification, ingress ownership, AC assignment request, queue submission, lower-frame route selection | No contention, ACK/BA result, scheduler, or PHY decision |
| `HcfQueueService` | Central TID/UP-to-AC mapping, shared/per-peer queue resolution through a policy, enqueue provenance, eligibility snapshots | No backoff, TXOP, BA-window mutation, or scheduling algorithm |
| `HcfChannelAccessController` | EDCAF request/grant, internal collision routing, winning AC, TXOP begin/end/release, contention resume | No queue storage, response timer, or MU resource selection |
| `HcfExchangeEngine` | The single active exchange, frame-sequence runner, start-RX/inactivity timers, expected response, completion/abort, stale callback rejection | No scheduler, retry counter, BA agreement, or PHY formula |
| `HcfOriginatorService` | Typed transmitted/received/failed MPDU/control/mgmt outcomes; calls ACK, BA, recovery, and rate-control owners | No frame-sequence type tests or queue selection |
| `HcfRecipientService` | A-MPDU decode/admission, immediate ACK/CTS/BA procedures, recipient agreement updates, delivery to MAC | No originator state or scheduler |
| `HcfTransmissionService` | Ordered preparation pipeline: retry bit, ACK policy, aggregate request, mode request, Duration/protection request, `ITx` handoff | No mode legality formula, exchange selection, or retry result |
| `HcfAggregationService` | Aggregate byte materialization and temporary aggregate-to-constituent ledger | No eligibility, BA policy, PHY/TXOP bound, signal, or packet lifetime decision |
| `HcfBaRetryService` | Common per-MPDU ACK/BA/retry transaction API over existing authoritative handlers | No duplicate counter/window/queue state |
| `HcfMacSapTracker` | Service-data-unit IDs, region provenance, and buffer-service-byte accounting | No queue ownership or scheduler choice |
| `HcfObservationSink` | Typed semantic events mapped once to existing HCF NED signals and watches | No protocol branch or state mutation |

Working names may change during implementation, but a rename must retain these
boundaries. Do not merge components merely to reduce file count, and do not create a
component that only forwards every member of another component.

### Typed immutable contexts

Feature code receives snapshots rather than the HCF object:

- `HcfLocalContext`: local MAC address, role, BSSID, mode-set identity, current time,
  and current winning AC/TXOP snapshot;
- `HcfPeerSnapshot`: association epoch, AID, directional negotiated capabilities,
  TWT eligibility, OMI constraints, and capability-generation values;
- `HcfQueueSnapshot`: candidate packet identity, source queue token, AC/TID, enqueue
  time, retry/BA eligibility, bytes, and association epoch;
- `HcfPhySnapshot`: channel width/frequency, antennas, power/sensitivity, GI/LTF,
  puncturing, and immutable link estimates;
- `HcfExchangeResult`: exchange family, transmitted member identities, response
  identity, per-user results, timeout/abort reason, and terminal ownership action.

Snapshots must not contain mutable module pointers or expose a way to dequeue, mutate
BA state, transmit, or schedule. Actions that change state use narrow command
interfaces with `prepare`, `validate`, `commit`, `rollback`, and `complete` semantics.

### Explicit exchange selection

Provider composition is explicit and NED-visible. `Hcf.ned` contains one
interface-typed `featureSet` submodule. The base, VHT, and HE NED types select common,
VHT, or HE feature-set defaults without a C++ virtual factory. The feature set is a
composition-only object: it owns or resolves typed feature components, returns
provider descriptors, and contains no protocol decisions, timers, queues, or packet
state. `HcfRuntime` keeps non-owning typed references while OMNeT++ owns module
lifetimes; any non-module helper is privately owned by its feature component and is
destroyed before that component unregisters.

Each descriptor has a fixed `HcfExchangeClass` rather than an arbitrary numeric
priority. Initialization rejects duplicate providers for a class and missing SU
support. Feature-off composition omits the relevant descriptor. The selector visits
the fixed classes, so NED declaration order, subclass construction, and runtime
registration order cannot change behavior.

After an EDCAF wins, an `IHcfExchangeSelector` examines immutable context and asks
the configured providers in a deterministic order. The following is a candidate
order that Phase 0 must prove or correct before Phase 2 implements it:

1. pending HE UL Trigger exchange;
2. eligible HE DL MU exchange;
3. eligible VHT group-management or DL MU exchange;
4. required HT/VHT/HE sounding exchange;
5. ordinary SU HCF exchange;
6. release the channel if no provider can commit an exchange.

Only providers enabled by operation mode, local capability, negotiated peer
capability, station role, and configuration participate. A provider returns either a
typed rejection reason or a `PreparedHcfExchange`; it may not mutate queues or BA
state during probing.

`PreparedHcfExchange` contains the frame-sequence object, immutable plan, reserved
packet identities, expected response policy, and a provider-owned transaction token.
The selector commits exactly one prepared exchange after validation. Rejected plans
roll back without changing queue order, sequence allocation, ACK status, retry state,
or RNG consumption.

### Frame-sequence contract without downcasts

Keep the shared `IFrameSequenceHandler::ICallback` source-compatible because DCF also
implements it. `HcfExchangeEngine` becomes the HCF-side implementation of that
generic callback and translates its operations into an HCF-only typed event contract:

- `transmissionRequested(PreparedTransmission)`;
- `transmissionCompleted(TransmissionResult)`;
- `responseReceived(ResponseResult)`;
- `responseTimedOut(ResponseTimeout)`;
- `exchangeCompleted(HcfExchangeResult)`;
- `exchangeAborted(HcfExchangeResult)`.

Each event carries an exchange-family identifier and provider transaction token.
VHT/HE code must never discover active behavior with `dynamic_cast` on the callback or
running frame sequence. Frame sequences must never recover MAC, HCF, data-service, or
BA-handler objects through parent-module paths.

HCF-specific frame sequences receive the typed event/result sink through their HCF
exchange context; the generic callback does not gain amendment methods or no-op
defaults. DCF continues to use the existing callback directly and is covered by a
compile gate plus focused DCF retry/response regressions.

Add narrow callback contracts for provider-specific operations, for example
`IVhtDlMuExchangeCallback`, `IHeDlMuExchangeCallback`, and
`IHeUlExchangeCallback`, only when the information cannot be represented in the
common result types. These contracts live in the 802.11 MAC contract package and must
not name concrete HCF classes.

### PHY plan boundary

Schedulers return policy choices, not transmit-ready PPDUs. A validated plan contains:

- exchange and PPDU family;
- recipients, AIDs, TIDs/ACs, and association epochs;
- RU geometry or VHT user positions/spatial streams;
- requested MCS/NSS/coding/GI/bandwidth and PSDU budgets;
- ACK method and all expected responses;
- complete TXOP-duration budget including SIFS and response transmissions;
- the capability and CSI generations against which it was validated.

The PHY-facing finalizer is the only owner of legal MCS/RU/NSS/GI/preamble
combinations, symbol rounding, PPDU duration, and canonical TXVECTOR creation. It
returns a valid immutable plan or a diagnostic rejection; it never silently repairs
an invalid choice. Existing `HeDlMuPlan` and `HeUlMuPlan` are retained and strengthened
as this boundary rather than bypassed.

### Amendment components

#### HT

`HtHcfFeature` owns only HT-specific state and exchange providers:

- HT sounding retry/pending state and CSI feedback;
- MCS request/feedback attachment and standalone MFB transmission;
- HT implicit Block Ack eligibility and aggregate exchange variant;
- legacy HT Multi-TID extension gating.

Common A-MPDU construction and common ACK/BA/retry operations remain in common
services. HT frame formats, timers, and sounding exchange classes are not shared with
VHT or HE.

#### VHT

Split VHT into three cooperating owners:

- `VhtSoundingService`: dialog tokens, SU/MU sounding retry, CSI freshness, NDP/feedback
  exchange, and channel-width invalidation;
- `VhtGroupMembershipService`: pending/acknowledged Group ID state and the immutable
  radio membership projection required by IEEE 802.11-2024 Clause 11.39;
- `VhtDlMuExchangeProvider`: capability-gated candidate snapshots, scheduler call,
  `VhtDlMuPlan` validation, atomic reservation, and per-user completion mapping.

The scheduler remains a NED-pluggable algorithm. It receives immutable candidates and
returns selections only. It cannot initiate sounding, reorder ADDBA packets, mutate
Group ID state, or dequeue frames.

#### HE peer and queue state

`HePeerStateService` owns association-epoch keyed state: OMI, CSI generation, link/PHY
snapshots, TWT eligibility views, and invalidation notifications. It does not own MIB
association state; it subscribes to the MIB and derives immutable snapshots.

`HeQueueService` owns dynamic per-STA/per-AC banks and their association epochs,
including retirement and safe destruction. It exposes reservation tokens and queue
snapshots rather than raw mutable queues to schedulers. The generic queue service uses
this policy only for associated HE AP peers and otherwise resolves the ordinary EDCAF
queue.

Frames tied to an old association epoch are retired by this service after the active
exchange releases them. No other HE component keeps a second packet-retirement map.

#### HE downlink

`HeDlMuExchangeProvider` owns the DL prepare/validate/commit transaction:

1. gather immutable candidates across shared and per-STA queues;
2. apply role, association, TWT, OMI, capability, puncturing, CSI, BA-window, and TXOP
   eligibility;
3. request sounding through `HeSoundingService` when a candidate requires it;
4. call the configured DL scheduler;
5. validate through `HeDlMuPlan` and the PHY plan finalizer;
6. atomically reserve the selected MPDUs without changing their identity;
7. return an HE DL frame sequence and expected MU-BAR/sequential-BAR response plan;
8. map per-user results back into the common ACK/BA/retry service;
9. commit or roll back the queue reservation exactly once.

ADDBA bootstrap is a separate typed prerequisite outcome, not a side effect of
scheduler candidate collection. SU fallback is returned to the common selector with
an explicit reason and retains the original winning AC/TXOP.

#### HE uplink AP scheduling

`HeUlTriggerService` owns AP-side BSR cache, report freshness, Trigger policy, UORA
OCW/backoff, Trigger IDs, scheduler prepare/commit, and result observations. The
existing `HeUlCoordinator` is migrated toward this role.

It produces an immutable `HeUlMuPlan`; it does not construct the Trigger packet,
schedule response timers, own STA response packets, or update common retry state.
Trigger selection remains capability-, role-, TWT-, OMI-, puncturing-, and TXOP-gated.

#### HE triggered STA response

`HeTriggeredUlExchangeService` owns the STA-side transaction keyed by Trigger ID:

- structural and semantic Trigger validation;
- selected AID/RU/TID/AC and Trigger-derived PHY snapshot;
- atomic packet reservation and cloned sequence-number transaction;
- QoS Null, BSR, NDP feedback, data, BAR, and A-MPDU response construction;
- SIFS/deadline validation and expected Multi-STA BA/MU-BAR mapping;
- response timeout, late/foreign BA rejection, per-MPDU retry, and reinsertion;
- exactly-once semantic response events.

It uses the common BA/retry and queue transaction APIs. It replaces the mutable
`triggeredUlExchanges` map in `HeHcf`; no shadow retry or packet ownership state remains
in the adapter or frame sequence.

#### HE sounding and PHY projection

`HeSoundingService` owns NDPA/NDP/BFRP/feedback dialog state and CSI updates. Its
interface accepts snapshots and action callbacks; it never receives a `HeHcf *`, raw
frame-sequence handler, or mutable scheduler context.

The existing `IIeee80211HeLinkPhyContext` becomes the sole projection from radio/MIB
state into immutable HE local/peer PHY snapshots. HE providers consume this contract
and never downcast the radio or reproduce PHY duration formulas.

## State-ownership ledger

The implementation must maintain this table in code comments or developer
documentation as ownership moves:

| State | Final owner |
| --- | --- |
| EDCAF AIFS/CW/backoff/channel ownership and TXOP | `Edca`/`Edcaf`/`TxopProcedure` |
| Active exchange, expected response, exchange timers | `HcfExchangeEngine` |
| Shared and per-peer queues, association-epoch retirement | `HcfQueueService` + `HeQueueService` policy |
| Sequence allocation | originator data service transaction API |
| ACK status and retry counters | existing ACK/recovery procedures behind `HcfBaRetryService` |
| BA agreements and reorder windows | existing originator/recipient BA handlers |
| Temporary aggregate-member mapping | `HcfAggregationService` |
| HT sounding/MFB | `HtHcfFeature` |
| VHT CSI/dialog state | `VhtSoundingService` |
| VHT Group ID state | `VhtGroupMembershipService` |
| HE peer OMI/derived CSI/link generation | `HePeerStateService` |
| HE AP BSR/UORA/Trigger scheduling state | `HeUlTriggerService` |
| HE STA Trigger response and Multi-STA BA correlation | `HeTriggeredUlExchangeService` |
| Selected PHY parameters and duration | validated plan + PHY mode/finalizer |
| Semantic event emission | action owner through `HcfObservationSink` |

No migration patch may leave two writable owners. During an adapter phase, the old
path forwards to the new owner; it must not synchronize a copied field.

## Implementation sequence

Each phase is a separate reviewable patch series. Keep the release build and that
phase's focused tests green before beginning the next phase. Use only one production
writer at a time and run an independent architecture/WLAN review after every phase
that changes ownership or behavior-sensitive callbacks.

### Phase 0 — Baseline and missing characterization

Purpose: freeze observable behavior before changing ownership.

Actions:

1. Record the exact commit, clean/dirty status, build mode, compiler command, test
   commands, configs, run numbers, seeds, and artifact paths.
2. Add focused tests for currently under-observed HCF-family behavior:
   - OMI ingress -> peer-state event -> DL NSS constraint and UL MU exclusion, with an
     OMI-off control;
   - TWT sleeping/awake eligibility for both DL and UL at the service-period boundary,
     including reassociation while sleeping;
   - headerless HE NDP ingress/egress, no ACK/BA/retry side effect, and malformed or
     foreign NFRP rejection;
   - puncturing at the exact adjacent-20-MHz boundary and unsupported-peer SU fallback;
   - HE DL planning/commit failure preserving queue order, ACK ownership, packet
     identity, and next-TXOP SU fallback;
   - UL no-allocation, stale/unknown BSR, all-TWT/OMI-disabled, response timeout, and
     late Multi-STA BA cases;
   - HCF unicast/multicast/broadcast and PHY-loss retry behavior;
   - mixed legacy/HT/VHT/HE coexistence with fixed seeds.
3. Capture signal names/counts and packet/event ordering for one base HCF, one VHT MU,
   one HE DL MU, and one HE UL Trigger exchange.
4. Inventory every repository dependency on concrete `Hcf`, `VhtHcf`, or `HeHcf`
   types, every implementation of `IFrameSequenceHandler::ICallback`, every
   VHT/HE/EHT frame-sequence callback constructor, and every derived test NED/C++
   type. Classify each use as public compatibility, test observation, injection seam,
   or coupling to remove.
   For `EhtUlMuTxOpFs`, identify a production constructor/caller and runnable
   configuration if one exists. If none exists, record it as dormant and require only
   source/build and focused constructor-contract coverage; do not invent an EHT UL
   runtime path in this refactor.
5. Derive the exchange-class priority table from source and captured behavior for
   every applicable mode/role/precondition. Record sounding, Group ID, ADDBA, UL, DL,
   SU fallback, channel-release, event-order, and RNG-consumption precedence. The
   candidate table in this document is not accepted until this evidence exists.
6. Do not alter production behavior to make a desired characterization pass. Record
   mismatches as separate correctness candidates.

Exit gate: every state owner listed above has at least one focused positive and one
failure/boundary test, or an explicit documented reason why an existing module test is
the correct coverage level. The concrete-dependency inventory and evidence-backed
exchange-class priority table are reviewed artifacts.

### Phase 1 — Introduce contracts, snapshots, and transaction vocabulary

Purpose: create stable seams before moving code.

Candidate additions:

- `mac/contract/IQosCoordinationFunction.h`
- `mac/contract/IHcfExchangeProvider.h`
- `mac/contract/IHcfExchangeCallback.h`
- a paired C++/NED `IHcfFeatureSet` contract and concrete common/VHT/HE composition
  defaults;
- `mac/coordinationfunction/HcfContext.h`
- `mac/coordinationfunction/HcfExchangePlan.h`
- queue reservation and sequence-number transaction contracts in their owning
  packages.

Actions:

1. Introduce `IQosCoordinationFunction`, migrate `Ieee80211Mac`'s stored pointer and
   all MAC-facing calls to it, and remove concrete HCF/HE casts. Add a minimal test
   implementation that satisfies NED `like IHcf` without inheriting `Hcf`.
2. Define the typed feature-set contract, NED slot/defaults, provider ownership and
   destruction order, fixed exchange-class descriptors, duplicate/missing-provider
   validation, and feature-off omission. Adapters select NED composition; they do not
   override a C++ provider factory.
3. Define immutable local, peer, queue, PHY, exchange-plan, and exchange-result value
   types.
4. Define typed prepare/commit/rollback contracts with explicit packet ownership.
5. Add transaction tokens/generations to reject stale callbacks deterministically.
6. Adapt existing HCF paths to populate the types while still executing old code.
7. Add contract tests for incomplete snapshots, duplicate commit/rollback, stale
   generation, packet-order preservation, and exception safety.
8. Add effective-composition tests for `Hcf`, `VhtHcf`, `HeHcf`, feature-off cases,
   and derived test typenames; prove provider order is independent of NED declaration
   and construction order.
9. Do not add a generic service locator. `HcfRuntime` exposes named interfaces only.

Exit gate: VHT/HE code can receive all information needed for selection without
reading protected `Hcf` fields, although it may still temporarily call old execution
methods through adapters. `Ieee80211Mac` depends only on the C++ QoS coordination
contract, and provider ownership, selection order, and destruction are executable
contract tests rather than adapter conventions.

### Phase 2 — Establish the common exchange kernel

Purpose: make active-exchange and callback ownership singular.

Primary files:

- existing `HcfExchangeCoordinator`, `HcfResponseService`, and
  `FrameSequenceHandler`;
- new `HcfExchangeEngine` or an equivalent consolidation;
- `Hcf.h`/`Hcf.cc` adapter calls;
- the HCF-specific event contract, adapter, and frame-sequence tests; the shared
  `IFrameSequenceHandler::ICallback` declaration remains source-compatible.

Actions:

1. Move start-RX and inactivity timer ownership, expected-response identity,
   deferred-timeout state, active packet/transaction identity, completion, abort, and
   reset behind the exchange engine.
2. Make the engine the only HCF-side frame-sequence callback. It adapts the unchanged
   generic callback to typed events for the selected provider and common
   originator/recipient services. DCF remains a direct generic-callback client.
3. Preserve synchronous callback and timer action order exactly; characterize any
   stale same-time event before changing it.
4. Replace `Hcf`, `VhtHcf`, and `HeHcf` direct calls to
   `exchangeCoordinator.beginTransmission()` and concrete sequence inspection.
5. Replace frame-sequence `dynamic_cast<Hcf *>`/`dynamic_cast<HeHcf *>` and parent
   module lookup with typed callbacks and snapshots.
6. Retain compatibility wrappers temporarily, mark them for deletion, and assert that
   no new caller uses them.
7. Compile DCF callback implementations and run focused short-frame, long-frame, and
   RTS/CTS retry/response gates before accepting the callback adapter.

Exit gate: one object owns the complete exchange lifecycle; no variant or frame
sequence reads the running concrete sequence type or schedules an exchange timer;
DCF still implements the unchanged shared callback and preserves behavior.

### Phase 3 — Split common ingress, queue, originator, recipient, and transmission paths

Purpose: reduce `Hcf` to a façade without changing amendment behavior.

Actions:

1. Extract `HcfMacSapTracker` and prove service-data-unit byte accounting across
   fragmentation/A-MSDU/A-MPDU transformations.
2. Extract checked ingress classification and queue submission. Centralize TID-to-AC
   mapping and queue provenance; keep EDCA ownership unchanged.
3. Extract recipient processing, including A-MPDU deaggregation, immediate responses,
   recipient BA state, and delivery. Preserve malformed-member and foreign-member
   ownership paths.
4. Extract originator success/failure processing, ACK/BA result mapping, retry/drop,
   management result, and rate-control feedback.
5. Turn `HcfRetryService` into the common façade over existing mutable recovery/ACK/BA
   owners; it must remain stateless.
6. Extract the ordered transmission preparation pipeline. Retain PHY mode objects as
   the source of legality/rate/duration and retain `SingleProtectionMechanism` as its
   current authority.
7. Keep semantic signals emitted exactly once by the component that now owns the
   action, through an observation adapter that preserves NED names.

Exit gate: `Hcf` delegates all protocol work through typed interfaces; its source no
longer contains frame-subtype decision forests, aggregate construction, or retry
algorithms.

### Phase 4 — Migrate HT behavior

Purpose: prove the feature-provider model on the smallest amendment-specific surface.

Actions:

1. Move HT sounding, pending NDP validation, CSI feedback construction, retry timing,
   MCS request/feedback, and MFB completion into `HtHcfFeature`.
2. Move implicit Block Ack selection into an HT exchange provider while leaving
   aggregate materialization and BA/retry state in common services.
3. Gate all HT behavior on configured PHY family and directional negotiated
   capabilities; retain the legacy Multi-TID extension as an explicitly named,
   non-negotiated configuration gate.
4. Remove HT state from `Hcf` after every caller uses the feature contract.

Exit gate: base HCF contains no HT frame fields, sounding tokens, CSI/MFB state, or HT
implicit-BA selection logic.

### Phase 5 — Replace `VhtHcf` behavior with VHT composition

Purpose: remove the VHT subclass algorithm before tackling the larger HE migration.

Actions:

1. Extract VHT sounding/CSI and width invalidation into `VhtSoundingService`.
2. Retain `VhtGroupIdManager` as the single Group ID owner; introduce a typed radio
   membership adapter so it does not call through `VhtHcf`.
3. Split candidate collection from `IIeee80211VhtDlMuScheduler`; return immutable
   candidates and selections.
4. Validate a `VhtDlMuPlan` before any queue/BA mutation, then atomically reserve
   selected frames.
5. Migrate `VhtDlMuTxOpFs` to typed exchange callbacks and per-user results.
6. Remove queued-ADDBA reordering from scheduler probing. Represent BA bootstrap as a
   prerequisite exchange result handled by the selector.
7. Convert `VhtHcf` into a thin adapter whose NED defaults select the VHT feature set
   and which forwards only the defined façade notifications. It does not install or
   register providers in C++.
8. Verify disabled VHT features are behaviorally identical to base HCF and that
   one-antenna, stale CSI, missing membership, missing BA, non-AP, and non-VHT cases
   fall back without side effects.

Exit gate: `VhtHcf.{h,cc}` contains only compatibility/configuration code; no VHT
scheduler, sounding, packet, radio, or frame-sequence algorithm depends on it.

### Phase 6 — Extract HE peer and queue services

Purpose: move stable lifecycle state before moving DL/UL exchange algorithms.

Actions:

1. Move association listener handling, epoch generations, OMI state, derived CSI/link
   invalidation, and semantic peer-state events into `HePeerStateService`.
2. Move queue-bank creation, lookup, reservation, retirement, deferred in-flight
   release, and safe destruction into `HeQueueService`.
3. Replace raw `StationQueueBank *` and `IPacketQueue *` scheduler inputs with immutable
   queue snapshots and reservation tokens. Direct queue interfaces remain inside the
   queue service and common transmission pipeline only.
4. Make TWT eligibility a snapshot supplied by the authoritative TWT/MIB contract; do
   not copy wake state into HE services.
5. Preserve NED queue-bank module paths and statistics.

Exit gate: `HeHcf` owns no association listener, peer map, CSI table, queue-bank
manager, retirement map, or peer invalidation cascade.

### Phase 7 — Extract HE downlink provider

Purpose: isolate DL OFDMA/MU-MIMO preparation and completion.

Actions:

1. Move schedule-context construction to a pure candidate builder over local, peer,
   queue, BA, CSI, OMI, TWT, and PHY snapshots.
2. Separate ADDBA bootstrap, sounding prerequisite, scheduler selection,
   `HeDlMuPlan` validation, and commit into explicit states.
3. Move DL MU reservation/rollback and next-TXOP SU fallback into the provider.
4. Replace `HeDlMuTxOpFs` concrete inspection with typed member-transmitted,
   per-user-BA, timeout, and planning-failure events.
5. Route all ACK/BA/retry/rate-control outcomes through common services using original
   MPDU identities.
6. Keep the configured `IIeee80211HeDlScheduler` pluggable and algorithmically
   unchanged.

Exit gate: `HeHcfDl.cc` is deleted or reduced to compatibility forwarding, and no DL
provider mutates a queue or ACK/BA state before plan validation and atomic commit.

### Phase 8 — Extract HE AP UL Trigger provider

Purpose: isolate BSR/UORA scheduling from exchange execution.

Actions:

1. Move periodic Trigger consideration and its timer to `HeUlTriggerService`; timer
   ownership must not remain in `HeHcf`.
2. Preserve `HeUlCoordinator`'s buffer-report freshness, Trigger policy, UORA state,
   deterministic Trigger ID allocation, and scheduler prepare/commit semantics while
   narrowing its inputs to snapshots.
3. Move capability/TWT/OMI/puncturing/TXOP eligibility into the provider and produce a
   validated `HeUlMuPlan`.
4. Migrate `HeUlMuTxOpFs` from `HeHcf *` callbacks to the typed HE UL exchange
   contract.
5. Migrate `EhtUlMuTxOpFs`'s constructor signature to the same typed contract and
   update existing callers, if Phase 0 finds any. If the class is dormant, require a
   focused construction/compile contract test only. Do not add a production caller or
   claim runtime equivalence without a separately approved EHT feature change.
6. Ensure no Trigger is emitted and the channel is correctly released or falls back
   to SU when the plan has no valid allocations.

Exit gate: AP UL scheduling and UORA have one owner; neither `HeHcf` nor the frame
sequence accesses the coordinator's mutable state directly, and HE plus EHT UL frame
sequences use the typed callback. Any reachable EHT path preserves its characterized
behavior; a dormant EHT class remains build-compatible without being activated.

### Phase 9 — Extract HE triggered-response transactions

Purpose: isolate the most ownership-sensitive HE path.

Actions:

1. Move Trigger parsing, selected-user resolution, HE-TB response preparation,
   Trigger-correlation tags, and SIFS/deadline checks into
   `HeTriggeredUlExchangeService`.
2. Introduce a packet reservation transaction: clone sequence state, prepare all
   MPDUs/control frames, validate the full PHY plan, then commit sequence numbers and
   dequeue atomically.
3. Move `triggeredUlExchanges`, response timer, response deadline, timeout retry,
   Multi-STA BA mapping, late/foreign response rejection, and retry reinsertion into
   the service.
4. Use common BA/retry APIs; delete any duplicated per-MPDU retry transition.
5. Preserve response events and reason/bytes signals exactly once at commit.
6. Cover QoS Null, BSR, NDP feedback, BAR, data, and single-TID A-MPDU paths in both
   success and rollback tests.

Exit gate: every Trigger ID has one transaction owner and one terminal path; `HeHcfUl.cc`
no longer contains packet-transaction or Multi-STA BA recovery logic.

### Phase 10 — Extract HE sounding and TX/RX interception

Purpose: remove the last concrete callbacks and variant dispatch.

Actions:

1. Convert `HeSoundingCoordinator` into `HeSoundingService` with snapshot/action
   contracts; remove `HeHcf *`, raw handler, and raw MAC/PHY dependencies.
2. Register sounding as an exchange provider and return CSI results through typed
   events.
3. Replace HE TX/RX override chains with feature interceptors that return one of
   `CONSUMED`, `CONTINUE_COMMON`, or `REJECTED` plus an ownership result.
4. Move outgoing OMI/BSR decoration to an HE frame-decoration policy invoked once in
   the common transmission pipeline.
5. Move headerless NDP classification into typed PHY-indication routing and prove it
   cannot enter ordinary ACK/BA/retry paths.

Exit gate: `HeHcfTxRx.cc` is deleted or contains only adapter forwarding; there are no
concrete sequence casts and no protocol branch depends on subclass override order.

### Phase 11 — Collapse adapters and NED composition

Purpose: reach the final public structure without breaking configurations.

Actions:

1. Make all common runtime fields private and remove obsolete protected virtual
   methods after repository and downstream-facing adapter tests migrate to explicit
   contracts.
2. Reduce `Hcf` to module wiring and external callbacks.
3. Reduce `VhtHcf` and `HeHcf` to registered compatibility classes and NED defaults
   that select the appropriate interface-typed feature-set submodule. The adapter
   classes do not own a provider factory or registration hook.
4. Make independently variable algorithms NED-interface-typed with replaceable
   defaults. Keep small stateless implementation helpers as C++ values rather than
   creating gratuitous modules.
5. Preserve parameter propagation and wildcard precedence; compare effective configs
   for representative base, VHT, HE AP, HE STA, and EHT-using-HeHcf scenarios.
6. Decide separately whether to deprecate the old typenames. Do not remove them in
   this refactor.

Exit gate: no production class derives behavior from `Hcf`; no feature has access to
HCF internals; the old typenames select composition only.

### Phase 12 — Cleanup, documentation, and final audit

Actions:

1. Delete superseded helpers only after all call sites and tests use the new owner.
2. Reduce headers with forward declarations and contract includes; verify optional
   feature-off builds.
3. Update developer documentation with the exchange selection flow, state-ownership
   ledger, packet transaction rules, and extension recipe for another amendment.
4. Apply the full general and WLAN semantic review checklists.
5. Run architecture checks and reconcile only newly introduced output against the
   existing ledgers/baseline limitation.
6. Run final unit/module/simulation/fingerprint matrices. Investigate the first changed
   event for every fingerprint mismatch; do not update a CSV.

Exit gate: all completion criteria below pass and an independent reviewer reports no
unresolved `FLAG` or `QUESTION` for the changed scope.

## Expected file surface

The exact names should be finalized during Phase 1, but the expected shape is:

### New common files

- `mac/contract/IQosCoordinationFunction.h`
- a paired C++/NED `IHcfFeatureSet` contract with common/VHT/HE composition defaults;
- `mac/contract/IHcfExchangeProvider.h`
- `mac/contract/IHcfExchangeCallback.h`
- `mac/coordinationfunction/HcfRuntime.{h,cc}`
- `mac/coordinationfunction/HcfContext.h`
- `mac/coordinationfunction/HcfExchangePlan.{h,cc}`
- `mac/coordinationfunction/HcfExchangeEngine.{h,cc}`
- focused ingress, queue, originator, recipient, transmission, BA/retry, MAC-SAP, and
  observation services where the phase proves a cohesive owner.

### New or reshaped amendment files

- HT feature/sounding/implicit-BA provider files;
- VHT sounding, Group ID adapter, and DL MU provider files;
- HE peer-state, queue, DL provider, UL Trigger, triggered-response, sounding, and
  frame-decoration files;
- typed HE/VHT/EHT-compatible frame-sequence callback contracts.

### Modified compatibility/integration files

- `Hcf.{h,cc,ned}`, `VhtHcf.{h,cc,ned}`, `HeHcf.{h,cc,ned}`;
- `HeHcfDl.cc`, `HeHcfUl.cc`, `HeHcfTxRx.cc` until deletion;
- HCF/VHT/HE frame-sequence handler and sequence files, including
  `EhtUlMuTxOpFs.{h,cc}` where it inherits the HE UL callback contract;
- `Ieee80211Mac.{h,cc}` to replace concrete `Hcf` storage and HE dispatch with
  `IQosCoordinationFunction`;
- VHT/HE coordinator, scheduler, plan, queue-bank, and link/PHY contracts;
- focused tests and test-only fault-injection adapters.

Do not create all candidate files in one patch. Add a component only when its phase
moves a complete responsibility and its tests can name the new contract directly.

## Verification strategy

### Per-patch minimum

From the repository root:

```sh
git diff --check
make MODE=release -j$(nproc)
```

Run the smallest owner-local unit set for the patch, then the relevant module test.
Record command, working directory, exit code, config/run/seed, and artifacts.

### Common HCF/HT/VHT unit gate

```sh
inet_run_unit_tests -m release -f '(Hcf.*|Ieee80211DeferredRxTimeout_1|Ieee80211FecCodingReq_1|Ieee80211HtImplicitBlockAck_1|Ieee80211HtSoundingExchange_1|Ieee80211VhtAddbaQueueing_1|Ieee80211VhtCsiTxVector_1|Ieee80211VhtDlMuPhy_1|Ieee80211VhtDlMuScheduler_1|Ieee80211VhtGroupIdManager_1|VhtMpduAggregation_1|VhtBeamformingMimo_1).*\.test'
```

### HE owner-local unit gate

```sh
inet_run_unit_tests -m release -f '(Ieee80211HeAssociationLifecycle_1|Ieee80211HeQueueBankService_1|Ieee80211HeStaId_1|Ieee80211HeBsrBsrpIntegration_1|HeDlScheduler_1|HeUlScheduler_1|Ieee80211HeSchedulerValidation_1|Ieee80211HeDlMuTransaction_1|Ieee80211HeUlMuPlan_1|Ieee80211HeUlMuTransaction_1|Ieee80211HeMuAddbaValidation_1|Ieee80211HeBlockAckWindow_1|Ieee80211HeMuBlockAckGating_1|Ieee80211HeMultiStaBlockAckFs_1|Ieee80211HeMuSeqAck_1|Ieee80211HeTxopCoordinatorService_1|Ieee80211HeSoundingCoordinator_1|Ieee80211HeMuMimo_1|Ieee80211HeMuRx_1|Ieee80211HeUlControlFrames_1|Ieee80211HePreamblePuncturing_1|Ieee80211HeLdpcPacketExtension_1|Ieee80211HeTxVectorCrossLayer_1|Ieee80211HeUserPhyParameters_1|Ieee80211HeSuProtocolIdentity_1|Ieee80211TwtFrames_1).*\.test'
```

If a runner does not accept the combined expression, split it by owner. Do not pass
several `-f` flags or broaden immediately to the entire suite.

### Module gates

Run individually so a failure has one configuration context:

```sh
inet_run_module_tests -m release --no-build -u Cmdenv -f 'Ieee80211HeConfigurationContract_1\.test'
inet_run_module_tests -m release --no-build -u Cmdenv -f 'Ieee80211SharedMacModes_1\.test'
inet_run_module_tests -m release --no-build -u Cmdenv -f 'Ieee80211VhtDlMuNegative_1\.test'
inet_run_module_tests -m release --no-build -u Cmdenv -f 'Ieee80211HeDlMuExchange_1\.test'
inet_run_module_tests -m release --no-build -u Cmdenv -f 'Ieee80211HeUlTriggerExchange_1\.test'
inet_run_module_tests -m release --no-build -u Cmdenv -f 'Ieee80211FailedBarRecovery_1\.test'
```

`Ieee80211SharedMacModes_1` is the main cross-family compatibility gate. It checks
effective HCF types, g/HT/VHT/HE/EHT operation, VHT sounding/MU/Group ID, RTS/CTS,
ACK, TXOP bounds, timeouts, and HE SU fallback. The dedicated DL/UL module tests add
fault, rollback, timeout, and Multi-STA BA evidence.

For `EhtUlMuTxOpFs`, Phase 0 decides the gate from reachability evidence. If a current
production caller/configuration exists, add and run a fixed-seed module gate that
asserts the existing Trigger, response identity, and Block Ack behavior through the
typed contract. If it is dormant, add a focused construction/compile contract test
and keep existing EHT-through-`HeHcf` configurations in the cross-family gate; do not
manufacture a runtime regression for behavior that does not exist.

### Deterministic feature simulations

Start with one configuration/run/seed and a dedicated result directory:

```sh
bin/inet -u Cmdenv -f examples/ieee80211ac/dl_mu_mimo_baseline/omnetpp.ini -c VhtSingleUserBaseline -r 0 --seed-set=1 --sim-time-limit=1s --result-dir=/tmp/hcf-redesign-vht-su
bin/inet -u Cmdenv -f examples/ieee80211ac/dl_mu_mimo_baseline/omnetpp.ini -c VhtDlMuMimoThreeUsers -r 0 --seed-set=1 --sim-time-limit=1s --result-dir=/tmp/hcf-redesign-vht-mu
bin/inet -u Cmdenv -f examples/ieee80211ax/opmode_indication/omnetpp.ini -c OperatingModeIndication -r 0 --seed-set=0 --sim-time-limit=1s --result-dir=/tmp/hcf-redesign-he-omi
bin/inet -u Cmdenv -f examples/ieee80211ax/ndp_feedback/omnetpp.ini -c NdpFeedbackReport -r 0 --seed-set=0 --result-dir=/tmp/hcf-redesign-he-ndp
bin/inet -u Cmdenv -f examples/ieee80211ax/preamble_puncturing/omnetpp.ini -c DynamicPuncturing -r 0 --seed-set=0 --sim-time-limit=1s --result-dir=/tmp/hcf-redesign-he-puncturing
```

For TWT, use the scenario's fixed seed and compare a treatment with its control:

```sh
bin/inet -u Cmdenv -f examples/ieee80211ax/twt/omnetpp.ini -c IndividualUnannounced -r 0 --seed-set=20260623 --sim-time-limit=100s --result-dir=/tmp/hcf-redesign-twt-treatment
bin/inet -u Cmdenv -f examples/ieee80211ax/twt/omnetpp.ini -c Baseline -r 0 --seed-set=20260623 --sim-time-limit=100s --result-dir=/tmp/hcf-redesign-twt-control
```

Validate frames, exchange events, BA/retry identity, and effective configuration from
logs/captures/results. Throughput alone is not evidence that the exchange is correct.

### HE planning smoke

After the focused unit/module gates:

```sh
inet_run_speed_tests -m release --no-build -u Cmdenv -w tests/speed/ieee80211heplanning -c Dl4Sta80MHz -r 0
inet_run_speed_tests -m release --no-build -u Cmdenv -w tests/speed/ieee80211heplanning -c Su4Sta80MHz -r 0
```

Add one UL configuration, then wider bandwidths and larger station counts only after
the narrow runs pass.

### Coexistence and legacy checks

Use fixed seeds for at least:

- HCF QoS with RTS/CTS, Block Ack, fragmentation, and aggregation;
- HT40 with a legacy secondary-channel interferer;
- HE puncturing or frequency-selective channel with a legacy interferer;
- DCF retransmission baselines to prove the shared ACK/recovery contracts did not
  regress.

Run a short- and long-frame member of `Ieee80211Retransmission1-10`, plus the relevant
QoS/aggregation examples, before the final fingerprint gate.

### Fingerprints

Run the relevant existing rows from:

- `tests/fingerprint/ieee80211-he.csv`;
- the QoS, aggregation, Block Ack, fragmentation, hidden-node, and TXOP rows in
  `tests/fingerprint/examples.csv` and `tests/fingerprint/showcases.csv`.

There is no dedicated VHT HCF fingerprint row at plan creation. Add a focused
behavior test instead of treating that absence as permission to change behavior.

For every mismatch, reproduce against the exact parent commit using the same build
mode, path, config, run, and seed. Locate the first changed event and classify it
before continuing. Never update an expected fingerprint without explicit approval.

### Architecture checks

```sh
bash .agents/skills/inet-architectural-requirements/references/enforcement/check-architecture.sh
bash .agents/skills/inet-architectural-requirements/references/enforcement/check-architecture.sh src/inet/linklayer/ieee80211/mac/coordinationfunction
```

Reconcile the focused script's known scope limitation and the full check's existing
ledger entries. The acceptance condition is no new unexplained dependency or
visualization coupling, not a misleading claim that the baseline script is clean.

## Required test dimensions

The final evidence matrix must cover the applicable `AR-WLAN-QUAL-TESTS` dimensions:

| Dimension | Minimum evidence |
| --- | --- |
| Timing | SIFS, response timeout, receive-at-timeout, stale timer, TXOP bound |
| Sequence/retry | wrap at 4095/0, retry limits, original MPDU identity, partial BA |
| Invalid input | malformed/foreign Trigger/NDP/BA, unsupported modes/capabilities |
| Roles | AP and non-AP; associated, disassociated, and reassociated peer |
| Addressing | unicast, multicast, and broadcast |
| Modes | legacy, HT, VHT, HE, and existing EHT-through-HeHcf compatibility |
| Aggregation | one/many MPDU, PPDU/TXOP limits, BA-window full/partial/wrap |
| SU/MU | disabled/ineligible fallback, DL MU, UL Trigger, per-user failure |
| State freshness | CSI, BSR, OMI, TWT, Group ID, association epoch, channel width |
| Determinism | equal candidates, address/HOL tie-breaking, fixed seed/repetition |
| Ownership | prepare failure, partial commit failure, rollback, late callback |
| Callback compatibility | unchanged DCF callback plus typed VHT/HE/EHT HCF events |

## Stop conditions

Stop the current phase and diagnose before proceeding if:

- a patch changes RNG consumption, event count/order, timer scheduling, queue order,
  packet identity, or PHY vector without an approved behavior change;
- a packet can be deleted, dequeued, reinserted, retired, or acknowledged by two
  components;
- a scheduler or candidate builder mutates a queue, BA agreement, ACK state, retry
  counter, or radio;
- a provider uses a concrete HCF/frame-sequence/radio downcast or parent-module path;
- feature composition depends on adapter virtual dispatch, discovery order, an
  untyped registry, or duplicate exchange-class providers;
- the old and new paths maintain synchronized copies of protocol state;
- HE per-STA queues start or own contention independently of their EDCAF;
- a plan is committed before capability, PHY, TXOP, and response-duration validation;
- a fallback path starts a second TXOP or loses the winning AC;
- a semantic signal moves away from the action owner or is emitted twice;
- a fingerprint changes and the first changed event is not understood;
- a target file is sealed or the worktree contains overlapping user changes that
  cannot be preserved safely.

## Review gates

At the end of every ownership-moving phase, an independent reviewer must verify:

1. the target state has exactly one mutable owner;
2. callbacks name typed contracts rather than concrete classes;
3. packet and queue transactions have complete commit/rollback/terminal paths;
4. scheduler, plan validator, PHY finalizer, and exchange engine remain distinct;
5. amendment behavior is gated by mode, role, local and peer capabilities;
6. common code contains no VHT/HE concrete type branch that belongs in a provider;
7. NED declarations remain the external source of truth;
8. signals are emitted once by the action owner;
9. tests exercise the actual claim and the applicable WLAN boundaries;
10. no new exception-ledger or seal change is being smuggled into the patch.

For every IEEE 802.11 diff, emit the complete general architecture review checklist
and then the complete WLAN checklist with their prescribed summary footers.

## Completion criteria

The redesign is complete only when all of the following are true:

- `Hcf` is a small NED/OMNeT++ façade and composition root, not a protocol god object.
- `VhtHcf` and `HeHcf` contain no protocol algorithms and exist only as compatible
  configuration adapters.
- No production feature class inherits behavior from `Hcf`.
- `Ieee80211Mac` stores `IQosCoordinationFunction`, has no concrete HCF pointer or HE
  dispatch, and a non-`Hcf` implementation can satisfy the paired C++/NED contract.
- No VHT/HE/EHT frame sequence or service downcasts a callback to `Hcf`, `VhtHcf`, or
  `HeHcf`, and none locates collaborators by walking the parent module from a callback.
- The shared `IFrameSequenceHandler::ICallback` remains source-compatible and DCF
  retains its direct callback behavior; HCF typed events are an adapter-owned layer.
- HCF common fields are private to typed owners; the broad protected surface is gone.
- There is exactly one active-exchange/timer owner and exactly one owner for every
  mutable state listed in the ownership ledger.
- VHT Group ID, VHT/HE sounding, HE peer/queue state, HE DL, HE AP UL scheduling, and
  HE triggered STA response are separate cohesive components.
- Schedulers are side-effect-free proposal policies; plans are immutable and validated
  before packet/BA/queue commit; PHY mode/finalizer remains authoritative.
- `Hcf`, `VhtHcf`, and `HeHcf` typenames, current parameters, signals, statistics,
  effective configuration paths, and disabled-feature behavior remain compatible.
- Feature composition is selected through the interface-typed NED feature set, has
  explicit lifetime and fixed exchange classes, and is independent of adapter virtual
  factories, discovery order, and arbitrary registration priority.
- The Phase-0 dependency inventory is empty of unexplained concrete HCF users, and
  the implemented provider order matches the reviewed source/runtime evidence table.
- All required focused unit and module gates pass, deterministic feature simulations
  satisfy frame/timing/ownership invariants, and no fingerprint has an unexplained
  change.
- Release build, `git diff --check`, architecture checks with baseline reconciliation,
  feature-off build checks, and complete general/WLAN reviews are recorded.
- No fingerprint CSV, exception ledger, or sealing status was changed without explicit
  user approval.

There is deliberately no line-count target. Success is measured by explicit contracts,
single state ownership, independently testable amendment components, and the ability to
understand or extend one exchange family without reading the entire HCF hierarchy.
