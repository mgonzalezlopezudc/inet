# IEEE 802.11 non-QoS, HCF split, and per-AC implementation plan

## Purpose

This plan completes exactly three related pieces of the IEEE 802.11 MAC work:

1. make the supported non-QoS Data path complete when a QoS STA uses HCF/EDCA;
2. finish the split already started around `Hcf` without changing supported MAC behavior;
3. add only the per-access-category observations that are both missing and useful.

The plan is self-contained. It defines the supported behavior, current change
surface, patch order, architecture constraints, tests, commands, and completion
criteria. It does not depend on completing any other phase of the broader SWOT
program.

## Required outcome

At completion:

- a QoS STA can transmit and receive supported subtype-0 non-QoS Data without
  falling into a TID lookup, QoS retry procedure, or generic cast/error fallback;
- individually addressed non-QoS Data completes through normal Ack, timeout,
  retry, and retry-limit handling, while group-addressed Data completes without
  soliciting an Ack;
- `Hcf` remains the NED-facing module and callback bridge, but exchange state,
  response/timeout handling, frame preparation, aggregation bookkeeping, and
  stateless retry work have one explicit owner each;
- the HCF top-level paths perform checked classification and delegate to those
  owners;
- the selected observations can be queried by AC without reading private state;
- existing frame bytes, event timing, RNG use, NED type selection, public
  parameters, and existing signal/statistic names remain unchanged unless a
  patch explicitly documents a correctness fix;
- focused tests, legacy tests, architecture checks, semantic review, and the
  relevant fingerprints have no unexplained failures.

## Strict scope

### Included

- subtype-0 Data handled by a QoS STA through `Hcf` and EDCA;
- upper-frame classification and queue selection;
- shared sequence-number assignment for management and non-QoS Data;
- Normal Ack, timeout, retry-bit, SRC/LRC, retry-limit, and terminal-drop paths;
- unicast, multicast, and broadcast behavior;
- receive-side delivery and duplicate handling needed by that path;
- completion of the existing `HcfExchangeCoordinator`,
  `HcfResponseService`, `HcfFramePreparation`, `HcfAggregationService`, and
  `HcfRetryService` boundaries;
- safe, centralized frame classification used by the affected HCF paths;
- reduction of avoidable `Hcf.h` dependencies after ownership is stable;
- precise per-AC MPDU attempts/retries, losing internal collisions, TXOP MPDU
  count, and A-MPDU construction context;
- tests and validation needed to prove these changes.

### Excluded

- PCF, MCF, HCCA, polling, and controlled-channel-access implementation or API
  cleanup;
- new HE/EHT exchanges, OFDMA scheduling, MU-MIMO behavior, UORA, TWT, sounding,
  or PHY capability work;
- PHY mode, timing, RU, serialization, or external table changes;
- broad error-policy cleanup, broad TODO cleanup, logging cleanup, or signal
  inventory work;
- performance optimization or benchmarking;
- a new visitor framework, plugin interface, NED-replaceable helper family, or
  zero-time helper messages;
- changes below `src/inet/common/packet/`;
- fingerprint baseline updates. Such updates require separate approval after
  the first changed event is explained.

## Current implementation baseline

The current tree is already ahead of the earlier SWOT assessment. Implementation
must extend the existing boundaries rather than recreate them.

