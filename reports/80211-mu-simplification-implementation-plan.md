# IEEE 802.11 MU Architectural Simplification — Implementation Plan

## Outcome

Refactor HE DL MU, VHT DL MU, AP-triggered HE UL, and station HE-TB response handling into the same explicit lifecycle:

1. **Prepare** a complete, move-only value before changing protocol ownership.
2. **Commit** already-validated values in one run-to-completion call stack.
3. **Execute** the committed exchange across transmission, response, and timeout events.

This is primarily an extraction and ownership cleanup. The current HE and VHT DL implementations already build prepared MPDU copies, the container, and the PHY layout before queue mutation. Station-side HE-TB already has a move-only prepared response. Do not replace those correct boundaries with a new generic transaction framework.

The plan deliberately keeps:

- immutable grant/snapshot values;
- one prepared `Packet::dup()` image per selected queued MPDU;
- cloned sequence-number state adopted at commit;
- active per-user response and timeout state;
- HE Trigger IDs and correlation tags;
- HCF exchange/timer generations;
- EHT constructors and HE-based execution behavior.

It removes or consolidates:

- planning inside the first frame-sequence step;
- HE reservation maps and synchronous planning-failure callbacks;
- `PREPARED`/`COMMITTING`/`TERMINAL` planning phases;
- speculative protection snapshots;
- duplicate provider/feature versus frame-sequence active state;
- repeated queue membership scans after a closed same-stack handoff;
- optionally, HE/VHT DL numeric transaction IDs, but only in the final patch
  after structural ownership is tested;
- the station-side `prepareAndCommitResponse(..., publish, ...)` dual-mode implementation and outer packet rollback bookkeeping.

UL is in scope in two distinct places:

- **AP side:** build and commit a Trigger-based UL exchange before starting `HeUlMuTxOpFs`.
- **STA side:** prepare and commit an HE-TB response in `HeTriggeredUlExchangeService`.

The Trigger ID remains in both paths. It is the model's cross-node correlation key carried by `Ieee80211HeTriggerCorrelationTag`, not disposable planning state.

## Scope and constraints

Targeted source files under `src/inet/linklayer/ieee80211/` are currently unsealed. The sealed `src/inet/common/packet/` subtree is read-only and requires no changes. No relevant architecture or naming exception applies.

The controlling requirements are `AR-WLAN-ARCH-OWNERSHIP`, `AR-WLAN-MAC-EXCHANGE`, `AR-WLAN-MAC-SEQUENCE`, `AR-WLAN-MAC-QOS`, `AR-WLAN-MAC-MULTIUSER`, `AR-WLAN-PHY-AUTHORITY`, `AR-WLAN-PHY-TIMING`, `AR-WLAN-OBS-EVENTS`, `AR-WLAN-QUAL-TESTS`, `AR-PKT-CHUNKS`, `AR-PKT-TAGS`, `AR-COM-DIRECT`, `AR-QUAL-DETERMINISM`, and `AR-QUAL-LOGGING`.

Out of scope:

- changing IEEE-visible frame formats, timing, Retry/sequence semantics, or Block Ack behavior;
- changing schedulers or rate-control policy;
- changing `Packet`, `Chunk`, or tag APIs;
- proving a performance improvement without profiling;
- sharing one templated prepared-exchange base among HE, VHT, and UL;
- replacing Trigger-ID correlation;
- updating fingerprints.

## Commit rule

“Commit” means **no expected model-level rejection remains**, not that every called C++ method is literally `noexcept`.

Before the first mutation, preparation must have completed:

- exact packet selection and identity validation;
- capability, BA-window, retry, aggregation, RU/NSS/MCS, and TXOP checks;
- final per-user allocation projection;
- sequence-number assignment on cloned state;
- final header, trailer, FCS, packet-tag, and region-tag preparation;
- container/Trigger construction;
- canonical TXVECTOR/PPDU layout construction;
- all test fault hooks that represent recoverable failures;
- all allocations needed to publish active state.

After the boundary, a stale handle or impossible state is a programming error and terminates the simulation. No SU fallback or other policy choice may occur after the first mutation.

## Core DL data types

Add two family-specific pairs rather than a generic hierarchy:

- `framesequence/HeDlMuPreparedExchange.h/.cc`
- `framesequence/VhtDlMuPreparedExchange.h/.cc`

The following is the intended HE shape. The VHT type has the same ownership rules but stores VHT candidates, user positions, and sequential BAR modes instead of HE RUs and MU-BAR data.

```cpp
enum class HeDlMuAckMethod {
    EXPLICIT_SEQUENTIAL_BAR,
    MU_BAR_TRIGGER,
};

struct HeDlMuExecution
{
    struct User {
        MacAddress peer;
        uint16_t associationId = 0;
        Tid tid = 0;
        IIeee80211HeDlScheduler::RuAllocation allocation;
        std::vector<Packet *> packets;          // committed MPDUs, non-owning
    };

    Packet *containerPacket = nullptr;          // identity only after Tx takes ownership
    std::vector<User> users;                    // immutable after publication
    std::vector<HeDlMuMember> members;           // immutable interceptor/rate-control view
    HeDlMuAckMethod ackMethod;
    uint32_t ackTriggerId = 0;                   // retained for MU-BAR response matching
};

class HeDlMuPreparedExchange
{
  public:
    struct Mpdu {
        HcfQueueToken sourceQueueToken;
        HcfPacketIdentity packetIdentity;
        Packet *original = nullptr;              // stable handle, never owned
        bool alreadyInProgress = false;
        std::unique_ptr<Packet> preparedImage;   // header/FCS/tags finalized
    };

    struct User {
        IIeee80211HeDlScheduler::RuAllocation allocation;
        uint16_t associationId = 0;
        Tid tid = 0;
        std::vector<Mpdu> mpdus;
    };

  private:
    std::vector<User> users;
    std::vector<IIeee80211HeDlScheduler::RuAllocation> finalizedAllocations;
    std::vector<HeDlMuMember> members;
    std::unique_ptr<ISequenceNumberAssignment> resultingSequenceState;
    std::unique_ptr<Packet> containerPacket;
    HeDlMuAckMethod ackMethod;
    uint32_t ackTriggerId = 0;

  public:
    HeDlMuPreparedExchange(const HeDlMuPreparedExchange&) = delete;
    HeDlMuPreparedExchange& operator=(const HeDlMuPreparedExchange&) = delete;
    HeDlMuPreparedExchange(HeDlMuPreparedExchange&&) = default;
    HeDlMuPreparedExchange& operator=(HeDlMuPreparedExchange&&) = default;

    // Read-only accessors for commit; no live module or FrameSequenceContext is retained.
};
```

