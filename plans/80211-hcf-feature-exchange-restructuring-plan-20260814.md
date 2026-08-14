# IEEE 802.11 HCF feature and exchange restructuring plan

## Status

This plan starts from the current checkout on 2026-08-14. It is a structural
refactor: it must preserve the supported VHT, HE, EHT, HT, and legacy behavior.
It supersedes the naming and ownership proposals in the earlier discussion; it
does not undo the already established prepare/commit/execute boundaries in the
MU frame sequences.

The implementation must be delivered as a sequence of independently buildable,
reviewable patches. Do not implement the whole plan as one change.

## Intended result

The end state is:

```text
Hcf                                      NED-facing module and common HCF host
├── HcfExchangeEngine                   common active frame-sequence/timer owner
├── common EDCA/TXOP/ACK services
├── VhtHcfFeature                       VHT behavioral aggregate
│   ├── sounding / CSI
│   ├── Group ID membership / beamforming
│   └── VhtDlMuExchangeCoordinator      long-lived DL-MU coordinator
│       └── VhtDlMuExchange             zero or one committed active exchange
└── HeHcfFeature                        HE behavioral aggregate
    ├── peer, queue, sounding, TXOP, and UL services
    ├── HeDlMuExchangeCoordinator       long-lived DL-MU coordinator
    │   └── HeDlMuExchange              zero or one committed active exchange
    ├── HeUlTriggerService              AP-side Trigger exchange coordination
    └── HeTriggeredUlExchangeService    STA-side Trigger-derived response ledger
```

The NED-facing types stay exactly as they are:

```text
Hcf
VhtHcf extends Hcf
HeHcf extends Hcf
```

`VhtHcf` and `HeHcf` remain useful configuration/compatibility façades. They do
not become C++ behavioral subclasses with independent copies of common HCF
state. Internally, `Hcf` composes the selected `VhtHcfFeature` or
`HeHcfFeature`.

## Decisions fixed by this plan

1. Use `HeDlMuExchangeCoordinator`, not `HeDlMuExchangeProvider` or
   `HeDlMuExchangeOwner`. The class prepares, reserves, commits, falls back,
   correlates results, and retires state; “coordinator” describes that role.
2. Add actual `HeDlMuExchange` and `VhtDlMuExchange` objects. Each object
   represents one committed exchange. It is not the coordinator and not the
   frame-sequence state machine.
3. Keep `HeDlMuTxOpFs` and `VhtDlMuTxOpFs` as the procedural executors. They own
   step progression and packet handoff. Do not move their BAR/BA state machine
   into the exchange objects.
4. Keep HE/VHT exchange IDs. Put the ID in the concrete exchange object and use
   it to reject an event from an older exchange after a new one has started.
5. Keep HE Trigger IDs. They are carried between stations and the AP and are
   not interchangeable with local DL-MU exchange IDs.
6. Replace the vague combined `*ExchangeCallback` types with two directional,
   family-specific contracts: execution services and exchange events.
7. Do not introduce a generic `IExchangeReporter`, `IHcfFeature`,
   `MuExchangeBase`, or shared HE/VHT/UL transaction framework. Their required
   methods differ enough that such interfaces would hide rather than clarify
   ownership.
8. Rename `HeHcfRuntime` to `HeHcfFeature`. It is an amendment behavior
   aggregate, not a simulator runtime.
9. Remove the anonymous `HcfVhtRuntime` behavioral wrapper. Retain a small
   HCF-to-VHT action adapter because VHT still needs guarded calls back into the
   common HCF host.
10. Preserve the `featureSet` NED submodule, its typename defaults, every NED
    module path, parameter, signal, and statistic during this refactor.
11. Do not update fingerprint CSV files as part of this work.

## Current source facts the implementation must respect

- [`Hcf`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h)
  currently owns `HcfVhtRuntime` and `HeHcfRuntime`.
- The anonymous `HcfVhtRuntime` in
  [`Hcf.cc`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc)
  owns `VhtHcfFeature` and several continuation guards.
- [`HeHcfRuntime`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfRuntime.h)
  implements several HE action contracts and references services projected by
  `HeHcfFeatureSet`.
- [`HeDlMuExchangeProvider`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HeDlMuExchangeProvider.h)
  combines long-lived coordination with the implicit active exchange fields.
- [`VhtHcfFeature`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/VhtHcfFeature.h)
  embeds the VHT DL-MU lifecycle and active exchange fields directly.
- [`IHeDlMuExchangeCallback`](../src/inet/linklayer/ieee80211/mac/contract/IHeDlMuExchangeCallback.h)
  and [`IVhtDlMuExchangeCallback`](../src/inet/linklayer/ieee80211/mac/contract/IVhtDlMuExchangeCallback.h)
  mix queries made by a frame sequence with events reported by it.
- HE and VHT only publish active semantic exchange state after the respective
  frame sequence reports a successful plan commit. Failed preparation must not
  create a visible active exchange.
- EHT DL/UL frame sequences reuse HE execution contracts. EHT compatibility is
  therefore part of every HE contract migration, but this plan does not add an
  EHT HCF feature or EHT exchange coordinator.

## Scope and non-goals

### In scope

- role-revealing renames;
- explicit HE and VHT active exchange objects;
- one DL-MU coordinator per amendment family;
- directional HE/VHT execution-service and exchange-event contracts;
- symmetric `VhtHcfFeature` and `HeHcfFeature` composition inside `Hcf`;
- removal of the anonymous VHT behavioral runtime wrapper;
- safe lifetime and shutdown ordering;
- test and adapter migrations required by those changes.

### Out of scope

- changing scheduling, rate selection, RU/user selection, sounding policy, or
  Group ID policy;
- changing packet bytes, tags, FCS, TXVECTORs, Duration fields, timing, or
  Block Ack behavior;
- redesigning the already implemented MU preparation/commit algorithms;
- merging DL and UL transaction state;
- removing HE/VHT DL exchange IDs;
- removing on-air Trigger IDs or correlation tags;
- moving or renaming NED submodules;
- introducing a common exchange base class;
- unrelated cleanup in common HCF, DCF, PHY, or packet code;
- updating fingerprints.

## Architecture and sealing constraints

At plan creation, the targeted files under
`src/inet/linklayer/ieee80211/` are unsealed. The recursively sealed
`src/inet/common/packet/` subtree is not modified. No current architecture or
naming exception applies to this refactor. Recheck sealing immediately before
every production patch.

Applicable requirements include:

- `AR-ORG-CONTRACTS`, `AR-MOD-COMPOSITION`, `AR-MOD-PLUGGABLE`,
  `AR-COM-DIRECT`, `AR-LIFE-STAGES`, `AR-LIFE-OPERATIONS`,
  `AR-CFG-INFER`, `AR-QUAL-NAMING`, `AR-QUAL-TESTS`,
  `AR-QUAL-DETERMINISM`, `AR-QUAL-LOGGING`, and `AR-QUAL-TRACEABILITY`;
- `AR-WLAN-STD-TRACE`, `AR-WLAN-STD-GATING`,
  `AR-WLAN-ARCH-BOUNDARIES`, `AR-WLAN-ARCH-OWNERSHIP`,
  `AR-WLAN-ARCH-VARIANTS`, `AR-WLAN-FRAME-REPRESENTATION`,
  `AR-WLAN-PHY-AUTHORITY`, `AR-WLAN-PHY-TIMING`,
  `AR-WLAN-MAC-EXCHANGE`, `AR-WLAN-MAC-SEQUENCE`,
  `AR-WLAN-MAC-QOS`, `AR-WLAN-MAC-MULTIUSER`,
  `AR-WLAN-OBS-EVENTS`, and `AR-WLAN-QUAL-TESTS`.

The focused architecture checker currently reports broad pre-existing include
findings when run on the whole IEEE 802.11 coordination-function subtree. Save
the baseline output and reject newly introduced findings; do not treat the
pre-existing output as permission to add more dependency violations.

## Ownership model

### Common HCF exchange versus amendment DL-MU exchange

These are intentionally different objects:

```text
HcfExchangeEngine
    owns the active IFrameSequence, FrameSequenceContext,
    response/inactivity timers, and common frame-exchange generation.

HeDlMuExchange / VhtDlMuExchange
    records the amendment-specific semantic identity of one committed
    DL-MU exchange and which committed users have reported outcomes.
```

The common engine answers “which frame-sequence step is executing?” The
amendment exchange answers “does this user/member outcome belong to the current
DL-MU exchange, and was it already processed?” Neither replaces the other.

### Required lifetime invariants

