# IEEE 802.11 VHT/HE HCF outer-transaction removal plan

Date: 2026-08-13

Status: implementation-ready design; this document changes no production code

Baseline: commit `4c3ab90dbb`, which already moved ordinary SU, channel release,
HT sounding, VHT sounding, and one-queue transmission preparation off the generic
transaction path. This plan is the second stage: remove the generic transaction
shell that remains around already-selected VHT/HE actions.

This supersedes the remaining-transaction design in
`plans/80211-no-trasactional-simplification-plan-20260813.md`. That earlier plan
remains useful as the historical baseline and regression record; do not rewrite
it while executing this plan.

## 1. Decision and intended outcome

The current generic layer does not protect against operating-system threads.
There are no mutexes or atomics in the path, and OMNeT++ invokes these modules on
one simulation thread. The layer currently wraps a typed VHT or HE decision in:

```text
typed immutable grant snapshot
  -> type-erased HcfContext
  -> generic provider probe
  -> empty HcfExchangePlan reservation list
  -> PreparedHcfExchange state machine
  -> generic committer callback
  -> unwrap the same typed grant snapshot
  -> typed VHT/HE commit
```

That outer path can be removed. The following protections cannot be removed:

- same-stack reentrancy when `startFrameSequence()` immediately calls
  `prepareStep()` and the sequence finishes before the start call returns;
- stale OMNeT++ self-message rejection across completed and later exchanges;
- exact queue-token, packet-identity, association-epoch, and scheduler-plan
  validation immediately before mutation;
- provider-local callback correlation for HE/VHT multi-user exchanges;
- exception-safe restoration around packet ownership transfer, reservation,
  protection configuration, dialog-token allocation, and sequence startup;
- deterministic admission order and feature gating.

The target control flow is:

```text
Hcf::channelGranted(ac)
  -> HcfExchangeEngine::channelGranted()       // sequence/timer generation only
  -> TxopProcedure::startTxop(ac)
  -> prepare exactly one typed immutable grant
  -> switch on that grant's typed StartKind
  -> validate immediately before first mutation
  -> invoke the owning VHT/HE service directly
  -> owner-local callbacks complete/abort the feature operation
  -> HcfExchangeEngine completes the frame sequence and resumes contention
```

There will be no generic provider probing, generic exchange class, type-erased
grant context, generic transaction identity, or generic terminal callback.

## 2. Non-negotiable safety invariants

1. `HcfExchangeEngine` remains the sole owner of the active frame sequence,
   response timers, deferred timeout, retry transition, and contention resumption.
2. Engine generation is private timer-staleness metadata. It is never used as a
   VHT/HE protocol identity and is never compared with a provider-local ID.
3. Every feature callback ID has one documented owner, allocation point,
   invalidation point, and zero-invalid rule. IDs advance monotonically and are
   never decremented or reused after a failed start.
4. Preparing a grant is observational: no dequeue, queue reservation, BA-window
   mutation, RNG draw, timer scheduling, dialog-token commit, or frame-sequence
   start is allowed.
5. Committing a grant revalidates every borrowed identity immediately before its
   first mutation.
6. A synchronous terminal callback during commit is accepted once, deferred,
   and drained after the start call returns. A duplicate callback is an error;
   a late callback from an older local ID is ignored or rejected as specified by
   that owner.
7. Pre-handoff failures restore all state changed by that commit. Post-handoff
   failures follow the frame sequence's explicit terminal path and are not
   silently rolled back across an irrevocable ownership boundary.
8. EDCAF, pending queue, `InProgressFrames`, retry, sequence-number, ACK/BA,
   rate-selection, protection, scheduler, and PHY-mode authorities do not move.
9. `IHcfFeatureSet` remains the NED-paired replacement boundary, but it no longer
   exposes generic exchange providers or a generic committer.
10. Each HE/VHT service shuts down its own timers and owned packets before its
    dependencies are destroyed. No terminal callback is introduced in an `Hcf`
    destructor.
11. No NED type, parameter, INI compatibility option, packet format, or PHY
    behavior is changed.
12. Production LOC and conceptual state count must decrease. Do not replace the
    selector with another generic dispatcher or shared transaction framework.

## 3. Final ownership model

| State | Owner after this refactor | Required guard |
|---|---|---|
| Active frame sequence and response timers | `HcfExchangeEngine` | private nonzero generation |
| VHT DL-MU plan/container/user completion | `VhtHcfFeature` | VHT-local `exchangeId` plus exact plan/packet checks |
| VHT sounding dialog | `VhtSoundingService` / `VhtHcfFeature` | dialog token and startup rollback |
| VHT ADDBA prerequisite packet | BA handler + queue + `InProgressFrames` | exact reservation and RAII compensation |
| HE DL-MU reservation/container/users | `HeDlMuExchangeProvider` | HE-DL-local `exchangeId`, queue tokens, association epochs |
| HE UL trigger exchange | HE UL services | trigger ID and response ledger |
| HE sounding dialog | `HeSoundingService` | HE sounding action/dialog token |
| Feature composition and HE service lifetime | `IHcfFeatureSet` implementation | NED type contract and explicit shutdown |
| Temporary SU aggregate | `HcfTransmissionPreparationService` | exception-safe temporary ownership only |

The local IDs are deliberately distinct namespaces:

```cpp
// Engine-only. Captured by self-message scheduling; never leaves the engine.
using ExchangeGeneration = uint64_t;

// VHT feature-only. Carried by VHT DL-MU frame-sequence callbacks.
using VhtDlMuExchangeId = uint64_t;

// HE DL provider-only. Carried by HE DL-MU callbacks.
using HeDlMuExchangeId = uint64_t;

// HE UL/on-air semantics. Existing trigger ID rules remain unchanged.
using HeTriggerId = uint32_t;
```

Do not add these aliases to one common identity header unless they materially
improve type safety without creating a new coupling point. Owner-local aliases
or small value classes are preferred.

## 4. Production file disposition

### Delete after all live references are removed

- `src/inet/linklayer/ieee80211/mac/contract/IHcfExchangeProvider.h`
- `src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangePlan.h`
- `src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangePlan.cc`
- `src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangeSelector.h`
- `src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangeSelector.cc`
- `src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfRuntime.h`
- `src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfRuntime.cc`