`HeDlMuExecution` is shared immutably between the active owner and frame sequence:

```cpp
using HeDlMuExecutionPtr = std::shared_ptr<const HeDlMuExecution>;
```

The shared pointer owns only the immutable execution description. The normal Tx path continues to own the transmitted container packet. This prevents dangling references without creating a second mutable exchange owner.

Do the same for `VhtDlMuExecution` and `VhtDlMuPreparedExchange`. Do not add a common base class: the two plans have different PHY fields, response procedures, and retry sources.

Move the existing nested HE acknowledgement enum to this header and keep `using AckMethod = HeDlMuAckMethod` in `HeDlMuTxOpFs` during source compatibility. Likewise, move the useful fields of the current nested `ActiveAllocation` into `HeDlMuExecution::User`; do not keep two representations.

## Phase 1: preparation

### HE DL MU

Move the body of `HeDlMuTxOpFs::buildMuContainerPacket()` into `HeDlMuPreparedExchange::prepare()`. Preserve its present ordering; do not rewrite its algorithms during extraction.

```cpp
std::optional<HeDlMuPreparedExchange> HeDlMuPreparedExchange::prepare(
        const HeDlMuPlan& schedulerPlan,
        FrameSequenceContext& context,
        const HeDlMuPreparationServices& services,
        HeDlMuAckMethod ackMethod,
        const HeDlMuLimits& limits,
        HeMuPlanDiagnostic& diagnostic)
{
    // 1. Resolve exact queues and packet identities from the value-only plan.
    auto selected = selectExactPackets(schedulerPlan, context, services, diagnostic);
    if (!selected)
        return std::nullopt;

    // 2. Run packing, BA-slot, duration, capability, and PHY legality checks.
    auto packed = HeDlMuPackingPlanner::plan(makePackingParameters(*selected));
    if (!packed || packed->allocations.size() < 2)
        return fail(diagnostic, "HE DL MU packing left fewer than two users");

    HeDlMuPreparedExchange result;
    result.ackMethod = ackMethod;
    result.ackTriggerId = allocateIeee80211HeTriggerId();
    result.finalizedAllocations = projectFinalAllocations(
            schedulerPlan, packed->allocations);

    // 3. Assign fresh numbers only on a cloned allocator. Retry MPDUs retain theirs.
    result.resultingSequenceState = services.dataService.cloneSequenceNumberState();
    for (const auto& packedUser : packed->users) {
        User user = makePreparedUser(packedUser);
        for (auto original : packedUser.packets) {
            Mpdu mpdu;
            mpdu.original = original;
            mpdu.packetIdentity = HcfPacketIdentity(original->getId());
            mpdu.sourceQueueToken = packedUser.sourceQueueToken;
            mpdu.preparedImage.reset(original->dup());

            auto header = mpdu.preparedImage->removeAtFront<
                    Ieee80211DataOrMgmtHeader>();
            if (!header->getRetry())
                result.resultingSequenceState->assignSequenceNumber(header);
            setHeDlMuHeaderFields(header, packedUser, ackMethod);
            mpdu.preparedImage->insertAtFront(header);
            finalizeFcsAndTags(*mpdu.preparedImage, *original, services.fcsMode);
            user.mpdus.push_back(std::move(mpdu));
        }
        result.users.push_back(std::move(user));
    }

    // 4. Build the complete container and canonical PHY handoff from prepared images.
    result.containerPacket = buildHeDlMuContainer(result.users, packed->duration);
    auto canonical = buildCanonicalHeDlMuTxVector(
            result.finalizedAllocations, *result.containerPacket, schedulerPlan);
    if (!canonical)
        return fail(diagnostic, "HE DL MU TXVECTOR disagrees with packed layout");
    attachCanonicalTxVector(*result.containerPacket, *canonical);

    // 5. Construct final immutable member/execution data now, before mutation.
    result.members = buildHeDlMuMembers(result.users);
    services.beforePreparedCommit(result); // existing fault injection, still pre-mutation
    return result;
}
```

`HeDlMuPreparationServices` is a short-lived aggregate of references used only during the call: mode set, data service, ACK/BA handlers, rate selection, queue-token resolver, transmitter address, BSS color, and FCS mode. It must not be stored in the returned object.

`HeDlMuExchangeProvider::PreparedStart` becomes move-only and holds the prepared exchange plus the context that will later execute it:

```cpp
struct PreparedStart {
    StartKind kind = StartKind::SINGLE_USER_FALLBACK;
    AccessCategory accessCategory = AC_BE;
    std::optional<HeSoundingService::StartAction> soundingAction;
    std::optional<HeDlMuPreparedExchange> dlMu;
    std::unique_ptr<FrameSequenceContext> frameSequenceContext;
    HcfQueueToken stageQueueToken;
    HcfPacketIdentity stagePacketIdentity;
    IIeee80211HeDlScheduler *scheduler = nullptr; // commit target, not plan state
    IIeee80211HeDlScheduler::ScheduleContext scheduleContext;

    PreparedStart(const PreparedStart&) = delete;
    PreparedStart& operator=(const PreparedStart&) = delete;
    PreparedStart(PreparedStart&&) = default;
};
```

For an HE DL MU candidate, `prepareStart()` creates one `FrameSequenceContext`, passes it to `HeDlMuPreparedExchange::prepare()`, and retains the same context for execution. On preparation failure it selects SU fallback before protection is configured and before any queue/sequence/ACK/scheduler mutation.

### VHT DL MU

Extract `VhtDlMuTxOpFs::buildMuContainerPacket()` into the analogous preparation function:

```cpp
std::optional<VhtDlMuPreparedExchange> VhtDlMuPreparedExchange::prepare(
        const VhtDlMuPlan& plan,
        FrameSequenceContext& context,
        const VhtDlMuPreparationServices& services,
        VhtDlMuPlanDiagnostic& diagnostic)
{
    VhtDlMuPreparedExchange result;
    result.resultingSequenceState = services.dataService.cloneSequenceNumberState();

    for (const auto& candidate : plan.getUsers()) {
        auto packets = selectVhtPackets(candidate, context, services, diagnostic);
        if (!packets)
            return std::nullopt;

        PreparedUser user;
        user.candidate = candidate;
        for (auto selection : *packets) {
            PreparedMpdu mpdu;
            mpdu.original = selection.packet;
            mpdu.alreadyInProgress = selection.inProgress;
            mpdu.sourceQueueToken = candidate.sourceQueueToken;
            mpdu.packetIdentity = HcfPacketIdentity(selection.packet->getId());
            mpdu.preparedImage.reset(selection.packet->dup());

            if (selection.inProgress)
                services.ackHandler.setRetryBitIfNeeded(mpdu.preparedImage.get());
            auto header = mpdu.preparedImage->removeAtFront<Ieee80211DataHeader>();
            if (!selection.inProgress && !header->getRetry())
                result.resultingSequenceState->assignSequenceNumber(header);
            setVhtDlMuHeaderFields(header, plan);
            mpdu.preparedImage->insertAtFront(header);
            finalizeFcsAndTags(*mpdu.preparedImage, *selection.packet,
                    services.fcsMode);
            user.mpdus.push_back(std::move(mpdu));
        }
        result.users.push_back(std::move(user));
    }

    result.containerPacket = buildVhtDlMuContainer(result.users, plan);
    auto txVector = buildCanonicalVhtDlMuTxVector(result.users, plan);
    if (!txVector)
        return fail(diagnostic, "VHT DL MU TXVECTOR disagrees with user layout");
    attachCanonicalTxVector(*result.containerPacket, *txVector);
    services.beforePreparedCommit(result);
    return result;
}
```

`VhtHcfFeature::commitDlMu()` performs this preparation before touching protection or lifecycle state. If preparation returns `nullopt`, it returns `FINISHED_SYNCHRONOUSLY` through the same existing fallback/finish path.

### AP-triggered HE UL

Add `framesequence/HeUlMuPreparedExchange.h/.cc` for the AP-side Trigger exchange:

```cpp
class HeUlMuPreparedExchange
{
  public:
    HeUlMuPlan plan;
    uint32_t triggerId = 0;
    std::unique_ptr<Packet> triggerPacket;
    simtime_t responseTimeout = SIMTIME_ZERO;

    HeUlMuPreparedExchange(const HeUlMuPlan& plan, uint32_t triggerId) :
        plan(plan), triggerId(triggerId) {}
    HeUlMuPreparedExchange(const HeUlMuPreparedExchange&) = delete;
    HeUlMuPreparedExchange(HeUlMuPreparedExchange&&) = default;
};
```

Move `HeUlMuTxOpFs::buildTriggerPacket()` into its factory. Multi-STA BlockAck construction stays in the executing frame sequence because its records are learned from later HE-TB responses.

```cpp
std::optional<HeUlMuPreparedExchange> HeUlMuPreparedExchange::prepare(
        const HeUlMuPlan& plan,
        uint32_t triggerId,
        const MacAddress& apAddress,
        physicallayer::Ieee80211ModeSet& modeSet,
        HeUlMuPlanDiagnostic& diagnostic)
{
    HeUlMuPreparedExchange result{plan, triggerId};
    result.triggerPacket = buildCompleteTriggerPacket(
            plan, triggerId, apAddress, modeSet, diagnostic);
    if (!result.triggerPacket)
        return std::nullopt;
    result.responseTimeout = modeSet.getSifsTime() +
            plan.getSchedule().commonDuration + modeSet.getSlotTime();
    return result;
}
```

`HeUlTriggerService::PreparedStart` owns this object. Allocate the Trigger ID during `prepareStart()`, not in the frame-sequence constructor. Consequently, preparation includes all Trigger encoding, Duration/NAV, trailer, and correlation-tag work.

```cpp
auto triggerId = coordinator->allocateTriggerId();
auto prepared = HeUlMuPreparedExchange::prepare(
        *plan, triggerId, localAddress, *modeSet, diagnostic);
if (!prepared)
    return std::nullopt;
return PreparedStart(accessCategory, triggerType, std::move(*prepared),
        schedulerContext);
```

### Station HE-TB response

Reuse `HeTriggeredUlExchangeService::PreparedTriggeredUlResponse`; do not add another response-plan class. Split the current dual-purpose `prepareAndCommitResponse()` into one preparation method and one commit method.

