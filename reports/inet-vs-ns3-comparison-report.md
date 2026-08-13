# INET versus ns-3 Wi-Fi implementation comparison

## Scope and basis

This report compares INET commit `9e61cec7239b57297d8c4df9cd508f4702a8948c` with clean ns-3 at `5e35cfbc28bd`. The INET revision removes the generic HCF outer transaction shell while retaining typed amendment-local preparation, rollback, and exchange correlation. The ns-3-dev checkout and comparison baseline are unchanged.

The comparison represented here is a static source, documentation and test audit; no simulations were run for it. LOC depends heavily on whether generated code and comments are counted:

- **Physical** means every line in the file.
- **Source** means nonblank, noncomment lines; preprocessor directives count as source.
- INET generated `_m.cc` and `_m.h` files are excluded unless explicitly stated.

## Overall assessment

INET is architecturally more explicit and extensible; ns-3 is locally easier to read and has the more mature conventional C++ test infrastructure. Both implementations are complex and intentionally incomplete models, rather than fully conformant 802.11 implementations.

| Dimension | INET | ns-3 | Assessment |
|---|---|---|---|
| Code organization | Strong contracts, NED composition, typed packets, separated policies and services; direct SU paths now stay with frame-sequence owners | Clear amendment directories and HT→VHT→HE class hierarchy | Slight INET advantage |
| Readability | Excellent system-level documentation, but many abstractions and cross-file hops | More familiar C++ flow and consistent Doxygen; large methods remain difficult | ns-3 advantage |
| Complexity | Distributed across numerous services, contracts, NED types and generated code | Concentrated in large inheritance-based coordinators with broad mutable state | Equally high, but with different shapes |
| Maintainability | Better amendment and policy substitution, with clearer ownership boundaries | Easier onboarding, but inheritance and shared cross-amendment classes increase coupling | Slight INET advantage for long-term extension |
| Testing | Strong standards traceability, focused tests and fingerprints; uneven test style | Very broad typed test matrices and boundary tests; large test files | ns-3 broader; INET more traceable |
| Standard coverage | Broader modeled surface in several n/ac/ax areas | Strong core HT/VHT/HE and OFDMA, but more explicit omissions | INET advantage |
| Full compliance | No | No | Neither should claim conformance |

The detailed evidence and the resulting design recommendation follow below.

## Code quality and architecture

INET’s strongest design choice is explicit composition. The HCF is assembled from replaceable EDCA, rate-selection, acknowledgment, protection and Block Ack policies in [Hcf.ned](src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.ned:99). Amendment behavior is exposed through typed runtimes, immutable snapshots, amendment-owned plans, validation, and local commit/rollback boundaries, as described in [ch-80211.rst](doc/src/developers-guide/ch-80211.rst:20). The latest HCF change removes the generic selector and outer transaction protocol: ordinary SU, release, and sounding remain with their frame-sequence owners, while VHT and HE runtimes commit only the typed starts that need preparation or reservation. This makes the ownership boundary more explicit and easier to follow locally.

The cost is a large conceptual surface: C++, NED, MSG-generated classes, contracts, callbacks, services and configuration all participate. [Hcf.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc) is now 2,870 physical lines, while `HeTriggeredUlExchangeService.cc` and `Ieee80211HePhyCalculator.cc` remain around 1,570. Dynamic casts and module-path resolution also weaken some nominal interfaces. INET is architecturally clean at the large scale, but it is not always easy to follow locally; removing the generic outer shell eliminates one particularly indirect path for ordinary SU traffic.

ns-3 uses a more conventional progression:

```text
FrameExchangeManager
  → QosFrameExchangeManager
    → HtFrameExchangeManager
      → VhtFrameExchangeManager
        → HeFrameExchangeManager
```

The equivalent PHY and PPDU types follow the same amendment inheritance. This is immediately understandable and avoids duplication; the rationale is documented in [wifi-design.rst](../ns-3-dev/src/wifi/doc/source/wifi-design.rst:1008). `TypeId`, `ObjectFactory`, configuration objects, and replaceable schedulers and managers provide useful extension points.

