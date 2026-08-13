# IEEE 802.11 HCF transaction-boundary simplification plan

Date: 2026-08-13

Status: implementation-ready design; no production code has been changed by this plan

Primary goal: remove generic HCF transaction machinery from ordinary SU, channel release, HT sounding, VHT SU sounding, VHT MU sounding prerequisites, and one-queue aggregation while preserving the existing explicit frame-sequence state machine and all genuinely multi-owner transaction guarantees.

This plan is deliberately prescriptive. The conceptual C++ is written against the current symbols and should be followed closely, but it is not a copy-and-paste patch: reread each target immediately before editing and adapt only for compile errors or source drift. Any larger design change must stop the phase and amend this plan first.

## 1. Final boundary

| Exchange or operation | Final execution model | Reason |
|---|---|---|
| Ordinary SU DATA/ACK | Direct `HcfFs` frame sequence | One exchange owner; retry/ACK/queue owners already exist |
| Ordinary SU RTS/CTS/DATA/ACK | Direct `HcfFs` frame sequence | Multi-frame does not imply multi-owner preparation |
| Channel release | Direct coordinator completion | No reserved state |
| HT sounding | Direct `HtSoundingFs` | Current generic rollback and completion are no-ops |
| VHT SU sounding | Direct `VhtSoundingFs` with local token guard | Dialog token is the only speculative mutable state |
| VHT MU sounding prerequisite | Direct `VhtSoundingFs` with local token guard | It creates no `VhtDlMuPlan`; the next grant recomputes eligibility |
| One-queue SU A-MPDU | Straight-line local handoff | Only the temporary aggregate needs exception-safe ownership |
| VHT group management | Transactional in this refactor | Preserve current behavior; evaluate separately later |
| VHT ADDBA prerequisite | Transactional | Moves an exact queued packet and changes BA prerequisite state |
| VHT DL-MU data plan | Transactional | Coordinates queues, packet identities, BA state, users, and completion |
| HE UL trigger | Transactional | Coordinates scheduler and multi-user state |
| HE sounding/recovery/DL-MU | Transactional | Coordinates HE provider-owned state |
| HE `SINGLE_USER` with a prepared `dlStart` | Transactional | Per-STA staging/reservation exists despite the SU air exchange |
| HE ordinary `SINGLE_USER` without `dlStart` | Direct common SU | No HE-owned preparation must be committed |

The governing rule is:

> Use `HcfExchangePlan` only when preparation coordinates or reserves independently owned mutable state. The number of frames in an exchange is not a reason to use a transaction.

## 2. Non-negotiable invariants

Every phase must preserve these invariants:

1. `HcfExchangeCoordinator` and `HcfExchangeEngine` remain the single owners of the active frame sequence, response timers, retry/recovery transition, and contention resumption.
2. Direct exchanges never call `HcfExchangeSelector::selectAndCommit()` and never call `HcfExchangeSelector::exchangeTerminated()`.
3. Transactional exchanges still prepare once, validate once, commit once, and receive at most one terminal result with the exact token and generation.
4. The engine's current `HcfTransactionIdentity` remains temporarily named as-is and continues to guard stale timer generations for both direct and transactional sequences. Do not add another token/generation type in this work.
5. EDCAF, pending queue, `InProgressFrames`, retry, sequence-number, ACK, BlockAck, rate-selection, protection, and PHY-mode authorities do not move.
6. HT sounding retains its current behavior: `createSoundingSequence()` revalidates eligibility and records the retry/cooldown attempt before starting the sequence. The old generic transaction did not roll this back, so this refactor must not claim new HT rollback semantics.
7. VHT sounding retains its existing local `commitDialogToken()` / `rollbackDialogToken()` guard. Cooldown is recorded only after sequence startup succeeds.
8. A VHT MU sounding prerequisite does not carry or commit a DL-MU plan. A later EDCAF grant recomputes a fresh plan using the resulting CSI.
9. A temporary SU A-MPDU is deleted exactly once after a successful borrowed handoff or after an exception. The source packet is never deleted or transferred by preparation.
10. No NED module, NED parameter, INI option, compatibility flag, executor hierarchy, or second dispatch framework is introduced.
11. Production LOC must decrease. New code must replace branching/ceremony rather than add a permanent parallel path.

## 3. Target control flow

After this refactor, the grant path is intentionally small:

```text
Hcf::channelGranted
  -> validate grant / collision / wide-channel rules
  -> HcfExchangeEngine::channelGranted
  -> TxopProcedure::startTxop
  -> HE runtime, if present
       direct release or ordinary SU
       OR selector for a prepared HE exchange
  -> otherwise VHT runtime, if present
       direct release, ordinary SU, SU sounding, or MU sounding
       OR selector for VHT group/ADDBA/DL-MU
  -> otherwise direct release, HT sounding, or ordinary SU
  -> HcfExchangeEngine owns the resulting sequence and timers
```

The selector must be visibly absent from the common path.

## 4. Exact production change surface

| File | Planned responsibility |
|---|---|
| `src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h` | Direct release/HT APIs; remove common context/commit APIs |
| `src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc` | Top-level grant dispatch, direct HT/SU/release, nested VHT runtime, terminal wiring |
| `src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangeEngine.h/.cc` | Conditional transaction terminal notification; retain timers/generation |
| `src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfRuntime.h/.cc` | Stop requiring direct descriptors; retain transactional selector |
| `src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfFeatureSet.h/.cc` | Register true transaction providers only; retain HE prepared-SU provider |
| `src/inet/linklayer/ieee80211/mac/contract/IHcfFeatureSet.h` | Update contract comments; keep the existing `ExchangeCommitter` API name |
| `src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfContext.h` | Remove HT mode-name projection; remove direct-only enum values |
| `src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangePlan.h/.cc` | Transactional-class predicate and reduced class order |
| `src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfRuntime.h/.cc` and `HeHcf.cc` | Direct HE release/common SU; retain prepared HE fallback selection |
| `src/inet/linklayer/ieee80211/mac/coordinationfunction/VhtHcfFeature.h/.cc` | Optional transaction class, direct sounding, transactional commit split |
| `src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfTransmissionPreparationService.h/.cc` | Replace staged mini-transaction with `prepareAndTransmit()` |
| `doc/src/developers-guide/ch-80211.rst` and `WHATSNEW` | Explain final boundary and extension-contract change |

No NED, MSG, serializer, PHY, mode, radio, INI, or packet-format file is in the
planned production change surface. If compilation appears to require such a
change, stop and reassess instead of expanding scope automatically.

## 5. Required phase discipline

Each numbered phase below is one reviewable commit unless the phase explicitly says otherwise. At the end of every phase:

- build the matching INET mode before running tests;
- run the phase's focused tests;
- inspect `git diff --check` and `git diff --stat`;
- stop at the first unexplained failure;
- do not update fingerprints;
- do not proceed with a compatibility workaround that is not described here.

Before editing anything under `src/inet`, rerun the sealing check. The relevant HCF paths are currently unsealed, but that fact must be revalidated at execution time.

---

## Phase 0 — Freeze the baseline and source contract

### Objective

Record a reproducible before-state for direct SU, sounding, aggregation, and the VHT/HE transactions that must remain intact.

### Read-only checks

```sh
git status --short
git rev-parse HEAD
rg -n 'src/inet/linklayer/ieee80211' \
  .agents/skills/inet-architectural-requirements/references/enforcement/sealing-status.md
```

Record the working tree state. Do not overwrite unrelated user changes.

