# Detailed implementation plan

The program should be delivered as a sequence of small, behavior-preserving patches. Stages describe dependency order; each work package below should normally be its own PR or commit series.

```text
Stage 0 ──┬── Stage 1: state ownership ──┬── Stage 3: HE plans ── Stage 4: HCF
          │                             │                         │
          └── Stage 2: PHY authorities ─┘                         │
                                        Stage 5: Block Ack ◄──────┘
                                                   │
                                        Stage 6: measured optimization
                                                   │
                                        Stage 7: remaining isolation
```

Stages 1 and 2 are logically parallel, but production changes should still be merged serially with one writer at a time.

## Program-wide constraints

Unless a work package explicitly declares a behavior change, preserve:

- Existing NED types, parameters, defaults, gates, `typename` choices, signals and statistics.
- Packet bytes, tags, frame ordering, timing, retries, random-number consumption and event ordering.
- Current legacy, HT, VHT, HE and EHT fallback behavior.
- Current scheduler extension points in [HeHcf.ned](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.ned:82).
- Current HCF policy slots in [Hcf.ned](src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.ned:98).

Use pure C++ values or services for deterministic calculations and internal state owners. Add a C++ interface plus NED `moduleinterface` only when an algorithm is genuinely independently configurable or replaceable.

Before each production patch:

1. Recheck the sealing status of every target and new file.
2. Record applicable general and `AR-WLAN-*` requirements.
3. Establish the relevant IEEE revision and clause if normative behavior changes.
4. Add or identify a test that exercises the exact contract being moved.
5. Keep compatibility adapters until every consumer has migrated.
6. Never leave two writable representations of the same state.

## Stage 0 — Characterize behavior and establish baselines

### WP0.1: Produce the ownership and mutation inventory

Inventory every reader, writer and invalidation path for:

- Association state, BSSID, station role and AID allocation.
- Advertised and negotiated HT/VHT/HE/EHT capabilities.
- Peer link estimates and freshness.
- Local PHY operation and mode legality.
- EDCA, TXOP, retry and aggregation state.
- Originator/recipient Block Ack agreements and reorder windows.
- Radio-mode intent from transmission, TWT, lifecycle and MLO.

Primary anchors are [Ieee80211Mib.h](src/inet/linklayer/ieee80211/mib/Ieee80211Mib.h:70), [Hcf.h](src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h:57), [Ieee80211MgmtSta.h](src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtSta.h:35), and the HE planning code in [HeHcfUl.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfUl.cc:485).

For every datum, record:

- Authoritative owner.
- Mutation operations.
- Readers.
- Lifetime and reset conditions.
- Generation or epoch needed to detect stale snapshots.
- Semantic event emitted when it changes.

Exit criterion: every mutable datum has one proposed owner and no unresolved dual-writer relationship.

### WP0.2: Add characterization tests

Add tests before moving behavior:

- `Ieee80211MibOwnership_1.test`: association, reassociation, deauthentication, AID release/reuse, capability replacement and stale snapshots.
- `HcfDelegationContract_1.test`: current callback, timer and event order.
- `Ieee80211TimingIndex_1.test`: current SIFS, slot, RX-start and response timing values.
- `HeRuPlanRoundTrip_1.test`: scheduler allocation through serialization and receiver selection.
- Management transition tests for duplicate, reordered and invalid authentication/association frames.
- Golden packet and capture bytes for Trigger, BAR/BA, MU PHY and management frames.

Keep test-only patches separate from production extraction.

### WP0.3: Establish performance baselines

Create a benchmark-only harness; do not add wall-clock assertions to correctness tests.

Measure:

- Mode and RU lookup calls and candidate scans.
- PHY finalizer invocations.
- Scheduler candidates and attempted layouts.
- Packing candidates, PSDU-length calculations and BA eligibility queries.
- Plan and capability snapshot copies.
- Process CPU time, elapsed time and peak RSS.

Use 1, 4, 9 and 37 stations; 20, 80 and 160 MHz; SU, DL-MU and UL-MU. Run at least ten fresh release processes per point with capture, visualization and result recording disabled.