The weakness is scale pressure on the hierarchy. `HtFrameExchangeManager` is about 2,237 lines and `HeFrameExchangeManager` about 2,864. The HE manager owns PSDU maps, transmission parameters, triggers, scheduler state, and several timers and events in [he-frame-exchange-manager.h](../ns-3-dev/src/wifi/model/he/he-frame-exchange-manager.h:344). Amendment-specific branching accumulates inside these central owners. Shared types such as `WifiTxVector`, `WifiRemoteStationManager`, and Minstrel-HT also couple several generations.

The resulting readability distinction is:

- For understanding one execution path, ns-3 is usually easier.
- For understanding ownership, substitution, and architectural intent, INET is better documented and more explicit.
- Both contain coordinator classes that are too large for comfortable maintenance.

## Size and distribution

### Overall MAC and PHY size

| Implementation area | Files | Physical LOC | Source LOC |
|---|---:|---:|---:|
| INET MAC core | 531 | 57,352 | 44,905 |
| INET management + MIB | 48 | 10,939 | 8,742 |
| INET other link-layer Wi-Fi | 30 | 2,950 | 2,322 |
| **INET complete Wi-Fi link layer** | **609** | **71,241** | **55,969** |
| **INET PHY** | **178** | **32,381** | **26,054** |
| **INET handwritten total** | **791** | **103,622** | **82,023** |
| ns-3 MAC-classified model | 117 | 60,364 | 38,963 |
| ns-3 PHY-classified model | 77 | 31,708 | 20,416 |
| ns-3 shared Wi-Fi core | 145 | 42,968 | 27,587 |
| **ns-3 `src/wifi/model`** | **339** | **135,040** | **86,966** |
| ns-3 Wi-Fi helpers | 20 | 8,119 | 5,124 |
| **ns-3 model + helpers** | **359** | **143,159** | **92,090** |

The ns-3 MAC/PHY split is necessarily approximate: types such as `WifiTxVector`, common headers, and packet utilities serve both layers, so a separate shared-core category is retained.

INET additionally contains about **83,651 physical LOC of generated `_m.cc` and `_m.h` code** across the complete Wi-Fi link layer and PHY. Including that code would inflate INET to roughly 188K physical LOC without representing handwritten maintenance effort.

Excluding generated code, ns-3 is about **12% larger in source LOC overall**: approximately **92K versus INET’s 82K**. The PHY totals are close, while ns-3 carries more shared infrastructure and rate-control code; INET carries more explicit coordination, service, and declarative NED/MSG structure.

### Current HCF files

These counts describe INET commit `9e61cec7239b57297d8c4df9cd508f4702a8948c`:

| File | Physical | Source |
|---|---:|---:|
| [Hcf.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc) | 2,870 | 2,573 |
| [Hcf.h](src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h) | 472 | 389 |
| [HcfContext.h](src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfContext.h) | 314 | 264 |
| [HcfExchangeEngine.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangeEngine.cc) | 334 | 288 |
| [HcfExchangeEngine.h](src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangeEngine.h) | 131 | 103 |
| [HcfFeatureSet.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfFeatureSet.cc) | 22 | 12 |
| [HcfFeatureSet.h](src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfFeatureSet.h) | 72 | 53 |
| [HcfRetryService.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfRetryService.cc) | 102 | 89 |
| [HcfRetryService.h](src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfRetryService.h) | 57 | 37 |
| **Combined** | **4,374** | **3,808** |

`Hcf.cc` alone contains about **6% of the handwritten INET MAC-core source LOC**.

### Significant MAC subsystems