1. `Hcf` owns one `HcfExchangeEngine`.
2. `Hcf` owns at most one selected amendment feature.
3. Each amendment feature owns its DL-MU coordinator.
4. Each DL-MU coordinator owns at most one active concrete DL-MU exchange.
5. A frame sequence never owns or shares ownership of the concrete exchange.
6. A frame sequence holds non-owning pointers to stable execution-service and
   event interfaces owned by the feature/coordinator.
7. Every asynchronous HE/VHT DL-MU event carries an exchange ID.
8. An event with an ID different from the active ID is stale and performs no
   state mutation.
9. A repeated terminal event for an already completed member/user performs no
   second mutation or notification.
10. An active exchange is published only after the frame-sequence commit point.
11. The `Packet *` values in an exchange are identity references. The queue,
    in-progress-frame service, aggregate, or transmit path remains the owner.
12. The concrete exchange is retired only after all committed users terminate,
    or when an explicit abort/shutdown path invalidates the exchange.
13. The exchange engine destroys the active frame sequence before the feature
    and coordinator event sinks are destroyed.
14. HE UL services and Trigger-ID ledgers remain independent from the HE DL-MU
    coordinator.

## Target C++ types

### Family-specific contract types

Add contract-neutral headers
`mac/contract/HeDlMuExchangeTypes.h` and
`mac/contract/VhtDlMuExchangeTypes.h`. Put types shared by an implementation,
frame sequence, and contract in these headers. A contract header must never
include `coordinationfunction/HeDlMuExchange.h` or
`coordinationfunction/VhtDlMuExchange.h`.

Use aliases to make IDs self-documenting without introducing wrapper arithmetic
or serialization code. Move the existing `HeDlMuMember` and
`HeDlMuUserOutcome` declarations out of the combined callback header:

```cpp
// HeDlMuExchangeTypes.h
using HeDlMuExchangeId = uint64_t;
constexpr HeDlMuExchangeId NO_HE_DL_MU_EXCHANGE = 0;

struct HeDlMuMember {
    HcfPacketIdentity packetIdentity;
    Packet *packet = nullptr;                    // non-owning
    MacAddress peer;
    Tid tid = 0;
    AccessCategory accessCategory = AC_BE;
    int mcs = 0;
    int numberOfSpatialStreams = 1;
    int ruToneSize = 0;
};

enum class HeDlMuUserOutcome {
    BLOCK_ACK_RECEIVED,
    BLOCK_ACK_TIMED_OUT,
};

// VhtDlMuExchangeTypes.h
using VhtDlMuExchangeId = uint64_t;
constexpr VhtDlMuExchangeId NO_VHT_DL_MU_EXCHANGE = 0;

enum class VhtDlMuUserResult {
    TRANSMITTED,
    BLOCK_ACK_RECEIVED,
    BLOCK_ACK_TIMED_OUT,
};
```

Keep allocation monotonic and fail loudly before wrap to zero, matching the
current behavior. Do not reuse a retired ID during a simulation.

### `HeDlMuExchange`

Add
[`HeDlMuExchange.h`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HeDlMuExchange.h)
and `HeDlMuExchange.cc`.

The object contains only committed semantic state:

```cpp
class INET_API HeDlMuExchange
{
  private:
    HeDlMuExchangeId id = NO_HE_DL_MU_EXCHANGE;
    Packet *containerPacket = nullptr;              // non-owning identity
    std::vector<HeDlMuMember> members;              // committed immutable view
    std::vector<bool> transmittedMembers;           // indexed like members
    std::vector<MacAddress> users;                  // unique, deterministic order
    std::vector<bool> completedUsers;               // indexed like users

    int findMember(const HeDlMuMember& member) const;
    int findUser(const MacAddress& peer) const;

  public:
    HeDlMuExchange(HeDlMuExchangeId id,
            Packet *containerPacket,
            std::vector<HeDlMuMember> members);

    HeDlMuExchange(const HeDlMuExchange&) = delete;
    HeDlMuExchange& operator=(const HeDlMuExchange&) = delete;
    HeDlMuExchange(HeDlMuExchange&&) = default;
    HeDlMuExchange& operator=(HeDlMuExchange&&) = default;

    HeDlMuExchangeId getId() const { return id; }
    Packet *getContainerPacket() const { return containerPacket; }
    const std::vector<HeDlMuMember>& getMembers() const { return members; }

    bool isContainer(const Packet *packet) const
        { return packet != nullptr && packet == containerPacket; }

    // true means this was a valid first report; false means unknown/duplicate.
    bool recordMemberTransmitted(const HeDlMuMember& member);
    bool recordUserOutcome(const MacAddress& peer);
    bool isComplete() const;
};
```

`findMember()` compares the full committed identity tuple used by the current
provider—packet identity/pointer, peer, TID, and access category—not merely the
peer address. A user may legitimately have more than one committed member.

Constructor validation:

```cpp
HeDlMuExchange::HeDlMuExchange(HeDlMuExchangeId id,
        Packet *container, std::vector<HeDlMuMember> committedMembers) :
    id(id), containerPacket(container), members(std::move(committedMembers)),
    transmittedMembers(members.size(), false)
{
    if (id == 0 || containerPacket == nullptr || members.empty())
        throw cRuntimeError("Invalid committed HE DL MU exchange");

    for (const auto& member : members) {
        if (member.packet == nullptr || member.peer.isUnspecified())
            throw cRuntimeError("Invalid HE DL MU member");
        if (std::find(users.begin(), users.end(), member.peer) == users.end())
            users.push_back(member.peer);
    }
    completedUsers.assign(users.size(), false);
}
```

Do not store the scheduler, queues, sequence-number allocator, ACK handler,
protection snapshot, `FrameSequenceContext`, or frame-sequence step state here.
Those belong to planning/coordination/execution respectively.

### `VhtDlMuExchange`

Add
[`VhtDlMuExchange.h`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/VhtDlMuExchange.h)
and `VhtDlMuExchange.cc`.

```cpp
class INET_API VhtDlMuExchange
{
  private:
    VhtDlMuExchangeId id = NO_VHT_DL_MU_EXCHANGE;
    Packet *containerPacket = nullptr;              // non-owning identity
    std::vector<std::vector<Packet *>> userPackets; // non-owning identities
    std::vector<std::vector<bool>> failedPackets;   // indexed like userPackets
    std::vector<bool> completedUsers;

  public:
    VhtDlMuExchange(VhtDlMuExchangeId id,
            Packet *containerPacket,
            std::vector<std::vector<Packet *>> userPackets);

    VhtDlMuExchangeId getId() const { return id; }
    Packet *getContainerPacket() const { return containerPacket; }
    const std::vector<std::vector<Packet *>>& getUserPackets() const
        { return userPackets; }

    bool containsPacket(const Packet *packet) const;
    bool recordFailedPacket(const Packet *packet);
    bool recordUserResult(unsigned int userIndex,
            VhtDlMuUserResult result);
    bool isComplete() const;
};
```

`TRANSMITTED` is nonterminal. Block Ack received/timed out is terminal. Reject an
out-of-range user index and suppress a duplicate terminal result. Preserve the
current feature behavior for failure/retry reporting; do not invent a new retry
policy in this object. Initialize `failedPackets` with the same shape as
`userPackets`; `recordFailedPacket()` returns true only for a packet identity in
the committed exchange that has not previously been reported failed.

### HE directional contracts

Add two headers under `mac/contract`:

```cpp
class INET_API IHeDlMuExecutionServices
{
  public:
    virtual ~IHeDlMuExecutionServices() = default;

    virtual queueing::IPacketQueue *resolveHeDlMuQueue(
            HcfQueueToken token) const = 0;
    virtual Packet *getReservedHeDlMuPacket(
            HeDlMuExchangeId id, const MacAddress& peer) const = 0;
    virtual bool isReservedHeDlMuPacket(
            HeDlMuExchangeId id, const MacAddress& peer,
            const Packet *packet) const = 0;
    virtual IOriginatorBlockAckAgreementHandler *
            getHeDlMuBlockAckHandler() const = 0;
    virtual IOriginatorMacDataService *
            getHeDlMuOriginatorDataService() const = 0;
    virtual IQosRateSelection *getHeDlMuRateSelection() const = 0;
    virtual MacAddress getHeDlMuTransmitterAddress() const = 0;
    virtual int getHeDlMuFcsMode() const = 0;
    virtual uint8_t getHeDlMuBssColor() const = 0;
    virtual uint16_t getHeDlMuAssociationId(
            const MacAddress& peer) const = 0;
    virtual std::optional<Ieee80211NegotiatedHeCapabilities>
            getHeDlMuNegotiatedCapabilities(
                    const MacAddress& peer) const = 0;
};

class INET_API IHeDlMuExchangeEvents
{
  public:
    virtual ~IHeDlMuExchangeEvents() = default;

    virtual void heDlMuPlanFinalized(HeDlMuExchangeId id,
            const std::vector<HeDlMuMember>& members) = 0;
    virtual void heDlMuPlanCommitted(HeDlMuExchangeId id,
            Packet *containerPacket,
            const std::vector<HeDlMuMember>& members) = 0;
    virtual void heDlMuMemberTransmitted(HeDlMuExchangeId id,
            const HeDlMuMember& member) = 0;
    virtual void heDlMuUserOutcome(HeDlMuExchangeId id,
            const MacAddress& peer, HeDlMuUserOutcome outcome) = 0;
};
```

