# IEEE 802.11 HCF split implementation plan

## Purpose

This plan covers only Workstream H: finish the split already started around
`Hcf`. It is a behavior-preserving refactor of the supported HCF/EDCA paths.
It does not complete missing MAC features, add observations, or change IEEE
802.11 behavior.

The intended result is a smaller and clearer orchestration boundary:

- `Hcf` remains the NED-facing module and the bridge to OMNeT++ scheduling,
  packet ownership, EDCA, frame-sequence, Block Ack, and PHY services;
- exchange lifecycle, response policy, pure frame-preparation decisions,
  A-MPDU construction bookkeeping, and stateless implicit-Block-Ack retry work
  each have one authoritative owner;
- base HCF, VHT HCF, and HE HCF variants invoke the same lifecycle contract;
- supported exchanges retain identical frames, event timing, callbacks, retry
  behavior, random-number use, and results.

This document is self-contained: it defines scope, current ownership, target
ownership, patch order, evidence gates, tests, commands, architecture checks,
and completion criteria.

## Working-tree checkpoint

The plan is written against the current checkout, which is not clean. Before
starting the first production patch, preserve and validate the existing diff:

- `Hcf.cc` and `Hcf.h` currently contain a small typed data-service boundary
  change that replaces concrete `OriginatorQosMacDataService` access with
  `IOriginatorMacDataService` operations;
- the corresponding uncommitted edits are in
  `IOriginatorMacDataService.h`, `OriginatorMacDataService.h`,
  `OriginatorQosMacDataService.h`, and
  `OriginatorQosMacDataService.cc`;
- these edits are a useful prerequisite for reducing HCF coupling, but they are
  not the HCF god-object split itself and must remain separately attributable.

The implementation owner must first record the exact patch/base, build it, and
run the affected focused tests. The HCF split then starts from that verified
state; it must not silently reset, overwrite, or absorb unrelated working-tree
changes.

## Required outcome

At completion:

1. `HcfExchangeCoordinator` is the sole authority for exchange lifecycle state,
   active packet identity, expected response identity, and exchange timers.
2. `HcfResponseService` is the sole authority for response classification,
   deferred receive-timeout policy, and response-versus-timeout action order.
3. `HcfFramePreparation` contains only pure, deterministic, mode-independent
   preparation decisions. PHY legality, rate, mode, timing, and TXOP policy stay
   with their existing owners.
4. `HcfAggregationService` owns A-MPDU materialization and the temporary,
   non-owning aggregate-to-constituent ledger. Eligibility, duration trimming,
   Block Ack policy, packet ownership, and signal emission stay with their
   existing owners.
5. `HcfRetryService` remains stateless and limited to implicit-Block-Ack member
   matching and ordered calls into the real mutable-state owners.
6. Top-level HCF paths perform checked classification before accessing concrete
   frame fields, but retain every currently supported route and every current
   unsupported/error outcome.
7. `Hcf.h` includes only definitions required for inheritance, inline code,
   templates, and value members; pointer-only collaborators are forward
   declared where legal.
8. Focused unit tests, the release build, architecture checks, and the two
   selected HCF fingerprints have no unexplained failure or trajectory change.

There is no line-count target. A helper boundary is complete when ownership is
unambiguous, mutable state has one owner, and the remaining bridge code is
necessary.

## Strict scope

### Included

- [`Hcf`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h),
  [`HcfExchangeCoordinator`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangeCoordinator.h),
  [`HcfResponseService`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfResponseService.h),
  [`HcfFramePreparation`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfFramePreparation.h),
  [`HcfAggregationService`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfAggregationService.h),
  and [`HcfRetryService`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfRetryService.h);
- `VhtHcf` and `HeHcfTxRx` call sites only where necessary to preserve one
  common exchange-lifecycle contract;
- the existing `FrameSequenceHandler`, EDCA, recovery, Ack, Block Ack, and PHY
  contracts where HCF delegates to them;
- checked classification needed by at least three HCF dispatch sites;
- focused unit-test additions and narrow HCF fingerprint verification;
- direct-include cleanup after behavior and ownership stabilize.
- the ordinary single-user branch of `Hcf::transmitFrame` only where a typed,
  side-effect-preserving preparation boundary is demonstrated; HE/MU and
  amendment-specific branches remain with their specialized owners.

### Excluded