### Simplify

- `Hcf.h/.cc`: remove `runtime`, generic terminal sink, generic commit routing,
  selector accessors, and type-erased grant handling.
- `HcfExchangeEngine.h/.cc`: replace generic transaction identity with one
  private timer generation; remove generic terminal callbacks and abort reason.
- `IHcfFeatureSet.h`: retain configuration, amendment kind, and typed HE service
  bundle; remove descriptors and `ExchangeCommitter`.
- `HcfFeatureSet.h/.cc`: remove action-provider map and wrapper transactions;
  preserve HE service ownership and shutdown order.
- `HcfContext.h`: retain durable immutable snapshot value types; remove
  `HcfExchangeClass`, `NUM_HCF_EXCHANGE_CLASSES`, `HcfContextRejectionCode`, and
  the type-erased `HcfContext` container.
- `VhtHcfFeature.h/.cc`: make `StartKind` the only grant discriminant; strengthen
  local DL-MU lifecycle, validation, and failure cleanup.
- `IVhtDlMuExchangeCallback.h` and `VhtDlMuTxOpFs.h/.cc`: add an explicit typed
  precommit-planning failure callback and use VHT-local IDs.
- `HeTxopCoordinatorService.h/.cc`: use a typed HE `StartKind`, not a generic HCF
  exchange class.
- `HeHcfRuntime.h/.cc` and `HeHcf.cc`: prepare and commit `GrantSnapshot`
  directly; remove `HcfContext` wrapping.
- `HeDlMuExchangeProvider.h/.cc`: retain real reservation/callback guards,
  remove the empty `rollbackStart()` seam, and document its local lifecycle.
- `HeSoundingService.h/.cc`: stop implementing `IHcfExchangeProvider`; retain
  typed `prepareSounding()` and `commitPreparedSounding()`.
- `HcfTransmissionPreparationService.h/.cc`: behavior unchanged; replace
  misleading “transactional” wording with “exception-safe temporary ownership.”

### Preserve unless compilation proves a direct dependency

- NED and INI files;
- schedulers and rate control;
- PHY mode and duration selection;
- packet/chunk definitions;
- frame-sequence exchange behavior other than callback names and the new VHT
  planning-failed notification.

The file `HcfContext.h` should not be renamed in this refactor. It still contains
widely used immutable HCF/HE queue, peer, packet, PHY, and sounding snapshots.
A later name cleanup can split those types without mixing structural churn into
this behavioral change.

## 5. Execution discipline

Each phase below is one reviewable commit. Before every edit under `src/inet`:

```sh
git status --short
rg -n 'src/inet/linklayer/ieee80211' \
  .agents/skills/inet-architectural-requirements/references/enforcement/sealing-status.md
```

The current paths are unsealed because only `src/inet/common/packet/` is sealed
recursively. Recheck this at execution time. Preserve unrelated changes,
especially the current untracked report.

At each phase boundary:

```sh
git diff --check
git diff --stat
```

Build and run the phase's focused tests. Stop on the first unexplained failure.
Do not update any fingerprint CSV. If implementation requires a new NED/INI
compatibility flag, another owner for mutable state, or a second dispatch
framework, stop and amend this design before continuing.

---

## Phase 0 — Freeze the post-first-stage baseline

### Objective

Capture the exact current behavior and reference inventory before removing the
remaining shell.

### Commands

```sh
git rev-parse HEAD
git status --short

rg -n '\b(HcfExchangeSelector|PreparedHcfExchange|IHcfExchangeTransaction|IHcfExchangeProvider|HcfExchangePlan|HcfTransactionIdentity|HcfExchangeClass|HcfContext)\b' \
  src/inet/linklayer/ieee80211 tests/unit tests/module doc WHATSNEW

make MODE=release -j$(nproc)

inet_run_unit_tests -m release \
  -f '(HcfExchangeEngine|HcfExchangeCoordinator|HcfExchangePlan|HcfExchangeSelector|HcfFeatureSet|HcfFeatureSetReplacement|HcfTransmissionPreparationService|Ieee80211HtSoundingExchange|HtSoundingRetryState|Ieee80211VhtSoundingCodec|Ieee80211VhtDlMuScheduler|Ieee80211VhtAddbaQueueing|HeDlMuExchangeProvider|Ieee80211HeDlMuTransaction|Ieee80211HeUlMuTransaction|Ieee80211HeTxopCoordinatorService).*\.test'

inet_run_module_tests -m release --no-concurrent \
  -f '(Ieee80211SharedMacModes|Ieee80211VhtDlMuNegative|Ieee80211HeDlMuExchange|Ieee80211HeUlTriggerExchange|Ieee80211Retransmission(1|2|5|6|8)).*\.test'
```

Record command, cwd, mode, exit status, and first failure. Record pre-existing
architecture-check findings separately from refactor-introduced findings; the
focused checker currently reports broad include-graph violations in this
subtree, so a nonzero baseline is not evidence that this change introduced them.

### Exit criterion

The baseline is reproducible and all generic symbol users are classified as
“delete,” “migrate to typed owner,” or “durable snapshot type to retain.”

---

## Phase 1 — Establish owner-local terminal and rollback guarantees

### Objective

Close existing local failure windows before the generic selector stops masking
them with an active outer lifecycle. The old selector remains wired throughout
this phase.

### 1.1 Make local IDs monotonic and non-reusable

Replace rollback logic that decrements counters. Use an allocator that skips
zero and fails loudly on wrap:

```cpp
uint64_t VhtHcfFeature::allocateDlMuExchangeId()
{
    if (nextDlMuExchangeId == 0)
        throw cRuntimeError("VHT DL-MU exchange ID space exhausted");
    const auto id = nextDlMuExchangeId++;
    if (nextDlMuExchangeId == 0)
        throw cRuntimeError("VHT DL-MU exchange ID wrapped");
    return id;
}
```

Never do this:

```cpp
// Forbidden: a callback from the failed start could alias a later exchange.
--nextDlMuExchangeId;
```

Apply the same rule to the HE DL-MU provider's local counter. Existing on-air
trigger/dialog-token wrapping rules remain protocol-specific and are not
silently converted to `uint64_t`.