| Concern | INET physical/source | ns-3 physical/source |
|---|---:|---:|
| Coordination/frame exchange | `coordinationfunction/`: 20,547 / 17,489 | FEM hierarchy through HE: 9,725 / 6,555 |
| Channel access and contention/TXOP | 1,667 / 1,251 | 4,862 / 2,982 |
| Aggregation | 753 / 481 | Included below |
| Aggregation + Block Ack | 3,128 / 2,227 | 4,625 / 2,692 |
| Rate selection/control | 2,737 / 2,067 | 11,317 / 8,231 |
| Management + peer state | 9,685 / 7,921 C++ LOC | 15,864 / 10,220 |
| HE triggered-UL service | 2,022 / 1,845 | HE MU scheduler classes: 2,196 / 1,485 |

The coordination row captures the architectural difference:

- INET spreads frame-exchange behavior among HCF services, typed amendment runtimes, local preparation/rollback helpers, and frame-sequence classes; common SU/release/sounding paths are dispatched directly and no longer bypass a provider selector because no generic selector remains.
- ns-3 concentrates more of it in the Frame Exchange Manager inheritance chain.
- ns-3 has substantially more rate-control code, largely due to its larger set of algorithms.

### Significant PHY subsystems

| Concern | INET physical/source |
|---|---:|
| Mode catalog and timing | 10,306 / 8,234 |
| Packet-level PHY | 16,888 / 14,037 |
| Bit-level PHY | 3,473 / 2,443 |
| Channel models | 1,593 / 1,266 |
| **Complete PHY** | **32,381 / 26,054** |

Representative cross-project groupings are:

| Concern | INET physical/source | ns-3 physical/source |
|---|---:|---:|
| Central mode/PHY authority | `Ieee80211ModeSet.{cc,h}`: 1,931 / 1,795 | `WifiPhy.{cc,h}`: 4,130 / 2,483 |
| HE calculations/entity | `Ieee80211HePhyCalculator.{cc,h}`: 2,319 / 1,977 | `HePhy.{cc,h}`: 2,547 / 1,811 |
| Radio, receiver, and transmitter | 3,455 / 2,990 | `PhyEntity` + interference: 3,871 / 2,359 |
| Amendment PHY + PPDU classes | Distributed across INET mode/packet-level trees | HT/VHT/HE: 7,240 / 4,836 |

### Largest handwritten files

#### INET

| File | Physical | Source |
|---|---:|---:|
| `Hcf.cc` | 2,870 | 2,573 |
| `Ieee80211MacHeaderSerializer.cc` | 1,746 | 1,637 |
| `Ieee80211MgmtFrameSerializer.cc` | 1,894 | 1,617 |
| `Ieee80211ModeSet.cc` | 1,696 | 1,601 |
| `HeTriggeredUlExchangeService.cc` | 1,570 | 1,447 |
| `Ieee80211HeSigCodec.cc` | 1,522 | 1,436 |
| `Ieee80211HePhyCalculator.cc` | 1,579 | 1,432 |
| `Ieee80211PhyHeaderSerializer.cc` | 1,410 | 1,261 |
| `Ieee80211Radio.cc` | 1,316 | 1,192 |
| `Ieee80211HeTxVector.h` | 1,325 | 1,139 |

#### ns-3

| File | Physical | Source |
|---|---:|---:|
| `ap-wifi-mac.cc` | 3,290 | 2,621 |
| `wifi-mac.cc` | 2,713 | 2,301 |
| `ctrl-headers.cc` | 2,630 | 2,247 |
| `he-frame-exchange-manager.cc` | 2,864 | 2,172 |
| `wifi-remote-station-manager.cc` | 2,565 | 2,163 |
| `wifi-phy.cc` | 2,415 | 2,116 |
| `sta-wifi-mac.cc` | 2,391 | 1,871 |
| `ht-frame-exchange-manager.cc` | 2,237 | 1,755 |
| `minstrel-ht-wifi-manager.cc` | 2,208 | 1,744 |
| `he-phy.cc` | 1,913 | 1,622 |

## Testing and maintenance safety