The first migration keeps a source-compatibility umbrella:

```cpp
class INET_API IHeDlMuExchangeCallback :
        public IHeDlMuExecutionServices,
        public IHeDlMuExchangeEvents
{
  public:
    virtual ~IHeDlMuExchangeCallback() = default;
};
```

New production constructors take the two narrow interfaces separately:

```cpp
HeDlMuTxOpFs(IHeDlMuExecutionServices *services,
        IHeDlMuExchangeEvents *events,
        const HeDlMuPlan& plan,
        HeDlMuExchangeId id,
        AckMethod ackMethod,
        ...);
```

The old constructor remains temporarily and delegates:

```cpp
HeDlMuTxOpFs(IHeDlMuExchangeCallback *callback, ...)
    : HeDlMuTxOpFs(callback, callback, ...)
{
}
```

This preserves EHT and test compilation while making dependency direction
visible. Delete the umbrella only after every production and test caller uses
the split constructor.

### VHT directional contracts

Use the same pattern, but keep it VHT-specific:

```cpp
class INET_API IVhtDlMuExecutionServices
{
  public:
    virtual ~IVhtDlMuExecutionServices() = default;
    virtual Ieee80211Mac *getVhtDlMuMac() const = 0;
    virtual IOriginatorMacDataService *
            getVhtDlMuOriginatorDataService() const = 0;
    virtual queueing::IPacketQueue *resolveVhtDlMuQueue(
            HcfQueueToken sourceQueueToken) const = 0;
};

class INET_API IVhtDlMuExchangeEvents
{
  public:
    virtual ~IVhtDlMuExchangeEvents() = default;
    virtual void vhtDlMuPlanCommitted(VhtDlMuExchangeId id,
            Packet *containerPacket,
            const std::vector<std::vector<Packet *>>& userPackets) = 0;
    virtual void vhtDlMuFrameFailed(VhtDlMuExchangeId id,
            Packet *packet) = 0;
    virtual void vhtDlMuUserResult(VhtDlMuExchangeId id,
            unsigned int userIndex, VhtDlMuUserResult result) = 0;
};

class INET_API IVhtDlMuExchangeCallback :
        public IVhtDlMuExecutionServices,
        public IVhtDlMuExchangeEvents
{
  public:
    using UserResult = VhtDlMuUserResult; // temporary source compatibility
};
```

Add the exchange ID to failed-frame reporting as part of the interface migration.
It must not allow an old frame failure to mutate retry/rate state for a later
active exchange.

### `HeDlMuExchangeCoordinator`

After the mechanical rename, the coordinator has two different states:

```cpp
class INET_API HeDlMuExchangeCoordinator :
        public IHeDlMuExecutionServices,
        public IHeDlMuExchangeEvents
{
  public:
    class INET_API IActions : public IHeDlMuExecutionServices
    {
      public:
        virtual ~IActions() = default;

        virtual bool stageHeDlMuPacket(HcfQueueToken queueToken,
                HcfPacketIdentity packetIdentity,
                AccessCategory accessCategory) = 0;
        virtual bool startHeDlMuSingleUserIfEligible(
                AccessCategory accessCategory) = 0;
        virtual HeDlMuProtectionSnapshot captureHeDlMuProtection(
                AccessCategory accessCategory) const = 0;
        virtual void configureHeDlMuProtection(
                AccessCategory accessCategory) = 0;
        virtual void restoreHeDlMuProtection(
                AccessCategory accessCategory,
                const HeDlMuProtectionSnapshot& snapshot) = 0;

        virtual bool startHeDlMuExchange(
                AccessCategory accessCategory,
                const HeDlMuPlan& plan,
                HeDlMuExchangeId id,
                HeDlMuTxOpFs::AckMethod ackMethod,
                const StartupParameters& parameters,
                IHeDlMuExecutionServices *services,
                IHeDlMuExchangeEvents *events) = 0;

        // Semantic effects in common HCF; names differ from event overrides
        // so the direction of the call is unambiguous.
        virtual void notifyHeDlMuMemberTransmitted(
                HeDlMuExchangeId id,
                const HeDlMuMember& member) = 0;
        virtual void notifyHeDlMuUserOutcome(
                HeDlMuExchangeId id,
                const MacAddress& peer,
                HeDlMuUserOutcome outcome) = 0;
    };

  private:
    // Pre-commit coordination state. This is not an active Exchange.
    HeDlMuExchangeId pendingExchangeId = NO_HE_DL_MU_EXCHANGE;
    std::map<MacAddress, std::vector<Packet *>> reservedPackets;
    IIeee80211HeDlScheduler *pendingScheduler = nullptr;
    IIeee80211HeDlScheduler::ScheduleContext pendingScheduleContext;
    std::vector<IIeee80211HeDlScheduler::RuAllocation> pendingAllocations;
    std::optional<AccessCategory> pendingProtectionAccessCategory;
    std::optional<HeDlMuProtectionSnapshot> pendingProtectionSnapshot;

    // Committed semantic exchange state.
    std::unique_ptr<HeDlMuExchange> activeExchange;

    IActions *actions = nullptr;
    HeSoundingService *soundingService = nullptr;
    bool forceNextSingleUser[AC_NUMCATEGORIES] = {};
    HeDlMuExchangeId nextExchangeId = 1;

    HeDlMuExchange *findActive(HeDlMuExchangeId id)
    {
        return activeExchange != nullptr && activeExchange->getId() == id
                ? activeExchange.get() : nullptr;
    }
};
```

The coordinator implements each `IHeDlMuExecutionServices` query as a direct
forward to `actions`. `HeHcfFeature` implements `IActions`, including those
service queries. When the coordinator starts the executor it passes `this` as
both narrow ports:

```cpp
return actions->startHeDlMuExchange(ac, plan, id, ackMethod, parameters,
        /* services = */ this,
        /* events = */ this);
```

This is the final compileable replacement for the current
`IActions : IHeDlMuExchangeCallback` inheritance. Patch 9 may delete the
combined callback only after all implementations use this contract.

The reservation ID is pending before commit; the concrete exchange is active
after commit. Never overload one field to mean both states.

Core event logic:

```cpp
void HeDlMuExchangeCoordinator::heDlMuPlanCommitted(
        HeDlMuExchangeId id, Packet *container,
        const std::vector<HeDlMuMember>& members)
{
    if (id == 0 || id != pendingExchangeId || activeExchange != nullptr)
        throw cRuntimeError("Invalid HE DL MU commit event");

    validateCommittedMembersAgainstReservation(id, members);
    activeExchange = std::make_unique<HeDlMuExchange>(
            id, container, std::vector<HeDlMuMember>(members));
    clearPendingCommitStateWithoutRetiringId();
}

bool HeDlMuExchangeCoordinator::heDlMuMemberTransmitted(
        HeDlMuExchangeId id, const HeDlMuMember& member, bool notify)
{
    auto *exchange = findActive(id);
    if (exchange == nullptr || !exchange->recordMemberTransmitted(member))
        return false;
    if (notify)
        actions->notifyHeDlMuMemberTransmitted(id, member);
    return true;
}

bool HeDlMuExchangeCoordinator::heDlMuUserOutcome(
        HeDlMuExchangeId id, const MacAddress& peer,
        HeDlMuUserOutcome outcome, bool notify)
{
    auto *exchange = findActive(id);
    if (exchange == nullptr || !exchange->recordUserOutcome(peer))
        return false;
    if (notify)
        actions->notifyHeDlMuUserOutcome(id, peer, outcome);
    if (exchange->isComplete())
        activeExchange.reset();
    return true;
}
```

The two `bool` methods above are internal/testable helpers with the additional
`notify` argument. The interface overrides stay exact and simply call them:

```cpp
void HeDlMuExchangeCoordinator::heDlMuMemberTransmitted(
        HeDlMuExchangeId id, const HeDlMuMember& member)
{
    heDlMuMemberTransmitted(id, member, true);
}

void HeDlMuExchangeCoordinator::heDlMuUserOutcome(
        HeDlMuExchangeId id, const MacAddress& peer,
        HeDlMuUserOutcome outcome)
{
    heDlMuUserOutcome(id, peer, outcome, true);
}
```