- completing non-QoS Data handling or changing any current non-QoS outcome;
- per-access-category signals, statistics, counters, or result filters;
- PCF, MCF, HCCA, polling, or controlled-channel-access implementation;
- new HE/EHT exchanges, OFDMA, MU-MIMO, UORA, TWT, sounding, or capability
  behavior;
- changes to PHY mode tables, rate selection, PPDU legality, duration
  calculation, or radio timing;
- new packet/header representations, MSG definitions, NED parameters, or
  signals;
- changes to sequence-number, retry-counter, contention, TXOP, or Block Ack
  semantics;
- general TODO cleanup, logging cleanup, performance work, or a new visitor or
  plugin framework;
- broad `Ieee80211SharedMacModes` or HE/EHT regression campaigns;
- fingerprint baseline updates. Any baseline change requires separate user
  approval after the first changed event has been explained.

If a proposed HCF extraction requires any excluded change, split it into a
separate workstream instead of widening this plan.

## No-change contract

Every production patch in this workstream must preserve:

- the exact transmitted and received frame sequence;
- packet bytes, header fields, tags, FCS handling, and A-MPDU member order;
- event numbers relative to the exchange, timer scheduling/cancellation order,
  and callback order;
- channel-access, backoff, retry, recovery, TXOP, and Block Ack decisions;
- random-number calls and the resulting RNG/backoff trajectory;
- packet ownership, duplication, deletion, and aggregate-ledger lifetimes;
- public and protected HCF APIs except for a demonstrably test-only facade;
- NED type selection, parameters, signals, statistics, and result names;
- currently supported, ignored, dropped, and rejected frame categories;
- base, VHT, and HE variant behavior.

An exact exception must be documented before implementation and moved out of
this workstream. “Architecturally cleaner” is not sufficient justification for
a simulation trajectory change.

## Current implementation baseline