ns-3 has exceptionally broad behavioral matrices: 49 Wi-Fi test source files and hundreds of registered test cases. Particularly strong areas include sequence-number wraparound and Block Ack boundaries in [block-ack-test-suite.cc](../ns-3-dev/src/wifi/test/block-ack-test-suite.cc:82), aggregation across HT/VHT/HE in [wifi-aggregation-test.cc](../ns-3-dev/src/wifi/test/wifi-aggregation-test.cc:50), and OFDMA matrices in [wifi-mac-ofdma-test.cc](../ns-3-dev/src/wifi/test/wifi-mac-ofdma-test.cc:2406). The typed `TestCase` style is generally less fragile than parallel INET INI parameter vectors.

INET has stronger direct standards traceability in selected tests. [Ieee80211McsCoverage_1.test](tests/unit/Ieee80211McsCoverage_1.test:29) cites current tables and checks width, GI, NSS, and MCS combinations. [Ieee80211OnWireBitCompliance_1.test](tests/unit/Ieee80211OnWireBitCompliance_1.test:30) uses byte-exact fixtures. HE DL/UL MU tests exercise timeouts, partial responses, and invalid exchanges. INET also has HE trajectory fingerprints in [ieee80211-he.csv](tests/fingerprint/ieee80211-he.csv:1).

Both have test-maintenance problems:

- INET contains very large `.test` files, hard-coded module paths, positional configuration matrices, and some older stdout-comparison tests.
- ns-3 has test files exceeding 4,000–6,000 lines and requires manual CMake registration.
- ns-3 explicitly states that its n/ac/ax error-rate curves still lack validation in [wifi-testing.rst](../ns-3-dev/src/wifi/doc/source/wifi-testing.rst:55).
- Neither checkout provides comprehensive external validation for every n/ac/ax MAC and PHY behavior.

## Standards coverage and compliance

The audit compares representative behavior against IEEE 802.11-2024 clauses covering capability declaration, Block Ack, EDCA TXOPs, HE rate selection, and MU transmission.

INET has the broader, more directly traceable modeled surface:

- HT/VHT capability state is carried through management and retained per peer.
- Basic, compressed, Multi-TID, and Multi-STA Block Ack representations and procedures exist. The optional historical HT Multi-TID path is explicitly labeled an experimental legacy extension, so it should not be confused with current normative operation.
- HE DL/UL OFDMA, trigger-based uplink, and configurable multi-user schedulers are explicitly gated in [HeHcf.ned](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.ned:29).
- PHY modes and TXVECTOR validation act as central authorities for legality and timing.
- VHT sounding and DL MU-MIMO exist, though only as a deliberately narrow subset documented in [ch-80211.rst](doc/src/users-guide/ch-80211.rst:176).

ns-3 implements strong core HT/VHT/HE operation and substantial OFDMA behavior, but its checked-in documentation and source identify important gaps:

- No n/ac/ax beamforming, RIFS, HCF/HCCA, authentication, or encryption.
- Preamble puncturing is PHY-only and not exploited by the MAC.
- MU-MIMO is minimal; mixed OFDMA/MU-MIMO is rejected.
- Basic and Multi-TID Block Ack paths are unsupported.
- HE capability serialization omits variable MCS/NSS forms and PPE thresholds in [he-capabilities.cc](../ns-3-dev/src/wifi/model/he/he-capabilities.cc:138).
- Several known Wi-Fi correctness issues are listed in [wifi-design.rst](../ns-3-dev/src/wifi/doc/source/wifi-design.rst:197).

INET is not complete either. Its documentation and source explicitly identify HE-LTF, synchronization, SIG decoding, VHT MU, coding, and other packet-level approximations. These prevent any claim of waveform-level or complete IEEE conformance.

The defensible standards conclusion is therefore:

- **INET currently has better standards traceability and somewhat broader n/ac/ax feature coverage.**
- **ns-3 has stronger broad behavioral test matrices and a more mature conventional implementation style.**
- **Neither is fully standards compliant; both are packet-level research models with documented abstraction boundaries.**

