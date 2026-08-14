# Analysis: 802.11 MU Implementation Simplification

An independent code-level analysis of the IEEE 802.11 Multi-User (DL MU-OFDMA for HE/EHT, DL MU-MIMO for VHT) implementation under `src/inet/linklayer/ieee80211/mac/`, given two invariants:

1. **INET is single-threaded** — OMNeT++ processes events sequentially; no concurrent mutation is possible.
2. **MU planning occurs during a single event** — from EDCAF channel-access grant through frame-sequence instantiation and container-packet construction, `simTime()` does not advance.

## Prior Report Assessment

The [existing report](reports/80211-simplification-by-gemini-20260813.md) correctly identifies the major categories of over-engineering. My independent analysis confirms its core findings and adds precision on the scope, risks, and boundary conditions.

---

## Simplification Opportunities — Detailed Findings

### 1. Transaction Correlation Tokens (HE: `transactionToken`, VHT: `exchangeId`)

**Files:**
- [`HeDlMuExchangeProvider.h`](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeDlMuExchangeProvider.h) — `nextTransactionToken` (L139), `activeTransactionToken` (L129)
- [`VhtHcfFeature.h`](src/inet/linklayer/ieee80211/mac/coordinationfunction/VhtHcfFeature.h) — `nextDlMuExchangeId` (L155), `lastRetiredDlMuExchangeId` (L156), `DlMuLifecycle::exchangeId` (L150)
- [`IHeDlMuExchangeCallback.h`](src/inet/linklayer/ieee80211/mac/contract/IHeDlMuExchangeCallback.h) — `uint64_t transactionToken` on 5 callback methods (L77–L86)
- [`IVhtDlMuExchangeCallback.h`](src/inet/linklayer/ieee80211/mac/contract/IVhtDlMuExchangeCallback.h) — `uint64_t exchangeId` on 3 callback methods (L38–L45)
- [`HeDlMuTxOpFs.h`](src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h) — stored token field (L80), propagated through constructor (L110)
- [`VhtDlMuTxOpFs.h`](src/inet/linklayer/ieee80211/mac/framesequence/VhtDlMuTxOpFs.h) — stored `exchangeId` field (L42), propagated through constructor (L57)

**Analysis:**