| Area | Current owner and evidence | Remaining split issue |
| --- | --- | --- |
| Top-level orchestration | [`Hcf`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc) owns module callbacks and connects EDCA, frame sequences, services, recovery, Block Ack, and PHY logic. | It also retains lifecycle and response-policy decisions that overlap the newer helpers. |
| Exchange lifecycle | [`HcfExchangeCoordinator`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangeCoordinator.cc#L49) holds state, active packet identity, expected response identity, and timer objects. | `Hcf` still inspects coordinator state and invokes terminal operations; base, VHT, and HE code can invoke `beginTransmission` independently. |
| Response policy | [`HcfResponseService`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfResponseService.cc#L23) classifies responses and owns deferred-timeout policy. | `Hcf::handleMessage`, `processLowerFrame`, corruption handling, and callback-bundle construction still spread the service protocol across several paths. |
| Frame-sequence execution | `FrameSequenceHandler` owns operational steps, step history, and sequence callbacks. | This ownership must remain distinct from coordinator lifecycle state. |
| Frame preparation | [`HcfFramePreparation`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfFramePreparation.cc#L14) owns the pure initial-protection decision. | The remaining candidate extraction is small; most adjacent code has PHY, TXOP, sounding, or packet side effects and must stay outside the helper. |
| Aggregation | [`HcfAggregationService`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfAggregationService.cc#L20) builds A-MPDUs and records their constituent identities. | HCF still coordinates member mutation, duration trimming, construction, tagging, ledger use, and emission. Only construction and ledger handoff belong in the service. |
| Aggregate ledger | [`AmpduTransmissionLedger`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/AmpduTransmissionLedger.h) is non-owning and keyed by the transmitted aggregate packet. | Every terminal path must take or discard exactly once without changing caller ownership. |
| Retry helper | [`HcfRetryService`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfRetryService.cc#L18) processes implicit-Block-Ack members through callbacks. | It must not grow into a second retry-state owner. Generic retry handling stays in HCF plus EDCAF/recovery/Ack owners. |
| Header dependencies | [`Hcf.h`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h#L103) stores many collaborator pointers and three helper value members. | Pointer-only concrete includes can be moved to `Hcf.cc`; value members require complete definitions. |

Important lifecycle call sites to audit together are:

- channel-access request: `Hcf.cc`, `channelAccessRequested`, near line 412;
- awaiting a response: `Hcf.cc`, near lines 423–426;
- channel grant: `Hcf.cc`, near line 565;
- preparation: `Hcf.cc`, near line 988;
- base transmission transition: `Hcf.cc`, near line 1963;
- VHT transmission transition: `VhtHcf.cc`, near line 537;
- HE transmission transition: `HeHcfTxRx.cc`, near line 451;
- retry/recovery: `Hcf.cc`, near line 1654;
- completion/reset: `Hcf.cc`, near lines 1113–1128;
- abort: `Hcf.cc`, near line 1742.

Line numbers are navigation aids, not stable requirements. The implementing
patch must identify symbols again against its current parent commit.

## Target ownership model

| Component | Authoritative responsibility | Must not own |
| --- | --- | --- |
| `Hcf` | NED-facing module callbacks; OMNeT++ scheduling/cancellation bridge; packet ownership transfer; orchestration calls into EDCA, frame-sequence, Block Ack, recovery, and PHY authorities | Shadow lifecycle/timer state; response classification policy; duplicate aggregate ledger; duplicate retry counters |
| `HcfExchangeCoordinator` | Lifecycle transitions; active packet/exchange identity; expected response/step identity; timer objects; terminal idempotence and stale-callback rejection | Frame-sequence step execution; response contents; EDCAF retry or queue state; PHY timing calculation |
| `FrameSequenceHandler` | Operational frame-sequence steps, history, callbacks, and sequence completion | A parallel exchange lifecycle or response-timeout policy |
| `HcfResponseService` | Response classification; late/foreign/corrupt decisions; deferred-timeout state; ordered response/cancel/timeout actions | OMNeT++ module calls, packet ownership, coordinator lifecycle, PHY timer calculation |
| `HcfFramePreparation` | Pure deterministic decisions over immutable inputs | Packet ownership/mutation; module lookups; rate/mode legality; timers; TXOP, retry, Block Ack, or amendment state |
| `HcfAggregationService` | A-MPDU byte construction; FCS-preserving assembly; non-owning aggregate-to-constituent ledger | Eligibility; candidate selection; PHY/TXOP limits; agreement lifecycle; retry mutation; signal emission; caller packet ownership |
| `HcfRetryService` | Stateless matching and deterministic ordering of implicit-Block-Ack member callbacks | Retry counters, Ack status, sequence windows, queues, packets, or exchange state |
| `Edcaf` and recovery/Ack procedures | Queue, contention, TXOP, retry counters, Ack status, and terminal retry decisions | HCF lifecycle or response classification |
| Block Ack handlers | Agreement and reorder-window lifecycle | Generic HCF exchange state |
| PHY/mode owners | Rate, mode, PPDU legality, duration, and physical timing | HCF exchange state or aggregate ledger |

The typed `HcfResponseService::Actions` callback bundle is an acceptable
boundary. Replace it only if a smaller typed contract removes at least three
duplicated policy branches without moving module calls or packet ownership into
the service.

## Architecture and standards constraints

This refactor does not add normative behavior, but it must preserve the HCF and
EDCA architecture. At each affected exchange decision, keep the existing IEEE
802.11 revision/clause trace or add one if the code is otherwise ambiguous.
Do not claim HCCA completion: HCF contains EDCA and HCCA concepts, while this
workstream changes only supported HCF/EDCA orchestration.

Apply these architecture requirements during design and review:

- `AR-MOD-COMPOSITION`, `AR-MOD-PLUGGABLE`, `AR-ORG-CONTRACTS`, and
  `AR-COM-DIRECT`;
- `AR-WLAN-ARCH-BOUNDARIES`, `AR-WLAN-ARCH-OWNERSHIP`, and
  `AR-WLAN-ARCH-VARIANTS` for the base/VHT/HE relationship;
- `AR-WLAN-MAC-EXCHANGE` and `AR-WLAN-MAC-SEQUENCE`;
- `AR-WLAN-PHY-AUTHORITY` and `AR-WLAN-PHY-TIMING`;
- `AR-WLAN-STD-TRACE` and `AR-WLAN-STD-GATING`;
- `AR-QUAL-TESTS`, `AR-QUAL-DETERMINISM`, `AR-QUAL-TRACEABILITY`, and
  `AR-QUAL-NAMING`.

Packet-representation and observability requirements do not apply unless the
actual diff changes packet/header representation or signals. Such a change is
outside this plan and must not be smuggled into the refactor.

The current candidate files are unsealed, and no architecture or naming
exception covers this work. Recheck sealing and exception ledgers immediately
before the first production edit. Do not add or remove seals, and do not edit an
exception ledger as part of this workstream.

## Patch graph

Implement in this order. Keep each production patch reviewable and green before
starting the next one.

```text
H0 characterization tests
  -> H1 checked classification (only if shared by >=3 paths)
  -> H2 coordinator authority
  -> H3 response/timeout boundary
  -> H4 frame-preparation boundary audit
  -> H5 aggregation construction and ledger handoff
  -> H6 stateless retry boundary
  -> H7 include reduction and final verification
```

Only one production-code writer should operate at a time. A regression or
review lane may work concurrently only against a stable commit or diff.

## H0. Characterize the existing contracts first

### Files

- [`HcfExchangeCoordinator_1.test`](../tests/unit/HcfExchangeCoordinator_1.test)
- [`HcfResponseService_1.test`](../tests/unit/HcfResponseService_1.test)
- [`HcfFramePreparation_1.test`](../tests/unit/HcfFramePreparation_1.test)
- [`AmpduTransmissionLedger_1.test`](../tests/unit/AmpduTransmissionLedger_1.test)
- [`Ieee80211FecCodingReq_1.test`](../tests/unit/Ieee80211FecCodingReq_1.test)
- [`Ieee80211HtImplicitBlockAck_1.test`](../tests/unit/Ieee80211HtImplicitBlockAck_1.test)
- new `tests/unit/HcfFrameClassification_1.test` only if H1 adds a classifier

### Actions

1. Extend the coordinator test with duplicate `complete`, `abort`, and `reset`
   calls; stale callbacks after abort; stale packet and expected-step identity;
   and exactly-once terminal effects.
2. Extend the response-service test with late and foreign responses after timer
   cancellation, repeated cancel/timeout requests, no-active-sequence timeout,
   and exact callback/action ordering.
3. Extend the frame-preparation table with:
   - HT plus `NON_HT_MIXED` unicast -> legacy RTS/CTS;
   - multicast and broadcast -> no protection;
   - missing negotiated HT capability -> no protection;
   - non-HT PHY or protection mode off -> no protection.
4. Preserve the existing ledger tests for non-owning record overwrite,
   implicit-Block-Ack flag, constituent pointer order, atomic take, and
   idempotent discard. Add cases only where a later call-site change needs a
   previously unobserved invariant.
5. Extend the implicit-Block-Ack retry test so an unrelated in-progress member
   remains untouched, every failed member invokes the applicable callback once,
   the retired vector contains the original pointers in deterministic order,
   and an optional rate-control callback fires once per applicable member.

### Exit gate

The tests record current behavior without changing production code. If a
desired assertion contradicts current behavior, document the contradiction;
do not silently turn this refactor into a correctness change.

## H1. Centralize checked frame classification where it pays for itself

### Candidate files

- `Hcf.h` and `Hcf.cc`
- optional new `HcfFrameClassifier.h` and `HcfFrameClassifier.cc`
- `tests/unit/HcfFrameClassification_1.test`

### Actions

1. Inventory classification in upper-frame, lower-frame, transmitted-frame,
   failed-frame, and received-response paths.
2. Introduce a small enum/value classifier only if at least three independent
   HCF consumers need the same categorization. Otherwise use local checked
   conversions and add no abstraction.
3. Distinguish currently recognized Data, QoS Data, management, Ack, RTS, CTS,
   BlockAckReq, supported Block Ack variants, aggregate containers, and known
   headerless indications at their existing boundaries.
4. Use checked conversion before concrete-field access. Keep Block Ack variant
   decoding explicit because variants expose different fields.
5. Return a route/category only. The classifier must not own packets, mutate
   headers, select an AC, change retry policy, or execute a frame sequence.
6. Preserve the current handling of subtype-0 non-QoS Data, malformed
   type/concrete-class mismatches, and unsupported subtypes. This patch must not
   complete or broaden non-QoS support.

### Exit gate

At least three call sites share the classifier, all concrete access is checked,
and every pre-existing route has the same process/ignore/drop/error outcome.
If the reuse threshold is not met, close H1 with local checked casts and a
documented decision not to add the helper.

## H2. Make `HcfExchangeCoordinator` authoritative

### Files

- `HcfExchangeCoordinator.h` and `HcfExchangeCoordinator.cc`
- `Hcf.h` and `Hcf.cc`
- `VhtHcf.cc`
- `HeHcfTxRx.cc`
- `HcfExchangeCoordinator_1.test`

### Actions

1. Write the legal state table for request, grant, preparation, transmission,
   awaiting response, retry/recovery, completion, abort, and reset.
2. Add explicit coordinator operations for terminal idempotence and stale
   callback decisions so `Hcf` no longer reads raw state to reproduce them.
3. Route each actual exchange action through exactly one lifecycle transition.
4. Audit the base, VHT, and HE `beginTransmission` call sites together. Some
   callbacks reach specialized transmit paths before falling back to the base;
   prove the runtime path before removing a call.
5. Keep active packet identity, expected response/step identity, and timer
   objects in the coordinator. Keep only `scheduleAt`, `scheduleAfter`, and
   `cancelEvent` execution in the module bridge.
6. Preserve `FrameSequenceHandler` as the owner of operational sequence steps
   and history. Do not mirror those steps in coordinator state.
7. Make completion and abort release the exchange, EDCAF/TXOP, and contention
   continuation at most once.
8. Do not make permissive transitions merely to tolerate duplicate callers.
   First consolidate callers, then narrow the accepted state table where the
   characterization tests prove it safe.

### Mandatory evidence gate

Before deleting or moving a `beginTransmission` call, capture one focused state
trace for each affected base, VHT, and HE callback path. The trace must identify
the entry symbol, coordinator state before and after the call, active packet,
expected step, and scheduled timer. If a unit seam cannot expose this without
changing production behavior, add one narrow module test rather than using the
broad shared-modes campaign.

### Exit gate

The coordinator is the only lifecycle-state authority; duplicate and stale
terminal callbacks cannot cause a second transition or side effect; base, VHT,
and HE paths retain their specialized transmission behavior.

## H3. Finish the response and timeout boundary

### Files

- `HcfResponseService.h` and `HcfResponseService.cc`
- `Hcf.h` and `Hcf.cc`
- `HcfResponseService_1.test`
- `FrameSequenceHandlerAbortCallback_1.test` if the terminal callback seam is
  affected

### Actions

1. Route `Hcf::handleMessage`, `processLowerFrame`, and corrupted-frame handling
   through one response-service protocol.
2. Keep response classification, late/foreign decisions, corrupt-response
   decisions, deferred timeout state, and response-versus-timeout ordering in
   `HcfResponseService`.
3. Keep coordinator state and timer identity in `HcfExchangeCoordinator`.
4. Keep packet ownership, `FrameSequenceHandler` calls, and OMNeT++ event
   scheduling/cancellation in `Hcf` through the typed `Actions` adapter.
5. Construct the action adapter once per orchestration entry or store one safe
   adapter if its lifetime is explicit. Do not create callbacks that can outlive
   their owning module.
6. Preserve the effective order encoded by the current service:
   classification and response processing occur before applicable cancellation,
   and a deferred receive timeout is serviced only at its existing precedence
   point.
7. Preserve headerless accepted responses, corrupt frames, late frames, foreign
   frames, frames not addressed to the active exchange, and inactive-sequence
   handling.

### Mandatory evidence gate

Before changing the order of `processResponseAndCancel...` or
`handleDeferredStartRxTimeout`, capture the existing ordered action list for:

- a valid response arriving before timeout;
- timeout becoming due while reception is active;
- a corrupt frame at the response boundary;
- a late or foreign response after cancellation.

Any order change is out of scope unless the existing order is first proven to
be a bug and approved as a separate behavior change.

### Exit gate

HCF supplies effects through one adapter but contains no response-policy branch
duplicated from the service. Each timer is scheduled/cancelled at most once and
each terminal frame-sequence callback occurs at most once.

## H4. Close the pure frame-preparation boundary

### Files

- `HcfFramePreparation.h` and `HcfFramePreparation.cc`
- `Hcf.h` and `Hcf.cc`
- `HcfFramePreparation_1.test`

### Actions

1. Audit the wrapper around `selectInitialProtection` and the adjacent
   `startFrameSequence` code.
2. Remove a redundant HCF wrapper or move immutable input extraction only when
   the same inputs and enum result are preserved exactly.
3. Extract an additional decision only if it has no packet ownership transfer,
   mutation, timer, queue, TXOP, retry, Block Ack, amendment-specific state,
   module lookup, or PHY/rate/duration authority.
4. Leave `setFrameMode`, rate selection, `ModeReq` stamping, sounding, TXOP
   setup, PHY legality, and duration calculations with their current owners.
5. Add a side-effect-free table test for every extracted decision.

### Exit gate

The protection decision has a clear pure boundary and the adjacent mutable or
PHY-authoritative work remains outside it. It is acceptable—and preferable—to
finish H4 with a documented “no further safe extraction” result if no code
meets all criteria.

## H5. Close aggregation construction and ledger handoff

### Files

- `HcfAggregationService.h` and `HcfAggregationService.cc`
- `AmpduTransmissionLedger.h` and `AmpduTransmissionLedger.cc`
- `Hcf.h` and `Hcf.cc`
- `Ieee80211FecCodingReq_1.test`
- `AmpduTransmissionLedger_1.test`

### Actions

1. Keep candidate eligibility, Block Ack agreement decisions, retry-bit/header
   mutation, PHY/TXOP duration trimming, and selected mode in HCF and their
   current authorities.
2. Pass preselected, fully prepared constituent frames and required immutable
   construction inputs to `HcfAggregationService`.
3. Make aggregate materialization and ledger recording one explicit service
   operation, preserving member order, byte content, FCS behavior, tags, and
   the original aggregate `Packet *` identity used as the ledger key.
4. Make success/take, failure/discard, implicit-Block-Ack completion, and
   abort/destruction cleanup explicit. A ledger entry must be consumed or
   discarded exactly once.
5. Preserve the ledger as non-owning. The service must not delete constituent
   packets or extend their lifetime.
6. Keep aggregate signal emission in HCF after successful construction; do not
   add, rename, or move signals.
7. If `Hcf::buildAmpduPacket` has no production consumer, update the focused FEC
   test to exercise `HcfAggregationService` directly, then remove the test-only
   facade. Do not remove any facade that has a production or subclass consumer.

### Mandatory evidence gate

Before moving any member mutation or duration-trimming code, compare normal
Block Ack and implicit-Block-Ack paths for:

- constituent pointer identity and order;
- header and Retry-bit mutation order;
- duration calculation and last-member trimming;
- temporary packet creation/deletion;
- ledger record, take, and discard keys;
- FEC and mode tags.

If the two paths do not share identical prerequisites and ownership, leave the
mutation/trim logic in HCF. The split can be complete without moving it.

### Exit gate

There is one construction implementation and one ledger. Every terminal path
consumes or discards its entry once, normal and implicit Block Ack retain their
current bytes and member identity, and HCF still owns policy and side effects.

## H6. Constrain the stateless retry boundary

### Files

- `HcfRetryService.h` and `HcfRetryService.cc`
- `Hcf.h` and `Hcf.cc`
- `Ieee80211HtImplicitBlockAck_1.test`

### Actions

1. Keep the service limited to matching implicit-Block-Ack timeout members and
   invoking supplied operations in deterministic order.
2. Keep retry counters and CW state in EDCAF/recovery procedures, Ack status in
   Ack handlers, agreement/windows in Block Ack handlers, packet ownership and
   rate-control notifications in their current orchestration owners, and
   lifecycle state in the coordinator.
3. Remove duplicated pure matching/retirement calculations from HCF only when
   the same callback order and original packet pointers are preserved.
4. Do not add stored packets, counters, windows, timers, module pointers, or
   exchange state to `HcfRetryService`.
5. Include the service from `Hcf.cc`, not `Hcf.h`, if it remains static-only and
   is not part of the class declaration.

### Exit gate

The helper is stateless, unrelated members are untouched, each applicable
member is processed once, and the existing mutable owners receive the same
calls in the same order. If the audit finds no duplicate pure calculation, H6
closes with tests and boundary documentation rather than forced production
movement.

## H7. Reduce `Hcf.h` coupling

### Files

- `Hcf.h` and `Hcf.cc`
- affected HCF variant translation units and focused tests

### Actions

1. Inventory every direct include in `Hcf.h` and record why the declaration
   requires it.
2. Forward-declare pointer/reference-only collaborator types where legal and
   move their concrete includes to `Hcf.cc`.
3. Retain complete definitions required by inheritance, inline functions,
   templates, and value members such as the coordinator, response service, and
   aggregation service.
4. Do not replace value members with pointers merely to remove includes.
5. Do not change public/protected APIs, virtual dispatch, or object lifetime for
   a dependency-count target.
6. Compile direct consumers that previously may have depended on transitive
   includes, especially the HCF aggregation/FEC and implicit-Block-Ack tests.
7. Record the before/after direct include count. Claim dependency reduction,
   not faster compilation, unless a separate reproducible incremental-build
   measurement demonstrates it.

### Exit gate

`Hcf.h` exposes only declaration-required types, all consumers include what
they use, and the release build succeeds without public API or behavior change.

## Verification strategy

### Per-patch gates

For every patch:

1. run the narrowest affected unit test first;
2. run the full focused HCF unit-test filter;
3. build release mode when production C++ or headers changed;
4. inspect packet ownership and timer/callback order in the diff;
5. run the architecture audit after the production diff stabilizes;
6. run the two selected fingerprints at the integration boundary;
7. obtain independent regression/review evidence before merging the completed
   workstream.

Use deterministic fixtures. A test must assert states, actions, original packet
pointers, member order, and callback counts—not only absence of exceptions.

### Focused unit matrix

| Test | Required invariant |
| --- | --- |
| `HcfExchangeCoordinator_1.test` | Legal lifecycle, active identity, stale rejection, terminal idempotence, reset, and exactly-once effects |
| `HcfResponseService_1.test` | Classification, corrupt/late/foreign handling, deferred timeout, cancellation, and exact action order/count |
| `HcfFramePreparation_1.test` | Pure protection-decision boundary table and no side effects |
| `HcfFrameClassification_1.test` | Checked categories and unchanged route outcome, if a shared classifier is added |
| `AmpduTransmissionLedger_1.test` | Non-owning record/take/discard, exact key/member identity, order, and idempotence |
| `Ieee80211FecCodingReq_1.test` | Aggregate bytes plus FEC/mode tag propagation through the service boundary |
| `Ieee80211HtImplicitBlockAck_1.test` | Retry-limit boundary, unrelated member preservation, original pointers, and once-per-member callbacks |
| `FrameSequenceHandlerAbortCallback_1.test` | Failed -> aborted -> finished order and exactly-once terminal callback when affected |

### Build and unit commands

Run from the repository root:

```sh
make MODE=release -j$(nproc)

inet_run_unit_tests -m release \
  -f '(HcfExchangeCoordinator|HcfResponseService|HcfFramePreparation|AmpduTransmissionLedger|Ieee80211FecCodingReq|Ieee80211HtImplicitBlockAck|HcfFrameClassification).*\.test'
```

If no classifier test is added, remove `HcfFrameClassification` from the
filter. Preserve the exact working directory, command, exit status, and result
artifact paths in the implementation report.

### Focused integration gates

There is no narrow existing HCF module test. Do not use
`Ieee80211SharedMacModes_1.test` as the strict gate because it expands into a
broad legacy/HT/VHT/HE/EHT matrix. Add one narrow module test only if a required
state/timer invariant cannot be observed through the unit seams.

Use the existing fragmentation showcase fingerprints as the focused
module-level exchange gates, one configuration and run at a time:

```sh
inet_run_fingerprint_tests -m release --no-build --no-concurrent \
  -w 'showcases/wireless/fragmentation' -i 'omnetpp.ini' \
  -c 'HCFfrag' -r 0

inet_run_fingerprint_tests -m release --no-build --no-concurrent \
  -w 'showcases/wireless/fragmentation' -i 'omnetpp.ini' \
  -c 'HCFfragblockack' -r 0
```

These are the existing rows in
[`tests/fingerprint/showcases.csv`](../tests/fingerprint/showcases.csv). Keep
the checked-in fingerprints unchanged. A mismatch is a failed gate until the
first changed event is located and explained; an explained mismatch still does
not authorize updating the CSV.

If a patch touches a frame path shared with DCF, additionally run the existing
`DCFnoFrag` and `DCFfrag` configurations. Otherwise they are outside the strict
HCF gate.

### Architecture command

Run after production changes stabilize:

```sh
bash .agents/skills/inet-architectural-requirements/references/enforcement/check-architecture.sh \
  src/inet/linklayer/ieee80211
```

Then perform the semantic review the script cannot provide: authoritative
ownership, base/VHT/HE variant consistency, direct contracts, packet lifetime,
PHY authority, timer ordering, deterministic behavior, traceability, naming,
and current sealing status.

## Stop and investigate conditions

Stop the current patch before further refactoring if any of these occurs:

- base, VHT, and HE paths show different reasons for duplicate lifecycle calls;
- a response test changes process/cancel/deferred-timeout order;
- a timer is scheduled, cancelled, or serviced more or fewer times;
- a packet pointer, A-MPDU member order, tag, FCS, or header byte changes;
- aggregate ledger state survives completion/abort or is consumed twice;
- a retry callback moves to a different mutable owner or changes order/count;
- the change adds an event, RNG call, queue operation, or frame-sequence step;
- a focused fingerprint changes;
- safe completion would require a non-QoS, per-AC, HCCA, PHY, or new HE/EHT
  behavior change.

Use focused Cmdenv logs or event logs to identify the first divergent action.
Use packet captures when frame bytes or exchanges differ. Escalate to source
debugging only when those artifacts cannot establish the callback/state cause.
Do not mask a divergence by weakening tests or updating fingerprints.

## Review checklist

### Ownership

- [ ] Every mutable state item has one authoritative owner.
- [ ] `Hcf` is an effect/orchestration bridge, not a second policy owner.
- [ ] Coordinator state and frame-sequence operational state are not mirrored.
- [ ] Response policy exists only in `HcfResponseService`.
- [ ] Retry counters, Ack status, Block Ack windows, and EDCAF state remain with
      their established owners.
- [ ] PHY/rate/duration authority has not moved into an HCF helper.

### Lifecycle and timing

- [ ] One lifecycle transition occurs per exchange action.
- [ ] Duplicate/stale complete and abort callbacks are harmless and explicit.
- [ ] Active packet and expected-step identity are validated at boundaries.
- [ ] Response processing, timer cancellation, and deferred timeout retain their
      established order.
- [ ] Every timer and terminal callback has an exactly-once test.

### Packets and aggregation

- [ ] Checked conversion precedes concrete header access.
- [ ] Existing unsupported/error outcomes are unchanged.
- [ ] Candidate selection and duration trimming remain outside the aggregation
      construction service.
- [ ] Aggregate bytes, tags, FCS, constituent identity, and order are unchanged.
- [ ] Ledger entries are non-owning and taken/discarded once on every terminal
      path.

### Variants and dependencies

- [ ] Base, VHT, and HE transition paths use the same coordinator contract.
- [ ] Specialized VHT/HE transmission and PHY callbacks remain intact.
- [ ] Pointer-only collaborators are forward declared only where legal.
- [ ] Value-member and inheritance definitions remain directly included.
- [ ] Consumers no longer rely on accidental transitive includes.

### Scope and quality

- [ ] No non-QoS completion, per-AC metric, HCCA, PHY, or feature change entered
      the diff.
- [ ] No NED, MSG, signal/statistic, or packet-representation change entered the
      diff.
- [ ] No seal or exception ledger was modified.
- [ ] Focused tests, release build, architecture audit, and HCF fingerprints
      pass without unexplained differences.
- [ ] Fingerprint CSV files remain unchanged.

## Completion criteria

Workstream H is complete only when all of the following are true:

1. the target ownership table describes the implementation without exceptions
   hidden in `Hcf` or a variant subclass;
2. the coordinator is the sole lifecycle/timer-identity authority and terminal
   actions are exactly once;
3. the response service owns response and deferred-timeout policy while HCF
   performs only typed effects;
4. the frame-preparation boundary contains only pure decisions, with any
   deliberately retained code documented by ownership reason;
5. aggregation construction and the non-owning ledger have one implementation
   and explicit terminal cleanup, while policy and packet ownership remain in
   HCF/current owners;
6. the retry helper is demonstrably stateless;
7. checked classification is reused where justified, or the no-helper decision
   is documented with local checked access;
8. `Hcf.h` has no avoidable concrete dependency and no consumer relies on a
   transitive include;
9. every no-change invariant has focused evidence;
10. the release build, focused unit suite, architecture audit, `HCFfrag`, and
    `HCFfragblockack` pass with checked-in fingerprints unchanged;
11. an independent review confirms scope, ownership, timing, packet lifetime,
    and base/VHT/HE consistency.

The implementation report should list each patch, parent commit, exact commands
and exit statuses, artifact paths, ownership decisions, intentionally retained
bridges, and any deferred work. Deferred non-QoS, observability, HCCA, PHY, or
feature work must remain separate from the HCF split.