## Transactional processing

### Is ns-3 transactional?

No. The ns-3 Wi-Fi implementation has some transactional-like operations, but it is not transactional in the same architectural sense as INET’s amendment-local preparation and reservation paths. The current INET code no longer has a generic HCF selector, outer transaction plan, or transaction-owned exchange engine.

| Property | INET HCF | ns-3 Wi-Fi |
|---|---|---|
| Selection and preparation | Typed VHT/HE grant snapshots and amendment-owned prepared starts; common SU/release paths dispatch directly | Scheduler stores mutable selection state |
| Prepared plan | `VhtGrantSnapshot`, `HeDlMuPlan`, and typed UL/DL prepared starts, only where the amendment owns staged state | `DlMuInfo`/`UlMuInfo` and `WifiTxParameters`, but no generic plan contract |
| Explicit validation | Local provider, PHY, duration, queue, and frame-sequence checks; the engine validates its callback bundle | Distributed assertions and validity checks |
| Explicit commit | Typed `commitPreparedGrant()` and `commitStart()` calls; direct paths start their sequence directly | Effectively begins when the FEM accepts or moves scheduler output |
| Generic rollback | None; VHT dialog-token, HE reservation/protection/queue, and aggregate handoff use narrower local guards | No; only local undo and recovery mechanisms |
| Exchange identity | Monotonic HCF engine generations for timer/sequence state plus VHT/HE amendment-local IDs or tokens for staged state | No equivalent generic exchange identity |
| Terminal result | Typed amendment callbacks plus frame-sequence completion/abort; no generic selector terminal callback | Distributed success and timeout callbacks |
| Stale callback rejection | Owner-level VHT/HE identities/tokens and HCF engine generation checks | Timer and current-state checks, but no transaction identity |

INET now separates common exchange execution from amendment-owned preparation. The common path is:

```text
channel grant → typed grant preparation → typed commit/direct start → frame sequence → exchange engine
```

The composition-only [HcfFeatureSet](src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfFeatureSet.h:17) exposes amendment services but does not select or commit exchanges. `Hcf` dispatches ordinary single-user, release, and HT sounding directly to their frame-sequence owners. The private VHT runtime captures a [VhtGrantSnapshot](src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc:122) and either starts sounding, starts common SU, or calls `VhtHcfFeature::commitPreparedGrant()` for group management, Block Ack prerequisites, and DL MU. The HE runtime performs the analogous typed dispatch in [HeHcf.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc:313).

The retained safeguards are local to the owner that actually stages state. VHT uses a grant snapshot and a narrow rollback guard for speculative dialog-token/protection changes; its DL-MU path carries an amendment-local exchange ID through typed callbacks. HE’s [HeDlMuExchangeProvider](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeDlMuExchangeProvider.h:22) owns packet reservations, protection snapshots, a transaction token, and rollback/finalization. HE UL trigger preparation is committed by its own service. These are state-consistency and stale-callback safeguards in a single-threaded event loop, not locks or memory-safety machinery for concurrent execution.

The removed generic completeness, duplicate-candidate, future-enqueue, provider-selection, and terminal-ownership checks belonged to the discarded outer protocol. They were not needed to serialize execution in INET’s single-threaded event loop. Protocol legality, ownership, stale-callback, and exception-safety checks remain at the HCF engine and amendment-service boundaries where they protect an actual invariant.

The HCF engine itself is now deliberately small. [HcfExchangeEngine](src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangeEngine.h:18) owns the active frame sequence, response timers, monotonic exchange generations, and completion/abort handling. Its action validation checks that the call-scoped HCF callbacks are complete; it no longer requires transaction-owner or terminal-sink callbacks. Direct completion therefore stays within the frame-sequence/engine path.

