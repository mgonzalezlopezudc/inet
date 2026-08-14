After reading the Opus report and rechecking the implementation, I would revise my earlier assessment. My overall criticism of the original Gemini report remains valid, but I was too conservative about several individual mechanisms.

The corrected conclusion is:

> The simplification is architecturally feasible and probably worthwhile. Most planning-time transaction machinery can be removed after planning and packing are moved ahead of frame-sequence startup. Active exchange state must remain, but numeric correlation tokens and several rollback mechanisms may not be necessary.

## Review of my earlier claims

| My earlier claim | Revised assessment |
|---|---|
| Numeric transaction IDs require an equivalent generation/object identity | **Too strong.** The numeric HE/VHT IDs can probably be removed completely. |
| Planning-only phases may be removed, but active state remains | **Confirmed.** |
| Reservation/rollback removal requires a strictly `noexcept` commit | **Too strong.** Expected/recoverable failures must precede mutation; invariant failures after commit may terminate the simulation loudly. |
| Packet duplication is not really “deep cloning” | **Confirmed.** |
| Sequence-state cloning cannot simply become counter decrements | **Confirmed.** |
| Queue scans require a new no-reentrancy contract before removal | **Overly conservative.** The final scans are likely redundant in the present call path. |
| Protection snapshots can be removed by reordering | **Confirmed and stronger than I initially stated.** |
| Snapshot consolidation should preserve an immutable value boundary | **Confirmed.** |
| The UL implementation provides a simpler precedent | **Incorrect claim in the Opus report.** UL contains analogous transaction machinery. |

## 1. Transaction IDs: I was too conservative

I previously said numeric tokens could only be replaced by an equivalent exchange generation or object identity. Source inspection suggests that is not necessarily required.