`routeTransmittedContainer()` delegates to `activeExchange->isContainer()` and
iterates `activeExchange->getMembers()`. `getActiveMembers()` returns an empty
static view when no exchange exists, or preferably becomes
`const HeDlMuExchange *getActiveExchange() const` so callers must acknowledge
the optional state.

### `VhtDlMuExchangeCoordinator`

Add a dedicated class after the VHT exchange object exists:

```cpp
class INET_API VhtDlMuExchangeCoordinator :
        public IVhtDlMuExchangeEvents
{
  public:
    class IActions {
      public:
        virtual ~IActions() = default;
        virtual void processVhtDlMuFailedFrame(Packet *packet) = 0;
    };

  private:
    IActions *actions = nullptr;
    VhtDlMuExchangeId nextExchangeId = 1;
    VhtDlMuExchangeId pendingExchangeId = NO_VHT_DL_MU_EXCHANGE;
    VhtDlMuExchangeId lastRetiredExchangeId = NO_VHT_DL_MU_EXCHANGE;
    std::unique_ptr<VhtDlMuExchange> activeExchange;

  public:
    VhtDlMuExchangeId beginPendingExchange();
    void abandonPendingExchange(VhtDlMuExchangeId id);
    void retireExchange(VhtDlMuExchangeId id);
    const VhtDlMuExchange *getActiveExchange() const
        { return activeExchange.get(); }

    virtual void vhtDlMuPlanCommitted(VhtDlMuExchangeId id,
            Packet *container,
            const std::vector<std::vector<Packet *>>& users) override;
    virtual void vhtDlMuFrameFailed(VhtDlMuExchangeId id,
            Packet *packet) override;
    virtual void vhtDlMuUserResult(VhtDlMuExchangeId id,
            unsigned int userIndex, VhtDlMuUserResult result) override;
};
```

After callback splitting, the final VHT aggregate declaration is explicit:

```cpp
class INET_API VhtHcfFeature :
        public IVhtDlMuExecutionServices,
        public VhtDlMuExchangeCoordinator::IActions,
        public IVhtGroupIdManager::ILocalMembershipListener
{
  private:
    VhtDlMuExchangeCoordinator dlMuCoordinator;
    // existing VHT policy/sounding/group fields
};
```

`VhtHcfFeature::configure()` calls `dlMuCoordinator.configure(this)`. Its
execution-service methods answer MAC/data/queue queries; its coordinator-action
method forwards a validated failed packet to the existing common-HCF failure
action. It no longer implements `IVhtDlMuExchangeEvents` after Patch 5. Its
packet-interception paths obtain only a `const VhtDlMuExchange *` from the
coordinator to test container/member identities; they cannot mutate completion
state directly.

Failed-frame forwarding is exchange-scoped and idempotent:

```cpp
void VhtDlMuExchangeCoordinator::vhtDlMuFrameFailed(
        VhtDlMuExchangeId id, Packet *packet)
{
    if (activeExchange == nullptr || activeExchange->getId() != id)
        return; // stale exchange
    if (!activeExchange->recordFailedPacket(packet))
        return; // foreign or duplicate packet
    actions->processVhtDlMuFailedFrame(packet);
}
```

This check is required because a failed-frame report updates retry/rate-control
state. An ID check alone is insufficient if a foreign packet is supplied with
the current ID.

The VHT coordinator owns ID allocation, pending/active/retired validation, and
terminal completion. `VhtHcfFeature` owns VHT policy and supplies execution
services. The frame sequence therefore receives two different objects:

```cpp
auto sequence = txOpFactory->create(
        plan, modeSet, ackHandler, frameSequenceCallback,
        /* services = */ this,
        /* events = */ &dlMuCoordinator,
        exchangeId);
```

### `HeHcfFeature`

Rename, do not behaviorally rewrite, `HeHcfRuntime` first:

```cpp
class INET_API HeHcfFeature :
        public HeDlMuExchangeCoordinator::IActions,
        public IHeDlMuSnapshotSource,
        public HeUlTriggerService::IActions,
        public IHeUlMuExchangeCallback,
        public IHeUlMuSnapshotSource,
        public HeTriggeredUlExchangeService::IActions,
        public HeSoundingService::IActions,
        public HeHcfTxRxInterceptor::IActions
{
  public:
    struct Bindings {
        Hcf *owner = nullptr;                     // non-owning host
        Ieee80211Mac *mac = nullptr;
        Edca *edca = nullptr;
        IIeee80211HeDlScheduler *dlScheduler = nullptr;
        HeUlCoordinator *ulCoordinator = nullptr;
        IOriginatorBlockAckAgreementHandler *blockAckHandler = nullptr;
        IOriginatorBlockAckAgreementPolicy *blockAckPolicy = nullptr;
        std::function<bool()> isFrameSequenceRunning;
        std::function<void(const MacAddress&)> invalidateBasePeer;
    };

  private:
    Hcf *hcf = nullptr;
    HcfHeFeatureServices services;
    Bindings bindings;

    HeDlMuExchangeCoordinator dlMuCoordinator;
    HeTxopCoordinatorService txopCoordinator;
    HeUlTriggerService ulTriggerService;
    HeFrameDecorationPolicy frameDecorationPolicy;
    std::unique_ptr<HeHcfTxRxInterceptor> txRxInterceptor;

    // Existing HE fields follow unchanged.
};
```

Do not add a broad `IHcfFeature` merely to hold these methods. The HCF host and
HE feature are a compile-time same-node composition. Existing typed action
contracts and `Bindings` are clearer than a new catch-all virtual interface.

After the type rename, move the HE DL coordinator out of
`HeHcfFeatureSet` and into `HeHcfFeature`. Keep peer/queue/sounding/triggered-UL
services in the feature-set bundle for this refactor; moving every service is a
separate composition decision and is not needed for the naming/ownership goal.

### `VhtHcfFeature` and the HCF action adapter

`VhtHcfFeature` remains the VHT aggregate and owns a
`VhtDlMuExchangeCoordinator`. The current anonymous `HcfVhtRuntime` becomes a
small adapter that does not own VHT behavior:

```cpp
class HcfVhtActions final : public VhtHcfFeature::IActions
{
  private:
    Hcf& hcf;

    // Preserve current recursion/continuation guards exactly.
    bool continuingFrameSequence = false;
    bool continuingRecipientFrame = false;
    bool continuingSetFrameMode = false;
    bool continuingTransmitFrame = false;
    bool continuingTransmittedFrame = false;
    bool continuingReceivedFrame = false;
    bool continuingTransmissionComplete = false;

  public:
    explicit HcfVhtActions(Hcf& hcf) : hcf(hcf) {}

    // Every method is a one-line forwarding/guard operation into Hcf.
    virtual Ieee80211Mac *getMac() const override;
    virtual FrameSequenceContext *buildFrameSequenceContext(
            AccessCategory ac) override;
    virtual bool hasFrameToTransmit(AccessCategory ac) const override;
    virtual void releaseChannel(AccessCategory ac) override;
    virtual void startSingleUserExchange(AccessCategory ac) override;
    virtual void startFeatureFrameSequence(
            IFrameSequence *sequence, FrameSequenceContext *context) override;
    virtual void continueBaseFrameSequence(AccessCategory ac) override;
    // ...the remaining existing VHT actions, without policy state.
};
```

`Hcf` owns the adapter and feature separately:

```cpp
std::unique_ptr<HcfVhtActions> vhtActions;
std::unique_ptr<VhtHcfFeature> vhtFeature;
std::unique_ptr<HeHcfFeature> heFeature;
```

Declare `vhtActions` before `vhtFeature`, or explicitly reset the feature first,
so the feature never outlives its action target.

### Feature-set projection

Keep the NED contract and rename only the C++ vocabulary after the feature
objects exist:

```cpp
enum class HcfFeatureKind {
    COMMON,
    VHT,
    HE,
};

struct HcfHeFeatureServices
{
    HePeerStateService *peerStateService = nullptr;
    HeQueueService *queueService = nullptr;
    HeTriggeredUlExchangeService *triggeredUlExchangeService = nullptr;
    HeSoundingService *soundingService = nullptr;

    bool isComplete() const;
};

class IHcfFeatureSet
{
  public:
    virtual HcfFeatureKind getFeatureKind() const
        { return HcfFeatureKind::COMMON; }
    virtual HcfHeFeatureServices getHeFeatureServices() { return {}; }
};
```

Do not rename the NED `featureSet` submodule or its `typename` values. This C++
rename happens only after `HeDlMuExchangeCoordinator` is owned by
`HeHcfFeature`; otherwise the service bundle would still need the old field.