Exit criterion: measured variance and the actual dominant hotspots are known before optimization work is authorized.

## Stage 1 — Establish state ownership and radio-mode control

Keep `Ieee80211Mib` as the existing NED module and compatibility façade. Do not replace its module path or parameters.

### WP1.1: Add state-owner classes

Introduce pure C++ owners under `mib/`:

- `Ieee80211AssociationState`: role, BSS identity, association status and AID allocation.
- `Ieee80211PeerCapabilityState`: advertised and negotiated capabilities plus legacy rates.
- `Ieee80211LinkState`: peer link estimates and freshness.
- `Ieee80211PhyOperationState`: local capability and operation data derived from the active PHY.

Each owner must expose operations rather than mutable maps. For example:

```cpp
associatePeer(...)
reassociatePeer(...)
removePeer(...)
updatePeerCapabilities(...)
updateLinkEstimate(...)
```

Each state owner gets a monotonically increasing generation. Peer-scoped generations must distinguish reassociation of the same MAC address.

### WP1.2: Add immutable snapshots

Proposed values:

- `Ieee80211AssociationSnapshot`
- `Ieee80211PeerCapabilitySnapshot`
- `Ieee80211LinkSnapshot`
- `Ieee80211PhyOperationSnapshot`

Every snapshot should carry its source generation and association epoch. Extend the existing HE snapshot boundary in [IIeee80211HeLinkPhyContext.h](src/inet/linklayer/ieee80211/mac/contract/IIeee80211HeLinkPhyContext.h:24) instead of creating a parallel HE state system.

### WP1.3: Migrate writers first

Migrate one writer family per patch:

1. STA management.
2. AP management.
3. Ad hoc and simplified management.
4. PHY-derived initialization.
5. Peer teardown and reassociation invalidation.

Once a writer migrates, prevent direct writes to the corresponding old public field. Do not keep façade and owner writable simultaneously.

### WP1.4: Migrate readers

Migrate in bounded groups:

1. HE/VHT/HT coordination and sounding.
2. Rate selection and fragmentation.
3. RX, DS and MLD paths.
4. TWT.
5. Radio and remaining management readers.

After all consumers move, make the old MIB data bags private and then remove them.

### WP1.5: Extract radio-mode arbitration

Introduce `Ieee80211RadioModeController`, owned by `Ieee80211Mac`. It should arbitrate explicit intents from:

- Initial configuration.
- Transmission start/completion.
- TWT awake/sleep state.
- Lifecycle shutdown/start.
- MLO link state.

The controller should calculate one effective mode and ask the MAC to send the existing radio configuration request. Preserve `initialRadioMode` and the existing NED topology.

Do not assign this solely to HCF: TWT, lifecycle and MLO are wider than one coordination function.

### Stage 1 exit criteria

- No external class mutates a MIB data member.
- A peer’s old snapshot cannot affect a new association epoch.
- Each association/capability/link change emits one semantic event.
- Existing NED configuration remains valid.
- Initial, transmitting, receiver and sleep transitions are unchanged.

Focused gates:

- [Ieee80211HeAssociationLifecycle_1.test](tests/unit/Ieee80211HeAssociationLifecycle_1.test)
- [Ieee80211HeLinkPhyContext_1.test](tests/unit/Ieee80211HeLinkPhyContext_1.test)
- [Ieee80211HeConfigurationContract_1.test](tests/module/Ieee80211HeConfigurationContract_1.test)
- [Ieee80211SharedMacModes_1.test](tests/module/Ieee80211SharedMacModes_1.test)

## Stage 2 — Establish canonical PHY value authorities

These are pure deterministic PHY services, not NED modules.

### WP2.1: Introduce `Ieee80211ModeKey`

The key should encode all identity-relevant values:

- PHY family and preamble format.
- MCS.
- NSS or space-time streams.
- Bandwidth.
- Guard interval.
- Coding.
- Any center-frequency mode that affects identity.