```cpp
PreparedTriggeredUlResponse HeTriggeredUlExchangeService::prepareResponse(
        const TriggerProcessingSnapshot& snapshot,
        uint32_t triggerId,
        const ResponseSelection& selection,
        const Ieee80211HeTriggerUserInfo& user,
        bool randomAccess)
{
    PreparedTriggeredUlResponse result;
    result.triggerId = triggerId;
    result.exchange = buildExchangeMetadata(snapshot, selection, user, randomAccess);

    // One resulting allocator state; live state is untouched.
    result.preparedSequenceState = prepareSequenceState();
    result.preparedPacketOwners = prepareSelectedMpdus(
            snapshot, selection, user, result.preparedSequenceState,
            result.exchange);

    result.responsePacket = buildResponsePacketFromPreparedMpdus(
            snapshot, user, result.preparedPacketOwners, result.responseHeader);
    result.txReservation = actions->prepareTriggeredUlHandoff(
            result.responsePacket.get(), result.responseHeader);
    if (!result.txReservation)
        throw cRuntimeError("HE-TB Tx preparation returned no reservation");

    actions->validateTriggeredUlPackets(
            result.exchange.sourceQueueToken, result.originalPackets);
    stageLedgerNode(result);       // allocation-free map insertion at commit
    observerBeforeCommit(result); // all recoverable fault hooks run here
    return result;
}
```

Keep BAR, QoS Null, BSRP, NFRP, preassociation management, scheduled RU, and UORA branches as small helpers called by this method. Do not combine them into a variant/template hierarchy.

For random access, prepare every legal RU-specific response before consuming the UORA random draw, exactly as today. The packet mutation is common; only the response container/TXVECTOR and RU metadata differ.

## Phase 2: commit

### HE DL MU

Commit remains a direct same-event call. Preserve current semantic-event ordering unless a focused test proves it irrelevant.

```cpp
HeDlMuExecutionPtr HeDlMuExchangeProvider::commitPreparedDlMu(
        PreparedStart& start)
{
    ASSERT(start.dlMu.has_value());
    auto& prepared = *start.dlMu;

    // Last read-only assertions. No scans after this point.
    validatePacketHandles(prepared);
    ASSERT(activeExchange == nullptr);

    // Allocate and completely populate immutable published state before the boundary.
    auto executionValue = makeExecution(prepared);
    executionValue.containerPacket = prepared.getContainerPacket();
    auto execution = std::make_shared<const HeDlMuExecution>(
            std::move(executionValue));

    // First model mutation. Failures below are invariant failures, not fallback.
    start.scheduler->commitSchedule(
            start.scheduleContext, prepared.getFinalizedAllocations());
    actions->commitHeDlMuSequenceState(prepared.getResultingSequenceState());

    for (auto& user : prepared.getUsers()) {
        for (auto& mpdu : user.mpdus) {
            auto queue = actions->resolveHeDlMuQueue(mpdu.sourceQueueToken);
            queue->removePacket(mpdu.original);
            replacePacketState(*mpdu.original, *mpdu.preparedImage);
            auto header = mpdu.original->peekAtFront<
                    Ieee80211DataOrMgmtHeader>();
            actions->getHeDlMuAckHandler(start.accessCategory)->
                    frameGotInProgress(header);
            start.frameSequenceContext->getInProgressFrames()->
                    addInProgressFrame(mpdu.original);
        }
    }

    activeExchange.emplace(ActiveHeDlMuExchange{execution});
    return execution;
}
```

`getHeDlMuAckHandler()` is a narrow addition to the provider action port; it returns the authoritative per-AC ACK handler and introduces no new owner.

Protection is configured only after successful preparation and immediately before commit/start:

```cpp
bool HeDlMuExchangeProvider::commitStart(PreparedStart&& start)
{
    if (!stillMatchesPendingGrant(start))
        return false;

    if (start.kind != StartKind::HE_DL_MULTIUSER)
        return commitNonMuStart(std::move(start));

    actions->configureHeDlMuProtection(start.accessCategory);
    auto execution = commitPreparedDlMu(start);
    auto containerPacket = start.dlMu->releaseContainerPacket();
    actions->startHeDlMuExchange(execution,
            std::move(containerPacket),
            std::move(start.frameSequenceContext));
    return true;
}
```

No protection snapshot is needed because a recoverable preparation failure occurs before `configureHeDlMuProtection()`.

### VHT DL MU

Use the same boundary, without inventing a scheduler commit that VHT does not currently have:

```cpp
VhtDlMuExecutionPtr VhtHcfFeature::commitPreparedDlMu(
        VhtDlMuPreparedExchange& prepared,
        FrameSequenceContext& context)
{
    auto execution = makeImmutableExecution(prepared); // allocate first
    validatePacketHandles(prepared);

    for (auto& user : prepared.getUsers())
        for (auto& mpdu : user.mpdus)
            if (!mpdu.alreadyInProgress)
                actions->getEdca()->getEdcaf(user.accessCategory)->getAckHandler()->
                        frameGotInProgress(mpdu.preparedImage->peekAtFront<
                                Ieee80211DataOrMgmtHeader>());

    actions->getOriginatorDataService()->commitSequenceNumberState(
            prepared.getResultingSequenceState());

    for (auto& user : prepared.getUsers())
        for (auto& mpdu : user.mpdus) {
            if (!mpdu.alreadyInProgress)
                resolveQueue(mpdu.sourceQueueToken)->removePacket(mpdu.original);
            replacePacketState(*mpdu.original, *mpdu.preparedImage);
            if (!mpdu.alreadyInProgress)
                context.getInProgressFrames()->addInProgressFrame(mpdu.original);
        }

    activeDlMu.emplace(ActiveVhtDlMuExchange{execution});
    return execution;
}
```