### Build and baseline tests

From the repository root:

```sh
make MODE=release -j$(nproc)

inet_run_unit_tests -m release \
  -f '(Ieee80211HtSoundingExchange|HtSoundingRetryState|HcfFeatureSet|HcfExchangePlan|HcfExchangeSelector|HcfExchangeEngine|HcfExchangeCoordinator|HcfAggregationPlanning|HcfTransmissionPreparationService|Ieee80211VhtSoundingCodec|Ieee80211VhtDlMuScheduler|Ieee80211VhtAddbaQueueing|Ieee80211HeMuAddbaValidation|Ieee80211HeDlMuTransaction|Ieee80211HeUlMuTransaction).*\.test'

inet_run_module_tests -m release --no-concurrent \
  -f '(Ieee80211SharedMacModes|Ieee80211VhtDlMuNegative|Ieee80211HeDlMuExchange|Ieee80211HeUlTriggerExchange|Ieee80211Retransmission(1|2|5|6|8)).*\.test'
```

If a runner rejects the regex syntax, consult its help and correct the invocation once; do not retry the identical failed command. Preserve command, cwd, mode, exit status, and first relevant failure.

### Baseline facts to record

- `Ieee80211Retransmission1`: DATA/ACK succeeds.
- `Ieee80211Retransmission2`: DATA timeout retains/retries the packet.
- `Ieee80211Retransmission5`: RTS/CTS/DATA/ACK succeeds.
- `Ieee80211Retransmission6`: DATA fails after CTS and then retries.
- `Ieee80211Retransmission8`: blocked RTS produces CTS timeout/retry.
- HT sounding eligible and ineligible paths.
- VHT SU and MU sounding exchange order and feedback correlation.
- VHT DL-MU negative/fault-injection rollback.
- HE DL/UL MU transaction rollback.
- Temporary aggregate cleanup on success and failure.

### Exit criterion

All baseline results are either PASS or documented as pre-existing failures. No source file has changed.

---

## Phase 1 — Make engine terminal notification conditional

### Objective

Allow a direct frame sequence to use the existing engine/coordinator without pretending that an `HcfExchangeSelector` transaction owns it. This phase changes no routing yet, so behavior must remain identical.

### Files

- `src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangeEngine.h`
- `src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangeEngine.cc`
- `src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc`
- `tests/unit/HcfExchangeEngine_1.test`

### API change

Add one predicate to `HcfExchangeEngine::Actions`:

```cpp
struct Actions {
    // Existing callbacks...
    std::function<bool()> hasTransactionalOwner;
    std::function<void(HcfTransactionIdentity,
            HcfExchangeAbortReason)> exchangeTerminated;
    // Existing callbacks...
};
```

Update `checkActions()` to require both callbacks. Do not make the predicate optional; an empty function would hide wiring errors.

### Engine implementation

Only notify a terminal transaction when one is active:

```cpp
void HcfExchangeEngine::notifyTransactionTerminated(
        const Actions& actions, HcfExchangeAbortReason reason)
{
    if (actions.hasTransactionalOwner())
        actions.exchangeTerminated(activeTransactionIdentity, reason);
}
```

Use that helper in both terminal locations:

```cpp
void HcfExchangeEngine::preparationCompletedWithoutSequence(
        const Actions& actions)
{
    ActionScope scope(*this, actions);
    exchangeCoordinator.preparationCompletedWithoutSequence();
    responseService.clearTimerState();
    actions.cancelTimer(exchangeCoordinator.getStartRxTimer());
    actions.cancelTimer(exchangeCoordinator.getInactivityTimer());
    notifyTransactionTerminated(actions, HcfExchangeAbortReason::NONE);
    clearTransaction();
}

void HcfExchangeEngine::frameSequenceFinished()
{
    if (!exchangeCoordinator.complete())
        return;
    responseService.clearTimerState();
    deferredTimeoutGeneration = 0;
    auto context = frameSequenceHandler->getContext();
    auto& actions = getActiveActions();
    notifyTransactionTerminated(actions, terminalAbortReason);
    actions.frameSequenceFinished(context);
    exchangeCoordinator.reset();
    clearTransaction();
    actions.resumeContention();
}
```

Wire the predicate in `Hcf::makeExchangeActions()`:

```cpp
actions.hasTransactionalOwner = [this] {
    return runtime->getExchangeSelector().hasActiveExchange();
};
actions.exchangeTerminated = [this](HcfTransactionIdentity identity,
        HcfExchangeAbortReason reason) {
    if (exchangeTerminalSinkForTesting)
        exchangeTerminalSinkForTesting(identity, reason);
    else
        runtime->getExchangeSelector().exchangeTerminated(identity, reason);
};
```

Do not put `hasActiveExchange()` inside `exchangeTerminated`; the engine predicate makes the direct/transactional ownership decision explicit and independently testable.

### Tests

Extend `HcfExchangeEngine_1.test` with two cases:

```cpp
bool transactional = false;
int terminalCalls = 0;
actions.hasTransactionalOwner = [&] { return transactional; };
actions.exchangeTerminated = [&](auto, auto) { terminalCalls++; };

// Direct sequence.
engine.channelGranted();
engine.beginPreparation();
engine.startFrameSequence(makeSequence(), makeContext(), actions);
finishSequence(engine, actions);
REQUIRE(terminalCalls == 0);
REQUIRE(resumeContentionCalls == 1);
REQUIRE(!engine.isStartRxTimerScheduled());

// Transactional sequence.
transactional = true;
engine.channelGranted();
engine.beginPreparation();
engine.startFrameSequence(makeSequence(), makeContext(), actions);
finishSequence(engine, actions);
REQUIRE(terminalCalls == 1);
```

Also cover `preparationCompletedWithoutSequence()` with the predicate false and true.

### Exit criterion

- Existing routing still activates the selector, so existing behavior is unchanged.
- Engine tests prove direct completion skips the terminal callback while still clearing timers and resuming contention.
- No identity rename or new generation type exists.

---

## Phase 2 — Add direct common grant routing and channel release

### Objective

Remove ordinary SU and channel release from `HcfContext -> provider -> plan -> committer`. Keep all amendment transactions reachable.

### Files

- `Hcf.h`, `Hcf.cc`
- `HeHcfRuntime.h`, `HeHcfRuntime.cc`, `HeHcf.cc`
- `HcfFeatureSet.h`, `HcfFeatureSet.cc`
- `HcfRuntime.h`, `HcfRuntime.cc`
- focused HCF feature/runtime tests

### 2.1 Extract the direct release helper

Declare in `Hcf`'s protected section so the friend runtimes can call it:

```cpp
void releaseChannel(AccessCategory ac);
```

Move the old `CHANNEL_RELEASE` commit body without semantic changes:

```cpp
void Hcf::releaseChannel(AccessCategory ac)
{
    auto edcaf = edca->getEdcaf(ac);
    exchangeEngine->preparationCompletedWithoutSequence(makeExchangeActions());
    edcaf->releaseChannel(this);
    edcaf->getTxopProcedure()->endTxop();
}
```

Order is intentional: first finish the engine's preparation state and clear timers, then release the EDCAF, then end the TXOP. Do not reorder without evidence.

### 2.2 Replace common `channelGranted()` selection

Keep all existing validation, collision, wide-channel, engine, and TXOP code. Replace only context construction and unconditional selection:

```cpp
exchangeEngine->channelGranted();
edcaf->getTxopProcedure()->startTxop(ac);

if (heRuntime != nullptr) {
    heRuntime->startFrameSequence(ac);
    return;
}
if (vhtRuntime != nullptr) {
    vhtRuntime->startFrameSequence(ac);
    return;
}
if (!hasEligibleFrame) {
    releaseChannel(ac);
    return;
}
startFrameSequence(ac);       // direct HT sounding or direct common SU
```

Do not introduce a generic `GrantDecision`, executor, visitor, or strategy type. The amendment runtime already owns amendment-specific decisions.

### 2.3 Route HE direct outcomes before selection

HE must prepare its grant snapshot exactly once. Its `SINGLE_USER` cases are not all equivalent.

Conceptual `HeHcfRuntime::startFrameSequence()`:

```cpp
void HeHcfRuntime::startFrameSequence(AccessCategory ac)
{
    auto context = buildGrantSelectionContext(ac, hasFrameToTransmit(ac));
    auto snapshot = context.findProviderSnapshot<
            HeTxopCoordinatorService::GrantSnapshot>();
    if (snapshot == nullptr)
        throw cRuntimeError("HE grant did not capture an exact snapshot");

    if (snapshot->exchangeClass == HcfExchangeClass::CHANNEL_RELEASE) {
        hcf->releaseChannel(ac);
        return;
    }

    if (snapshot->exchangeClass == HcfExchangeClass::SINGLE_USER &&
            !snapshot->dlStart.has_value()) {
        hcf->startSingleUserExchange(ac);
        return;
    }

    auto identity = exchangeEngine->getActiveTransactionIdentity();
    hcf->runtime->getExchangeSelector().selectAndCommit(context, identity);
}
```

Keep `SINGLE_USER + dlStart` transactional. That path can stage a per-STA packet and owns rollback-sensitive HE state. `FORCED_SINGLE_USER` also remains transactional in this refactor because consuming the forced flag is an HE-owned state transition.

### 2.4 Make VHT routing safe before switching the top-level grant path

The current `HcfVhtRuntime::startFrameSequence()` calls
`VhtHcfFeature::startFrameSequence()`, which commits every `StartKind`
directly. That legacy continuation API cannot be used from the new top-level
grant route until the runtime separates direct and transactional outcomes.

Implement this dispatcher in the same commit as the `channelGranted()` change:

```cpp
void HcfVhtRuntime::startFrameSequence(AccessCategory ac)
{
    if (!hcf->hasFrameToTransmit(ac)) {
        hcf->releaseChannel(ac);
        return;
    }

    auto snapshot = feature.prepareGrantSnapshot(ac);
    switch (snapshot.startKind) {
        case VhtGrantSnapshot::StartKind::COMMON_SINGLE_USER:
            hcf->startSingleUserExchange(ac);
            return;

        case VhtGrantSnapshot::StartKind::SU_SOUNDING:
        case VhtGrantSnapshot::StartKind::MU_SOUNDING:
            // Interim direct call; Phase 4 renames and narrows this API.
            feature.commitGrantSnapshot(snapshot);
            return;

        case VhtGrantSnapshot::StartKind::GROUP_MANAGEMENT:
        case VhtGrantSnapshot::StartKind::BLOCK_ACK_PREREQUISITE:
        case VhtGrantSnapshot::StartKind::DL_MULTIUSER: {
            HcfContext context(ac, {snapshot.exchangeClass});
            context.setProviderSnapshot(snapshot);
            hcf->runtime->getExchangeSelector().selectAndCommit(
                    context,
                    hcf->exchangeEngine->getActiveTransactionIdentity());
            return;
        }
    }
    throw cRuntimeError("Unknown VHT grant start kind");
}
```

This interim switch is mandatory. Without it, Phase 2 would make VHT ADDBA and
DL-MU direct by accident. Phase 4 removes the misleading sounding exchange
classes and replaces the interim `commitGrantSnapshot()` call with the narrow
`startSounding()` API.

### 2.5 Remove common action descriptors, but retain HE fallback support

`CommonHcfFeatureSet` becomes a composition root with no common transaction providers:

```cpp
std::vector<HcfExchangeProviderDescriptor>
CommonHcfFeatureSet::getExchangeProviderDescriptors()
{
    return {};
}
```

`HeHcfFeatureSet` must explicitly register `SINGLE_USER` for the prepared HE fallback only:

```cpp
std::vector<HcfExchangeProviderDescriptor>
HeHcfFeatureSet::getExchangeProviderDescriptors()
{
    auto descriptors = CommonHcfFeatureSet::getExchangeProviderDescriptors();
    if (getConfiguration().enableHeUlMuOfdma)
        descriptors.push_back(makeActionDescriptor(HcfExchangeClass::HE_UL_TRIGGER));
    descriptors.push_back(makeActionDescriptor(HcfExchangeClass::FORCED_SINGLE_USER));
    if (getConfiguration().enableHeDlMuMimo)
        descriptors.push_back(makeActionDescriptor(HcfExchangeClass::HE_SOUNDING));
    descriptors.push_back(makeActionDescriptor(HcfExchangeClass::RECOVERY_SINGLE_USER));
    descriptors.push_back(makeActionDescriptor(HcfExchangeClass::HE_DL_MULTIUSER));
    descriptors.push_back(makeActionDescriptor(HcfExchangeClass::SINGLE_USER));
    return descriptors;
}
```

Tighten `HcfActionExchangeProvider::prepareExchange()` so an HE `SINGLE_USER` descriptor accepts only a snapshot with a non-empty `dlStart`:

```cpp
const bool hePreparedSingleUser =
        exchangeClass == HcfExchangeClass::SINGLE_USER;
if (hePreparedSingleUser) {
    auto snapshot = context.findProviderSnapshot<
            HeTxopCoordinatorService::GrantSnapshot>();
    if (snapshot == nullptr ||
            snapshot->exchangeClass != HcfExchangeClass::SINGLE_USER ||
            !snapshot->dlStart.has_value()) {
        rejection = {/* NO_ELIGIBLE_PACKET, exact identity, reason */};
        return nullptr;
    }
}
```

Do not allow this descriptor to become a route back to ordinary common SU.

### 2.6 Relax `HcfRuntime`

Delete the constructor checks requiring `SINGLE_USER` and `CHANNEL_RELEASE`. Delete `getSingleUserDescriptor()` from header and implementation. An empty descriptor vector is valid for `CommonHcfFeatureSet`; its selector exists but is never entered.

Keep:

```cpp
const HcfExchangeProviderDescriptor *findExchangeProviderDescriptor(
        HcfExchangeClass exchangeClass) const;
```

It remains useful for tests and diagnostics.

### Tests

Add a focused grant-dispatch seam instead of testing only `startFrameSequence()`. The seam may be a protected test subclass wrapper around the post-TXOP dispatch; do not add a production observer framework.

Direct channel release assertions:

```cpp
dispatchGrantedAc(AC_BE);        // empty queue
REQUIRE(!edcaf->isOwning());
REQUIRE(txopEndCalls == 1);
REQUIRE(frameSequenceStarts == 0);
REQUIRE(!selector.hasActiveExchange());
REQUIRE(providerPrepareCalls == 0);
REQUIRE(transactionTerminalCalls == 0);
```

Direct ordinary SU assertions:

```cpp
Packet *source = enqueueUnicastData(AC_BE);
dispatchGrantedAc(AC_BE);
REQUIRE(activeSequenceIs<HcfFs>());
REQUIRE(inProgress->getFrameToTransmit() == source);
REQUIRE(!selector.hasActiveExchange());
REQUIRE(providerPrepareCalls == 0);
finishAckExchange();
REQUIRE(frameSequenceFinishedCalls == 1);
REQUIRE(resumeContentionCalls == 1);
```