Build immutable indexes when `Ieee80211ModeSet` is constructed. Preserve existing pointer-based APIs as adapters while consumers migrate.

Add explicit tests for the current LDPC/BCC equivalence rules in [Ieee80211ModeSet.cc](src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.cc:1294). Key construction must not silently merge modes with different airtime or legality.

Split into two patches:

1. Add keys and indexes without changing callers.
2. Migrate lookup callers and remove repeated family scans.

### WP2.2: Introduce a timing profile

Add a value such as `Ieee80211Timing` containing:

- SIFS.
- Slot time.
- PHY RX-start delay.
- Response timeout derivation inputs.
- Any family/band-specific rounding policy.

First validate the current invariant that all relevant entries agree. Current accessors take timing from `entries[0]` ([Ieee80211ModeSet.h](src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h:155)); replace that implicit convention with an explicitly constructed profile.

Separate:

1. Profile creation and current-value equivalence.
2. HCF/frame-sequence caller migration.
3. Any intentional timing correction, which requires normative evidence and separate fingerprint review.

### WP2.3: Canonicalize RU identity and encoding

Retain [Ieee80211HeRu](src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h:32) as the PHY geometry authority.

Add:

- A logical RU key independent of mutable scheduler objects.
- A cached, read-only catalog per bandwidth.
- Direct lookup by logical key.
- `Ieee80211HeRuAllocationEncoding` for HE-SIG/Trigger grouping and ordering.

The encoding service must not select users. It transforms a validated allocation into its canonical wire representation.

Migrate consumers in this order:

1. Plan validation.
2. TX-vector creation.
3. PHY header serializer.
4. MAC Trigger serializer.
5. Receiver user/RU selection.

### Stage 2 exit criteria

- Mode lookup has one canonical identity path.
- Timing no longer depends on arbitrary vector position.
- RU validation and encoding use one canonical catalog and ordering.
- All existing valid profiles produce identical rates and durations.
- Golden serialization remains byte-identical.

Focused gates include [Ieee80211McsCoverage_1.test](tests/unit/Ieee80211McsCoverage_1.test), [Ieee80211AxModeProfile_1.test](tests/unit/Ieee80211AxModeProfile_1.test), [Ieee80211HeFixesBandSifsInvalidMcs_1.test](tests/unit/Ieee80211HeFixesBandSifsInvalidMcs_1.test), [Ieee80211HeRu_1.test](tests/unit/Ieee80211HeRu_1.test), and [Ieee80211HeMuPhyHeaderSerializer_1.test](tests/unit/Ieee80211HeMuPhyHeaderSerializer_1.test).

## Stage 3 — Complete immutable HE planning transactions

Extend the current plans instead of replacing them.

### WP3.1: Separate planning inputs from live state

Introduce copied planning snapshots containing:

- Peer/address and association epoch.
- Queue generation and scalar backlog information.
- BA eligibility/available-slot snapshot.
- Capability snapshot.
- Link estimate and freshness.
- PHY profile generation.
- TXOP budget.

Schedulers must receive copied planning inputs. They must not query MIB, queues, BA handlers, radio modules or CSI managers during `schedule()`.

### WP3.2: Make the DL plan genuinely immutable

[HeDlMuPlan](src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuPlan.h:52) currently copies queue and CSI handles through its schedule context. Replace them with stable candidate identifiers and copied decision inputs.

Separate:

- `HeDlMuPlanningSnapshot`: immutable input facts.
- `HeDlMuPlan`: user/resource/mode decision.
- `HeDlMuCommitContext`: live services used only during revalidation and commit.

### WP3.3: Consolidate UL finalization

The scheduler should return allocation intent, not authoritative PHY-derived timing fields.

Add a PHY-owned `Ieee80211HePpduFinalizer` that consumes a validated plan and produces:

- Canonical TX-vector.
- PPDU layout.
- UL Length/common duration.
- Padding, PE and LTF results.
- Structured rejection diagnostics.

Migrate DL, UL, sounding and Trigger-response callers separately. Remove duplicate finalization only after all callers use the new service.