`commitDlMu()` first prepares, then configures protection and commits. It next
moves `prepared.releaseContainerPacket()` into the frame-sequence constructor
and starts the sequence. This leaves the container owned throughout the
handoff; the immutable execution value stores only its identity pointer. Delete
`PREPARED`, `COMMITTING`, `pendingFailure`, protection rollback, and the
planning-failure callback only after this path is live.

### AP-triggered HE UL

Allocate the sequence object and frame-sequence context before the first mutation. Commit the scheduler exactly once, then publish the Trigger decision and start execution.

```cpp
bool HeUlTriggerService::commitStart(PreparedStart&& start)
{
    if (!hasPendingTrigger() || pendingTrigger != start.triggerType ||
            !coordinator->isEnabled())
        return false;

    auto committedSchedule = start.exchange.plan.getSchedule();
    auto triggerId = start.exchange.triggerId;
    auto context = actions->buildHeUlFrameSequenceContext(start.accessCategory);
    auto sequence = std::make_unique<HeUlMuTxOpFs>(
            std::move(start.exchange), exchangeCallback,
            actions->getHeUlModeSet(), actions->getHeUlLocalAddress());

    // Mutation boundary.
    actions->configureHeUlMuProtection(start.accessCategory);
    if (start.triggerType == IIeee80211HeUlTriggerPolicy::BASIC_TRIGGER)
        coordinator->commitSchedule(*start.scheduleContext, committedSchedule);
    coordinator->noteTriggerSent(start.triggerType, triggerId);

    accessRequested = false;
    pendingTrigger = IIeee80211HeUlTriggerPolicy::NO_TRIGGER;
    actions->startHeUlMuExchange(std::move(sequence), std::move(context));
    return true;
}
```

Remove `allocateHeUlTriggerId()` and `heUlMuPlanCommitted()` from `IHeUlMuExchangeCallback`; they become preparation/commit responsibilities of `HeUlTriggerService`. Keep the response-processing methods on that callback.

### Station HE-TB response

Scheduled-response commit becomes straightforward:

```cpp
void HeTriggeredUlExchangeService::commitPreparedResponse(
        PreparedTriggeredUlResponse&& prepared)
{
    if (exchanges.count(prepared.triggerId) != 0)
        throw cRuntimeError("Duplicate HE-TB Trigger ID before commit");

    ASSERT(!prepared.stagedExchange.empty());
    auto& exchange = prepared.stagedExchange.mapped();

    // All map nodes, Tx reservation, response packet, sequence state, and packet
    // images already exist. This is the mutation boundary.
    auto committedPackets = actions->commitTriggeredUlPackets(
            exchange.sourceQueueToken,
            prepared.originalPackets,
            rawPointers(prepared.preparedPacketOwners));
    exchange.packets = committedPackets;

    // Preserve the current publication ordering. Tx handoff commit is noexcept.
    actions->commitTriggeredUlHandoff(std::move(prepared.txReservation));

    if (prepared.hasBlockAckRequest)
        actions->commitTriggeredUlBlockAckRequest(
                prepared.preparedBlockAckReq,
                prepared.blockAckReqAccessCategory);
    else
        actions->commitTriggeredUlSequenceState(
                *prepared.preparedSequenceState.state);

    auto inserted = exchanges.insert(std::move(prepared.stagedExchange));
    if (!inserted.inserted)
        std::terminate(); // duplicate was rejected before the mutation boundary
    scheduleNextTimeout();
    actions->emitTriggeredUlResponse(prepared.event);
}
```

For UORA, preserve the single random draw but remove `transferPrecommit()`:

```cpp
void HeTriggeredUlExchangeService::commitRandomAccessResponse(
        std::vector<PreparedTriggeredUlResponse>&& candidates,
        const RandomAccessPreparation& uora)
{
    if (!uora.attempt) {
        actions->commitTriggeredUlRandomAccess(uora); // OBO decrement only
        return;
    }

    // Candidate packet updates and resulting sequence state are identical;
    // every RU-specific container and Tx reservation is already prepared.
    validateCommonPacketPreparation(candidates);
    auto committedPackets = commitPreparedPackets(candidates.front());

    // No expected failure remains. The RNG draw is consumed exactly once.
    auto selectedIndex = actions->commitTriggeredUlRandomAccess(uora);
    ASSERT(selectedIndex >= 0 && selectedIndex < (int)candidates.size());

    auto selected = std::move(candidates[selectedIndex]);
    ASSERT(!selected.stagedExchange.empty());
    selected.stagedExchange.mapped().packets = std::move(committedPackets);
    commitPreparedSequenceOrBar(selected);
    actions->commitTriggeredUlHandoff(std::move(selected.txReservation));
    publishPreparedExchange(std::move(selected));
}
```

Move `beforeRandomAccessCommit` and every other fault-injection hook before `commitPreparedPackets()`. After that point, UORA stale-state or insertion failure is an invariant violation. This permits deletion of outer `rollbackPacketOwners`, `queueOrder`, `queueCommitted`, `precommit()`, `transferPrecommit()`, and `rollback()`. `HeQueueService::commitPacketReservation()` may retain its narrow internal strong guarantee; it is local queue exception safety, not cross-component transaction orchestration.

Do not remove `triggerId`, `exchanges`, association epoch, BSSID/AID/TID checks, deadline checks, response timer handling, or late/foreign Block Ack rejection.

## Phase 3: execution

### HE DL MU