### 1.2 Give VHT stale planning an explicit owner callback

Extend `IVhtDlMuExchangeCallback`:

```cpp
virtual void vhtDlMuPlanningFailed(
        uint64_t exchangeId,
        VhtDlMuPlanningFailure reason) = 0;
```

In `VhtDlMuTxOpFs::prepareStep()`, notify before returning no step:

```cpp
try {
    return prepareDlMuStep();
}
catch (const VhtDlMuStalePlan&) {
    callback->vhtDlMuPlanningFailed(
            exchangeId, VhtDlMuPlanningFailure::STALE_PLAN);
    return nullptr;
}
```

Add an owner-local lifecycle to `VhtHcfFeature`; do not create a reusable generic
transaction class:

```cpp
enum class DlMuPhase { IDLE, PREPARED, COMMITTING, ACTIVE, TERMINAL };

struct DlMuLifecycle {
    DlMuPhase phase = DlMuPhase::IDLE;
    uint64_t exchangeId = 0;
    bool ownershipCommitted = false;
    std::optional<VhtDlMuPlanningFailure> pendingFailure;
};
```

Callback rules:

```cpp
void VhtHcfFeature::vhtDlMuPlanningFailed(
        uint64_t id, VhtDlMuPlanningFailure reason)
{
    if (id != dlMu.exchangeId)
        return;                         // late callback from an obsolete exchange
    if (dlMu.phase == DlMuPhase::IDLE || dlMu.phase == DlMuPhase::TERMINAL)
        return;
    if (dlMu.pendingFailure)
        throw cRuntimeError("Duplicate VHT DL-MU planning failure");
    if (dlMu.ownershipCommitted)
        throw cRuntimeError("VHT DL-MU planning failed after ownership commit");
    if (dlMu.phase == DlMuPhase::COMMITTING) {
        dlMu.pendingFailure = reason;   // same-stack callback; drain after start
        return;
    }
    clearActiveDlMuExchange(id);
}
```

Startup becomes explicitly reentrancy-safe:

```cpp
void VhtHcfFeature::commitDlMu(const VhtGrantSnapshot& snapshot)
{
    validateDlMuSnapshot(snapshot);     // no mutation before this line succeeds
    if (dlMu.phase != DlMuPhase::IDLE)
        throw cRuntimeError("Another VHT DL-MU exchange is active");

    const auto id = allocateDlMuExchangeId();
    dlMu = {DlMuPhase::PREPARED, id, false, std::nullopt};
    auto rollback = makeScopeGuard([&] {
        if (!dlMu.ownershipCommitted)
            restoreDlMuPreHandoffState(snapshot, id);
    });

    dlMu.phase = DlMuPhase::COMMITTING;
    configureProtectionAndStart(snapshot, [&] {
        actions->startFeatureFrameSequence(
                txOpFactory->create(*snapshot.dlMuPlan, /* ... */, id),
                snapshot.accessCategory);
    });

    if (dlMu.pendingFailure) {
        restoreDlMuPreHandoffState(snapshot, id);
        rollback.release();             // state was restored exactly once
        return;                         // sequence finished synchronously
    }
    dlMu.phase = DlMuPhase::ACTIVE;
    rollback.release();
}
```

Every cleanup path first enters `TERMINAL`, applies terminal effects once, then
clears to `IDLE`. `processVhtDlMuUserResult()` follows the same rule after the
last distinct user outcome. This makes a duplicate callback distinguishable
from a valid callback on an active exchange even though `TERMINAL` may be brief:

```cpp
void VhtHcfFeature::finishDlMuExchange(uint64_t id)
{
    if (id != dlMu.exchangeId || dlMu.phase == DlMuPhase::IDLE)
        return;
    if (dlMu.phase == DlMuPhase::TERMINAL)
        throw cRuntimeError("Duplicate VHT DL-MU terminal callback");
    dlMu.phase = DlMuPhase::TERMINAL;
    clearOwnedDlMuReferences();
    dlMu = {};
}
```

The actual implementation may use the repository's existing local rollback
guard instead of `makeScopeGuard`; the semantic order above is mandatory.

### 1.3 Restore VHT protection state on every pre-handoff failure

Capture protection state before mutation and restore it if sequence ownership
was not committed:

```cpp
struct ProtectionSnapshot {
    // Use TxopProcedure's actual typed state/accessors, not duplicated durations.
    TxopProtectionState state;
};

auto before = snapshotProtection(*txop);
try {
    configureProtection(*txop, grant);
    startSequence();
}
catch (...) {
    restoreProtection(*txop, before);
    throw;
}
```

If `TxopProcedure` has no safe typed snapshot/restore seam, add the smallest
owner-local API there. Do not duplicate PHY duration calculation or reach into
private members. Test failures after protection configuration and after sequence
allocation/start.

### 1.4 Make HE DL-MU same-stack completion explicit

Retain `ReservationRollbackGuard`, exact reservation identities, scheduler
finalization, and all existing callbacks. Add/document a local phase:

```cpp
enum class StartPhase {
    IDLE,
    PREPARED,
    RESERVED,
    COMMITTING,
    ACTIVE,
    TERMINAL,
};

struct ActiveStart {
    StartPhase phase = StartPhase::IDLE;
    uint64_t exchangeId = 0;
    std::optional<PlanningFailure> pendingFailure;
};
```

During `commitStart()`:

```cpp
bool HeDlMuExchangeProvider::commitStart(const PreparedStart& start)
{
    validatePreparedStart(start);       // queue/packet/epoch/capability checks
    activeStart.phase = StartPhase::PREPARED;
    reservePlan(start);
    activeStart.phase = StartPhase::RESERVED;
    ReservationRollbackGuard rollback(*this, start);
    activeStart.phase = StartPhase::COMMITTING;

    actions->startExchange(start, activeStart.exchangeId);

    if (activeStart.pendingFailure) {
        rollbackReservation(start);
        rollback.release();             // explicit, exactly-once compensation
        clearActiveStart();
        return false;
    }
    activeStart.phase = StartPhase::ACTIVE;
    rollback.release();
    return true;
}
```