### WP3.4: Add reserve/commit/rollback

Commit protocol:

1. Verify association, capability, PHY, queue and BA generations.
2. Revalidate head packets and available BA slots.
3. Reserve selected packets without removing them.
4. Finalize exact PSDU lengths.
5. Construct all required packets and tags.
6. Commit queue removal and protocol state atomically.
7. Roll back every reservation on failure.

A failed plan must not consume a sequence number, retry counter, queue packet, BA slot or scheduler service history.

### WP3.5: Remove concrete PHY discovery

Replace the concrete adapter in [HeHcf.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc:77) with the existing owner-neutral link/PHY contract. The adapter may discover concrete modules during construction, but plan creation must consume only its typed snapshots.

### Stage 3 exit criteria

- Scheduler selects; PHY validates and finalizes.
- Plans contain no queue, timer, manager or MIB pointer.
- Stale plans are rejected before mutation.
- Failed commits are observably neutral.
- Existing scheduler NED types remain source-compatible.

Focused gates:

- [Ieee80211HeSchedulerValidation_1.test](tests/unit/Ieee80211HeSchedulerValidation_1.test)
- [Ieee80211HeDlMuTransaction_1.test](tests/unit/Ieee80211HeDlMuTransaction_1.test)
- [Ieee80211HeUlMuTransaction_1.test](tests/unit/Ieee80211HeUlMuTransaction_1.test)
- [Ieee80211HeUlControlFrames_1.test](tests/unit/Ieee80211HeUlControlFrames_1.test)
- [Ieee80211HeDlMuExchange_1.test](tests/module/Ieee80211HeDlMuExchange_1.test)
- [Ieee80211HeUlTriggerExchange_1.test](tests/module/Ieee80211HeUlTriggerExchange_1.test)

## Stage 4 — Decompose HCF around one exchange owner

Keep `Hcf` as the simple-module/NED façade and callback bridge during migration.

### WP4.1: Introduce an explicit exchange state model

Add `HcfExchangeCoordinator` with states covering:

- Idle.
- Awaiting channel grant.
- Preparing.
- Transmitting.
- Awaiting response.
- Retrying or recovering.
- Completing or aborting.

It becomes the single owner of:

- Active frame sequence.
- Expected-response identity.
- Response and inactivity timers.
- Completion/abort transition.
- TXOP continuation decision.

Initially, every method delegates to existing `Hcf` behavior. This first patch must be structurally neutral.

### WP4.2–WP4.5: Extract responsibilities one at a time

Suggested pure collaborators:

- `HcfFramePreparation`
- `HcfResponseService`
- `HcfAggregationService`
- `HcfRetryService`

Migration order:

1. Response classification and dispatch.
2. Frame preparation.
3. Aggregation construction and constituent bookkeeping.
4. Retry/recovery decisions.

Keep signals with the authoritative owner. Same-instant calls remain direct C++ calls; do not introduce zero-time messages.

Only introduce NED policy slots if a responsibility later needs an independently configurable implementation.

### Stage 4 exit criteria

- One exchange coordinator owns every exchange timer and transition.
- No callback can complete an exchange twice.
- Packet identity survives aggregation and retry.
- TXOP and EDCA ownership are unchanged.
- No additional simulation events are introduced.

Focused gates include [Ieee80211SharedMacModes_1.test](tests/module/Ieee80211SharedMacModes_1.test), [Ieee80211FailedBarRecovery_1.test](tests/module/Ieee80211FailedBarRecovery_1.test), retransmission module tests, and the HE DL/UL exchange tests.

## Stage 5 — Consolidate Block Ack ownership

Do not create replacement originator/recipient procedures; strengthen the existing contracts.

### WP5.1: Add explicit keys and snapshots

Introduce stable peer/TID agreement keys and immutable snapshots containing:

- Agreement state.
- Window start and size.
- Occupied or acknowledged sequence positions.
- Retry eligibility.
- Agreement generation and association epoch.

Expose queries such as “available originator slots” rather than returning mutable agreement objects.