`HeDlMuTxOpFs` receives only immutable execution data and the normal execution services. Its first step sends the prebuilt container; it no longer performs planning or calls back with planning success/failure.

```cpp
HeDlMuTxOpFs::HeDlMuTxOpFs(
        HeDlMuExecutionPtr execution,
        std::unique_ptr<Packet> containerPacket,
        physicallayer::Ieee80211ModeSet *modeSet,
        IFrameSequenceHandler::ICallback *callback,
        IHeDlMuExecutionCallback *outcomes) :
    execution(std::move(execution)), containerPacket(std::move(containerPacket)),
    modeSet(modeSet),
    callback(callback), outcomes(outcomes),
    sequence(new SequentialFs({
        new StepFs("HE-MU-PPDU", [this](auto *, auto *context) {
            ASSERT(containerPacket.get() == execution->containerPacket);
            return new TransmitStep(containerPacket.release(),
                    context->getIfs(), true);
        }),
        buildAckTail(execution)
    }))
{
    ASSERT(this->execution && this->execution->containerPacket);
}
```

The provider becomes the sole mutable active owner:

```cpp
struct ActiveHeDlMuExchange {
    HeDlMuExecutionPtr execution;
    std::set<const Packet *> transmittedMembers;
    std::set<MacAddress> completedUsers;
};

void HeDlMuExchangeProvider::onUserOutcome(
        const MacAddress& peer, HeDlMuUserOutcome outcome)
{
    if (!activeExchange || !containsPeer(*activeExchange->execution, peer) ||
            !activeExchange->completedUsers.insert(peer).second)
        return;

    actions->heDlMuUserOutcome(peer, outcome);
    if (allUsersCompleted(*activeExchange))
        activeExchange.reset();
}
```

The frame sequence and provider share the immutable `HeDlMuExecution`; only the provider mutates completion state. `HeHcfTxRxInterceptor` reads the provider's execution members instead of parallel fields in `HeDlMuTxOpFs`.

### VHT DL MU

`VhtDlMuTxOpFs::prepareStep(0)` becomes only:

```cpp
if (step == 0) {
    ASSERT(containerPacket.get() == execution->containerPacket);
    return new TransmitStep(containerPacket.release(), context->getIfs(), true);
}
```

The VHT frame sequence receives the released prepared container as a
`std::unique_ptr<Packet>`, just like HE. Later BAR/BA steps use
`execution->users`. `VhtHcfFeature` owns
`ActiveVhtDlMuExchange { execution, completedUsers }`. The frame sequence
reports `onUserResult(userIndex, result)` without maintaining a second
completion vector.

### AP-triggered HE UL

`HeUlMuTxOpFs` owns the committed AP-side execution because it is already the single frame-exchange state machine. It sends the prepared Trigger, collects HE-TB responses by Trigger ID/RU/AID, and builds the response-dependent Multi-STA BlockAck.

```cpp
IFrameSequenceStep *HeUlMuTxOpFs::prepareStep(FrameSequenceContext *context)
{
    if (step == 0)
        return new TransmitStep(prepared.triggerPacket.release(),
                context->getIfs(), true);
    if (step == 1)
        return new HeUlReceiveCollectionStep(
                prepared.triggerId, callback, prepared.plan.getSchedule(),
                prepared.plan.getTriggerType(), prepared.responseTimeout,
                prepared.plan.getSchedule().commonDuration,
                modeSet->getPhyRxStartDelay());
    if (step == 2 && prepared.plan.getTriggerType() !=
            IIeee80211HeUlTriggerPolicy::NFRP_TRIGGER)
        return new TransmitStep(buildMultiStaBlockAckPacket(),
                modeSet->getSifsTime(), true);
    return nullptr;
}
```

The Trigger ID remains on the Trigger, received HE-TB correlation, and Multi-STA BlockAck.

### Station HE-TB

`HeTriggeredUlExchangeService::exchanges[triggerId]` remains the active owner after response handoff. The existing timer and `processMultiStaBlockAck()` paths remain behaviorally unchanged. Simplification ends at publication; timeout/retry/retirement is not folded into preparation.

## Required interface changes

Keep the new ports narrow. They expose the three lifecycle operations; they do
not introduce a transaction coordinator.

```cpp
// Additions to the HE DL provider's action port.
virtual std::unique_ptr<FrameSequenceContext>
        buildHeDlMuFrameSequenceContext(AccessCategory ac) = 0;
virtual IAckHandler *getHeDlMuAckHandler(AccessCategory ac) = 0;
virtual void commitHeDlMuSequenceState(
        const ISequenceNumberAssignment& state) = 0;
virtual void startHeDlMuExchange(
        HeDlMuExecutionPtr execution,
        std::unique_ptr<Packet> containerPacket,
        std::unique_ptr<FrameSequenceContext> context) = 0;

// VHT uses its existing EDCA/data-service accessors plus this prepared start.
virtual void startVhtDlMuExchange(
        VhtDlMuExecutionPtr execution,
        std::unique_ptr<Packet> containerPacket,
        std::unique_ptr<FrameSequenceContext> context) = 0;

// Additions to the AP UL Trigger service's action port.
virtual std::unique_ptr<FrameSequenceContext>
        buildHeUlFrameSequenceContext(AccessCategory ac) = 0;
virtual void startHeUlMuExchange(
        std::unique_ptr<HeUlMuTxOpFs> sequence,
        std::unique_ptr<FrameSequenceContext> context) = 0;
```

The station-side triggered-UL `IActions` interface already has the required
prepare/commit seams: clone/commit sequence state, validate/commit/rollback
packets, prepare/commit Tx handoff, and prepare/commit Block Ack request. Do not
add another abstraction there. Keep `commitTriggeredUlHandoff()` `noexcept`.