The tokens serve as correlation IDs to reject "late," "duplicate," or "out-of-order" callbacks. In the actual code:
- [`HeDlMuExchangeProvider::heDlMuPlanningFailed()`](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeDlMuExchangeProvider.cc#L638-L665) — validates `token != 0 && token == activeTransactionToken && members.empty() && !reservedPackets.empty()` before processing.
- [`HeDlMuExchangeProvider::heDlMuUserOutcome()`](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeDlMuExchangeProvider.cc#L614-L636) — validates `token == activeTransactionToken` before dispatching.
- [`VhtHcfFeature::vhtDlMuPlanningFailed()`](src/inet/linklayer/ieee80211/mac/coordinationfunction/VhtHcfFeature.cc#L855-L870) — validates `exchangeId == dlMu.exchangeId` and rejects stale/terminal states.
- [`VhtHcfFeature::processVhtDlMuUserResult()`](src/inet/linklayer/ieee80211/mac/coordinationfunction/VhtHcfFeature.cc#L891-L904) — validates `exchangeId == dlMu.exchangeId`.

**Why removable:** In a single-threaded simulator with at most one active EDCAF exchange, callbacks are always dispatched synchronously within the same event or during the subsequent transmission/ACK events of the *same* frame exchange. There is no concurrent or interleaved exchange that could produce a stale token. A simple boolean `hasActiveExchange` flag is sufficient; the monotonic counter and per-callback validation can be removed entirely.

**Estimated lines removable:** ~80 lines (HE) + ~40 lines (VHT) = ~120 lines of token allocation, validation, and propagation.

---

### 2. Multi-Phase Lifecycle State Machines

**Files:**
- [`HeDlMuExchangeProvider.h`](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeDlMuExchangeProvider.h#L117-L121) — `StartPhase { IDLE, COMMITTING, ACTIVE }`
- [`VhtHcfFeature.h`](src/inet/linklayer/ieee80211/mac/coordinationfunction/VhtHcfFeature.h#L141-L153) — `DlMuPhase { IDLE, PREPARED, COMMITTING, ACTIVE, TERMINAL }` plus `DlMuLifecycle` struct with `ownershipCommitted`, `pendingFailure`

**Analysis:**

The `COMMITTING` state exists because `commitStart()` / `commitDlMu()` calls `startHeDlMuExchange()` / `startFeatureFrameSequence()` which synchronously constructs the frame sequence, which synchronously calls `buildMuContainerPacket()`, which may synchronously fail and invoke the `planningFailed` callback *before* `commitStart` returns. The `COMMITTING` flag intercepts this to defer the failure to the `pendingPlanningFailure` field, which is checked after `startHeDlMuExchange` returns (see [`HeDlMuExchangeProvider::commitStart()`](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeDlMuExchangeProvider.cc#L375-L402) lines 387–398).

**Why removable:** The `COMMITTING`→`pendingPlanningFailure` pattern is a workaround for the fact that planning and execution are entangled — `buildMuContainerPacket()` runs inside the frame-sequence constructor/startup, so it can only signal failure via a callback. If planning is restructured to complete *before* the frame sequence is instantiated, the state machine collapses to `IDLE ↔ ACTIVE`. The `PREPARED` and `TERMINAL` states in VHT serve similar defensive purposes.

**Estimated lines removable:** ~60 lines (HE) + ~80 lines (VHT) = ~140 lines.

---

### 3. Reservation/Rollback Guards

**Files:**
- [`HeDlMuExchangeProvider.h`](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeDlMuExchangeProvider.h#L26-L38) — `ReservationRollbackGuard` RAII class
- [`HeDlMuExchangeProvider.cc`](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeDlMuExchangeProvider.cc#L427-L489) — `reservePlan()`, `rollbackReservation()`, `finalizeReservation()`
- [`VhtHcfFeature.cc`](src/inet/linklayer/ieee80211/mac/coordinationfunction/VhtHcfFeature.cc#L30-L43) — `RollbackGuard` (general-purpose RAII)
- [`VhtHcfFeature.cc`](src/inet/linklayer/ieee80211/mac/coordinationfunction/VhtHcfFeature.cc#L585-L625) — `commitDlMu()` with `protectionRollback` and `rollback` guards

**Analysis:**

`reservePlan()` (lines 427–475) does a full O(N×M) scan through all queues for all scheduled users, validating each packet identity and header fields. `rollbackReservation()` clears the `reservedPackets` map, `activeTransactionToken`, protection state, pending scheduler context, and pending allocations. `finalizeReservation()` (lines 491–523) validates every member against the reserved set and commits the scheduler.

The VHT path uses multiple nested `RollbackGuard` instances that restore protection state and clear exchange state if any exception occurs during `commitDlMu()`.

**Why removable:** Both `reservePlan()` and `finalizeReservation()` run in the same event as `commitStart()`. No external mutation can occur between reservation and commit. If planning produces a validated plan first, the "reserve → build → (fail? rollback : commit)" sequence simplifies to "plan → (valid? commit : fallback)".

> [!IMPORTANT]
> The scheduler `commitSchedule()` callback (line 518) carries real side-effects: it updates the scheduler's internal state to reflect the completed allocation. Any simplified flow must still call this after a successful plan.

**Estimated lines removable:** ~100 lines (HE reservation) + ~40 lines (VHT rollback) = ~140 lines.

---

### 4. Deep Packet Cloning and Content Replacement

**Files:**
- [`HeDlMuTxOpFs.cc`](src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc#L1057-L1089) — `originalPacket->dup()`, header/trailer rewrite on copy, region tag restoration
- [`HeDlMuTxOpFs.cc`](src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc#L1258-L1273) — post-commit: `originalPacket->removeAll()`, `insertAtBack(peekAll())`, `clearTags()`, `copyTags()`, region tag assignment
- [`VhtDlMuTxOpFs.cc`](src/inet/linklayer/ieee80211/mac/framesequence/VhtDlMuTxOpFs.cc#L220-L264) — `original->dup()`, sequence number assignment on copy, header/trailer rewrite on copy
- [`VhtDlMuTxOpFs.cc`](src/inet/linklayer/ieee80211/mac/framesequence/VhtDlMuTxOpFs.cc#L352-L365) — post-commit: `removeAll()`, `insertAtBack()`, `clearTags()`, `copyTags()`, region tag assignment

**Analysis:**

The pattern is:
1. Clone every selected packet (`dup()`)
2. Rewrite headers/trailers on the clone (sequence numbers, ack policy, duration, FCS)
3. After all clones succeed, replace the original's content with the clone's content

This produces ~2× the packets, chunks, and tag copies during MU planning. The purpose is to keep the originals untouched in case planning fails — "speculative mutation on a throwaway copy."

**Why removable:** Given single-event planning:
- Sequence numbers can be assigned on the originals directly, because the counter can trivially be rolled back (it's just `N` decrements of a monotonic counter) or, better, assigned only after feasibility is fully confirmed.
- Header fields (ack policy, duration) can be set on the originals as part of the final commit step.
- The container packet only needs to serialize the data content once via `peekData()`, not through an intermediate full packet clone.

> [!WARNING]  
> The `preparedPacket->getRegionTags() = originalPacket->getRegionTags()` assignment at [HeDlMuTxOpFs.cc:1086](src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc#L1086) repairs region tags that header/trailer replacement may have invalidated. Any simplified approach that modifies headers in-place must ensure region tags remain valid, or must use the mutable-header API that preserves region tag regions.

**Estimated lines removable:** ~70 lines (HE) + ~60 lines (VHT) = ~130 lines. This also eliminates significant heap allocation and memcpy overhead.

---

### 5. Sequence Number State Cloning and Rollback

**Files:**
- [`ISequenceNumberAssignment.h`](src/inet/linklayer/ieee80211/mac/contract/ISequenceNumberAssignment.h) — `clone()` and `copyStateFrom()` methods
- [`IOriginatorMacDataService.h`](src/inet/linklayer/ieee80211/mac/contract/IOriginatorMacDataService.h#L36-L37) — `cloneSequenceNumberState()`, `commitSequenceNumberState()`
- [`HeDlMuTxOpFs.cc`](src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc#L1057-L1060) — clone; line 1257 — commit
- [`VhtDlMuTxOpFs.cc`](src/inet/linklayer/ieee80211/mac/framesequence/VhtDlMuTxOpFs.cc#L118-L119) — clone; line 321 — second clone (for rollback); line 337 — commit; line 346 — rollback restore

**Analysis:**

The VHT path is particularly elaborate: it clones the state *twice* — once for speculative assignment (`sequenceState`), once for the rollback baseline (`originalSequenceState`). If the commit `try` block fails, it restores the original state (line 346) and also reverses `ackHandler->frameGotInProgress()` calls.

**Why removable:** Sequence numbers are pure monotonic counters indexed by (receiver, TID). In a single-event planning pipeline:
1. Calculate how many new sequence numbers would be needed per (receiver, TID)
2. Confirm the plan is valid
3. Assign sequence numbers directly from the live counters as part of the commit step

No cloning or rollback is needed because no assignment happens until after validation. The `ISequenceNumberAssignment::clone()` and `copyStateFrom()` APIs were added specifically for this transactional pattern and could be deprecated.

**Estimated lines removable:** ~30 lines (HE) + ~40 lines (VHT) + ~20 lines (interface/implementations) = ~90 lines.

---

### 6. Defensive Queue Membership Verification

**Files:**
- [`HeDlMuTxOpFs.cc`](src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc#L1202-L1211) — O(N×M) loop checking every packet is still in its source queue
- [`VhtDlMuTxOpFs.cc`](src/inet/linklayer/ieee80211/mac/framesequence/VhtDlMuTxOpFs.cc#L310-L318) — O(N×M) loop checking every packet is still in queue or in-progress

**Analysis:**

These loops iterate over all selected packets (N) and for each, linearly scan the source queue (M packets) to verify pointer identity. The check throws `cRuntimeError` / `VhtDlMuStalePlan` if any packet moved.

**Why removable:** Between the start of `buildMuContainerPacket()` and the commit point, execution is single-threaded and synchronous. No timer, message arrival, or dequeue operation can fire. Queue contents are immutable during this window.

**Estimated lines removable:** ~15 lines (HE) + ~12 lines (VHT) = ~27 lines.

---

### 7. Protection State Snapshotting and Restoration

**Files:**
- [`HeDlMuExchangeProvider.h`](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeDlMuExchangeProvider.h#L51-L65) — `HeDlMuProtectionSnapshot` struct
- [`HeDlMuExchangeProvider.h`](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeDlMuExchangeProvider.h#L100-L104) — `captureHeDlMuProtection()`, `configureHeDlMuProtection()`, `restoreHeDlMuProtection()` in `IActions`
- [`HeDlMuExchangeProvider.cc`](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeDlMuExchangeProvider.cc#L40-L48) — `restorePendingProtection()`
- [`VhtHcfFeature.cc`](src/inet/linklayer/ieee80211/mac/coordinationfunction/VhtHcfFeature.cc#L594-L597) — protection snapshot capture and rollback guard

**Analysis:**

Protection (RTS/CTS or MU-RTS) is configured *before* the MU plan is fully committed. If planning fails, the protection state must be restored. This is only needed because protection is configured speculatively.

**Why removable:** If protection is configured only after the plan is confirmed valid, no snapshot/restore is needed.

**Estimated lines removable:** ~30 lines (HE) + ~10 lines (VHT) = ~40 lines.

---

### 8. `ITransactionObserver` / Fault Injection Hooks

**Files:**
- [`HeDlMuTxOpFs.h`](src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h#L39-L44) — `ITransactionObserver` interface with `beforePacketCommit()`
- [`HeDlMuTxOpFs.cc`](src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc#L1217-L1226) — `try/catch` block exercising `transactionObserver->beforePacketCommit()`
- [`VhtDlMuTxOpFs.h`](src/inet/linklayer/ieee80211/mac/framesequence/VhtDlMuTxOpFs.h#L50-L51) — virtual hooks `beforePacketCommit()`, `afterCommitMutation()`
- [`VhtDlMuTxOpFs.cc`](src/inet/linklayer/ieee80211/mac/framesequence/VhtDlMuTxOpFs.cc#L325-L349) — `try/catch` with ACK rollback + sequence state restoration on exception from hooks

**Analysis:**

These are test-only hooks that allow unit tests to inject exceptions mid-commit to verify rollback correctness. The VHT `try/catch` block (lines 325–349) is particularly complex: it tracks per-packet `ackApplied` booleans and reverses `frameGotInProgress()` calls individually.

**Why removable:** If there is no rollback to test (because planning completes before commit), these hooks lose their purpose. The `try/catch` with manual ack-state and sequence-number rollback can be removed entirely.

> [!NOTE]
> Any unit tests that inject faults through `ITransactionObserver` or the virtual `beforePacketCommit()`/`afterCommitMutation()` hooks will need to be updated or removed.

**Estimated lines removable:** ~20 lines (HE) + ~35 lines (VHT) = ~55 lines.

---

### 9. Intermediate Data Structure Pipeline

**Current data flow (HE):**
```
Queues → HeDlMuCandidateSnapshot → HeDlMuPreparationSnapshot
       → IIeee80211HeDlScheduler::ScheduleContext → CandidateInfo
       → RuAllocation → HeDlMuPlan
       → HeDlMuPackingPlanner::Parameters → PackingPlan → SelectedAllocation
       → ActiveAllocation → HeDlMuMember → IeeeHeTxVectorRequest
```

**Current data flow (VHT):**
```
Queues → VhtGrantSnapshot → VhtDlMuPlan → PreparedMember → ActiveUser
       → Ieee80211VhtMuUser → Ieee80211VhtTxVector
```

**Analysis:**

Many of these structs copy the same MAC addresses, TIDs, capability information, and queue tokens across multiple representations. For example, `HeDlMuCandidateSnapshot` copies most of the same fields as `CandidateInfo`, which then feeds into `RuAllocation`, which feeds into `SelectedAllocation`. Each conversion step copies vectors, maps, and nested capability structs.

**Why simplifiable:** A single `DlMuPlanningContext` struct that owns references to the live queue and peer state — read-only during the single planning event — can replace multiple intermediate copies. The scheduler can output allocations directly against this context, and the packing planner can consume the same context.

> [!IMPORTANT]
> `HeDlMuPreparationSnapshot` was explicitly designed as a *value snapshot* that decouples planning from live module state ([`IHeDlMuSnapshotSource.h`](src/inet/linklayer/ieee80211/mac/contract/IHeDlMuSnapshotSource.h#L47-L49)). While the value-snapshot pattern adds copying overhead, it provides a clean testing and determinism boundary: unit tests can construct arbitrary snapshots without wiring up live modules. Simplifying this requires either preserving the snapshot as a single consolidated struct or accepting tighter coupling to live state. The value-snapshot approach has legitimate benefits for testability.

**Estimated reduction:** Several hundred lines of struct definitions and conversion code, but this is a larger refactoring with testability trade-offs.

---

## Summary of Removable Machinery

| Category | HE Lines | VHT Lines | Total |
|:---|---:|---:|---:|
| Transaction tokens / exchange IDs | ~80 | ~40 | ~120 |
| Multi-phase lifecycle state machines | ~60 | ~80 | ~140 |
| Reservation / rollback guards | ~100 | ~40 | ~140 |
| Deep packet cloning + content replacement | ~70 | ~60 | ~130 |
| Sequence number state cloning + rollback | ~30 | ~40 | ~70 |
| Defensive queue membership scans | ~15 | ~12 | ~27 |
| Protection state snapshot / restoration | ~30 | ~10 | ~40 |
| Fault injection hooks + rollback test infra | ~20 | ~35 | ~55 |
| **Subtotal (conservative)** | **~405** | **~317** | **~722** |

The existing report estimated ~500 lines from `HeDlMuExchangeProvider` and `VhtHcfFeature` alone. My estimate of ~722 lines spans more files (including the frame sequences and contracts) but is broadly consistent.

---

## Observations the Prior Report Did Not Cover

### A. The UL MU path does not have this problem

The [`HeUlMuTxOpFs`](src/inet/linklayer/ieee80211/mac/framesequence/HeUlMuTxOpFs.cc) and [`HeTriggeredUlExchangeService`](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeTriggeredUlExchangeService.h) do *not* use transaction tokens, reservation/rollback guards, or deep packet cloning for MU planning. This confirms the transactional machinery is specific to DL MU and is not a framework-wide pattern.

### B. EHT inherits all HE complexity

[`EhtDlMuTxOpFs`](src/inet/linklayer/ieee80211/mac/framesequence/EhtDlMuTxOpFs.h) inherits directly from `HeDlMuTxOpFs` and propagates the `transactionToken` through its constructor (line 28). Any simplification of the HE path automatically benefits EHT.

### C. The `LegacyHeDlMuExchangeCallback` adapter

A [100-line shim class](src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc#L63-L102) exists solely to adapt the `IHeDlMuExchangeCallback` interface for unit tests that don't have a full `HeHcf` stack. All the callback stubs (`heDlMuPlanFinalized`, `heDlMuPlanCommitted`, `heDlMuPlanningFailed`, etc.) implement no-ops. This adapter can be simplified or removed if the callback interface is reduced.

### D. `VhtDlMuStalePlan` is a dedicated exception class

[`VhtDlMuTxOpFs.h`](src/inet/linklayer/ieee80211/mac/framesequence/VhtDlMuTxOpFs.h#L19-L23) defines a custom `cRuntimeError` subclass specifically to distinguish "stale plan" failures from other runtime errors. The `commitDlMu()` method in `VhtHcfFeature` catches `VhtDlMuStalePlan` to trigger the fallback path. If planning is separated from execution, this exception class and its catch handler become unnecessary.

### E. Scheduler `commitSchedule()` callback must be preserved

Both paths call `scheduler->commitSchedule()` after successful MU transmission planning. This updates the scheduler's internal state (e.g., fairness counters, round-robin pointers). Any simplified flow must preserve this call.

---

## Open Questions

> [!IMPORTANT]
> **Q1: Scope of this analysis.** Is this analysis purely investigatory, or are you looking for an implementation plan to actually perform these simplifications? The refactoring would be a substantial multi-file change affecting production code, unit tests, and potentially fingerprints.

> [!IMPORTANT]
> **Q2: Value-snapshot testability.** The `HeDlMuPreparationSnapshot` / `HeDlMuCandidateSnapshot` struct pipeline (Category 9) provides a clean testability boundary. Should this be preserved even if the transactional machinery is removed, or is tighter coupling to live module state acceptable?

> [!IMPORTANT]
> **Q3: Existing test coverage.** The `ITransactionObserver` and `VhtDlMuTxOpFs` virtual hooks are used by unit tests that verify rollback correctness. If the transactional machinery is removed, should these tests be deleted, or should they be replaced with tests that verify the simplified pipeline's failure-to-fallback behavior?