### UL treatment

UL is deliberately not forced into the DL shape:

```text
AP HeUlTriggerService
    prepares/commits Trigger exchanges and starts HeUlMuTxOpFs.

STA HeTriggeredUlExchangeService
    owns its existing per-Trigger-ID Exchange ledger and response timeout.
```

During the `HeHcfRuntime` to `HeHcfFeature` rename:

- update the action target names;
- keep `HeUlTriggerService`, `HeUlCoordinator`, and
  `HeTriggeredUlExchangeService` separate;
- retain Trigger-ID allocation and `Ieee80211HeTriggerCorrelationTag` handling;
- retain Basic Trigger, BSRP, NFRP, UORA, HE-TB response, Multi-STA BA,
  deadline, AID, RU, association-epoch, duplicate, and late-response checks;
- do not route UL events through `HeDlMuExchangeCoordinator`;
- preserve EHT UL constructor compatibility.

## Patch-by-patch implementation

Each patch below is a hard checkpoint. A failed checkpoint blocks the next
patch. Mechanical rename patches must contain no behavior changes.

### Patch 0 — capture the baseline

No production edits.

Record:

- Git SHA and `git status --short`;
- release and debug build status;
- exact unit/module commands, exit codes, logs, and result directories;
- current architecture-check output;
- current fingerprints as read-only evidence.

Commands from the repository root:

```sh
make MODE=release -j$(nproc)

inet_run_unit_tests -m release \
  -f '(Hcf.*|HeDlMuExchangeProvider|HeDlScheduler.*|HeUl.*|Ieee80211HeDlMu.*|Ieee80211HeUl.*|Ieee80211VhtDlMu.*|Ieee80211VhtGroupIdManager|Ieee80211EhtDlMuExchangeEngine|Ieee80211EhtProductionPath|Ieee80211HeTxopCoordinatorService).*\.test'

make MODE=debug -j$(nproc)

inet_run_module_tests -m debug \
  -f '(Ieee80211HeDlMuExchange_1|Ieee80211HeUlTriggerExchange_1|Ieee80211VhtDlMuNegative_1|Ieee80211QosCoordinationFunctionContract_[12]|Ieee80211HeConfigurationContract_1|Ieee80211SharedMacModes_1).*'

bash .agents/skills/inet-architectural-requirements/references/enforcement/check-architecture.sh \
  src/inet/linklayer/ieee80211/mac/coordinationfunction
```

GO: baseline behavior is understood and all unexpected failures are explained.

### Patch 1 — mechanically rename HE provider to coordinator

Rename files and symbols together:

```text
HeDlMuExchangeProvider.h/.cc
    -> HeDlMuExchangeCoordinator.h/.cc

HeDlMuExchangeProvider
    -> HeDlMuExchangeCoordinator

HeDlMuExchangeProvider_1.test
    -> HeDlMuExchangeCoordinator_1.test

dlMuExchangeProvider
    -> dlMuExchangeCoordinator

getDlMuExchangeProvider()
    -> getDlMuExchangeCoordinator()
```

Update:

- `HcfFeatureSet.h/.cc`;
- `IHcfFeatureSet.h`;
- `HeHcfRuntime.h/.cc`, `HeHcf.cc`, `HeHcfDl.cc`, and interceptor code;
- `HeTxopCoordinatorService`;
- all unit/module mocks and includes.

Keep a temporary source alias for one patch only if downstream repository code
outside this subtree requires it:

```cpp
using HeDlMuExchangeProvider [[deprecated(
        "use HeDlMuExchangeCoordinator")]] = HeDlMuExchangeCoordinator;
```

Do not keep both filenames or duplicate implementations. The alias belongs in
a compatibility header and is deleted in Patch 9.

Verification:

```sh
rg -n 'HeDlMuExchangeProvider|dlMuExchangeProvider|getDlMuExchangeProvider' \
  src/inet/linklayer/ieee80211 tests
```

Only an explicitly documented compatibility alias may remain.

GO: release build and the renamed coordinator, HE DL transaction, HE TXOP
coordinator, and EHT DL tests pass with no runtime-output change.

### Patch 2 — extract `HeDlMuExchange`

Add the class specified above and unit-test it directly.

Replace coordinator fields:

```cpp
// Remove:
Packet *containerPacket;
std::vector<HeDlMuMember> members;
std::set<const Packet *> transmittedMembers;
std::set<MacAddress> completedUsers;

// Add:
HeDlMuExchangeId pendingExchangeId = 0;
std::unique_ptr<HeDlMuExchange> activeExchange;
```

Retain reservation, pending scheduler, protection, and fallback fields in the
coordinator. They are not part of a committed exchange.

Map old methods one-for-one:

```text
isActiveContainer       -> activeExchange->isContainer
routeTransmittedContainer -> activeExchange->getMembers + recordMemberTransmitted
heDlMuMemberTransmitted -> activeExchange->recordMemberTransmitted
heDlMuUserOutcome       -> activeExchange->recordUserOutcome
terminal clear          -> activeExchange.reset()
```

The constructor is called only from `heDlMuPlanCommitted()`, never from
`prepareStart()`, `reservePlan()`, or `heDlMuPlanFinalized()`. If allocation at
the commit notification is considered unacceptable, preallocate a private
pending exchange value after all members are finalized and move it into
`activeExchange` at commit; do not publish it through active accessors early.

New direct tests:

- invalid zero ID, null container, empty member set;
- exact member identity acceptance;
- unknown member rejection;
- duplicate transmitted-member suppression;
- unique-user derivation when a user has several members;
- duplicate user-outcome suppression;
- completion only after every committed user terminates.

Coordinator tests:

- preparation failure leaves `activeExchange == nullptr`;
- reservation is pending but not active;
- successful plan commit creates exactly one exchange;
- old ID after a second exchange cannot mutate it;
- terminal completion destroys it exactly once.

### Patch 3 — extract `VhtDlMuExchange`

Add the VHT exchange class and replace only the embedded active data in
`VhtHcfFeature`:

```cpp
// Remove from VhtHcfFeature:
std::vector<bool> completedUsers;
Packet *activeContainerPacket;
std::vector<std::vector<Packet *>> activeUserPackets;

// Add temporarily:
std::unique_ptr<VhtDlMuExchange> activeDlMuExchange;
```

Keep `nextDlMuExchangeId`, `lastRetiredDlMuExchangeId`, and the lifecycle phase
in `VhtHcfFeature` for this patch. Do not extract the coordinator simultaneously.

Create the object only in `vhtDlMuPlanCommitted()`. Delegate result tracking and
container/user lookup to it. Preserve the current `TERMINAL` and retired-ID
duplicate handling until Patch 5 proves the coordinator replacement.

New direct tests:

- valid construction and read-only user-packet identity;
- user-index bounds;
- `TRANSMITTED` remains nonterminal;
- Block Ack received/timed out is terminal;
- duplicate terminal result suppression;
- all-users-complete detection.

Run `Ieee80211VhtDlMuNegative_1` after this patch; it is the strongest existing
VHT ownership/rollback/retry oracle.

### Patch 4 — split HE and VHT callback directions

Add the four narrow interfaces and change frame sequences to store two pointers:

```cpp
IHeDlMuExecutionServices *heServices = nullptr;
IHeDlMuExchangeEvents *heEvents = nullptr;

IVhtDlMuExecutionServices *vhtServices = nullptr;
IVhtDlMuExchangeEvents *vhtEvents = nullptr;
```

Before adding the interfaces, add the two `*ExchangeTypes.h` headers described
above and change the old combined callback headers to include them. This keeps
the transition buildable and allows Patch 9 to delete the umbrella headers
without deleting shared member/outcome types.

Mechanical call-site mapping:

```text
queue/data/rate/capability lookups -> *ExecutionServices
plan/member/user/failure reports  -> *ExchangeEvents
```

Keep the combined callback interfaces only as temporary derived compatibility
umbrellas. Keep old constructors as delegating adapters. Add new constructors
to `EhtDlMuTxOpFs` in the same patch or explicitly delegate its old HE callback
constructor to the new HE pair.

Do not:

- pass a concrete coordinator pointer into the frame sequence;
- downcast an interface to `Hcf`, `HeHcfFeature`, `VhtHcfFeature`, or a
  coordinator;
- create a generic `IExchangeReporter`;
- merge HE and VHT result enums;
- touch UL callback contracts in this patch.

Compile-time tests should assert that the TxOp frame sequences are constructible
from the two narrow contracts and that EHT remains constructible through its
compatibility adapter.

GO: HE DL, VHT DL, EHT DL, frame-sequence engine, stale-ID, and duplicate-result
tests all pass.