`heDlMuPlanningFailed()` records a pending failure while `COMMITTING`; otherwise
it executes the existing matching-ID rollback. `heDlMuPlanFinalized()` and
`heDlMuPlanCommitted()` retain the current precise precommit/ownership boundary.
Every terminal path enters `TERMINAL` before clearing to `IDLE`, so duplicate
same-stack and later-event callbacks can be classified without relying on the
deleted selector.

### 1.5 Preserve owner-local teardown order

Make callback invalidation part of each owner shutdown, not `Hcf` destruction:

```cpp
void HeDlMuExchangeProvider::shutdown()
{
    callbacksEnabled = false;
    if (activeStart.phase == StartPhase::PREPARED ||
            activeStart.phase == StartPhase::RESERVED ||
            activeStart.phase == StartPhase::COMMITTING)
        rollbackActiveReservation();
    cancelOwnedTimers();
    reclaimProviderOwnedPackets();
    activeStart = {};
}
```

Use only the operations the provider actually owns; do not invent a provider
timer if none exists. `HeHcfFeatureSet` must continue shutting down triggered UL
before service members are destroyed. VHT invalidates its callback target and
clears only VHT-owned references; frame-sequence-owned packets remain with the
engine/sequence teardown path.

Remove no generic APIs yet. This phase proves owner-local behavior while the old
outer terminal completion remains a no-op safety net.

### Tests and exit criterion

Add focused fault injection after:

1. exact reservation;
2. protection configuration;
3. local ID allocation;
4. frame-sequence allocation;
5. the synchronous `startFrameSequence()` entry;
6. plan-finalized but before ownership commit.

For each point assert queue length/order, exact packet owner, BA state, protection
state, active local ID/phase, timer state, and ability to start the next grant.
Add duplicate, stale, and synchronous planning-failed callback cases. Phase 1 is
complete only when owner-local tests pass with the selector still present.

---

## Phase 2 — Introduce typed grant APIs and bypass the selector

### Objective

Route all live VHT/HE grants directly by their already-computed typed outcome.
Keep the generic files compiled but unreachable for one phase so behavioral
failures are separated from deletion/compiler cleanup.

### 2.1 VHT: make `StartKind` authoritative

Remove `VhtGrantSnapshot::exchangeClass`. The immutable snapshot is its own
closed discriminated value:

```cpp
struct VhtGrantSnapshot {
    enum class StartKind {
        COMMON_SINGLE_USER,
        GROUP_MANAGEMENT,
        BLOCK_ACK_PREREQUISITE,
        MU_SOUNDING,
        DL_MULTIUSER,
        SU_SOUNDING,
    };

    StartKind startKind = StartKind::COMMON_SINGLE_USER;
    AccessCategory accessCategory = AC_BE;
    // Existing typed peer, epoch, queue, packet, mode, and plan fields.
};
```

Provide one direct commit entry:

```cpp
enum class GrantDisposition { STARTED, FINISHED_SYNCHRONOUSLY };

GrantDisposition VhtHcfFeature::commitPreparedGrant(
        const VhtGrantSnapshot& grant)
{
    switch (grant.startKind) {
        case StartKind::COMMON_SINGLE_USER:
            actions->continueBaseFrameSequence(grant.accessCategory);
            return GrantDisposition::STARTED;
        case StartKind::SU_SOUNDING:
        case StartKind::MU_SOUNDING:
            startSounding(grant);
            return GrantDisposition::STARTED;
        case StartKind::GROUP_MANAGEMENT:
            commitGroupManagement(grant);
            return GrantDisposition::STARTED;
        case StartKind::BLOCK_ACK_PREREQUISITE:
            commitBlockAckPrerequisite(grant);
            return GrantDisposition::STARTED;
        case StartKind::DL_MULTIUSER:
            return commitDlMu(grant);
    }
    throw cRuntimeError("Unknown VHT grant kind");
}
```

Use narrow per-kind validators before mutation:

```cpp
void VhtHcfFeature::validateBlockAckGrant(const VhtGrantSnapshot& grant) const
{
    if (!grant.packetIdentity.isValid() || !grant.sourceQueueToken.isValid())
        throw cRuntimeError("Incomplete VHT ADDBA grant");
    auto *queue = resolveQueue(grant.sourceQueueToken);
    auto *packet = findExactPacket(queue, grant.packetIdentity);
    if (packet == nullptr || !isAssociatedPeer(grant.peer) ||
            getPeerAssociationGeneration(grant.peer) != grant.associationGeneration)
        throw cRuntimeError("VHT ADDBA grant became stale before commit");
}
```

The nested `HcfVhtRuntime::startFrameSequence()` becomes:

```cpp
void startFrameSequence(AccessCategory ac)
{
    const auto grant = feature.prepareGrantSnapshot(ac);
    feature.commitPreparedGrant(grant);
}
```

No `HcfContext`, selector, engine transaction identity, or generic class appears.

### 2.2 HE: replace generic class with typed `StartKind`

Change `HeTxopCoordinatorService::GrantSnapshot`:

```cpp
struct GrantSnapshot {
    enum class StartKind {
        CHANNEL_RELEASE,
        FORCED_SINGLE_USER,
        UL_TRIGGER,
        SOUNDING,
        RECOVERY_SINGLE_USER,
        DL_MULTIUSER,
        PREPARED_SINGLE_USER,
        COMMON_SINGLE_USER,
    };

    StartKind startKind = StartKind::CHANNEL_RELEASE;
    AccessCategory accessCategory = AC_BE;
    std::optional<HeUlTriggerService::PreparedStart> ulTrigger;
    std::optional<HeDlMuExchangeProvider::PreparedStart> dlStart;
};
```

`prepareGrant()` still evaluates admission in the existing deterministic order.
It returns the exact typed snapshot directly:

```cpp
HeTxopCoordinatorService::GrantSnapshot
HeHcfRuntime::prepareGrant(AccessCategory ac, bool hasEligibleFrame)
{
    finalizeRetiredQueueBanksIfSafe();
    // Lazily capture UL/DL snapshots exactly once as today.
    return txopCoordinator.prepareGrant(ac, heMode, forcedSingleUser,
            hasExecutableFrame, preparationActions);
}
```

Commit directly:

```cpp
void HeHcfRuntime::commitGrant(const GrantSnapshot& grant)
{
    const auto ac = grant.accessCategory;
    switch (grant.startKind) {
        case StartKind::CHANNEL_RELEASE:
            hcf->releaseChannel(ac);
            return;
        case StartKind::COMMON_SINGLE_USER:
            hcf->startSingleUserExchange(ac);
            return;
        case StartKind::FORCED_SINGLE_USER:
            if (!getDlMuExchangeProvider().consumeForcedSingleUser(ac))
                throw cRuntimeError("Forced HE SU grant became stale");
            hcf->startSingleUserExchange(ac);
            return;
        case StartKind::UL_TRIGGER:
            if (!grant.ulTrigger || !ulTriggerService.commitStart(*grant.ulTrigger))
                throw cRuntimeError("Prepared HE UL grant became stale");
            return;
        case StartKind::SOUNDING:
        case StartKind::RECOVERY_SINGLE_USER:
        case StartKind::DL_MULTIUSER:
        case StartKind::PREPARED_SINGLE_USER:
            if (!grant.dlStart ||
                    !getDlMuExchangeProvider().commitStart(*grant.dlStart))
                throw cRuntimeError("Prepared HE DL grant became stale");
            return;
    }
    throw cRuntimeError("Unknown HE grant kind");
}
```

Then:

```cpp
void HeHcfRuntime::startFrameSequence(AccessCategory ac)
{
    const auto grant = prepareGrant(ac, hasFrameToTransmit(ac));
    commitGrant(grant);
}
```

Do not catch merely to call the current empty `rollbackStart()`. Real reservation
rollback remains inside `commitStart()` where ownership is known.

### 2.3 Feature gating and priority

Feature flags remain inputs to the typed preparation services. Preserve and test
the current semantic order explicitly instead of relying on selector sorting:

```cpp
// Illustrative HE order; keep the exact current eligibility predicates.
if (forcedSingleUser)
    return forcedSuGrant();
if (enableUlMu && auto ul = actions.prepareUlTrigger())
    return ulGrant(*ul);
if (auto dl = actions.prepareDlStart())
    return grantFor(*dl);        // SOUNDING, RECOVERY, or DL_MULTIUSER
if (hasCommonFrame)
    return commonSuGrant();
if (auto su = actions.prepareSingleUser())
    return preparedSuGrant(*su);
return releaseGrant();
```

Do not replace this with dynamic casts or scattered `isHe`/`isVht` checks.
Amendment selection remains the one existing `IHcfFeatureSet` runtime-kind
decision in `Hcf::initialize()`.

### Exit criterion

Repository traces show no live call to `HcfExchangeSelector::selectAndCommit()`.
All focused VHT/HE tests pass, including immediate sequence finish. The unused
generic shell still compiles, which makes this a clean behavioral checkpoint.

---

## Phase 3 — Reduce the engine identity to timer generation

### Objective

Remove generic transaction ownership from `HcfExchangeEngine` while retaining
all valid scheduled-event and equal-time timeout behavior.

### API and state

Remove from `Actions`:

```cpp
std::function<bool()> hasTransactionalOwner;
std::function<void(HcfTransactionIdentity, HcfExchangeAbortReason)>
        exchangeTerminated;
```

Replace state:

```cpp
uint64_t nextExchangeGeneration = 1;
uint64_t activeExchangeGeneration = 0;
uint64_t startRxTimerGeneration = 0;
uint64_t deferredTimeoutGeneration = 0;
```

Allocator and retirement:

```cpp
void HcfExchangeEngine::beginExchangeIfNeeded()
{
    if (activeExchangeGeneration != 0)
        return;
    if (nextExchangeGeneration == 0)
        throw cRuntimeError("HCF exchange generation exhausted");
    activeExchangeGeneration = nextExchangeGeneration++;
    if (nextExchangeGeneration == 0)
        throw cRuntimeError("HCF exchange generation wrapped");
}

void HcfExchangeEngine::clearExchange()
{
    activeExchangeGeneration = 0;
    startRxTimerGeneration = 0;
    deferredTimeoutGeneration = 0;
}
```

Timer scheduling and delivery retain exact generation checks:

```cpp
void HcfExchangeEngine::scheduleStartRxTimer(simtime_t timeout)
{
    startRxTimerGeneration = activeExchangeGeneration;
    getActiveActions().scheduleTimer(startRxTimer, timeout);
}

bool HcfExchangeEngine::handleMessage(cMessage *message, const Actions& actions)
{
    if (message == startRxTimer) {
        if (activeExchangeGeneration == 0 ||
                startRxTimerGeneration != activeExchangeGeneration)
            return true; // retired self-message
        // Existing timeout/deferred-timeout logic follows unchanged.
    }
    // Existing inactivity handling follows unchanged.
}
```

Remove `terminalAbortReason`, `notifyTransactionTerminated()`,
`getActiveTransactionIdentity()`, and `isActiveTransaction()`. Frame completion
becomes only engine/coordinator cleanup:

```cpp
void HcfExchangeEngine::frameSequenceFinished()
{
    if (!exchangeCoordinator.complete())
        return;
    responseService.clearTimerState();
    deferredTimeoutGeneration = 0;
    auto *context = frameSequenceHandler->getContext();
    auto& actions = getActiveActions();
    actions.frameSequenceFinished(context);
    exchangeCoordinator.reset();
    clearExchange();
    actions.resumeContention();
}
```

Preserve the current required ordering if tests show `clearExchange()` must occur
before a callback that can synchronously request another grant. The invariant is
that the retired generation cannot be observed as active by the next exchange.

### Tests

Rewrite `HcfExchangeEngine_1.test` around engine responsibilities:

- direct zero-step sequence finishes on the same C++ stack;
- `frameSequenceFinished` and `resumeContention` occur exactly once;
- no generic terminal callback exists;
- a stale dispatched start-RX timer cannot affect the next exchange;
- equal-time RX/timeout deferral is cleared by a successful response;
- duplicate finish/abort callbacks do not double-resume contention;
- preparation without a sequence retires the generation and timers.

Adjust `Ieee80211DeferredRxTimeout_1.test` to the new `Actions` surface without
weakening its equal-time assertions.