In ns-3, `MultiUserScheduler::NotifyAccessGranted()` immediately updates scheduler members and saves computed MU information in `m_lastTxInfo` in [multi-user-scheduler.cc](../../ns-3-dev/src/wifi/model/he/multi-user-scheduler.cc:235). `HeFrameExchangeManager` then obtains mutable scheduler output and directly invokes `SendPsduMapWithProtection()` in [he-frame-exchange-manager.cc](../../ns-3-dev/src/wifi/model/he/he-frame-exchange-manager.cc:127). That method moves the PSDU map and transmission parameters into live FEM state immediately at lines 191–197. There is no intervening generic transaction object, reservation identity, or commit guard.

ns-3 does have local transactional-like behavior:

- Aggregation tentatively modifies `WifiTxParameters` and calls `UndoAddMpdu()` when size or timing validation fails in [ht-frame-exchange-manager.cc](../../ns-3-dev/src/wifi/model/ht/ht-frame-exchange-manager.cc:1522).
- MPDUs normally remain queued while marked in flight and are removed after acknowledgment.
- Timeout paths reset in-flight state, set retry flags, and update Block Ack state in [he-frame-exchange-manager.cc](../../ns-3-dev/src/wifi/model/he/he-frame-exchange-manager.cc:1433).
- Success and failure callbacks provide protocol-level recovery.

Those mechanisms protect individual operations, but they do not form one atomic transaction spanning scheduling, queue reservation, PHY validation, ownership transfer, and terminal completion. Current INET makes the same distinction without a generic outer protocol: each amendment service owns only the preparation, reservation, rollback, and callback correlation needed by its own exchange.

One nuance is important: neither implementation can roll back an already transmitted on-air exchange. INET’s local rollback applies before ownership transfer or when a typed commit fails; after a frame sequence starts, the owning service and exchange engine apply normal completion, timeout, retry, and recovery logic.

The precise architectural verdict is:

- **INET:** typed amendment-local preparation and rollback mechanisms, plus a shared direct frame-sequence engine; no generic HCF transaction protocol.
- **ns-3:** an imperative state machine with speculative local undo and callback-based recovery.
- **Not equivalent**, although both preserve selected safety properties through local mechanisms.

This is an architectural choice, not an IEEE compliance requirement.

## Complexity trade-off and recommended boundary

The generic HCF transaction shell was overextended for a single-threaded event-driven simulator. The latest commit removes that shell and leaves each amendment service responsible for the state it actually stages: direct frame-sequence ownership for ordinary exchanges and typed preparation for prepared multi-owner exchanges.

### Why the remaining preparation safeguards have value

INET’s remaining preparation safeguards protect logical state consistency, not memory or thread safety. A VHT or HE service can capture a candidate and validate it before consuming packets, sequence numbers, protection state, or scheduler observations. The owner then performs a typed commit:

`typed prepare → owner validation → typed commit → frame sequence`

When the owner has staged state, failure is handled locally before sequence ownership is transferred:

`typed prepare → local rollback/fallback`

This is enforced by the VHT and HE runtime/service contracts, including [HeDlMuExchangeProvider::ReservationRollbackGuard](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeDlMuExchangeProvider.h:26), typed grant commits in [HeHcf.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc:329), and the HCF engine’s generation checks in [HcfExchangeEngine.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfExchangeEngine.cc:70). Direct paths use the same engine without constructing a generic plan or activating a selector.

These guarantees are particularly valuable when a transmission decision spans:

- several per-STA queues;
- RU, MCS, NSS, bandwidth, and duration selection;
- sequence-number and Block Ack state;
- association epochs;
- scheduler accounting;
- callbacks that can arrive after state has changed.

The generic plan and selector tests were removed with the generic shell. Focused coverage remains in [HcfExchangeEngine_1.test](tests/unit/HcfExchangeEngine_1.test:1), [HcfFeatureSet_1.test](tests/unit/HcfFeatureSet_1.test:1), [HeDlMuExchangeProvider_1.test](tests/unit/HeDlMuExchangeProvider_1.test:1), and [Ieee80211HeUlMuTransaction_1.test](tests/unit/Ieee80211HeUlMuTransaction_1.test:1), covering the retained engine, composition, reservation, and HE transaction behavior.