Only one frame sequence can be running in `FrameSequenceHandler`, and the sequence object owns the callbacks until it is finished and deleted in [FrameSequenceHandler.cc](src/inet/linklayer/ieee80211/mac/framesequence/FrameSequenceHandler.cc#L192). Old frame-sequence objects cannot normally call back after deletion. Stale response timers are already protected separately by HCF exchange generations in [HcfExchangeEngine.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangeEngine.cc#L173).

Moreover, transaction tokens would not protect against an old wireless response being misclassified by a new sequence: such a response would be handled by the new frame sequence and would therefore carry the new sequence’s token. Response validation must instead use peer, TID, trigger ID, RU, and current step state, as HE already does in [HeDlMuTxOpFs.cc](src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc#L479).

Therefore:

- `nextTransactionToken`, `nextDlMuExchangeId`, wrap checks, and token parameters can probably be removed.
- Active member state and per-user completion suppression still remain.
- The replacement is structural ownership by the single running frame sequence, not necessarily another correlation ID.

The Opus report’s “boolean” wording is slightly too small—the implementation still needs active members and completion state—but its conclusion about removing numeric IDs is plausible.

## 2. Lifecycle phases: the Opus distinction is correct

The Opus report correctly narrows the target to planning phases.

`COMMITTING` and `pendingPlanningFailure` exist because frame-sequence startup synchronously enters `prepareStep()`, where packing can fail and call backward into the provider before `commitStart()` returns:

- HE: [HeDlMuExchangeProvider.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeDlMuExchangeProvider.cc#L375)
- VHT: [VhtHcfFeature.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/VhtHcfFeature.cc#L611)
- immediate step preparation: [FrameSequenceHandler.cc](src/inet/linklayer/ieee80211/mac/framesequence/FrameSequenceHandler.cc#L104)

Moving complete packing and PPDU/TXVECTOR preparation before frame-sequence construction can eliminate `PREPARED`, `COMMITTING`, and pending-planning-failure plumbing.

My earlier point still applies only to execution: `ACTIVE` state, member ownership, ACK/BlockAck expectations, and per-user completion must survive later events. The Opus proposal retains an `IDLE ↔ ACTIVE` lifecycle, so my earlier “stateless pipeline” criticism applies to the Gemini report, not fairly to this one.

## 3. I overstated the need for a literally non-throwing commit

I previously required a completely non-fallible or `noexcept` commit. That is stricter than ordinary INET practice and stricter than necessary.

The correct boundary is:

- All expected model-level rejection—capability loss, invalid RU, insufficient BA space, excessive duration, TXOP overflow, unsupported response method—must happen before mutation.
- No replaceable policy decision or recoverable fallback decision should occur after the first mutation.
- An impossible invariant violation after the boundary may throw and abort the simulation, as required by `AR-QUAL-LOGGING`; it does not necessarily require reconstructing observer-visible queue state and continuing with SU fallback.

The existing HE code itself adopts this policy at [HeDlMuTxOpFs.cc](src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc#L1252).

Thus reservation maps and planning rollback can be eliminated if the new prepared execution plan already contains:

- exact packet handles and identities;
- final post-packing users and allocations;
- prepared headers, FCS, container contents, and TXVECTOR/layout;
- final scheduler accounting inputs;
- sequence-number updates;
- ACK/in-progress transitions.

The virtual `commitSchedule()` still needs careful placement because it has real scheduler side effects at [HeDlMuExchangeProvider.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeDlMuExchangeProvider.cc#L511).

## 4. Packet “deep cloning”: my correction stands

Both reports characterize `Packet::dup()` as deep packet cloning and imply roughly doubled content memory. That is inaccurate.

The `Packet` copy constructor shares immutable chunk content and copies packet/tag containers in [Packet.cc](src/inet/common/packet/Packet.cc#L82). Header replacement then creates copy-on-write chunks and metadata work, so the current approach is not free, but it is not normally a full byte-for-byte payload copy.

There are still costs:

- packet wrapper allocation;
- header/trailer COW allocations;
- tag and region-tag manipulation;
- extra aggregate-member copies in VHT;
- copying prepared state back into originals.

But claims of “significant heap allocation and memcpy overhead” need profiling. The reports provide no measurements.

Removing prepared packet copies may still simplify the code, but complexity reduction is currently better supported than performance improvement.

## 5. Sequence numbers: the Opus rollback explanation is wrong

The statement that sequence state can be rolled back by “N decrements of a monotonic counter” should be retracted.

QoS sequence allocation maintains:

- per-`(receiver,TID)` maps;
- per-address shared maps;
- a shared global counter;
- key insertion behavior;
- modulo-4096 sequence values.

See [QoSSequenceNumberAssignment.cc](src/inet/linklayer/ieee80211/mac/sequencenumberassignment/QoSSequenceNumberAssignment.cc#L25). There is no generic safe decrement operation.

Sequence cloning can nevertheless be simplified, but one of these designs is required:

1. Retain speculative cloned state and adopt it once.
2. Introduce a batch sequence-allocation plan that computes final sequence values and the resulting authoritative state without mutating the live owner.
3. Assign live sequence numbers only after every recoverable failure, accepting that invariant failures afterward terminate the simulation.

Fresh MPDUs and retries must remain distinct; retries retain their existing sequence numbers.

## 6. Queue scans: I was probably too cautious

I previously emphasized possible synchronous reentrancy. I found no concrete production path that mutates the selected queues between final packing and commit.

Because selection, packing, TXVECTOR construction, and commit occur in one run-to-completion call stack, the final O(N×M) membership scans in:

- [HeDlMuTxOpFs.cc](src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc#L1199)
- [VhtDlMuTxOpFs.cc](src/inet/linklayer/ieee80211/mac/framesequence/VhtDlMuTxOpFs.cc#L307)

are likely redundant in the normal implementation.

A good redesign could validate packet ownership once when producing the immutable execution plan and then rely on the frame-sequence ownership boundary. A cheap debug assertion may still be useful, but retaining repeated linear scans solely for hypothetical reentrancy is not compelling.

## 7. Protection rollback can probably be removed

The Opus report is persuasive here.

Protection snapshots exist because protection is configured before the still-fallible frame-sequence preparation:

- HE: [HeDlMuExchangeProvider.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeDlMuExchangeProvider.cc#L376)
- VHT: [VhtHcfFeature.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/VhtHcfFeature.cc#L592)

If complete PPDU preparation succeeds first, protection can then be configured exactly once before starting transmission. Expected planning failure would never alter TXOP protection, so no restoration is required.

My earlier warning about SU fallback inheriting speculative protection remains true for the current ordering, but the Opus redesign removes that ordering.

## 8. Existing rollback tests do not mandate the mechanism

I previously relied heavily on tests proving rollback behavior. Those tests establish semantic requirements:

- planning failure leaves queue and sequence state unchanged;
- fallback selects the intended packet;
- failed planning emits no ownership changes;
- subsequent exchanges still work.

They do not prove that rollback is the only valid implementation. Under a prepare-before-mutation design, failure leaves state untouched by construction, so rollback-specific fault hooks can appropriately be replaced with tests at the new feasibility boundary.

The existing `ITransactionObserver` and VHT post-mutation fault hooks are therefore removable after equivalent semantic coverage is introduced.

## Problems in the Opus report

Despite being more precise than the Gemini report, it contains several factual or unsupported claims:

- “Deep packet cloning” and approximately doubled packet/chunk memory are inaccurate because chunks are shared.
- Sequence rollback by decrement is incorrect.
- Performance improvements and the estimated 722 removable lines are unmeasured estimates, not demonstrated results.
- A single context holding live queue and peer-state references would weaken the current deterministic, testable value-snapshot boundary. A consolidated value plan is preferable.
- `LegacyHeDlMuExchangeCallback` is a compatibility constructor path, not proven to exist solely for unit tests; EHT exposes the corresponding legacy constructor too.
- Most importantly, the claim that UL MU lacks analogous machinery is false. `HeTriggeredUlExchangeService` uses trigger correlation, cloned sequence state, prepared packet copies, queue validation, rollback snapshots, and commit rollback in [HeTriggeredUlExchangeService.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeTriggeredUlExchangeService.cc#L1121). UL is not evidence that transactions are unnecessary.

## Revised final verdict

For the Opus report specifically:

- Remove numeric HE/VHT exchange IDs: **probably safe**.
- Collapse planning phases to `IDLE ↔ ACTIVE`: **safe after planning is extracted**.
- Remove reservations and planning rollback: **safe with a complete immutable execution plan**.
- Remove protection snapshots: **safe after reordering**.
- Remove final queue rescans: **probably safe**.
- Remove packet/sequence cloning: **conditionally safe**, but requires a concrete batch sequence and tag-preserving packet-preparation design.
- Consolidate snapshots: **safe only as a consolidated immutable value plan**, not live mutable references.
- Delete active exchange state: **not safe and not actually required by the Opus report**.

So I retract the blanket implication that the transactional machinery largely needs to stay. Most of it is compensating for planning being performed inside frame-sequence execution and can be removed by fixing that boundary. My enduring requirements are narrower: retain one explicit active exchange owner, preserve exact MPDU/sequence/BA semantics, keep planning immutable and testable, and ensure all recoverable failure precedes ownership mutation.