### Exit criterion

- Ordinary common SU and empty-grant release never enter the selector.
- HE prepared SU fallback still enters the selector.
- Existing SU DATA/ACK and RTS/CTS tests pass.
- No direct compatibility descriptor remains in `CommonHcfFeatureSet`.

---

## Phase 3 — Make HT sounding direct

### Objective

Replace the HT mode-name projection and generic provider transaction with a direct pointer-based decision and an explicit `tryStart` method.

### Files

- `Hcf.h`, `Hcf.cc`
- `HcfFeatureSet.cc`
- `HcfContext.h`
- `tests/unit/Ieee80211HtSoundingExchange_1.test`
- `tests/unit/HcfFeatureSet_1.test`

### API replacement

Delete:

```cpp
std::optional<std::string> prepareHtSoundingModeIdentity(AccessCategory) const;
void startHtSoundingExchange(AccessCategory, const std::string&);
```

Add:

```cpp
const physicallayer::IIeee80211Mode *selectHtSoundingMode(
        AccessCategory ac) const;
bool tryStartHtSounding(AccessCategory ac);
```

### Conceptual implementation

```cpp
const IIeee80211Mode *Hcf::selectHtSoundingMode(AccessCategory ac) const
{
    auto edcaf = edca->getEdcaf(ac);
    auto packet = edcaf->getInProgressFrames()->getFrameToTransmit();
    if (packet == nullptr)
        return nullptr;

    auto header = packet->peekAtFront<Ieee80211MacHeader>();
    auto modeRequest = packet->findTag<Ieee80211ModeReq>();
    auto mode = modeRequest == nullptr ?
            rateSelection->computeMode(packet, header,
                    edcaf->getTxopProcedure()) :
            modeRequest->getMode();
    if (mode == nullptr)
        return nullptr;
    return htFeature->isSoundingEligible(header->getReceiverAddress(), mode) ?
            mode : nullptr;
}

bool Hcf::tryStartHtSounding(AccessCategory ac)
{
    auto mode = selectHtSoundingMode(ac);
    if (mode == nullptr)
        return false;

    auto edcaf = edca->getEdcaf(ac);
    auto packet = edcaf->getInProgressFrames()->getFrameToTransmit();
    if (packet == nullptr)
        return false;
    auto header = packet->peekAtFront<Ieee80211MacHeader>();

    // This call revalidates eligibility and records the HT sounding attempt.
    std::unique_ptr<IFrameSequence> sequence(
            htFeature->createSoundingSequence(
                    header->getReceiverAddress(), mode));
    if (sequence == nullptr)
        return false;

    setFrameMode(packet, header, mode);
    auto txop = edcaf->getTxopProcedure();
    if (!txop->isProtectionConfigured())
        txop->configureProtection(TxopProcedure::InitialProtection::NONE);
    exchangeEngine->beginPreparation();
    startExchangeFrameSequence(sequence.release(), buildContext(ac));
    return true;
}
```

`startFrameSequence()` becomes:

```cpp
void Hcf::startFrameSequence(AccessCategory ac)
{
    if (heRuntime != nullptr) {
        heRuntime->startFrameSequence(ac);
        return;
    }
    if (vhtRuntime != nullptr && !vhtRuntime->isContinuingFrameSequence()) {
        vhtRuntime->startFrameSequence(ac);
        return;
    }
    if (!hasFrameToTransmit(ac)) {
        releaseChannel(ac);
        return;
    }
    if (!tryStartHtSounding(ac))
        startSingleUserExchange(ac);
}
```

### Exception-safety decision

Do not add an HT rollback guard. The current `HtHcfFeature::createSoundingSequence()` records the attempt before the sequence starts, and the removed generic transaction has empty rollback. Preserving that behavior is the smallest correct refactor. A separate HT token rollback improvement would require a separately justified design and tests.

### Delete after compiling the direct path

- the already-disabled common `HT_SOUNDING` provider code and tests;
- `HcfContext::htSoundingModeIdentity`;
- constructor argument and getter for that string;
- mode-name lookup loop in `startHtSoundingExchange()`;
- obsolete includes needed only for `std::string`/`optional`, if unused.

Do not remove the enum value yet; enum cleanup occurs after all users and tests migrate.

### Tests

Extend `Ieee80211HtSoundingExchange_1.test`:

```cpp
Packet *source = addFrameAndGetIdentity(peer, AC_BE);
REQUIRE(hcf->tryStartHtSoundingForTest(AC_BE));
REQUIRE(sequenceTransmissions.size() == 1);
REQUIRE(sequenceTransmissions[0]->getName() ==
        std::string("HT-NDP-Announcement"));
REQUIRE(!hcf->getExchangeSelectorForTesting().hasActiveExchange());
REQUIRE(inProgress->getFrameToTransmit() == source);
REQUIRE(providerPrepareCalls == 0);
```

Also prove:

- ineligible sounding returns `false` and falls through to ordinary SU;
- request token/cooldown advances once on successful creation;
- queued DATA identity/header remains intact;
- feedback correlation and peer invalidation still work;
- no transaction terminal callback occurs.

### Exit criterion

No HT sounding path constructs `HcfContext`, `HcfExchangePlan`, or a mode-name identity.

---

## Phase 4 — Make VHT SU and MU sounding direct

### Objective

Keep `VhtGrantSnapshot` as a useful immutable value, but route sounding directly and never label MU sounding as a committed DL-MU transaction.

### Files

- `VhtHcfFeature.h`, `VhtHcfFeature.cc`
- nested `HcfVhtRuntime` in `Hcf.cc`
- `HcfFeatureSet.cc`
- VHT sounding and shared-mode tests

### 4.1 Remove the generic sounding class from the snapshot

`VhtGrantSnapshot::StartKind` already distinguishes `SU_SOUNDING` and `MU_SOUNDING`. Do not add `VHT_SOUNDING` to `HcfExchangeClass`.

For direct snapshots, set no transaction class. Change the field to an optional
and populate it only for transactional `GROUP_MANAGEMENT`,
`BLOCK_ACK_PREREQUISITE`, and `DL_MULTIUSER` snapshots:

```cpp
std::optional<HcfExchangeClass> exchangeClass;
```

`COMMON_SINGLE_USER`, `SU_SOUNDING`, and `MU_SOUNDING` leave it empty. This is
the one chosen representation; do not retain `SINGLE_USER` as a neutral VHT
label and do not add another classification enum.

Delete these assignments:

```cpp
// Delete from MU_SOUNDING preparation:
snapshot.exchangeClass = HcfExchangeClass::VHT_DL_MULTIUSER;

// Delete from SU_SOUNDING preparation:
snapshot.exchangeClass = HcfExchangeClass::VHT_SU_SOUNDING;
```

Also remove `soundingModeIdentity`; the snapshot already carries the authoritative `soundingMode` pointer.

### 4.2 Expose a narrow direct commit

Make the existing `commitSounding()` callable by the runtime without exposing
unrelated commit helpers. Add this public API:

```cpp
void startSounding(const VhtGrantSnapshot& snapshot);
```

Its body should be the current `commitSounding()` body, renamed only for clarity:

```cpp
void VhtHcfFeature::startSounding(const VhtGrantSnapshot& snapshot)
{
    if (snapshot.startKind != VhtGrantSnapshot::StartKind::SU_SOUNDING &&
            snapshot.startKind != VhtGrantSnapshot::StartKind::MU_SOUNDING)
        throw cRuntimeError("VHT direct sounding received a non-sounding snapshot");

    auto ndpMode = snapshot.soundingMode;
    if (ndpMode == nullptr ||
            !soundingService.getCoordinator().mayAttempt(snapshot.peer))
        throw cRuntimeError("Prepared VHT sounding became stale before start");

    auto token = soundingService.commitDialogToken(snapshot.dialogToken);
    RollbackGuard rollback([this, token] {
        soundingService.rollbackDialogToken(token);
    });

    auto sequence = new VhtSoundingFs(
            actions->getMac()->getMib(), &soundingService.getCsiCache(),
            snapshot.peer, snapshot.associationId,
            snapshot.associationGeneration, token, snapshot.soundingNsts,
            actions->getModeSet(), ndpMode, beamformingGainDb,
            snapshot.muFeedback,
            snapshot.muFeedback ? snapshot.soundingNsts : 1);
    actions->startFeatureFrameSequence(sequence, snapshot.accessCategory);
    soundingService.getCoordinator().recordAttempt(snapshot.peer);
    rollback.release();
}
```

Do not move `recordAttempt()` before `startFeatureFrameSequence()`.

### 4.3 Route the prepared snapshot once

Replace nested `HcfVhtRuntime::startFrameSequence()` with one prepare and one switch:

```cpp
void startFrameSequence(AccessCategory ac)
{
    if (!hcf->hasFrameToTransmit(ac)) {
        hcf->releaseChannel(ac);
        return;
    }

    auto snapshot = feature.prepareGrantSnapshot(ac);
    switch (snapshot.startKind) {
        case VhtGrantSnapshot::StartKind::COMMON_SINGLE_USER:
            hcf->startSingleUserExchange(ac);
            return;

        case VhtGrantSnapshot::StartKind::SU_SOUNDING:
        case VhtGrantSnapshot::StartKind::MU_SOUNDING:
            feature.startSounding(snapshot);
            return;

        case VhtGrantSnapshot::StartKind::GROUP_MANAGEMENT:
        case VhtGrantSnapshot::StartKind::BLOCK_ACK_PREREQUISITE:
        case VhtGrantSnapshot::StartKind::DL_MULTIUSER: {
            if (!snapshot.exchangeClass.has_value())
                throw cRuntimeError("Transactional VHT grant has no exchange class");
            HcfContext context(ac, {*snapshot.exchangeClass});
            context.setProviderSnapshot(snapshot);
            hcf->runtime->getExchangeSelector().selectAndCommit(
                    context,
                    hcf->exchangeEngine->getActiveTransactionIdentity());
            return;
        }
    }
    throw cRuntimeError("Unknown VHT grant start kind");
}
```

Update all VHT provider/committer checks to reject a missing class before
dereferencing it:

```cpp
if (!snapshot->exchangeClass.has_value() ||
        *snapshot->exchangeClass != exchangeClass)
    throw cRuntimeError("Selected VHT transaction does not match its snapshot");
```

### 4.4 Narrow the VHT transactional committer

`HcfVhtRuntime::commitSelectedExchange()` must accept only:

```cpp
HcfExchangeClass::VHT_GROUP_MANAGEMENT
HcfExchangeClass::VHT_DL_MULTIUSER
```

Rename `VhtHcfFeature::commitGrantSnapshot()` to
`commitTransactionalGrant()` and remove all direct cases from it. The final
implementation is:

```cpp
void VhtHcfFeature::commitTransactionalGrant(
        const VhtGrantSnapshot& snapshot)
{
    switch (snapshot.startKind) {
        case VhtGrantSnapshot::StartKind::GROUP_MANAGEMENT: {
            auto txop = actions->getEdca()->getEdcaf(
                    snapshot.accessCategory)->getTxopProcedure();
            if (!txop->isProtectionConfigured())
                txop->configureProtection(
                        TxopProcedure::InitialProtection::NONE);
            actions->startFeatureFrameSequence(
                    new VhtGroupIdManagementFs(
                            actions->getMac()->getMib(), groupIdManager,
                            snapshot.peer, snapshot.groupId,
                            snapshot.userPosition,
                            snapshot.associationGeneration,
                            snapshot.channelWidth),
                    snapshot.accessCategory);
            return;
        }
        case VhtGrantSnapshot::StartKind::BLOCK_ACK_PREREQUISITE:
            commitBlockAckPrerequisite(snapshot);
            return;
        case VhtGrantSnapshot::StartKind::DL_MULTIUSER:
            commitDlMu(snapshot);
            return;
        default:
            throw cRuntimeError("Direct VHT grant entered transactional commit");
    }
}
```

Do not split more helpers in this phase.

Delete `VhtHcfFeature::startFrameSequence(AccessCategory)` after the runtime
switch is in place. Delete the old `commitGrantSnapshot()` declaration and
definition after all transactional callers use `commitTransactionalGrant()`
and direct sounding callers use `startSounding()`.

### Tests

Add a VHT `IActions` test double whose `startFeatureFrameSequence()` throws once:

```cpp
auto before = feature.getSoundingServiceForTesting().getNextDialogToken();
auto snapshot = makeSuSoundingSnapshot(before);  // repeat for MU_SOUNDING

actions.throwOnStart = true;
REQUIRE_THROWS(feature.startSounding(snapshot));
REQUIRE(feature.getSoundingServiceForTesting().getNextDialogToken() == before);
REQUIRE(feature.getSoundingServiceForTesting()
        .getCoordinator().mayAttempt(peer));
REQUIRE(!selector.hasActiveExchange());

actions.throwOnStart = false;
feature.startSounding(snapshot);
REQUIRE(feature.getSoundingServiceForTesting().getNextDialogToken() == before + 1);
REQUIRE(!feature.getSoundingServiceForTesting()
        .getCoordinator().mayAttempt(peer));
REQUIRE(!selector.hasActiveExchange());
```

For MU sounding additionally assert:

```cpp
REQUIRE(snapshot.startKind == VhtGrantSnapshot::StartKind::MU_SOUNDING);
REQUIRE(!snapshot.dlMuPlan.has_value());
REQUIRE(providerPrepareCalls == 0);
```

Then, after simulated feedback updates CSI, prepare a second grant and prove a fresh `DL_MULTIUSER` plan is built. Do not reuse a transaction identity or cached plan from sounding.

The existing `Ieee80211SharedMacModes_1.test` remains the integration proof for NDPA -> NDP -> feedback, CSI use, beamformed SU data, group actions, MU PPDU, and delivery.

### Exit criterion

- Both VHT sounding kinds bypass the selector.
- Failed startup restores the dialog token and does not start cooldown.
- `MU_SOUNDING` is no longer represented as `VHT_DL_MULTIUSER`.
- Actual VHT group/ADDBA/DL-MU paths remain transactional.

---

## Phase 5 — Narrow the transaction vocabulary and contracts

### Objective

Delete direct-only exchange classes and generic action-provider ceremony after all direct paths compile and pass.

### 5.1 Enum strategy

Remove:

```cpp
HcfExchangeClass::HT_SOUNDING
HcfExchangeClass::VHT_SU_SOUNDING
```

Retain:

```cpp
HcfExchangeClass::SINGLE_USER       // HE prepared fallback only
HcfExchangeClass::CHANNEL_RELEASE   // semantic HE grant outcome only
```