| Area | Current fact | Remaining issue |
| --- | --- | --- |
| HCF upper classification | [`Hcf::processUpperFrame`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc#L350) sends every concrete `Ieee80211DataHeader` to `Edca::classifyFrame`, which maps `getTid()` to an AC. | A subtype-0 Data header has no valid QoS TID contract. It needs an explicit supported route before TID mapping. |
| EDCA mapping | [`Edca::mapTidToAc`](../src/inet/linklayer/ieee80211/mac/channelaccess/Edca.cc#L32) centrally implements Table 10-1. | Preserve this single owner; do not duplicate TID-to-AC mapping in HCF. |
| Sequence assignment | [`QoSSequenceNumberAssignment`](../src/inet/linklayer/ieee80211/mac/sequencenumberassignment/QoSSequenceNumberAssignment.cc#L13) already routes non-QoS and group-addressed frames through the shared sequence space. | Lock the behavior down with wrap-around and retransmission-identity tests; do not add a second allocator. |
| Ack tracking | [`QosAckHandler`](../src/inet/linklayer/ieee80211/mac/originator/QosAckHandler.cc#L36) already has a management/non-QoS key space. | HCF must consistently select this path based on subtype, not merely on the concrete Data class. |
| Recovery | [`NonQosRecoveryProcedure`](../src/inet/linklayer/ieee80211/mac/originator/NonQosRecoveryProcedure.cc#L110) already owns SRC/LRC, Ack success, multicast completion, retry-limit, and CW updates. | Several HCF branches still send all Data headers to `QosRecoveryProcedure` or retain non-QoS TODO/error fallbacks. |
| Frame-sequence context | [`Hcf::buildContext`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc#L598) and [`HcfFs`](../src/inet/linklayer/ieee80211/mac/framesequence/HcfFs.cc#L63) currently assume the QoS context used by an EDCA TXOP. | A focused runtime test must establish whether subtype-0 Data can use that context unchanged or needs a narrowly typed non-QoS variant. Do not infer the answer from class names. |
| Exchange coordinator | [`HcfExchangeCoordinator`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangeCoordinator.h#L31) owns exchange lifecycle state, active packet identity, expected response identity, and timer objects. | HCF still performs some transition/idempotence decisions and timer plumbing around it. |
| Response service | [`HcfResponseService`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfResponseService.h#L31) owns response classification and deferred timeout state. | HCF still assembles callback actions and contains response-route plumbing that should be reduced to a bridge. |
| Frame preparation | [`HcfFramePreparation`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfFramePreparation.h#L25) contains one stateless protection decision. | Move only additional pure, mode-independent base-HCF preparation decisions that are demonstrably present in `Hcf`; do not move PHY legality or amendment state. |
| Aggregation | [`HcfAggregationService`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfAggregationService.h#L32) builds A-MPDUs and owns the non-owning aggregate-to-constituent ledger. | Remove remaining duplicate construction/bookkeeping from HCF, while leaving eligibility, Block Ack lifecycle, packet ownership, and signal emission with their current owners. |
| Retry helper | [`HcfRetryService`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfRetryService.h#L38) handles the stateless member-matching sequence for implicit Block Ack timeout. | Keep it stateless; ordinary mutable retry state remains in the existing recovery and EDCAF owners. |
| Header coupling | [`Hcf.h`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h#L17) includes many concrete collaborators. | Forward-declare pointer-only types after the ownership moves; retain full definitions for value members and inheritance requirements. |
| Per-AC attempts/retries | [`Edcaf.ned`](../src/inet/linklayer/ieee80211/mac/channelaccess/Edcaf.ned#L40) records `packetSentToPeer` and Retry-bit filters at each `edcaf[*]` path. | These count top-level transmitted packets, not constituent MPDU attempts inside an A-MPDU. Preserve them for compatibility and add a precise MPDU event. |
| Retry-limit drops | Each EDCAF contains a `QosRecoveryProcedure` with `retryLimitReached`; its NED statistic is already per AC. | No new QoS retry-limit signal is needed. Non-QoS Data must use the shared management/non-QoS procedure and retain its documented AC context at the HCF drop site. |
| Internal collisions | HCF emits aggregate `edcaCollisionDetected` with the number of colliding EDCAFs. | It does not identify each losing AC. Add one canonical event at each losing EDCAF decision. |
| TXOP metrics | Each EDCAF has a `TxopProcedure`; `txopStarted`, `txopEnded`, and `txopDuration` already exist. | Add initiated MPDU count per TXOP; do not duplicate duration. |
| Aggregation metrics | HCF already emits `ampduCreated` and `ampduNumMpdus`. | Add AC context to the single HCF-owned construction event and derive per-AC statistics without moving aggregation policy into EDCA. |
| Drop reasons | MAC/HCF already expose reason-filtered aggregate drops, and per-AC QoS retry-limit drops already exist. | Do not add generic per-AC receive drops: `NOT_ADDRESSED_TO_US` and duplicate reception are not transmit-AC outcomes. Add no new drop reason unless the emitting owner can identify the AC unambiguously. |

## Normative and architectural contracts

Use IEEE Std 802.11-2024 as the normative baseline. The generated standards
corpus is current; the relevant source chunks are shown so implementation review
can reproduce the lookup without consulting a PDF.

| Contract | Standard evidence | Implementation consequence |
| --- | --- | --- |
| Subtype-0 Data remains valid | 10.2.3.1, corpus `80211ax-2024:chunk:05035` | A QoS STA can use subtype-0 Data for compatibility, especially group-addressed traffic. Do not reject it merely because HCF is present. |
| Shared sequence space | 10.3.2.14.2 and Table 10-5, chunks `05109` and `05111` | Management and applicable non-QoS Data use the shared modulo-4096 sequence space. Retransmission retains the same sequence/fragment identity. |
| Ack behavior | 10.3.2.11, chunk `05090` | Individually addressed non-QoS Data expects immediate Ack. Group-addressed Data receives no Ack/BlockAck response and completes locally without an Ack timeout. |
| Non-QoS recovery | 10.3.4.4, chunk `05129` | Failed immediate-Ack MPDUs update SRC/LRC according to the RTS threshold, set Retry on retransmission, and stop once the applicable limit or lifetime is reached. |
| EDCA ownership | 10.23.2.1/.2/.4, chunks `05249`, `05252`, and `05254` | Four EDCAFs own their queue/contention/TXOP state. A losing internal collision updates retry/CW state but does not itself set the Retry header bit. |
| TXOP bounds | 10.23.2.9, chunk `05264` | A TXOP metric spans acquisition through relinquishment. A zero limit has defined one-frame semantics and is not unlimited. |

The primary architecture rules are:

- `AR-MOD-COMPOSITION`, `AR-ORG-CONTRACTS`, `AR-COM-DIRECT`;
- `AR-OBS-SIGNALS`, `AR-OBS-NED-TRUTH`;
- `AR-QUAL-TESTS`, `AR-QUAL-FINGERPRINT`, `AR-QUAL-DETERMINISM`,
  `AR-QUAL-NAMING`, `AR-QUAL-LOGGING`, `AR-QUAL-TRACEABILITY`;
- `AR-WLAN-STD-TRACE`, `AR-WLAN-ARCH-BOUNDARIES`,
  `AR-WLAN-ARCH-OWNERSHIP`, `AR-WLAN-FRAME-REPRESENTATION`;
- `AR-WLAN-MAC-EXCHANGE`, `AR-WLAN-MAC-SEQUENCE`,
  `AR-WLAN-MAC-QOS`, `AR-WLAN-OBS-EVENTS`, and
  `AR-WLAN-QUAL-TESTS`.

Only `src/inet/common/packet/` is currently sealed. All planned production
targets are outside that sealed subtree and are currently unsealed. Recheck
[`sealing-status.md`](../.agents/skills/inet-architectural-requirements/references/sealing-status.md)
immediately before each source patch. If the status changes, stop and request
permission for every newly sealed target. Never edit generated `*_m.h` or
`*_m.cc` files.

## Behavioral decisions fixed by this plan

These decisions prevent individual patches from inventing incompatible rules.

1. A subtype-0 Data frame entering HCF uses `AC_BE` as the explicit supported
   fallback because it carries no QoS Control TID. This is an INET model policy,
   not a claim that subtype-0 encodes AC_BE on air. Frames carrying a valid QoS
   TID continue through the existing centralized Table 10-1 mapping.
2. The concrete chunk class and frame-control subtype must agree. Classification
   may inspect `getType()`, but every route that accesses concrete fields uses a
   checked conversion and rejects a mismatch at the boundary.
3. QoS and non-QoS Data may share the concrete `Ieee80211DataHeader` class, but
   they do not share recovery key space: subtype, not concrete class alone,
   selects the procedure.
4. Individually addressed subtype-0 Data uses Normal Ack only in this work and
   is exercised against a non-QoS peer. This plan completes handling of a frame
   already selected for that compatibility path; it does not add peer-capability
   discovery or change the upper encapsulation policy. Block Ack and A-MPDU
   eligibility remain QoS-only capability boundaries.
5. Group-addressed subtype-0 Data never starts an Ack timer, never enters a retry
   loop, and is retired after its transmission callback. This means successful
   MAC transmission, not confirmed delivery by every receiver.
6. An exchange may complete or abort at most once. Duplicate late callbacks are
   ignored only when they are demonstrably stale; impossible active-exchange
   transitions remain programming errors.
7. Existing public signals and statistics remain available. New observations
   supplement their precision without silently changing their meaning.

## Patch sequence and dependencies

Implement one production writer at a time. Every numbered patch must be
reviewable and pass its narrow test before the next patch begins.

```text
N0 characterization tests
 ├─ N1 classification and sequence contract
 └─ N2 Ack/recovery and group completion
      └─ N3 receive/delivery completion

H0 checked frame classifier
 └─ H1 authoritative exchange coordinator
     └─ H2 response/timeout boundary
         ├─ H3 preparation boundary
         ├─ H4 aggregation/retry boundary
         └─ H5 Hcf.h dependency reduction

O0 metric contract tests
 └─ O1 attempts + collisions
     └─ O2 TXOP + A-MPDU per-AC context
```

`N0` and `O0` may be prepared as tests before production work. The `H*` patches
must remain behavior-preserving; do not mix their fingerprints with the
intentional behavior correction in `N*`.

## Workstream N: complete non-QoS Data handling

### N0. Add characterization and failing behavior tests

Add a focused `HcfNonQosData` unit/module fixture rather than extending a broad
HE matrix. Configure a QoS STA so HCF exists, then inject subtype-0 Data without
a `UserPriorityReq`.

Required cases:

- AP and non-AP originator roles;
- unicast success to a non-QoS peer: `ST_DATA`, `AC_BE`, shared sequence
  assignment, one Ack, retirement from in-progress state, no Block Ack state;
- Ack timeout: same sequence/fragment identity, Retry set only on the actual
  retransmission, correct SRC/LRC selection, and exactly one terminal drop at
  the configured limit;
- multicast and broadcast: no Ack response, no Ack timer, no retry, immediate
  local retirement after transmit;
- receive-side delivery of unicast, multicast, and broadcast subtype-0 Data;
- sequence wrap-around and last-sequence-per-RA collision handling;
- a frame-control/concrete-chunk mismatch and an unsupported Block Ack request
  for subtype-0 Data, each producing the documented boundary result rather than
  undefined access;
- the exact frame-sequence context, expected response, and timeout callback used
  by subtype-0 Data, so any `HcfFs` context change is evidence-driven;
- one legacy `qosStation=false` DCF case to prove that the HCF fix does not
  replace or alter the ordinary non-QoS stack.

The first version of the test should expose the current failures and record the
first failing branch. It must not weaken assertions to accommodate generic
`Unknown frame` errors.

### N1. Make classification and sequence ownership explicit

Target:

- [`Hcf.cc`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc);
- [`Edca.cc`](../src/inet/linklayer/ieee80211/mac/channelaccess/Edca.cc) and
  [`Edca.h`](../src/inet/linklayer/ieee80211/mac/channelaccess/Edca.h) only if
  the classification API must expose the explicit subtype-0 route;
- the existing sequence-assignment implementation only if a focused test proves
  a real defect.

Actions:

1. Classify management, QoS Data, and subtype-0 Data explicitly before queue
   lookup.
2. Route only QoS Data through `mapTidToAc`; route supported subtype-0 Data to
   `AC_BE` without calling `getTid()` as a classification input.
3. Keep classification before per-STA/shared queue selection and retain the AC
   for the full exchange; never reconstruct it later from packet fields.
4. Verify that `QoSSequenceNumberAssignment` remains the single shared allocator
   for management and subtype-0 Data in a QoS STA.
5. Do not allocate a new sequence number on retry or after moving a frame between
   pending and in-progress containers.
6. Use the N0 trace to decide the frame-sequence context. If the current QoS
   context already supplies only AC/TXOP/rate collaborators and behaves
   correctly, retain it. If it invokes a QoS-only Ack/retry operation, introduce
   the smallest typed context or operation needed; do not clone `HcfFs`.

Exit condition: every supported upper-frame class has an explicit AC and
sequence-space route, and invalid TIDs remain configuration/input errors only
for QoS Data.

### N2. Route Ack, recovery, collision, and group completion by subtype

Target the remaining non-QoS fallbacks in:

- `Hcf::handleEdcafInternalCollision`;
- `Hcf::originatorProcessRtsProtectionFailed`;
- `Hcf::originatorProcessFailedFrame`;
- `Hcf::processReceivedAck`;
- `Hcf::originatorProcessTransmittedFrame` and the transmit-data helper.

Actions:

1. Introduce one local checked classification result and use it consistently in
   these branches.
2. For QoS Data, retain `QosRecoveryProcedure`; for management and subtype-0
   Data, call the existing EDCA `mgmtAndNonQoSRecoveryProcedure` with the
   selected EDCAF's station retry counters.
3. Keep Ack status in `QosAckHandler`'s existing management/non-QoS key space.
4. On internal collision, increment the applicable non-QoS retry/CW state but
   do not set Retry until an on-air retransmission is prepared.
5. On Ack success, reset the matching SRC/LRC/CW state, clear Ack/in-progress
   state once, and report rate-control success with the correct retry count.
6. On timeout, update the counter once, report rate-control failure once, and
   either retain the frame with the same identity or drop it once at the limit.
7. On multicast/broadcast transmission, use the non-QoS multicast-completion
   procedure, retire the frame, and leave Ack/retry state empty.
8. Keep Block Ack and A-MPDU paths gated to QoS Data.

Exit condition: no supported subtype-0 Data reaches a QoS-only counter key,
Block Ack path, unchecked cast, empty `else`, or generic `Unknown frame` branch.

### N3. Complete receive-side delivery and capability boundaries

Trace subtype-0 Data through recipient Ack policy, duplicate removal,
decapsulation, and delivery to the upper layer. Separately characterize
`Hcf::originatorProcessReceivedDataFrame`, which handles a Data frame arriving
as a response during an active originator sequence: do not turn its current
error into acceptance or delivery until a focused exchange proves the expected
protocol action.

Actions:

1. Use checked frame/subtype classification at the receive boundary.
2. Generate Ack only for an individually addressed frame requiring immediate
   Ack; never Ack group-addressed Data.
3. Apply the existing non-QoS duplicate identity using transmitter address plus
   sequence/fragment identity. Do not invent a TID.
4. Deliver intact supported Data through the existing recipient MAC data
   service and retain existing `UserPriorityInd` behavior: subtype-0 carries no
   fabricated priority.
5. Drop malformed or unsupported combinations through the existing drop signal
   with a precise reason where one exists; internal impossible states still
   throw.
6. For an unexpected Data response in an active originator sequence, implement
   only the evidenced action—ignore/drop as foreign or complete a defined
   response step. Ordinary recipient delivery remains in the recipient path.

Exit condition: unicast, multicast, and broadcast subtype-0 Data have explicit,
tested transmit and receive outcomes in both QoS-STA/HCF and legacy DCF modes.

## Workstream H: finish the HCF split

All `H*` patches are refactors. Their tests must show identical frame order,
bytes, timers, callback order, RNG use, and fingerprints.

### H0. Centralize checked frame classification

Add one small base-HCF classifier only after the N-workstream tests define the
required categories. It should distinguish:

- QoS Data and subtype-0 Data;
- management;
- Ack, RTS, CTS, BlockAckReq, and each supported BlockAck variant;
- known headerless PHY indications and aggregate containers at their existing
  boundaries;
- malformed type/concrete-class mismatches and unsupported subtypes.

The helper returns a value/category; it does not own packets, mutate headers,
choose an AC, or implement a visitor hierarchy. Keep Block Ack variant decoding
explicit. Use checked casts before concrete field access.

Use the helper in at least the affected upper, lower, transmitted, failed, and
received-response paths; otherwise keep the classification local and do not add
an abstraction with only one consumer.

### H1. Make `HcfExchangeCoordinator` authoritative

Actions:

1. Route channel request/grant, preparation, transmit, await-response,
   retry/recovery, complete, abort, and reset through the coordinator.
2. Audit base and override call sites together, including duplicate
   `beginTransmission` calls in `Hcf`, `VhtHcf`, and HE transmit/receive code;
   retain exactly one transition per actual exchange action.
3. Move duplicate-complete/abort and stale-callback decisions into explicit
   coordinator operations. HCF should not inspect raw state merely to reproduce
   a transition rule.
4. Keep timer objects and active/expected identity in the coordinator. HCF may
   call OMNeT++ scheduling APIs as the module-context bridge, but it must not
   keep shadow timer or exchange state.
5. Preserve `FrameSequenceHandler` as owner of the operational frame-sequence
   steps and history.
6. Assert that completion/abort releases the EDCAF/TXOP and resumes contention
   at most once.

Extend [`HcfExchangeCoordinator_1.test`](../tests/unit/HcfExchangeCoordinator_1.test)
with duplicate completion, duplicate abort, stale packet, stale expected-step,
and reset-after-terminal cases.

### H2. Finish the response and timeout boundary

Actions:

1. Move late/foreign/addressed response classification and deferred timeout
   sequencing behind `HcfResponseService`.
2. Replace repeated action-bundle construction with one stable adapter owned by
   HCF, or a smaller typed contract if that removes callbacks without creating a
   second state owner.
3. Keep only OMNeT++ `cancelEvent`/`scheduleAfter` calls and packet ownership
   transfer in HCF.
4. Make the order explicit: classify, process, determine whether the receive
   step completed, cancel the response timer if appropriate, then service any
   deferred timeout.
5. Preserve handling for corrupted frames, headerless accepted responses, late
   frames, and frames not addressed to the active exchange.

Extend [`HcfResponseService_1.test`](../tests/unit/HcfResponseService_1.test)
and [`FrameSequenceHandlerAbortCallback_1.test`](../tests/unit/FrameSequenceHandlerAbortCallback_1.test)
to prove action order and exactly-once terminal callbacks.

### H3. Finish pure frame preparation

Move only deterministic decisions whose complete inputs are already resolved
and immutable. Candidate code must satisfy all of these conditions:

- no packet ownership transfer;
- no timer, queue, TXOP, retry, Block Ack, or amendment-specific mutable state;
- no PHY table, rate, legality, or duration authority duplicated from the mode;
- no OMNeT++ module lookup;
- a focused input/output unit test is sufficient.

Keep `HcfFramePreparation::selectInitialProtection` and extend the helper only
where current `transmitFrame` logic meets those conditions. Leave policy and
side effects in their existing owners. Extend
[`HcfFramePreparation_1.test`](../tests/unit/HcfFramePreparation_1.test) for every
moved decision.

### H4. Close aggregation and retry bookkeeping boundaries

Actions:

1. Keep A-MPDU byte construction, length calculation, and temporary
   aggregate-to-constituent ledger in `HcfAggregationService`.
2. Leave aggregation eligibility, Block Ack agreement lifecycle, selected PHY
   mode, PPDU-duration legality, packet ownership, and semantic signal emission
   with their current owners.
3. Make ledger operations explicit for successful take, failed discard,
   implicit Block Ack, and destruction/abort cleanup. No stale entry may retain
   a packet identity.
4. Keep `HcfRetryService` stateless. It may match members and order calls into
   existing Ack/recovery/rate-control owners; it must not store retry counters,
   sequence windows, or packets.
5. Do not move ordinary non-QoS mutable recovery state into either service.

Extend [`Ieee80211FecCodingReq_1.test`](../tests/unit/Ieee80211FecCodingReq_1.test)
for ledger record/take/discard and constituent identity, and retain
[`Ieee80211HtImplicitBlockAck_1.test`](../tests/unit/Ieee80211HtImplicitBlockAck_1.test)
as the timeout/retry-limit regression.

### H5. Reduce `Hcf.h` coupling

After H1-H4 settle:

1. forward-declare pointer-only collaborator types;
2. move their concrete includes to `Hcf.cc`;
3. keep full definitions required by inheritance, inline methods, value members,
   and templates;
4. remove no public/protected method solely to reduce line count;
5. run a full release build and record the before/after direct include count.

This patch claims dependency reduction only. It must not claim faster builds
unless an incremental-build measurement is recorded separately.

## Workstream O: add only the high-value per-AC gaps

### Metric contract

The following definitions are fixed before adding signals:

| Metric | Exact meaning | Owner/path | Action |
| --- | --- | --- | --- |
| MPDU transmission attempt | One constituent MPDU that reached the completed on-air transmission callback; an A-MPDU contributes one event per constituent. | Active `edcaf[*]` selected by the exchange | Add `mpduTransmissionAttempted` packet signal and count statistic. |
| MPDU retransmission attempt | An MPDU transmission attempt whose on-air Retry field is 1. Internal collision alone does not count. | Same EDCAF | Derive with the existing IEEE 802.11 Retry result filter. |
| Retry-limit drop | One terminal discard because the applicable retry limit was reached. | Existing per-EDCAF recovery procedure for QoS; documented HCF AC context for subtype-0 | Reuse existing signal/statistic; do not add a duplicate. |
| Internal collision | One losing EDCAF decision. If three ACs collide and one wins, count one event on each of the two losers. | Losing `edcaf[*]` / collision owner | Add one signal per loser; retain aggregate HCF signal for compatibility. |
| TXOP duration | Acquisition/start through `endTxop`. | Existing per-EDCAF `TxopProcedure` | Reuse existing statistic. |
| TXOP MPDU count | Number of originator MPDUs initiated by that EDCAF within one TXOP; exclude Ack/CTS responses sent by the peer. | Existing per-EDCAF `TxopProcedure` | Add an observation-only counter and emit its value once at TXOP end. |
| A-MPDU created | One constructed A-MPDU, including the existing implicit one-MPDU convention. This is construction, not success or delivery. | HCF aggregation owner with immutable AC context | Keep one canonical HCF event and derive per-AC statistics from its context. |
| A-MPDU MPDU count | Number of constituent MPDUs in that constructed A-MPDU. | Same | Add AC context to the existing event/statistic path; do not count the wrapper as an MPDU. |

Explicitly do not add:

- another retry-limit signal;
- another TXOP duration signal;
- per-AC `NOT_ADDRESSED_TO_US` or duplicate-reception drops;
- generic packet-drop-by-AC fields on every packet;
- structured event IDs or logging intended as a metrics API;
- MU/TB-triggered per-user metrics. They are not unambiguously owned by one
  contending EDCAF and belong to a separate MU observability design.

### O0. Add metric-contract tests first

Create one deterministic two-AC fixture using AC_BE and AC_VO. It must observe
the public signal/statistic surface, not private counters.

Required scenarios:

- one successful singleton MPDU on each AC;
- one Ack timeout and on-air retry;
- one simultaneous AC_BE/AC_VO contention result, with AC_BE recorded as the
  loser and AC_VO as the winner;
- one retry-limit discard;
- one TXOP containing more than one initiated MPDU;
- one A-MPDU with a known constituent count;
- recording disabled versus enabled produces identical frame sequence and
  fingerprint.

### O1. Add precise attempts and internal collisions

Actions:

1. Declare new signals/statistics in the authoritative NED type.
2. Emit `mpduTransmissionAttempted` once per constituent only after the current
   transmission callback identifies the active EDCAF and aggregate ledger.
3. Derive retransmission attempts using the on-air Retry field. Do not derive
   them from retry-counter increments.
4. Emit the internal-collision event from each losing EDCAF decision before its
   recovery update, carrying no mutable pointer that consumers must dereference.
5. Keep the existing HCF aggregate collision count unchanged for compatibility.

### O2. Add TXOP MPDU count and per-AC aggregation context

Actions:

1. Let `TxopProcedure` reset an observation-only initiated-MPDU counter at
   `startTxop`, increment it through one typed direct call from the canonical
   attempt path, and emit the final value at `endTxop`.
2. The counter must not influence TXOP admission, duration, queue selection, or
   any protocol decision.
3. Attach immutable AC context to the existing HCF A-MPDU construction event and
   add NED result filters/statistics for BK, BE, VI, and VO. Preserve existing
   aggregate statistic names and values.
4. Emit aggregation context at construction exactly once. Do not repeat the
   event on transmit success, retry, or Block Ack completion.
5. Prove that optional recording changes neither simulation events nor packet
   bytes.

Exit condition: the new metric-contract fixture can query each selected metric
by AC, and no metric depends on reading an EDCAF, queue, retry, or TXOP private
member from a result consumer.

## Verification matrix

| Patch | Narrow proof | Compatibility proof | Expected fingerprint behavior |
| --- | --- | --- | --- |
| N0-N1 | New `HcfNonQosData` classification/sequence cases | Legacy `Ieee80211_1` to `_4` DCF cases | No change until production fix; then intentional change only in the new path. |
| N2-N3 | Unicast Ack, timeout/retry/limit, multicast, broadcast, recipient delivery | Existing retransmission tests plus one legacy DCF case | Relevant HCF trajectories may change; explain first event before requesting any baseline update. |
| H0-H2 | HCF classifier, coordinator, response-service, abort-callback unit tests | Focused HCF module case | No fingerprint change permitted. |
| H3-H4 | Frame-preparation, aggregation ledger, implicit Block Ack tests | HCF fragmentation and fragmentation+BlockAck rows | No fingerprint change permitted. |
| H5 | Full release compile and include audit | Same focused unit/module tests | No fingerprint change permitted. |
| O0-O2 | New `EdcaPerAc` public-signal/statistic fixture | Recording on/off comparison | No packet/event fingerprint change permitted; result files may contain new metrics. |

## Exact validation commands

Run from the repository root. During development, run the single new or changed
test first; use the grouped commands only after that narrow test passes.

```sh
make MODE=release -j$(nproc)

inet_run_unit_tests \
  -m release \
  -f '(Hcf.*|FrameSequenceHandlerAbortCallback|Ieee80211VhtAddbaQueueing|Ieee80211HtImplicitBlockAck|Ieee80211FecCodingReq|EdcaClassification|HcfNonQosData).*\.test'

inet_run_module_tests \
  -m release \
  --no-build \
  --no-concurrent \
  -f 'Ieee80211(HcfNonQosData|EdcaPerAc|_[1-4]).*'

bash .agents/skills/inet-architectural-requirements/references/enforcement/check-architecture.sh \
  src/inet/linklayer/ieee80211

inet_run_fingerprint_tests \
  -m release \
  --no-build \
  --no-concurrent \
  -w 'showcases/wireless/fragmentation' \
  -i 'omnetpp.ini' \
  -c 'HCFfrag' \
  -r 0

inet_run_fingerprint_tests \
  -m release \
  --no-build \
  --no-concurrent \
  -w 'showcases/wireless/fragmentation' \
  -i 'omnetpp.ini' \
  -c 'HCFfragblockack' \
  -r 0
```

For every command record the exact working directory, build mode, filter,
configuration, run/seed, exit status, and artifact paths. Use Cmdenv for any
diagnostic simulation. If a fingerprint changes, stop and investigate the first
different event with focused logs, PCAP, or eventlog evidence; do not update the
CSV in this work.

Every production patch under the 802.11 subtree also requires:

1. the focused architecture check reconciled with the existing exception
   ledgers;
2. the complete general semantic review checklist and its `REVIEW:` footer;
3. the complete IEEE 802.11 semantic review checklist and its `WLAN REVIEW:`
   footer;
4. explicit verification against `AR-WLAN-QUAL-TESTS`, including the applicable
   boundary and legacy cases.

## Review and merge rules

- Keep the non-QoS correctness fix separate from behavior-preserving HCF split
  commits and from observability commits.
- Use one production-code writer at a time.
- Require an independent regression lane after N2-N3 and O1-O2, and an
  independent architecture/WLAN review after every nontrivial production diff.
- Do not accept a refactor because it reduces line count; accept it only when
  ownership becomes singular and behavior evidence remains identical.
- Do not accept a metric because a signal exists; prove its semantic emission
  point, AC attribution, and exactly-once count.
- Do not remove existing public signals, parameters, NED types, or statistics in
  this program.
- Keep unrelated existing TODOs and architecture/naming violations out of each
  patch.

## Completion checklist

- [ ] Subtype-0 Data is explicitly classified before QoS TID mapping.
- [ ] Management and subtype-0 Data retain one shared sequence-number owner.
- [ ] Unicast subtype-0 Data passes Ack, timeout, retry, and retry-limit tests.
- [ ] Multicast and broadcast subtype-0 Data pass no-Ack/no-retry tests.
- [ ] Receive-side delivery and duplicate handling are explicit and tested.
- [ ] No supported subtype-0 path reaches a QoS-only Block Ack/aggregation path,
      unchecked cast, empty branch, or generic fallback error.
- [ ] `HcfExchangeCoordinator` is the only exchange-lifecycle owner.
- [ ] `HcfResponseService` owns response/timeout classification and sequencing.
- [ ] Preparation, aggregation ledger, and retry helper boundaries are narrow
      and do not duplicate mutable state.
- [ ] `Hcf.h` contains no avoidable concrete pointer-only includes.
- [ ] Existing per-AC retry-limit and TXOP-duration metrics were reused, not
      duplicated.
- [ ] MPDU attempts/retries, losing internal collisions, TXOP MPDU counts, and
      A-MPDU construction/count are queryable by AC with fixed semantics.
- [ ] Recording neutrality is demonstrated.
- [ ] Focused unit and module tests pass.
- [ ] Legacy DCF compatibility tests pass.
- [ ] HCF fragmentation fingerprints have no unexplained change.
- [ ] Architecture checks and both semantic review checklists pass or have an
      explicitly reconciled pre-existing ledger entry.
- [ ] No fingerprint CSV, architecture exception ledger, naming exception
      ledger, or sealing status was changed without separate approval.