### Exit criterion

The engine exposes no provider identity or transaction result. It still rejects
stale timer work deterministically in release and debug builds.

---

## Phase 4 — Narrow the feature-set composition contract

### Objective

Preserve the NED replacement seam while deleting the generic provider registry,
committer callback, and thin `HcfRuntime` wrapper.

### Target `IHcfFeatureSet`

```cpp
class INET_API IHcfFeatureSet
{
  public:
    virtual ~IHcfFeatureSet() = default;
    virtual void configureFeatures(const HcfFeatureConfiguration&) {}
    virtual HcfAmendmentRuntimeKind getAmendmentRuntimeKind() const
        { return HcfAmendmentRuntimeKind::COMMON; }
    virtual HcfHeRuntimeServices getHeRuntimeServices() { return {}; }
};
```

Delete:

- `HcfExchangeProviderDescriptor`;
- `ExchangeCommitter`;
- `getExchangeProviderDescriptors()`;
- `configureExchangeCommitter()`.

`CommonHcfFeatureSet` becomes composition-only:

```cpp
class CommonHcfFeatureSet : public cSimpleModule, public IHcfFeatureSet
{
  protected:
    HcfFeatureConfiguration configuration;

  public:
    void configureFeatures(const HcfFeatureConfiguration& value) override
        { configuration = value; }
};
```

Retain `HeHcfFeatureSet` members and shutdown order:

```cpp
HeHcfFeatureSet::~HeHcfFeatureSet()
{
    triggeredUlExchangeService.shutdown();
}
```

Do not move this cleanup into `Hcf` or a replacement runtime wrapper.

### Remove `HcfRuntime`

`Hcf::initialize()` directly configures the NED-paired feature set:

```cpp
auto *featureSet = dynamic_cast<IHcfFeatureSet *>(getSubmodule("featureSet"));
if (featureSet == nullptr)
    throw cRuntimeError("HCF featureSet does not implement IHcfFeatureSet");

featureSet->configureFeatures(featureConfiguration);
switch (featureSet->getAmendmentRuntimeKind()) {
    case HcfAmendmentRuntimeKind::COMMON:
        break;
    case HcfAmendmentRuntimeKind::VHT:
        vhtRuntime = std::make_unique<HcfVhtRuntime>(this);
        break;
    case HcfAmendmentRuntimeKind::HE: {
        auto services = featureSet->getHeRuntimeServices();
        if (!services.isComplete())
            throw cRuntimeError("HE feature set returned incomplete services");
        heRuntime = std::make_unique<HeHcfRuntime>(services, bindings, /* ... */);
        break;
    }
}
```

The switch is the existing amendment composition decision, not exchange
selection. Do not add concrete feature-set type tests.

### Replacement-contract tests

Rewrite `HcfFeatureSetReplacement_1.test` to prove:

- the NED slot still accepts another `IHcfFeatureSet` implementation;
- `configureFeatures()` is called once with the effective flags;
- common and VHT runtime kinds do not require generic providers;
- an HE replacement must return a complete typed service bundle;
- incomplete HE services fail during initialization, before events run.

Remove assertions about custom exchange providers; that extension API is an
intentional incompatible C++ contract reduction. NED type compatibility remains.

### Exit criterion

`HcfRuntime` has no references and is deleted. Feature-set tests cover the
narrowed composition contract and HE lifetime; all provider-order tests have
been replaced by typed VHT/HE admission-order tests.

---

## Phase 5 — Delete the generic transaction shell

### Objective

Delete the now-unreferenced abstractions and remove type erasure from the
snapshot header.

### 5.1 Remove HE sounding's provider wrapper

Change:

```cpp
class HeSoundingService final : public IHcfExchangeProvider
```

to:

```cpp
class HeSoundingService final
```

Delete `HeSoundingTransaction`, `getExchangeClass()`, and `prepareExchange()`.
The typed API remains:

```cpp
auto action = soundingService.prepareSounding(snapshot);
if (!action)
    return std::nullopt;

// At the owning commit point:
soundingService.commitPreparedSounding(*action, accessCategory);
```

`commitPreparedSounding()` must retain dialog-token validation and start-action
checks. If startup mutates a token before calling into the sequence, use its
existing local rollback guard; do not reintroduce a generic lifecycle.

### 5.2 Remove type-erased `HcfContext`

Delete the generic enum/container portion:

```cpp
enum class HcfExchangeClass { /* delete */ };
constexpr size_t NUM_HCF_EXCHANGE_CLASSES = /* delete */;
enum class HcfContextRejectionCode { /* delete */ };
class HcfContext { /* delete */ };
```

Retain and independently test value types such as `HcfPacketIdentity`,
`HcfQueueToken`, `HcfPeerSnapshot`, `HcfQueueSnapshot`, `HcfPhySnapshot`, and
`HcfHeSoundingSnapshot`. Remove now-unused `<typeindex>`, provider map, and generic
eligibility-array includes.

Where a unit test used `HcfContext::validate()` to test durable snapshots, move
the assertion to the actual typed preparation service that consumes those
snapshots. Do not keep a context container solely for a test.

### 5.3 Delete files and clean includes

Delete the seven files listed in section 4. Then run:

```sh
rg -n '\b(HcfExchangeSelector|PreparedHcfExchange|IHcfExchangeTransaction|IHcfExchangeProvider|HcfExchangePlan|HcfExchangeResult|HcfExchangeRejection|HcfTransactionIdentity|HcfTransactionToken|HcfTransactionGeneration|HcfExchangeClass|HcfContext)\b' \
  src/inet/linklayer/ieee80211 tests/unit tests/module doc WHATSNEW
```

Expected result: zero references to deleted generic types. References to the
filename `HcfContext.h` are allowed only for retained typed snapshot values.

Do not delete `ITx::PreparedTransmission`; it is unrelated and remains required
by triggered UL packet handoff.

### 5.4 Remove obsolete tests, but port their invariants

Delete the pure-mechanism tests:

- `tests/unit/HcfExchangePlan_1.test`;
- `tests/unit/HcfExchangeSelector_1.test`.

Before deletion, map their useful assertions to owners:

| Old assertion | New owner/test |
|---|---|
| validate/commit once | VHT/HE typed commit tests |
| rollback on commit throw | VHT ADDBA/DL-MU and HE provider failure injection |
| synchronous terminal during commit | VHT/HE local lifecycle tests |
| stale identity rejection | provider-local callback-ID tests |
| deterministic provider order | VHT/HE typed coordinator-order tests |
| teardown abort | owning HE/VHT service shutdown tests |

An assertion is not considered ported merely because another test happens to
pass; the replacement test must name and force the relevant boundary.

### Exit criterion

The production and test trees have no generic transaction symbol. A clean
release build succeeds, and owner-local tests cover every retained semantic
invariant from the deleted unit tests.

---

## Phase 6 — Terminology and local API cleanup

### Objective

Make the remaining code explain what it protects, so future maintainers do not
recreate the multithreading interpretation.

Use these names:

- `exchangeGeneration` for engine timer correlation;
- `exchangeId` for provider-local callback correlation;
- `PreparedStart`, `GrantSnapshot`, `ReservationRollbackGuard`, and
  `PlanningFailure` for owner-local concepts;
- “exception-safe temporary ownership” for aggregate cleanup.

Avoid these names in the changed path unless they describe an actual local
protocol transaction:

- `transactionToken`;
- `transactionGeneration`;
- `transactionalOwner`;
- `commitTransactionalExchange`;
- `terminal transaction result`.

Example callback contract comment:

```cpp
/**
 * VHT-DL-local correlation ID. Allocated monotonically by VhtHcfFeature when
 * starting a DL-MU sequence, invalidated after planning failure or all user
 * outcomes, and never compared with HCF engine generations. Zero is invalid.
 */
virtual void processVhtDlMuUserResult(
        uint64_t exchangeId, unsigned int userIndex, UserResult result) = 0;
```

Update `HcfTransmissionPreparationService` comments only; do not change its
already-straight-line behavior in this phase.

### Exit criterion

`rg -n 'transaction'` over the changed HCF/VHT/HE files finds only justified
protocol-domain usage or historical documentation, not the deleted outer shell.

---

## Phase 7 — Focused deterministic regression matrix

### Required unit coverage

| Area | Required cases |
|---|---|
| Engine | zero-step same-stack finish; stale timer generation; equal-time RX/timeout; one contention resume |
| VHT admission | common SU, SU/MU sounding, group management, ADDBA prerequisite, DL-MU; feature-off fallbacks |
| VHT validation | stale queue token, packet identity, association generation, absent plan |
| VHT rollback | failure after reservation, protection, ID allocation, sequence allocation/start |
| VHT callbacks | planning failure during commit, duplicate/late failure, duplicate/late user result, ID non-reuse |
| HE admission | forced SU, UL trigger, sounding, recovery SU, DL-MU, prepared SU, common SU, release |
| HE DL provider | reservation/finalization/ownership boundaries, packed subset, duplicate/late callbacks |
| HE UL | exact packet IDs, timeout ledger, late BA, association-generation rejection |
| Feature set | effective flags, runtime kind, complete HE service bundle, replacement NED contract |
| Transmission preparation | operation order and temporary aggregate deletion exactly once |
| Teardown | service timers cancelled, provider packets reclaimed, late callback inert |

### Existing tests to preserve or extend

- `HcfExchangeEngine_1`
- `HcfExchangeCoordinator_1`
- `HcfTransmissionPreparationService_1`
- `Ieee80211HtSoundingExchange_1`
- `HtSoundingRetryState_1`
- `Ieee80211VhtSoundingCodec_1`
- `Ieee80211VhtDlMuScheduler_1`
- `Ieee80211VhtAddbaQueueing_1`
- `HeDlMuExchangeProvider_1`
- `Ieee80211HeDlMuTransaction_1` (rename only in a later cleanup if desired)
- `Ieee80211HeUlMuTransaction_1`
- `Ieee80211HeTxopCoordinatorService_1`

Test names containing “Transaction” may remain temporarily when they describe a
real HE protocol exchange and renaming would obscure the behavioral diff.

### Release commands

From the repository root:

```sh
make MODE=release -j$(nproc)

inet_run_unit_tests -m release \
  -f '(HcfExchangeEngine|HcfExchangeCoordinator|HcfFeatureSet|HcfFeatureSetReplacement|HcfTransmissionPreparationService|Ieee80211HtSoundingExchange|HtSoundingRetryState|Ieee80211VhtSoundingCodec|Ieee80211VhtDlMuScheduler|Ieee80211VhtAddbaQueueing|HeDlMuExchangeProvider|Ieee80211HeDlMuTransaction|Ieee80211HeUlMuTransaction|Ieee80211HeTxopCoordinatorService).*\.test'

inet_run_module_tests -m release --no-concurrent \
  -f '(Ieee80211SharedMacModes|Ieee80211VhtDlMuNegative|Ieee80211HeDlMuExchange|Ieee80211HeUlTriggerExchange|Ieee80211Retransmission(1|2|5|6|8)).*\.test'
```

Then run stress retransmission cases `3`, `7`, `9`, and `10` after the narrow
set passes. Run one configuration/run/seed at a time; retain the configured
`${repetition}` seeds in `Ieee80211SharedMacModes_1`.

### Debug/failure-injection commands

```sh
make MODE=debug -j$(nproc)

inet_run_unit_tests -m debug \
  -f '(HcfExchangeEngine|HcfTransmissionPreparationService|Ieee80211Vht.*|HeDlMuExchangeProvider|Ieee80211He.*Transaction).*\.test'
```

Use one quoted alternation regex per runner invocation. If the runner rejects a
filter, inspect help and correct the syntax; do not retry an identical command.

### Observable simulation requirements

Module tests must prove, with their existing packet/event/result evidence:

- legacy g/n/ac paths still deliver DATA/ACK and aggregation/BA as configured;
- VHT DL-MU still performs per-user outcomes and stale-plan retry;
- HE DL-MU still emits the expected MU PPDU, MU-BAR/TB BA, and timeout behavior;
- HE UL still emits the expected trigger/response/Multi-STA BA and suppresses
  out-of-window responses;