The implementation-facing file changes are therefore bounded:

- add the three prepared-exchange value files under `mac/framesequence/`;
- update `HeDlMuExchangeProvider`, `HeHcfRuntime`, and `HeHcfDl` to prepare,
  commit, and start HE DL values;
- update `HeDlMuTxOpFs`, `VhtDlMuTxOpFs`, and their callback interfaces to
  execute immutable values;
- update `VhtHcfFeature` to own the active VHT exchange;
- update `HeUlTriggerService` and `HeUlMuTxOpFs` for AP-side prepared Triggers;
- simplify `HeTriggeredUlExchangeService` in place for station HE-TB;
- add only constructor adapters in `EhtDlMuTxOpFs` and `EhtUlMuTxOpFs`.

`HcfFeatureSet.cc` is not part of the transaction path and needs no change.

## Removing HE/VHT DL numeric IDs

Do this last, as a separate patch.

Delete HE `nextTransactionToken`, `activeTransactionToken`, token parameters, and wrap checks only after:

- preparation no longer occurs inside a frame sequence;
- only one `ActiveHeDlMuExchange` can exist;
- frame-sequence callbacks cannot outlive their shared execution;
- late HCF timers are still rejected by `HcfExchangeEngine` generations;
- duplicate member/user outcomes are suppressed by active-owner sets;
- a completed frame sequence is deleted before another is started.

The reduced execution callback is:

```cpp
class IHeDlMuExecutionCallback
{
  public:
    virtual ~IHeDlMuExecutionCallback() = default;
    virtual void memberTransmitted(const HeDlMuMember& member) = 0;
    virtual void userOutcome(const MacAddress& peer,
            HeDlMuUserOutcome outcome) = 0;
};
```

Likewise VHT becomes:

```cpp
class IVhtDlMuExecutionCallback
{
  public:
    virtual ~IVhtDlMuExecutionCallback() = default;
    virtual void processFailedFrame(Packet *packet) = 0;
    virtual void userResult(unsigned int userIndex, UserResult result) = 0;
};
```

Do not replace IDs with a new monotonic generation merely to preserve the old shape. If runtime tests reveal a real late-callback path, keep the existing numeric generation and document that evidence instead of forcing token removal.

This deletion does **not** apply to HE Trigger IDs in AP UL or station HE-TB.

## EHT and legacy compatibility

Add prepared-exchange overloads without deleting current public constructors in the first patches:

```cpp
EhtDlMuTxOpFs(HeDlMuExecutionPtr execution, ...);
EhtUlMuTxOpFs(HeUlMuPreparedExchange&& prepared, ...);
```

The existing plan-based and legacy callback/rate-selection constructors remain adapters during migration. Once all in-tree production callers use prepared objects, either:

- retain the legacy constructors as explicitly tested compatibility paths that internally call the same preparation helper; or
- remove them only in a separate API cleanup with all current callers enumerated.

Do not let a legacy adapter reintroduce production planning callbacks into `HeDlMuExchangeProvider` or `VhtHcfFeature`.

## Ordered patch series

### Patch 1 — prepared values, no behavioral change

Add HE DL, VHT DL, and AP UL prepared-exchange files. Extract current builders into them while leaving existing constructors as adapters. Keep tokens, phases, rollback, and tests unchanged.

Definition of done:

- current frame sequences call the extracted builders;
- packet, tag, FCS, sequence, TXVECTOR, and allocation results are identical;
- no new mutable owner or generic base exists.

### Patch 2 — HE DL prepare before frame-sequence startup

Make `HeDlMuExchangeProvider::PreparedStart` move-only. Prepare the complete exchange and context in `prepareStart()`. In `commitStart(PreparedStart&&)`, configure protection, commit scheduler/sequence/queue/ACK state, publish `ActiveHeDlMuExchange`, and start the prebuilt sequence.

Delete:

- `ReservationRollbackGuard`;
- `reservedPackets` and reservation APIs;
- `pendingScheduler`, `pendingScheduleContext`, and `pendingAllocations` fields after their values move into `PreparedStart`;
- synchronous planning-failure callbacks;
- protection snapshot/restore fields for DL MU;
- repeated final queue scan, retaining one assertion at the handoff boundary if desired.

Keep numeric token fields until Patch 6.

### Patch 3 — VHT DL prepare before frame-sequence startup

Prepare in `VhtHcfFeature::commitDlMu()` before protection. Commit and publish one `ActiveVhtDlMuExchange`; start `VhtDlMuTxOpFs` with immutable execution data.

Delete:

- lazy step-0 container building;
- `PREPARED`, `COMMITTING`, `TERMINAL`, `pendingFailure`, and `ownershipCommitted` once no callback uses them;
- protection rollback for VHT DL MU;
- duplicated mutable active users/container fields in the frame sequence.

Keep exchange ID until Patch 6.

### Patch 4 — AP-triggered UL preparation

Build the Trigger and allocate Trigger ID in `HeUlTriggerService::prepareStart()`. Commit schedule/decision in `commitStart()` and start `HeUlMuTxOpFs` with `HeUlMuPreparedExchange`.

Delete Trigger building and scheduler-commit callbacks from the frame-sequence startup path. Keep response collection, Trigger ID matching, Multi-STA BlockAck construction, EHT UL inheritance, and active exchange state.

### Patch 5 — station HE-TB cleanup

Split `prepareAndCommitResponse()` into explicit preparation and commit. Keep one resulting sequence clone and prepared MPDU image per selected original. Prepare all Tx reservations and ledger nodes before mutation.