`CHANNEL_RELEASE` must have no provider descriptor and must never form a plan. `SINGLE_USER` may form a plan only for HE snapshots with a non-empty `dlStart`.

The resulting order is nine values:

```cpp
constexpr size_t NUM_HCF_EXCHANGE_CLASSES = 9;

const std::array<HcfExchangeClass, 9>& getHcfExchangeClassOrder()
{
    static const std::array<HcfExchangeClass, 9> order = {
        HcfExchangeClass::FORCED_SINGLE_USER,
        HcfExchangeClass::HE_UL_TRIGGER,
        HcfExchangeClass::HE_SOUNDING,
        HcfExchangeClass::RECOVERY_SINGLE_USER,
        HcfExchangeClass::HE_DL_MULTIUSER,
        HcfExchangeClass::VHT_GROUP_MANAGEMENT,
        HcfExchangeClass::VHT_DL_MULTIUSER,
        HcfExchangeClass::SINGLE_USER,
        HcfExchangeClass::CHANNEL_RELEASE,
    };
    return order;
}
```

### 5.2 Make plan eligibility explicit

Do not rely on every semantic enum being transactional. Add one small predicate next to the order:

```cpp
bool isTransactionalExchangeClass(HcfExchangeClass exchangeClass)
{
    switch (exchangeClass) {
        case HcfExchangeClass::FORCED_SINGLE_USER:
        case HcfExchangeClass::HE_UL_TRIGGER:
        case HcfExchangeClass::HE_SOUNDING:
        case HcfExchangeClass::RECOVERY_SINGLE_USER:
        case HcfExchangeClass::HE_DL_MULTIUSER:
        case HcfExchangeClass::VHT_GROUP_MANAGEMENT:
        case HcfExchangeClass::VHT_DL_MULTIUSER:
        case HcfExchangeClass::SINGLE_USER: // validated as HE dlStart later
            return true;
        case HcfExchangeClass::CHANNEL_RELEASE:
            return false;
    }
    return false;
}
```

Use it in `HcfExchangePlan::isComplete()`:

```cpp
if (!transactionIdentity.isValid() ||
        !isTransactionalExchangeClass(exchangeClass))
    return false;
```

This ensures `CHANNEL_RELEASE` cannot accidentally regain a transaction wrapper.

### 5.3 Delete obsolete common transaction wiring

Delete from `Hcf`:

- `buildGrantSelectionContext()`;
- common branches of `commitSelectedExchange()` for release/HT/SU.

Retain a narrowly named transactional committer used by provider wrappers:

```cpp
void Hcf::commitTransactionalExchange(
        HcfExchangeClass exchangeClass, const HcfContext& context)
{
    if (heRuntime != nullptr) {
        heRuntime->commitSelectedExchange(exchangeClass, context);
        return;
    }
    if (vhtRuntime != nullptr &&
            vhtRuntime->commitSelectedExchange(exchangeClass, context))
        return;
    throw cRuntimeError("No runtime can commit HCF transaction class %d",
            static_cast<int>(exchangeClass));
}
```

Configure the existing feature-set committer with this renamed method. Do not add a direct-exchange committer.

Retain `HcfActionExchangeTransaction` and `HcfActionExchangeProvider` for the
true transactional action classes in this plan. Do not redesign the retained
VHT/HE providers here; that would be a separate change with a separate safety
analysis.

### 5.4 Custom feature-set contract

Custom feature sets may still return true transactional providers. They may no longer replace ordinary SU, release, HT sounding, or VHT sounding through descriptors. This is an intentional contract narrowing, not a compatibility bug.

Do not add a compatibility flag. Update `HcfFeatureSetReplacement_1.test` to replace a real transaction such as `HE_DL_MULTIUSER` or `HE_UL_TRIGGER`, or remove the test if it only proves the obsolete direct extension point.

### Tests

Rewrite `HcfFeatureSet_1.test` expectations:

- Common descriptor vector is empty.
- VHT vector contains only `VHT_GROUP_MANAGEMENT` and `VHT_DL_MULTIUSER` when enabled.
- HE vector contains HE transaction classes plus `SINGLE_USER` for prepared HE fallback.
- No descriptor exists for `CHANNEL_RELEASE`.

Rewrite `HcfExchangePlan_1.test`:

- expected order has nine values;
- all lifecycle tests use `HE_DL_MULTIUSER` or `VHT_DL_MULTIUSER`;
- constructing a `CHANNEL_RELEASE` plan fails completeness;
- stale token/generation, rollback, completion, and exception cases remain unchanged.

Rewrite `HcfExchangeSelector_1.test` to use genuine transaction classes only.

### Static acceptance search

```sh
rg -n 'makeActionDescriptor\(HcfExchangeClass::(CHANNEL_RELEASE|HT_SOUNDING|VHT_SU_SOUNDING)' \
  src/inet/linklayer/ieee80211/mac/coordinationfunction

rg -n 'HcfExchangeClass::(HT_SOUNDING|VHT_SU_SOUNDING)' \
  src/inet/linklayer/ieee80211 tests
```

Both searches must return no matches. A `SINGLE_USER` descriptor is acceptable only in `HeHcfFeatureSet`, and the provider must require `dlStart`.

### Exit criterion

The transaction vocabulary describes actual retained transactions or necessary HE semantic outcomes, not ordinary direct operations.

---

## Phase 6 — Collapse SU transmission preparation to one straight-line call

### Objective

Remove the misleading `prepare -> PreparedTransmission -> commit/discard` mini-transaction used by every ordinary transmit. Keep only local exception-safe ownership of a materialized aggregate.

### Files

- `HcfTransmissionPreparationService.h/.cc`
- `Hcf.cc`
- `tests/unit/HcfTransmissionPreparationService_1.test`

### Delete

```cpp
enum class TerminalState;
struct PreparedTransmission;
PreparedTransmission prepare(...);
void commit(...);
void discard(...);
```

No production caller uses `discard()`, and `Hcf::transmitFrame()` currently calls `prepare()` immediately followed by `commit()`.

### Final API

```cpp
void prepareAndTransmit(const Request& request, IActions& actions) const;
```

Keep `deleteTemporaryPacket()` as the ownership callback and test seam. Use a
raw temporary pointer plus one tightly scoped `try/catch`, as shown below. Do
not introduce `unique_ptr`, a custom deleter, or another guard type in this
phase; the borrowed handoff and OMNeT++ ownership boundary must stay explicit.

### Conceptual implementation