### WP5.2: Clarify recipient ownership

`BlockAckReordering` currently accepts mutable `RecipientBlockAckAgreement *` values ([BlockAckReordering.h](src/inet/linklayer/ieee80211/mac/blockackreordering/BlockAckReordering.h:46)).

Replace that with a narrow contract:

- Recipient agreement owner supplies agreement/window state.
- Reordering owns buffered MPDUs and fragment occupancy.
- Window advancement occurs through one explicit operation.
- DELBA and reassociation tear down both states atomically.

### WP5.3: Migrate HCF and HE packing

Replace direct agreement and ACK-handler scans with snapshots and owner operations. Revalidate the snapshot generation at packing commit.

### WP5.4: Migrate variants incrementally

Recommended order:

1. Compressed BA.
2. Basic BA and fragmentation.
3. HT implicit BA.
4. Multi-TID BA.
5. HE Multi-STA BA.
6. Failure, timeout and DELBA paths.

### Stage 5 exit criteria

- Exactly one owner for agreement, originator outstanding state and recipient reorder state.
- HCF holds no shadow window.
- Wrap-around uses shared cyclic sequence operations.
- Reassociation invalidates every old peer/TID state.
- Multicast and broadcast never enter BA procedures.

Focused gates include [Ieee80211HeBlockAckWindow_1.test](tests/unit/Ieee80211HeBlockAckWindow_1.test), [Ieee80211HtMultiTidBlockAckReordering_1.test](tests/unit/Ieee80211HtMultiTidBlockAckReordering_1.test), [Ieee80211MultiTidBlockAck_1.test](tests/unit/Ieee80211MultiTidBlockAck_1.test), and [Ieee80211FailedBarRecovery_1.test](tests/module/Ieee80211FailedBarRecovery_1.test).

## Stage 6 — Optimize only demonstrated hotspots

Each optimization requires an isolated before/after benchmark and unchanged semantic output.

### Candidate optimizations

1. Use Stage 2 mode indexes to eliminate catalog scans and `dynamic_cast` lookup loops.
2. Reuse immutable RU catalogs and precomputed logical grouping views.
3. Make A-MPDU packing incremental:
   - Running aligned PSDU length.
   - Running per-user duration/capacity.
   - Per-TID selected count.
   - No reconstruction of the candidate vector for every packet.
4. Use TID-indexed BA occupancy maps and O(1) availability queries.
5. Cache capability/legality results using complete generation-bearing keys.
6. Apply receiver interval pruning only after the corresponding receiver policy has been extracted in Stage 7; do not optimize entangled receiver logic first.

### Performance acceptance

After Stage 0 calibrates host noise:

- No median CPU regression greater than 5%.
- No p95 regression greater than 10%.
- No peak-RSS regression greater than 10%.
- An improvement claim requires at least a 10% median reduction and more than twice measured baseline variation.
- Semantic counters, packet bytes, selected users/RUs and fingerprints must remain identical.

If a proposed cache does not produce a measurable end-to-end gain, do not merge it merely because its local lookup is faster.

## Stage 7 — Isolate receiver, serializers, management and observation

Treat every subsection as a separate patch series.

### WP7.1: Split serializer helpers

Keep current registered serializers as dispatch façades. Extract pure codecs:

- `Ieee80211BlockAckCodec`
- `Ieee80211HeMacCodec`
- `Ieee80211HePhyHeaderCodec`
- Family-specific HT/VHT/EHT helpers where justified

Do not register competing serializers for the same chunk unless the registry contract supports it. Require golden serialize/deserialize/re-serialize identity.

### WP7.2: Extract receiver policies

Keep `Ieee80211Receiver` as the NED-facing receiver and result/signal owner.

Extract pure services:

- `Ieee80211MuReceptionSelector`
- `Ieee80211HeSpatialReusePolicy`
- `Ieee80211MpduReceptionOutcomePolicy`
- A separate VHT selector if its semantics remain materially distinct

Return immutable decision records. The receiver emits the existing signals once and creates the authoritative `IReceptionResult`.