### Why ns-3 remains attractive

ns-3 has a more direct execution model. Its scheduler computes a result, the HE frame-exchange manager consumes it, marks frames in flight, and initiates protection and transmission. See [multi-user-scheduler.cc](../../ns-3-dev/src/wifi/model/he/multi-user-scheduler.cc:235) and [he-frame-exchange-manager.cc](../../ns-3-dev/src/wifi/model/he/he-frame-exchange-manager.cc:127).

It provides narrower safety mechanisms:

- tentative aggregation with local undo;
- queued and in-flight MPDU state;
- retry and Block Ack recovery through callbacks;
- assertions around expected state.

That is easier to trace for a normal single-user exchange. The downside is that the invariants are distributed among the scheduler, aggregators, FEM, queues, and completion callbacks. It does not provide INET’s typed amendment-local reservation and stale-callback safeguards, although INET also uses this direct style for ordinary SU exchanges.

### Where INET becomes overengineered

The remaining warning signs concern the size of the coordination area and the complexity of amendment-local state ownership, not a generic transaction shell that no longer exists:

- INET’s coordination-function area is roughly 17.5K handwritten source LOC, versus about 6.6K for the comparable ns-3 FEM hierarchy through HE. These scopes are not perfectly identical, but the difference is meaningful.
- The retained amendment-local machinery still crosses snapshots, reservations, providers, runtimes, frame sequences, and typed callbacks.
- [Hcf.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc) remains approximately 2.6K non-comment source LOC despite the removed generic shell and extracted services.
- The common path pays for the shared exchange engine, but no longer pays for provider probing, generic plan construction, or selector terminal completion.

The post-refactor design is therefore best characterized as:

> A simpler direct HCF engine with explicit amendment-local safety boundaries; ordinary SU exchanges do not pay for generic transaction selection or terminal bookkeeping.

### Recommended boundary

Keep amendment-local preparation and rollback when an exchange crosses independently owned mutable state:

- HE UL trigger, sounding/recovery, and DL-MU preparation;
- HE prepared single-user starts with per-STA staging or a provider-owned fallback;
- VHT group management, ADDBA prerequisites, and DL-MU preparation;
- multiple queue reservations or per-STA sequence/Block Ack state;
- association-sensitive reservations, scheduler observations, or credits;
- late PHY legality and duration validation when it belongs to a prepared multi-owner plan.

For ordinary SU ACK/RTS/CTS, channel release, HT sounding, VHT SU sounding, and the VHT MU sounding prerequisite, use the direct owner paths now present in the code:

- `HcfFs`, `HtSoundingFs`, or `VhtSoundingFs` started by the owning runtime;
- the shared `HcfExchangeEngine` for timers, retry/recovery, and contention resumption;
- the existing narrow local guard where speculative VHT dialog-token state must be restored;
- ordinary retry and completion handling afterward.

One-queue SU A-MPDU preparation also uses the straight-line `prepareAndTransmit()` handoff in [HcfTransmissionPreparationService.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfTransmissionPreparationService.cc:66). It provides exception-safe cleanup for a temporary aggregate, but it does not pretend that all baseline queue/retry mutations form a generic rollback transaction.

After a typed prepared start commits, neither design can roll back an over-the-air transmission. INET then uses amendment callbacks and the common engine’s completion, timeout, retry, and recovery paths; direct sequences complete through the same engine without generic transaction notification.

IEEE 802.11 specifies externally observable exchanges and timing, not an internal transaction architecture. Transactions therefore do not make INET more standards-compliant by themselves.

The resulting recommendation is now reflected in the implementation: keep ordinary SU paths with their frame-sequence owners, and retain only the typed preparation, reservation, rollback, and correlation needed by VHT/HE exchanges. The ns-3-dev side was not changed; its comparison remains the same imperative, non-generic-transaction model described above.