### Patch 5 — extract `VhtDlMuExchangeCoordinator`

Add `VhtDlMuExchangeCoordinator.h/.cc` and move only DL-MU lifecycle concerns
out of `VhtHcfFeature`:

```text
next/active/retired exchange IDs
pending/active/terminal validation
active VhtDlMuExchange ownership
plan-committed event
failed-frame event routing
per-user result and retirement
```

Keep in `VhtHcfFeature`:

```text
grant snapshot creation
scheduler and policy invocation
sounding and CSI
Group ID membership
beamforming tags
packet interception
common-HCF continuation decisions
execution-service queries
TxOp factory seam
```

The feature delegates commit orchestration:

```cpp
VhtHcfFeature::GrantDisposition VhtHcfFeature::commitDlMu(
        const VhtGrantSnapshot& snapshot)
{
    auto id = dlMuCoordinator.beginPendingExchange();
    auto sequence = txOpFactory->create(
            *snapshot.dlMuPlan,
            actions->getModeSet(),
            edcaf->getAckHandler(),
            actions->getFrameSequenceCallback(),
            this,                   // execution services
            &dlMuCoordinator,       // exchange events
            id);

    // Preserve the existing prepare -> protection -> commit -> start order.
    // On pre-commit failure, abandonPendingExchange(id) and take the existing
    // synchronous fallback path. Do not create an active VhtDlMuExchange.
    return prepareCommitAndStart(std::move(sequence), id, snapshot);
}
```

Preserve `ITxOpFactory` injection used by the VHT negative module test. Update
the factory signature to accept the two narrow interfaces; do not remove the
fault-injection seam.

Coordinator tests must cover:

- monotonic nonzero ID allocation;
- one pending and one active exchange maximum;
- pending abandonment;
- active creation at commit only;
- duplicate terminal event;
- last-retired event;
- exchange-1 result arriving while exchange 2 is active;
- out-of-range user result;
- exact-once failed-frame forwarding;
- foreign packet with current ID and valid packet with stale ID are both ignored.

### Patch 6 — rename `HeHcfRuntime` to `HeHcfFeature`

This patch is a type/file/layout rename with no ownership move yet.

Rename:

```text
HeHcfRuntime.h/.cc -> HeHcfFeature.h/.cc
HeHcfRuntime       -> HeHcfFeature
heRuntime          -> heFeature
getHeRuntime()     -> getHeFeature()
HcfHeRuntimeServices -> HcfHeFeatureServices
getHeRuntimeServices() -> getHeFeatureServices()
```

Rename implementation files for clarity where practical:

```text
HeHcfDl.cc -> HeHcfFeatureDl.cc
HeHcfUl.cc -> HeHcfFeatureUl.cc
```

Keep `HeHcf.cc` for the NED-facing `HeHcf` module registration and any genuinely
module-facing definitions. Move `HeHcfFeature::*` definitions out of it into the
feature implementation files so filenames match their primary type.

Header dependency rule:

- `Hcf.h` forward-declares `HeHcfFeature`;
- `HeHcfFeature.h` forward-declares `Hcf` and must not include `Hcf.h` merely to
  access private inline helpers;
- move HCF-dependent inline wrappers into `.cc` files;
- update friendship only for the narrow feature/action classes that actually
  need it.

Provide a one-patch compatibility accessor if needed:

```cpp
HeHcfFeature& Hcf::getHeFeature() const;

[[deprecated("use getHeFeature()")]]
HeHcfFeature& Hcf::getHeRuntime() const
{
    return getHeFeature();
}
```

The repository-wide search after the patch must find `HeHcfRuntime` only in a
documented compatibility alias/accessor. No comments or diagnostics should call
the new feature a runtime.

Mandatory tests in this patch include feature-set replacement, HE link/PHY
adapter, HE TX/RX interception, HE TXOP coordination, UL transactions, and HCF
module initialization.

### Patch 7 — make `HeHcfFeature` own the HE DL coordinator

Move `HeDlMuExchangeCoordinator` from `HeHcfFeatureSet` to a value member of
`HeHcfFeature`.

Before:

```text
HeHcfFeatureSet owns coordinator
HeHcfFeature references coordinator through service bundle
```

After:

```text
HeHcfFeature owns coordinator
HeHcfFeatureSet owns peer/queue/sounding/STA-trigger services
```

Update the service bundle to remove the coordinator field. During feature
construction/configuration:

```cpp
HeHcfFeature::HeHcfFeature(const HcfHeFeatureServices& services,
        const Bindings& bindings, ...)
    : services(services), hcf(bindings.owner), bindings(bindings), ...
{
    if (!services.isComplete())
        throw cRuntimeError("HE HCF feature has incomplete services");

    dlMuCoordinator.configure(this, services.soundingService);
}
```

Replace all `services.dlMuExchangeCoordinator` access with the member. Keep an
explicit accessor only where interceptors/tests need the coordinator’s read-only
active-exchange view.

Update `HcfFeatureSet_1` and replacement tests so “complete HE services” means
the four remaining external services. Add a feature-construction test proving
that each `HeHcfFeature` gets exactly one coordinator.

GO: no coordinator outlives its feature, and no second coordinator is created
by the feature set.

### Patch 8 — remove `HcfVhtRuntime` and complete feature symmetry

Extract its HCF forwarding behavior into `HcfVhtActions`, preserving every
continuation guard and call order. Move its owned `VhtHcfFeature` to an HCF
member.

Move the current `HcfVhtRuntime::startFrameSequence()` decision body into
`VhtHcfFeature::startFrameSequence()`. Add exactly three host actions needed by
that body—`hasFrameToTransmit()`, `releaseChannel()`, and
`startSingleUserExchange()`—to `VhtHcfFeature::IActions`. This keeps VHT grant
policy in the VHT feature while the adapter performs only common-HCF actions.

```cpp
void VhtHcfFeature::startFrameSequence(AccessCategory ac)
{
    if (!actions->hasFrameToTransmit(ac)) {
        actions->releaseChannel(ac);
        return;
    }

    auto snapshot = prepareGrantSnapshot(ac);
    switch (snapshot.startKind) {
        case VhtGrantSnapshot::StartKind::COMMON_SINGLE_USER:
            actions->startSingleUserExchange(ac);
            return;
        case VhtGrantSnapshot::StartKind::SU_SOUNDING:
        case VhtGrantSnapshot::StartKind::MU_SOUNDING:
            startSounding(snapshot);
            return;
        case VhtGrantSnapshot::StartKind::GROUP_MANAGEMENT:
        case VhtGrantSnapshot::StartKind::BLOCK_ACK_PREREQUISITE:
        case VhtGrantSnapshot::StartKind::DL_MULTIUSER:
            if (commitPreparedGrant(snapshot) ==
                    GrantDisposition::FINISHED_SYNCHRONOUSLY)
                actions->releaseChannel(ac);
            return;
    }
    throw cRuntimeError("Unknown VHT grant start kind");
}
```

During HCF initialization, move the current parameter/submodule resolution from
`HcfVhtRuntime::initialize()` into the existing HCF feature-construction block,
then call `vhtFeature->configure(vhtActions.get(), ...)`. Do not make
`VhtHcfFeature` read NED parameters or find submodules by path.

Replace HCF fields:

```cpp
// Remove:
std::unique_ptr<HcfVhtRuntime> vhtRuntime;

// Add:
std::unique_ptr<HcfVhtActions> vhtActions;
std::unique_ptr<VhtHcfFeature> vhtFeature;
```

Update all dispatch categories, not only grant handling:

- initialization/configuration;
- mode-set changes;
- channel-grant/frame-sequence selection;
- recipient frame interception;
- `setFrameMode` interception;
- transmit, transmitted, received, and transmission-complete interception;
- peer invalidation;
- frame-sequence completion;
- test TxOp-factory injection;
- destruction/shutdown.

Guard forwarding conceptually as follows:

```cpp
void HcfVhtActions::continueBaseFrameSequence(AccessCategory ac)
{
    ContinuationGuard guard(continuingFrameSequence);
    hcf.startFrameSequence(ac);
}

void Hcf::startFrameSequence(AccessCategory ac)
{
    if (heFeature != nullptr)
        return heFeature->startFrameSequence(ac);
    if (vhtFeature != nullptr && !vhtActions->isContinuingFrameSequence())
        return vhtFeature->startFrameSequence(ac);
    startCommonFrameSequence(ac);
}
```

Use the exact current guard semantics; the sketch illustrates ownership, not a
license to change recursion behavior. Retain the existing `ContinuationGuard`
name and its set-on-construction/clear-on-destruction behavior; do not introduce
a differently scoped helper while moving the code.

At this checkpoint HCF ownership is visibly symmetric:

```cpp
std::unique_ptr<VhtHcfFeature> vhtFeature;
std::unique_ptr<HeHcfFeature> heFeature;
```

The features are not required to implement a common virtual base. HCF already
knows the configured feature kind and calls family-specific methods.

### Patch 9 — rename the feature-set C++ discriminator and remove adapters

After no behavioral runtime remains, rename:

```text
HcfAmendmentRuntimeKind -> HcfFeatureKind
getAmendmentRuntimeKind -> getFeatureKind
```

This is a C++ vocabulary cleanup only. Preserve:

```text
Hcf.ned:    featureSet.typename = CommonHcfFeatureSet
VhtHcf.ned featureSet.typename = VhtHcfFeatureSet
HeHcf.ned: featureSet.typename = HeHcfFeatureSet
```

Remove, once searches prove they are unused:

- `HeDlMuExchangeProvider` alias;
- `getHeRuntime()` compatibility accessor;
- combined HE/VHT callback umbrella constructors;
- combined callback headers if no external compatibility policy requires them;
- comments and diagnostics using provider/runtime terminology.

Do not remove exchange IDs, Trigger IDs, last-retired guards, or common HCF
timer generations in this cleanup patch.

### Patch 10 — make shutdown ordering explicit

The current frame-sequence handler owns non-owning pointers to its callback/event
sinks. Therefore feature destruction must not precede active-sequence
destruction.

First make abnormal frame-sequence termination visible to the selected feature.
Add `frameSequenceAborted` to `HcfExchangeEngine::Actions`. The engine callback
keeps the current common state transition and then invokes the action:

```cpp
void HcfExchangeEngine::frameSequenceAborted()
{
    exchangeCoordinator.abort();
    getActiveActions().frameSequenceAborted();
}
```

`Hcf` forwards this only to the selected feature. The HE/VHT coordinator clears
an active concrete exchange for its current ID; VHT records the ID as retired so
later results remain stale. This cleanup does not synthesize Block Ack outcomes,
retry notifications, or success signals. If abort occurs before a plan was
committed, no active exchange exists and the call is a no-op. Update every
`HcfExchangeEngine::Actions` test fixture in the same patch.

Add a shutdown-only engine operation:

```cpp
void HcfExchangeEngine::shutdown(const Actions& actions)
{
    cancelTimers(actions);

    // Destroy the active frame sequence and context while all feature/event
    // targets still exist. Destruction does not synthesize protocol outcomes.
    frameSequenceHandler.reset();

    responseService.clearTimerState();
    activeActions = nullptr;
    activeExchangeGeneration = 0;
    startRxTimerGeneration = 0;
    deferredTimeoutGeneration = 0;
    shutDown = true;
}
```

Do not call `HcfExchangeCoordinator::reset()` here: its normal contract accepts
only the `COMPLETING` state and an active sequence may be in another state at
shutdown. `shutdown()` is terminal; add a `shutDown` flag and reject every later
engine operation. The engine and its embedded coordinator are destroyed after
the HCF destructor body, so no normal lifecycle transition is needed.

HCF teardown order:

```cpp
Hcf::~Hcf()
{
    if (exchangeEngine != nullptr)
        exchangeEngine->shutdown(makeExchangeActions());

    if (heFeature != nullptr)
        heFeature->shutdown();

    heFeature.reset();
    vhtFeature.reset();
    vhtActions.reset();
}
```

Feature shutdown then retires active semantic exchanges and cancels HE-owned UL
timers/ledgers. No frame sequence can call an event sink after that point.

The HE coordinator shutdown must handle both sides of the commit boundary while
its HCF action target is still alive:

```cpp
void HeDlMuExchangeCoordinator::shutdown()
{
    if (pendingExchangeId != NO_HE_DL_MU_EXCHANGE)
        rollbackReservation(pendingExchangeId); // restores pending protection

    reservedPackets.clear();
    pendingScheduler = nullptr;
    pendingScheduleContext = {};
    pendingAllocations.clear();
    pendingProtectionAccessCategory.reset();
    pendingProtectionSnapshot.reset();
    pendingExchangeId = NO_HE_DL_MU_EXCHANGE;

    activeExchange.reset();
    actions = nullptr;
    soundingService = nullptr;
}
```

Call this from `HeHcfFeature::shutdown()` before clearing queue/peer services.
`rollbackReservation()` may call protection-restoration actions, so it must run
before `actions` is detached. The VHT coordinator shutdown clears its pending
ID, active exchange, and action pointer; VHT has no pending state that is allowed
to survive the synchronous prepare/commit call stack.

Add a focused lifetime test with a fake frame sequence whose destructor asserts
that its event target is still alive. Cover teardown with an active HE DL, VHT
DL, and HE UL sequence. Run under the normal debug assertions; use LLDB or a
sanitizer only if the focused test exposes a failure.

Also add a direct engine unit test that calls each public operational entry after
terminal `shutdown()` and expects the same fail-loud diagnostic. At minimum cover
channel request/grant, preparation, frame-sequence start, transmit completion,
response processing, timer handling, and handler replacement; do not leave a
partially usable shut-down engine.

## Source change inventory

Expected new files:

```text
src/inet/linklayer/ieee80211/mac/coordinationfunction/HeDlMuExchange.h
src/inet/linklayer/ieee80211/mac/coordinationfunction/HeDlMuExchange.cc
src/inet/linklayer/ieee80211/mac/coordinationfunction/VhtDlMuExchange.h
src/inet/linklayer/ieee80211/mac/coordinationfunction/VhtDlMuExchange.cc
src/inet/linklayer/ieee80211/mac/coordinationfunction/VhtDlMuExchangeCoordinator.h
src/inet/linklayer/ieee80211/mac/coordinationfunction/VhtDlMuExchangeCoordinator.cc
src/inet/linklayer/ieee80211/mac/contract/HeDlMuExchangeTypes.h
src/inet/linklayer/ieee80211/mac/contract/VhtDlMuExchangeTypes.h
src/inet/linklayer/ieee80211/mac/contract/IHeDlMuExecutionServices.h
src/inet/linklayer/ieee80211/mac/contract/IHeDlMuExchangeEvents.h
src/inet/linklayer/ieee80211/mac/contract/IVhtDlMuExecutionServices.h
src/inet/linklayer/ieee80211/mac/contract/IVhtDlMuExchangeEvents.h
```

Expected renames:

```text
HeDlMuExchangeProvider.h/.cc -> HeDlMuExchangeCoordinator.h/.cc
HeHcfRuntime.h/.cc           -> HeHcfFeature.h/.cc
HeHcfDl.cc                   -> HeHcfFeatureDl.cc
HeHcfUl.cc                   -> HeHcfFeatureUl.cc
HeDlMuExchangeProvider_1.test -> HeDlMuExchangeCoordinator_1.test
```

Expected modifications include:

```text
Hcf.h/.cc
HcfExchangeEngine.h/.cc
HcfFeatureSet.h/.cc
IHcfFeatureSet.h
VhtHcfFeature.h/.cc
IHeDlMuExchangeCallback.h
IVhtDlMuExchangeCallback.h
HeDlMuTxOpFs.h/.cc
VhtDlMuTxOpFs.h/.cc
EhtDlMuTxOpFs.h/.cc
HeHcf.cc
HeHcfTxRxInterceptor.h/.cc
HeTxopCoordinatorService.h/.cc
affected unit/module test doubles
```

No NED file should require a semantic edit. A NED diff is a stop-and-review
event unless it is only a verified comment update.

## Verification matrix

### Per-patch fast gate

From the repository root:

```sh
git diff --check
make MODE=release -j$(nproc)
```

Then run the narrow unit tests named by the patch. Do not defer compile failures
to a later patch.

### Explicit exchange/coordinator gate

```sh
inet_run_unit_tests -m release \
  -f '(HeDlMuExchange|HeDlMuExchangeCoordinator|VhtDlMuExchange|VhtDlMuExchangeCoordinator|HcfExchangeEngine|HcfExchangeCoordinator|Ieee80211HeDlMuTransaction|Ieee80211HeMuSeqAck|Ieee80211VhtDlMuScheduler).*\.test'
```

Required assertions:

- no active exchange before commit;
- exactly one active exchange after commit;
- old-ID event cannot mutate the next exchange;
- duplicate terminal event is ignored exactly once;
- abort/timeout/normal completion retire exactly once;
- exact packet identities remain unchanged;
- frame-sequence destruction precedes event-sink destruction.

### Feature-composition gate

```sh
inet_run_unit_tests -m release \
  -f '(HcfFeatureSet|HcfFeatureSetReplacement|HcfIngressIntegration|HcfTransmissionPreparationIntegration|HtHcfFeature|Ieee80211HeTxopCoordinatorService|Ieee80211HeTxRxInterception|Ieee80211HeLinkPhyAdapter).*\.test'
```