For scheduled responses, remove outer rollback state. For UORA, move fault hooks and stale checks before mutation, commit the common packet preparation once, consume one random draw, then publish the selected candidate. Delete `precommit()`, `transferPrecommit()`, `rollback()`, and their outer backup fields only after the UORA failure tests have equivalent pre-boundary coverage.

Retain Trigger-ID ledger and all later response/timeout logic.

### Patch 6 — active-owner consolidation and optional DL ID removal

Make `HeDlMuExchangeProvider` and `VhtHcfFeature` the only mutable DL active owners, with frame sequences sharing immutable execution data. Update interceptors and rate-control paths to read those owners. Run late/duplicate callback tests, then remove HE/VHT DL numeric IDs if no callback can cross exchange lifetime.

This patch is optional: the architectural simplification succeeds even if the small numeric generations remain. Do not contort the design merely to remove two counters.

### Patch 7 — compatibility cleanup

Audit EHT and legacy constructors, delete obsolete planning callback methods and fault hooks, and remove unused structs/includes. Do not touch `HcfFeatureSet.cc`; it is composition-only. Do not weaken `HcfExchangeEngine` timer generations.

## Regression gates

Run the focused unit suite after every family is switched, not only after the final cleanup:

```sh
inet_run_unit_tests -m release -f \
  '(Ieee80211HeDlMuTransaction|HeDlMuExchangeProvider|Ieee80211HeMuSeqAck|Ieee80211VhtDlMuScheduler|Ieee80211HeTxopCoordinatorService|Ieee80211HeUlMuTransaction|Ieee80211HeUlControlFrames|Ieee80211EhtDlMuExchangeEngine).*\.test'
```

Required invariants:

1. **Preparation failure:** queue order/pointers, packet headers/tags/age, sequence state, ACK/BA state, scheduler accounting, protection, Trigger ledger, and semantic-event count remain unchanged.
2. **Sequence behavior:** fresh MPDUs receive exactly one new number; retries retain identity, number, and Retry bit; modulo-4096 wrap remains correct.
3. **Commit:** the final post-packing allocation is committed once; exact original packet objects move to in-progress; container headers/FCS/tags match prepared values.
4. **HE/VHT DL execution:** partial BA, timeout, duplicate/late outcome, and per-user completion affect only the active member once.
5. **AP UL:** Trigger wire fields, Duration/NAV, Trigger ID, schedule accounting, HE-TB receive window, and Multi-STA BA records remain unchanged.
6. **Station HE-TB:** scheduled and UORA responses preserve one RNG draw, OBO transition, queue identity, Trigger-ID correlation, association epoch, partial BA retirement, timeout retry, late/foreign response rejection, and wraparound.
7. **Compatibility:** HE-ineligible traffic still falls back to SU; legacy SU, VHT MU, HE MU, AP UL, station HE-TB, and EHT paths remain enabled only under their existing gates.

Existing tests that must be retained or replaced with equivalent semantic assertions:

- `tests/unit/Ieee80211HeDlMuTransaction_1.test`
- `tests/unit/HeDlMuExchangeProvider_1.test`
- `tests/unit/Ieee80211HeMuSeqAck_1.test`
- `tests/unit/Ieee80211VhtDlMuScheduler_1.test`
- `tests/unit/Ieee80211HeTxopCoordinatorService_1.test`
- `tests/unit/Ieee80211HeUlMuTransaction_1.test`
- `tests/unit/Ieee80211HeUlControlFrames_1.test`
- `tests/unit/Ieee80211EhtDlMuExchangeEngine_1.test`
- `tests/module/Ieee80211HeDlMuExchange_1.test`
- `tests/module/Ieee80211VhtDlMuNegative_1.test`
- `tests/module/Ieee80211SharedMacModes_1.test`

Add one missing unit case before changing Trigger correlation: process the same Trigger ID twice while a station-side exchange is active and assert one ledger entry, one response event, no second dequeue/sequence allocation, and no late terminal mutation.

After the source stabilizes, run the focused architecture check:

```sh
bash .agents/skills/inet-architectural-requirements/references/enforcement/check-architecture.sh \
  src/inet/linklayer/ieee80211/mac
```

Then run the HE DL and VHT module tests and the shared MAC mode matrix using their declared deterministic configurations. Fingerprints may be inspected, but no fingerprint CSV is updated without separate approval and a first-divergent-event explanation.

## Review gates

Do not merge a patch if any of the following is true:

- a container or AP Trigger is first built after scheduler/queue/sequence/ACK mutation;
- a recoverable policy decision remains after the first mutation;
- a frame sequence owns a second mutable copy of active completion state;
- a prepared execution value stores live queue, MIB, scheduler, or
  `FrameSequenceContext` state; a short-lived `PreparedStart` orchestration
  envelope may retain the scheduler commit target and owned execution context;
- retries receive new sequence numbers;
- packet-level or region tags are lost during prepared-image adoption;
- HE Trigger ID, RU/AID/TID, association epoch, or deadline validation is weakened;
- a scheduler commit occurs zero or multiple times;
- a removed fault hook has no equivalent pre-boundary semantic test;
- an EHT or legacy mode silently falls back to the wrong constructor/path.

The final design is successful when the production path reads plainly as:

```cpp
auto prepared = prepare(snapshot);     // complete and recoverable
if (!prepared)
    return fallback();

configureProtection();
auto active = commit(std::move(*prepared)); // no policy decisions remain
start(active);                         // stateful multi-event exchange
```

For station-triggered UL, the corresponding final form is:

```cpp
auto prepared = prepareResponse(triggerSnapshot); // includes Trigger-ID correlation
if (!prepared)
    return;

commitResponse(std::move(*prepared));              // publishes exchanges[triggerId]
// Multi-STA BA or timeout later completes the active ledger entry.
```