```cpp
void HcfTransmissionPreparationService::prepareAndTransmit(
        const Request& request, IActions& actions) const
{
    validateRequest(request);

    const bool implicitBlockAck = !request.container &&
            actions.isHtImplicitBlockAckEligible(request);
    actions.applySourceRetryState(request.packet);

    Request staged = request;
    if (!request.container)
        staged.header = request.packet->peekAtFront<Ieee80211MacHeader>();

    auto ackPolicy = actions.selectAckPolicy(staged, implicitBlockAck);
    auto mode = actions.selectMode(staged);
    if (mode == nullptr)
        throw cRuntimeError("HCF rate selection returned no PHY mode");
    actions.validateMode(staged, mode);

    auto aggregate = actions.planAggregation(
            staged, mode, ackPolicy, implicitBlockAck);
    if (aggregate.implicitBlockAck != implicitBlockAck)
        throw cRuntimeError("HCF aggregation changed implicit BlockAck decision");
    validateAggregatePlan(staged, aggregate);
    actions.validateAggregation(staged, mode, aggregate);
    actions.validateProtection(staged, mode, aggregate);

    auto header = actions.applyAckPolicy(
            request.packet, staged.header, ackPolicy, aggregate);
    if (header == nullptr)
        throw cRuntimeError("HCF Ack Policy returned no MAC header");
    header = actions.applyModePreparation(request.packet, header, mode);
    if (header == nullptr)
        throw cRuntimeError("HCF mode preparation returned no MAC header");
    actions.applyAggregateMemberState(request.packet, aggregate);

    Packet *transmitted = request.packet;
    bool temporary = false;
    if (aggregate.materialize) {
        transmitted = actions.materializeAggregate(request.packet, aggregate);
        if (transmitted == nullptr || transmitted == request.packet)
            throw cRuntimeError("HCF aggregate materialization returned no temporary packet");
        temporary = true;
    }

    try {
        if (!request.container) {
            actions.setMode(transmitted, header, mode);
            if (aggregate.implicitBlockAck)
                actions.setSourceMode(request.packet, header, mode);

            auto protection = actions.computeProtection(staged, mode, aggregate);
            validateProtectionPlan(request, protection);
            actions.recordSelectedMode(transmitted, mode);
            actions.observeSelectedRate(transmitted, mode);
            if (protection.updateDuration)
                actions.applyDuration(request.packet, header,
                        protection.duration);
        }

        actions.transmitBorrowed(transmitted, header, request.ifs);
    }
    catch (...) {
        if (temporary)
            actions.deleteTemporaryPacket(transmitted);
        throw;
    }

    if (temporary)
        actions.deleteTemporaryPacket(transmitted);
}
```

Preserve the current order exactly unless a test proves it wrong. In particular:

- retry state is applied before retry-sensitive policy/rate selection;
- aggregate validation happens before temporary materialization;
- selected mode is attached before protection duration calculation;
- implicit-BA source mode is propagated;
- `transmitBorrowed()` happens once;
- temporary cleanup happens once on both success and failure.

### Hcf call site

Replace:

```cpp
auto prepared = transmissionPreparationService.prepare(request, actions);
transmissionPreparationService.commit(prepared, actions);
```

with:

```cpp
transmissionPreparationService.prepareAndTransmit(request, actions);
```

### Test migration

Remove tests for artificial state transitions (`READY`, `COMMITTED`, `DISCARDED`, duplicate commit). Preserve behavior tests:

```cpp
service.prepareAndTransmit(request, actions);
REQUIRE(actions.handoffCount == 1);
REQUIRE(actions.lastSource == source);
REQUIRE(actions.temporaryDeleteCount == expectedTemporaryCount);
```

Inject failures at:

- mode validation, before materialization;
- `setMode`, after temporary materialization;
- protection computation;
- borrowed handoff.

For every post-materialization throw:

```cpp
REQUIRE(actions.temporaryDeleteCount == 1);
REQUIRE(inProgress->getFrameToTransmit() == source);
REQUIRE(actions.handoffCount <= 1);
REQUIRE(actions.transactionCallbacks == 0);
```

Do not assert generic rollback of retry/candidate owner mutations; the current service explicitly documents that those mutations are not transactional.

### Exit criterion

There is no `PreparedTransmission`, `commit()`, or `discard()` API, and one-queue aggregation retains exact ownership and wire behavior.

---

## Phase 7 — Consolidate tests around behavior, not removed machinery

### Objective

Make the test suite describe the final boundary and remove false requirements that direct exchanges be transaction providers.

### Test-by-test changes

#### `HcfExchangePlan_1.test`

- Keep immutable plan, queue-token, association-epoch, validation, commit, rollback, terminal completion, and exception tests.
- Use `HE_DL_MULTIUSER` and `VHT_DL_MULTIUSER` fixtures.
- Assert `CHANNEL_RELEASE` is not plan-complete.
- Remove HT/VHT sounding and ordinary SU plan fixtures.

#### `HcfExchangeSelector_1.test`

- Keep deterministic priority, one selected provider, one commit, one terminal callback, stale identity rejection, no-provider failure, and teardown abort.
- Use only real HE/VHT transaction classes.

#### `HcfFeatureSet_1.test`

- Common descriptors: empty.
- VHT descriptors: group management and DL-MU only when configured.
- HE descriptors: retained HE classes plus prepared-fallback `SINGLE_USER`.
- No ordinary SU/release/HT/VHT-sounding provider commits.

#### `HcfFeatureSetReplacement_1.test`

- Rewrite the replacement fixture to provide `HE_DL_MULTIUSER` instead of
  ordinary SU/release.
- Preserve the NED/composition contract test for supported transaction
  providers.

#### `HcfExchangeEngine_1.test`

- Direct completion: clears timers, finishes once, resumes once, zero terminal transaction calls.
- Transaction completion: exact identity and one terminal call.

#### `Ieee80211HtSoundingExchange_1.test`

- Test actual post-grant direct routing, not only `startFrameSequence()`.
- Assert selector idle and source packet identity preserved.

#### New/extended VHT sounding unit test

- SU and MU direct routing.
- Token rollback on injected sequence-start failure.
- No cooldown on failed start.
- No DL-MU plan on MU sounding.
- Fresh plan recomputation after CSI.

#### `HcfTransmissionPreparationService_1.test`

- One-call straight-line API.
- Exact event order.
- One borrowed handoff.
- Exact temporary cleanup on success/failure.
- Source identity retained.

### Preservation suite

These tests must remain behaviorally unchanged apart from enum/fixture compilation updates:

- `Ieee80211VhtDlMuScheduler_1.test`
- `Ieee80211VhtAddbaQueueing_1.test`
- `Ieee80211VhtDlMuNegative_1.test`
- `Ieee80211HeMuAddbaValidation_1.test`
- `Ieee80211HeDlMuTransaction_1.test`
- `Ieee80211HeUlMuTransaction_1.test`
- `Ieee80211HeDlMuExchange_1.test`
- `Ieee80211HeUlTriggerExchange_1.test`
- `Ieee80211SharedMacModes_1.test`
- retransmission tests 1, 2, 5, 6, and 8; then 3, 7, 9, and 10 as stress expansion.

### Full focused commands

```sh
make MODE=release -j$(nproc)

inet_run_unit_tests -m release \
  -f '(Ieee80211HtSoundingExchange|HtSoundingRetryState|HcfFeatureSet|HcfFeatureSetReplacement|HcfExchangePlan|HcfExchangeSelector|HcfExchangeEngine|HcfExchangeCoordinator|HcfAggregationPlanning|HcfTransmissionPreparationService|Ieee80211VhtSoundingCodec|Ieee80211VhtDlMuScheduler|Ieee80211VhtAddbaQueueing|Ieee80211HeMuAddbaValidation|Ieee80211HeDlMuTransaction|Ieee80211HeUlMuTransaction).*\.test'

inet_run_module_tests -m release --no-concurrent \
  -f '(Ieee80211SharedMacModes|Ieee80211VhtDlMuNegative|Ieee80211HeDlMuExchange|Ieee80211HeUlTriggerExchange|Ieee80211Retransmission(1|2|3|5|6|7|8|9|10)).*\.test'
```

For injected rollback assertions, repeat the narrow unit subset in debug mode after a matching debug build:

```sh
make MODE=debug -j$(nproc)
inet_run_unit_tests -m debug \
  -f '(HcfExchangePlan|HcfExchangeSelector|HcfExchangeEngine|HcfTransmissionPreparationService|Ieee80211HtSoundingExchange|Ieee80211Vht.*|Ieee80211He.*Transaction).*\.test'
```