Required assertions:

- `Hcf`, `VhtHcf`, and `HeHcf` still instantiate;
- the same `featureSet.typename` defaults are selected;
- exactly one amendment feature is active;
- feature-disabled configurations use common HCF;
- AP and non-AP initialization still succeeds;
- incomplete replacement feature sets still fail with typed diagnostics.

### HE DL gate

```sh
inet_run_unit_tests -m release \
  -f '(HeDlMuExchange|HeDlMuExchangeCoordinator|Ieee80211HeDlMuTransaction|Ieee80211HeDlMuMimoFairness|Ieee80211HeMuBlockAckGating|Ieee80211HeMuSeqAck|Ieee80211HeMultiStaBlockAckFs|HeDlScheduler.*|HcfAggregationPlanning).*\.test'

inet_run_module_tests -m debug -f 'Ieee80211HeDlMuExchange_1.*'
```

Verify selected users/RUs, MU PPDU, MU-BAR/sequential BAR distinction, partial
timeout, late response, SU fallback, packet identity, tags, and no undisposed
packets.

### VHT gate

```sh
inet_run_unit_tests -m release \
  -f '(VhtDlMuExchange|VhtDlMuExchangeCoordinator|Ieee80211VhtDlMuScheduler|Ieee80211VhtDlMuPhy|Ieee80211VhtGroupIdManager|VhtBeamformingMimo|Ieee80211VhtCsiTxVector|VhtMpduAggregation).*\.test'

inet_run_module_tests -m debug -f 'Ieee80211VhtDlMuNegative_1.*'
```

Verify Group ID authority, user positions, per-user failure/retry, stale/duplicate
events, precommit rollback, sounding/CSI ordering, and SU fallback.

### HE UL gate

```sh
inet_run_unit_tests -m release \
  -f '(HeUlTriggerService|HeUlScheduler.*|Ieee80211HeUlMuPlan|Ieee80211HeUlMuTransaction|Ieee80211HeUlControlFrames|Ieee80211HeUlTypedExchangeContract|Ieee80211HeUora|Ieee80211HeTxopCoordinatorService).*\.test'

inet_run_module_tests -m debug -f 'Ieee80211HeUlTriggerExchange_1.*'
```

Verify Trigger-ID correlation, BSRP, Basic Trigger, NFRP, UORA, HE-TB response,
Multi-STA BA, duplicate/late Trigger and BA handling, association epochs, and
disabled-UL EDCA behavior.

### EHT compatibility gate

```sh
inet_run_unit_tests -m release \
  -f '(Ieee80211EhtDlMuExchangeEngine|Ieee80211EhtProductionPath|Ieee80211EhtCapabilities|Ieee80211EhtMode|Ieee80211EhtPreamblePuncturing).*\.test'
```

Verify EHT configurations still use HE exchange machinery, compile through the
split HE interfaces, and do not create a second exchange owner.

### Final common/legacy gate

```sh
inet_run_unit_tests -m release \
  -f '(HcfOriginatorIntegration|HcfRecipientService|HcfResponseService|HcfTransmissionPreparationIntegration|HcfFrameDispatchService|HcfIngressIntegration|HcfExchangeEngine|HcfExchangeCoordinator|HcfFeatureSet|HcfFeatureSetReplacement|HtHcfFeature).*\.test'

inet_run_module_tests -m debug \
  -f '(Ieee80211QosCoordinationFunctionContract_[12]|Ieee80211HeConfigurationContract_1|Ieee80211SharedMacModes_1).*'
```

After all focused gates pass, run the complete module suite in debug mode.

### Representative deterministic simulations

Run one configuration and run number at a time:

```sh
inet -u Cmdenv -f examples/ieee80211ac/dl_mu_mimo_baseline/omnetpp.ini \
  -c VhtDlMuMimoThreeUsers -r 0 \
  --result-dir=/tmp/inet-hcf-restructure-vht-mu \
  --cmdenv-express-mode=false

inet -u Cmdenv -f examples/ieee80211ax/dl_ofdma_sched/omnetpp.ini \
  -c EqualSizedRUs_fBW -r 0 \
  --result-dir=/tmp/inet-hcf-restructure-he-dl \
  --cmdenv-express-mode=false

inet -u Cmdenv -f examples/ieee80211ax/ul_ofdma/omnetpp.ini \
  -c BacklogBased5ms -r 0 \
  --result-dir=/tmp/inet-hcf-restructure-he-ul \
  --cmdenv-express-mode=false

inet -u Cmdenv -f examples/ieee80211be/eht_dl_ofdma/omnetpp.ini \
  -c EqualSizedMRUs -r 0 \
  --result-dir=/tmp/inet-hcf-restructure-eht \
  --cmdenv-express-mode=false
```

Record exact commands, cwd, exit status, logs, and result paths. Compare the
first changed event if any trajectory differs. Do not update fingerprint CSV
files without separate user approval.

## Review checklist after every architecture patch

- Does exactly one object own the mutable state in question?
- Is the concrete exchange absent before commit?
- Are all packet pointers explicitly documented as owning or non-owning?
- Can an old exchange ID mutate the new exchange?
- Can a duplicate event emit a second semantic outcome?
- Does a frame sequence depend only on execution services and events?
- Did any code downcast to a concrete HCF feature/coordinator/frame sequence?
- Did HE UL accidentally route through the HE DL coordinator?
- Did EHT get a second owner or lose the HE compatibility path?
- Are VHT continuation guards still present and scoped exactly as before?
- Do `Hcf`, `VhtHcf`, `HeHcf`, and `featureSet.typename` remain unchanged?
- Does the active frame sequence die before its event sink?
- Did a new include cross an architecture boundary?
- Did RNG use, user ordering, timer order, or signal order change?
- Were fingerprints left untouched?

## Final source scans

```sh
rg -n 'HeDlMuExchangeProvider|HeHcfRuntime|HcfVhtRuntime' \
  src/inet/linklayer/ieee80211 tests

rg -n 'IHeDlMuExchangeCallback|IVhtDlMuExchangeCallback' \
  src/inet/linklayer/ieee80211 tests

rg -n 'dynamic_cast<.*(Hcf|HeHcf|VhtHcf|.*TxOpFs)' \
  src/inet/linklayer/ieee80211/mac

rg -n 'active(TransactionToken|ContainerPacket|UserPackets)|completedUsers' \
  src/inet/linklayer/ieee80211/mac/coordinationfunction
```

Expected final state:

- the old provider/runtime/VHT-runtime names are absent;
- combined callback names are absent unless an explicitly approved public
  compatibility policy retains them;
- no frame sequence downcasts to its feature/coordinator;
- active semantic correlation/completion state is found only in
  `HeDlMuExchange` and `VhtDlMuExchange`; frame sequences may retain their
  immutable execution-local allocation/user views and packet-handoff state;
- coordinator pending/reservation state remains outside the concrete exchange.

## GO/NO-GO criteria

GO only when:

- every patch builds independently;
- release and debug focused tests pass;
- the NED/configuration surface is unchanged;
- each family has one coordinator and at most one committed active exchange;
- stale/duplicate events cannot mutate or re-notify;
- packet identity/ownership, timer order, and deterministic scheduling remain
  unchanged;
- HE UL and EHT compatibility gates pass;
- feature/event-sink destruction ordering is proven;
- no new architecture violation is introduced;
- no fingerprint CSV is changed.

NO-GO on:

- a patch combining mechanical rename with behavior change;
- an active exchange created before frame-sequence commit;
- two copies of active user/member completion state;
- direct frame-sequence dependency on a concrete coordinator;
- a broad generic feature/reporter/exchange base introduced for symmetry alone;
- changed module paths, typenames, parameters, signals, or statistics;
- stale events affecting a later exchange;
- packet leaks, double deletion, or queue identity/order change;
- DL/UL state being merged;
- loss of VHT sounding/Group ID or HE Trigger correlation;
- unexplained RNG, timer, signal, or fingerprint changes.

## Completion definition

The restructuring is complete when the code tells the same story as the model:

- `Hcf` is the common host;
- `VhtHcfFeature` and `HeHcfFeature` are symmetric amendment aggregates;
- `HeDlMuExchangeCoordinator` and `VhtDlMuExchangeCoordinator` are long-lived
  lifecycle coordinators;
- `HeDlMuExchange` and `VhtDlMuExchange` are real, short-lived committed
  exchanges;
- frame sequences execute through narrow service/event contracts;
- UL keeps its Trigger-specific architecture;
- the external NED/configuration and observable protocol behavior are preserved.