- feature-off and MU-ineligible cases fall back to SU without provider probing;
- release occurs exactly once when no exchange is eligible.

Do not infer delivery or retry behavior from a test process exit alone when the
test is intended to validate an on-air exchange.

---

## Phase 8 — Architecture, fingerprint, and documentation gates

### Architecture checks

```sh
bash .agents/skills/inet-architectural-requirements/references/enforcement/check-architecture.sh \
  src/inet/linklayer/ieee80211/mac/coordinationfunction

bash .agents/skills/inet-architectural-requirements/references/enforcement/check-architecture.sh \
  src/inet/linklayer/ieee80211/mac
```

Reconcile output against Phase 0. Do not claim the known broad include-graph
baseline was introduced by this refactor, and do not add an exception-ledger
entry merely to make the command green. Any new violation is a blocker.

The final reviewer must explicitly audit the full general architecture checklist
and the full IEEE 802.11 overlay, particularly:

- AR-ORG-CONTRACTS and AR-MOD-PLUGGABLE for the narrowed feature-set seam;
- AR-COM-DIRECT for same-instant typed C++ calls;
- AR-WLAN-ARCH-OWNERSHIP and AR-WLAN-MAC-EXCHANGE for local mutable state;
- AR-WLAN-MAC-MULTIUSER for scheduler-plan versus PPDU separation;
- AR-WLAN-PHY-AUTHORITY and AR-WLAN-PHY-TIMING;
- AR-WLAN-STD-GATING and AR-WLAN-STD-TRACE;
- AR-QUAL-DETERMINISM, AR-QUAL-TESTS, and AR-WLAN-QUAL-TESTS.

This refactor changes no normative IEEE 802.11 behavior. Preserve existing
standard clause comments at the decisions they justify; do not add a standards
claim to explain a software-only lifecycle cleanup.

### Fingerprint gate

Run the smallest relevant existing HE fingerprint row first, with the exact
binary, mode, NED path, configuration, run, overrides, and seed:

```sh
tests/fingerprint/fingerprinttest -s -F tyf \
  -m '^(?:/examples/ieee80211ax/(dl_ofdma_sched|bsr|twt)/)' \
  tests/fingerprint/ieee80211-he.csv
```

Any mismatch is a failure until the first changed event is explained using the
appropriate log, event log, PCAP, or result evidence. The intended refactor may
remove internal C++ calls but should not change simulation events. Never update
the fingerprint CSV without explicit user approval.

### Documentation

Update:

- `doc/src/developers-guide/ch-80211.rst` with the final typed grant flow,
  owner-local identities, and narrowed extension contract;
- `WHATSNEW` with the removal of the generic provider/selector/plan shell.

Document that single-threaded execution permits straight-line typed dispatch,
while OMNeT++ event scheduling, same-stack reentrancy, and exception-safe
ownership still require the retained guards.

Do not modify `reports/inet-vs-ns3-comparison-report.md`; it is currently an
untracked user-owned file and is outside this implementation scope.

### Quantitative gate

Record before/after production counts for the deleted shell and changed files.
Acceptance requires:

- zero generic transaction types in production;
- fewer production lines and fewer lifecycle states overall;
- no new NED/INI parameters or modules;
- no new shared dispatcher/manager abstraction;
- no movement of queue, BA, scheduler, timer, or PHY authority.

---

## Phase 9 — Independent review and commit sequence

Use this commit sequence so every boundary is bisectable:

1. `802.11: harden VHT and HE local exchange cleanup`
2. `802.11: dispatch typed VHT and HE grants directly`
3. `802.11: reduce HCF engine identity to timer generation`
4. `802.11: narrow HCF feature-set composition contract`
5. `802.11: remove generic HCF transaction shell`
6. `802.11: clarify exchange correlation terminology`
7. `802.11: update HCF transaction-boundary documentation`

Before each commit, inspect the exact staged diff. Do not mix generated artifacts,
the unrelated report, or fingerprint CSV changes into these commits.

The independent review is complete only when it answers all of these questions:

1. Can a sequence finish inside `startFrameSequence()` without double cleanup,
   stale active state, or double contention resumption?
2. Can a retired timer fire at the same simulation time as a newer exchange and
   mutate that newer exchange?
3. Can any failed VHT/HE start reuse a local callback ID?
4. Does every mutation have an immediately preceding exact-snapshot validation?
5. Does every pre-handoff failure restore queue ownership, reservation, BA state,
   protection, local phase, and armed callbacks/timers?
6. Are post-handoff failures handled by one explicit frame-sequence/provider
   terminal path rather than an unsafe rollback?
7. Are feature gating and admission priority deterministic without a selector?
8. Is the `IHcfFeatureSet` NED replacement seam still usable and its incompatible
   C++ narrowing documented?
9. Does shutdown occur in owner order without callbacks into destroyed services?
10. Are release and debug tests green, and are fingerprint differences either
    absent or fully explained without updating expected values?

## 10. Definition of done

The simplification is complete when all of the following are true:

- VHT/HE grant preparation returns one typed immutable snapshot and direct typed
  dispatch commits it.
- The selector, provider, generic plan, prepared-exchange lifecycle, generic
  runtime wrapper, generic class, type-erased context, and engine transaction
  identity are deleted.
- Engine timer-generation checks, equal-time timeout behavior, and exact-once
  contention resumption remain covered.
- VHT/HE local IDs are monotonic, owner-scoped, documented, and protected against
  synchronous, duplicate, and late callbacks.
- VHT protection and ownership changes and HE reservation/ownership changes have
  tested compensation at every pre-handoff failure boundary.
- `IHcfFeatureSet` still supplies the NED composition seam and HE service lifetime
  without exposing generic exchanges.
- Focused release/debug unit and module suites pass.
- Architecture output introduces no new violation.
- Fingerprint review reports no unexplained trajectory change and no fingerprint
  CSV was modified.
- Documentation describes the final model as single-threaded direct dispatch with
  retained event-correlation and exception-safety guards—not as lock-free or
  concurrency-safe transaction processing.

If any retained owner cannot express its rollback and terminal semantics without
the generic selector, stop at that owner and keep the shell for that one path
temporarily. Do not weaken a demonstrated invariant to meet a deletion target.