### Determinism

Run one configuration/run first. Expand `Ieee80211SharedMacModes_1.test` to all configured repetitions only after the narrow case passes; preserve its existing `seed-set=${repetition}` behavior. Report exact seed/run and first divergence.

### Exit criterion

Tests prove observable frame, state, identity, and ownership behavior. No test exists merely to preserve deleted transaction ceremony.

---

## Phase 8 — Documentation, contract migration, and cleanup

### Objective

Make the new boundary discoverable and remove stale terminology/includes without expanding the design.

### Files

- `doc/src/developers-guide/ch-80211.rst`
- C++ class comments in the files changed above
- test descriptions

### Required documentation text

Document these facts plainly:

```text
Baseline SU and HT/VHT sounding use direct explicit frame sequences.
VHT and HE operations that reserve or coordinate multiple owners use prepared
HCF transactions. VHT sounding protects its dialog token with a local rollback
guard; it is not an HCF transaction. HE per-STA SU fallback remains
transactional when it carries a prepared provider start.
```

Document the extension-contract change:

```text
IHcfFeatureSet providers no longer replace ordinary SU, channel release, HT
sounding, or VHT sounding. They remain extension points for genuine prepared
VHT/HE exchanges. No compatibility mode is provided.
```

### Cleanup searches

```sh
rg -n 'prepareHtSoundingModeIdentity|startHtSoundingExchange|htSoundingModeIdentity|VHT_SU_SOUNDING|HT_SOUNDING|PreparedTransmission|\.discard\(' \
  src/inet/linklayer/ieee80211 tests doc

rg -n 'selectAndCommit\(' \
  src/inet/linklayer/ieee80211/mac/coordinationfunction

rg -n 'makeActionDescriptor\(HcfExchangeClass::(CHANNEL_RELEASE|HT_SOUNDING|VHT_SU_SOUNDING)' \
  src/inet/linklayer/ieee80211/mac/coordinationfunction
```

Expected:

- first and third searches: no matches except historical release notes if deliberately quoted;
- `selectAndCommit()` appears only in explicit VHT/HE transactional dispatch and selector tests;
- no new NED/INI compatibility parameter exists.

Run `git diff --check`, remove unused includes, and inspect warnings after both release and debug builds.

### Exit criterion

Documentation and code use the same direct-versus-transactional vocabulary, and the source has no stale direct transaction plumbing.

---

## Phase 9 — Architecture and regression acceptance

### Architecture checks

```sh
bash .agents/skills/inet-architectural-requirements/references/enforcement/check-architecture.sh \
  src/inet/linklayer/ieee80211/mac/coordinationfunction

bash .agents/skills/inet-architectural-requirements/references/enforcement/check-architecture.sh \
  src/inet/linklayer/ieee80211/mac
```

Reconcile output with the existing exception ledger; do not edit the ledger merely to make this patch pass. Report pre-existing violations separately from new violations.

The review must explicitly assess:

- AR-ORG-CONTRACTS
- AR-MOD-COMPOSITION
- AR-COM-DIRECT
- AR-QUAL-TESTS
- AR-QUAL-FINGERPRINT
- AR-QUAL-DETERMINISM
- AR-QUAL-TRACEABILITY
- AR-WLAN-ARCH-BOUNDARIES
- AR-WLAN-ARCH-OWNERSHIP
- AR-WLAN-ARCH-VARIANTS
- AR-WLAN-MAC-EXCHANGE
- AR-WLAN-MAC-SEQUENCE
- AR-WLAN-MAC-QOS
- AR-WLAN-MAC-MULTIUSER
- AR-WLAN-PHY-AUTHORITY
- AR-WLAN-PHY-TIMING
- AR-WLAN-OBS-EVENTS
- AR-WLAN-QUAL-TESTS

### Fingerprint gate

Run only after focused tests and code review. Select the smallest relevant existing rows from `tests/fingerprint/ieee80211-he.csv`, preserve exact binary/mode/seed, and compare before/after. Never update the CSV without explicit user approval.

Any mismatch is FAIL until the first changed event is explained with logs, event data, or packet evidence. “The refactor was expected to change events” is not sufficient.

### Size/readability gate

```sh
git diff --stat
git diff --numstat -- \
  src/inet/linklayer/ieee80211/mac/coordinationfunction
```

Acceptance requires a net reduction in production LOC across the affected coordination-function code. Test LOC may grow where it adds direct-path behavioral evidence.

### Final acceptance checklist

- [ ] Ordinary SU DATA/ACK bypasses `HcfExchangeSelector`.
- [ ] Ordinary RTS/CTS/DATA/ACK bypasses `HcfExchangeSelector`.
- [ ] Empty-grant channel release bypasses `HcfExchangeSelector`.
- [ ] HT sounding bypasses the selector and preserves source packet identity.
- [ ] VHT SU sounding bypasses the selector.
- [ ] VHT MU sounding bypasses the selector and carries no DL-MU plan.
- [ ] VHT sounding startup failure restores dialog token and does not record cooldown.
- [ ] A later VHT grant recomputes a fresh DL-MU plan after CSI.
- [ ] HE ordinary SU without `dlStart` is direct.
- [ ] HE SU fallback with `dlStart` remains transactional.
- [ ] VHT ADDBA, VHT DL-MU, HE UL, HE sounding/recovery, and HE DL-MU rollback tests pass.
- [ ] Direct completion clears timers and resumes contention exactly once without a selector terminal callback.
- [ ] Transactional completion delivers the exact identity exactly once.
- [ ] One-queue A-MPDU uses one straight-line handoff and deletes temporary storage exactly once.
- [ ] No queue/retry/sequence/ACK/BA owner moved.
- [ ] No new NED/INI parameter, compatibility path, or executor hierarchy exists.
- [ ] Release and debug focused tests pass with matching builds.
- [ ] No fingerprint CSV was changed.
- [ ] Architecture checks introduce no unexplained new violation.
- [ ] Production LOC decreases.

## 6. Explicit stop conditions

Stop execution and revise this plan before continuing if any of the following occurs:

1. A direct path requires a second token/generation mechanism to keep timers safe.
2. HT or VHT sounding is found to reserve queue, BA, sequence, or multi-user plan state not represented above.
3. VHT MU sounding preparation creates or mutates a `VhtDlMuPlan` rather than merely discovering missing CSI.
4. HE `SINGLE_USER + dlStart` cannot be isolated from ordinary direct SU without a new broad abstraction.
5. `prepareAndTransmit()` requires asynchronous lifetime beyond the current borrowed `ITx::transmitFrame()` call.
6. Removing direct descriptors breaks a documented public replacement contract beyond the acknowledged feature-set narrowing.
7. A focused transaction preservation test loses packet identity, queue order, retry state, BA state, or exact terminal correlation.
8. The implementation grows production LOC or adds a parallel routing framework.

## 7. Recommended commit sequence

1. `hcf: gate transaction terminal notification by selector ownership`
2. `hcf: route common SU and channel release directly`
3. `hcf: start HT sounding without provider transactions`
4. `hcf: start VHT SU and MU sounding directly`
5. `hcf: narrow transactional exchange classes and feature providers`
6. `hcf: collapse SU transmission preparation into one handoff`
7. `tests: assert direct HCF paths and preserve MU rollback`
8. `docs: describe the selective HCF transaction boundary`

Do not squash these until the full regression gate passes; the sequence is designed to make regressions bisectable.