After extraction, apply subcarrier interval pruning as an independently benchmarked optimization.

### WP7.3: Split STA management controllers

Keep `Ieee80211MgmtSta` as the command, lifecycle and NED façade. Add internal controllers:

- `Ieee80211StaScanController`
- `Ieee80211StaAuthenticationController`
- `Ieee80211StaAssociationController`
- `Ieee80211BssDescriptionSnapshot`

Migrate scan, authentication and association separately. One coordinator must own the AP list, current association and timer dispatch; individual controllers must not retain competing mutable copies.

### WP7.4: Add immutable capture projection

Move frame/radio metadata extraction into an immutable projection consumed by [Ieee80211RadiotapPcapCaptureAdapter.cc](src/inet/linklayer/ieee80211/pcap/Ieee80211RadiotapPcapCaptureAdapter.cc:1064).

The projection must:

- Be created from public packet/transmission information.
- Contain no protocol-state pointers.
- Be read-only.
- Have no callback into MAC or PHY decisions.
- Preserve current PCAP and Radiotap bytes.

### WP7.5: Remove `MACArrive`

Reconfirm absence of non-generated references, then remove it from [Ieee80211Frame.msg](src/inet/linklayer/ieee80211/mac/Ieee80211Frame.msg:135).

Do not edit generated `_m.h` or `_m.cc` manually. Regenerate through the normal build, rebuild INET and verify:

- Generated API removal.
- Parsim compatibility decision.
- Unchanged serialized frame bytes.
- Unchanged PCAP output.
- Expected fingerprint impact.

This is an independent schema/API patch, not part of the management migration.

## Verification commands and stage gates

From the repository root, after compiled-source changes:

```sh
make MODE=release -j$(nproc)

inet_run_unit_tests \
  -m release \
  -f '(Ieee80211He|Ieee80211HtMultiTid|Ieee80211MultiTid|HeDlScheduler|HeUlScheduler).*\.test'
```

Use one narrower filter during development. Run module tests serially for deterministic diagnosis:

```sh
inet_run_module_tests \
  -m release \
  --no-build \
  --no-concurrent \
  -f 'Ieee80211(SharedMacModes|HeConfigurationContract|HeUlTriggerExchange|HeDlMuExchange|FailedBarRecovery)_1.*'
```

For every stage:

1. Run the smallest focused test twice from the same release build.
2. Run one relevant legacy and one HE module test.
3. Run affected HE fingerprint rows.
4. Explain the first fingerprint divergence before any baseline discussion.
5. Do not update fingerprint CSV files without explicit approval.
6. Run focused architecture checks on both WLAN roots.
7. Apply the complete general and WLAN semantic review checklists.

Recommended deterministic example confirmations are:

- [UL OFDMA](examples/ieee80211ax/ul_ofdma/omnetpp.ini), `BacklogBased5ms`, run 0.
- [DL OFDMA scheduling](examples/ieee80211ax/dl_ofdma_sched/omnetpp.ini), `EqualSizedRUs_fBW`, run 0.
- [HT Block Ack](examples/ieee80211n/block_ack/omnetpp.ini), `FragBasicBlockAck` and `CompressedBlockAck`.

Run one configuration and run number at a time and preserve the exact command, seed, working directory, exit status and artifacts.

## Final completion criteria

The program is complete when:

- MIB state is private and owned through explicit operations.
- Snapshots carry sufficient generations to reject stale plans and caches.
- PHY mode, timing and RU identity each have one canonical representation.
- DL and UL plans contain no live mutable aliases.
- Plan commit is atomic and rollback-safe.
- One HCF exchange coordinator owns timers and completion.
- BA agreement, outstanding and reorder state have unambiguous owners.
- Optimizations are supported by repeatable measurements.
- Receiver, serializer, management and capture responsibilities are isolated without changing their public configuration.
- Relevant focused, legacy, HE and fingerprint gates pass.
- Architecture checks and both semantic review checklists pass with no unresolved finding.

No repository files were changed for this planning task.