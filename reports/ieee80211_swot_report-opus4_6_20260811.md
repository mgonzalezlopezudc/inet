# IEEE 802.11 INET Implementation — SWOT Analysis

> Static code review of the `src/inet/linklayer/ieee80211` and `src/inet/physicallayer/wireless/ieee80211` trees.
> No compilation or simulation was performed.

---

## 1. Codebase Overview

| Metric | Value |
|--------|-------|
| MAC layer C++/H lines | ~110 000 |
| PHY layer C++/H lines | ~58 000 |
| **Total C++/H lines** | **~168 000** |
| MAC `.cc`/`.h` files | 379 |
| MAC `.ned` module files | 102 |
| MAC `.msg` files | 7 |
| Contract interfaces (`I*.h`) | 57 |
| Unit test files | 538 |
| Module test files | 106 |
| Example directories | 6 top-level groups (a/b/g, n, ac, ax, be, wireless) |
| 802.11ax examples | 23 sub-examples, most with `walkthrough.md` |
| Open TODO items in MAC `.cc`/`.h` | 108 |
| `throw cRuntimeError` sites in MAC | 327 |

### Key directories

| Area | Path | Purpose |
|------|------|---------|
| MAC core | [mac/](src/inet/linklayer/ieee80211/mac) | Coordination functions, channel access, contention, frame sequences |
| Coordination | [coordinationfunction/](src/inet/linklayer/ieee80211/mac/coordinationfunction) | DCF, HCF, HeHcf, VhtHcf (61 files) |
| Frame sequences | [framesequence/](src/inet/linklayer/ieee80211/mac/framesequence) | Generic + protocol-specific frame exchange FSMs (56 files) |
| Block Ack | [blockack/](src/inet/linklayer/ieee80211/mac/blockack) | Originator/recipient BA agreements and procedures |
| Aggregation | [aggregation/](src/inet/linklayer/ieee80211/mac/aggregation) | MSDU/MPDU A-MSDU/A-MPDU (17 files) |
| Scheduler | [scheduler/](src/inet/linklayer/ieee80211/mac/scheduler) | HE/EHT DL/UL OFDMA schedulers (29 files) |
| Rate control | [ratecontrol/](src/inet/linklayer/ieee80211/mac/ratecontrol) | AARF, Onoe, Minstrel (HT/VHT/HE) |
| MIB | [mib/](src/inet/linklayer/ieee80211/mib) | Capabilities, association, peer state |
| Management | [mgmt/](src/inet/linklayer/ieee80211/mgmt) | STA/AP association, beacons, scanning |
| TWT | [twt/](src/inet/linklayer/ieee80211/twt) | Target Wake Time (802.11ax power save) |
| PHY modes | [mode/](src/inet/physicallayer/wireless/ieee80211/mode) | DSSS through EHT mode definitions |
| PHY packet | [packetlevel/](src/inet/physicallayer/wireless/ieee80211/packetlevel) | Transmitter, receiver, HE-SIG, HE MU utils |

---

## 2. SWOT Analysis

### 2.1 Strengths 💪

#### S1 — Exceptional standards breadth
The implementation spans **six PHY generations** (DSSS/OFDM, HT, VHT, HE, EHT) and covers features rarely seen in open-source Wi-Fi simulators: DL/UL OFDMA scheduling, HE Trigger-based PPDUs, UORA random access, BSS Coloring, TWT, preamble puncturing, MU-MIMO sounding (HT/VHT/HE), ER SU, NDP feedback reporting, and operating-mode indication. The 802.11be tree already introduces EHT DL OFDMA and MLO stubs. This is among the **most complete open-source 802.11ax/be implementations** available.

#### S2 — Clean contract/interface architecture
The `mac/contract/` directory defines **57 pure-virtual C++ interfaces** (`ICoordinationFunction`, `IChannelAccess`, `IRateControl`, `IFrameSequence`, `IAckHandler`, etc.) paired with `.ned` module interfaces. This allows **pluggable replacement** of MAC sub-components (rate control, aggregation policy, ack policy, recovery procedure, schedulers) without touching the coordination function. The separation mirrors the IEEE standard's architectural decomposition into MLME, MAC sublayer management, and frame-exchange primitives.

#### S3 — Well-factored frame-sequence DSL
The generic frame-sequence classes ([`SequentialFs`](src/inet/linklayer/ieee80211/mac/framesequence/GenericFrameSequences.h), `OptionalFs`, `RepeatingFs`, `AlternativesFs`, `IndexedRepeatingFs`) provide a **declarative DSL** for composing frame exchanges. Protocol-specific sequences (`DcfFs`, `HcfFs`, `HeDlMuTxOpFs`, `HeUlMuTxOpFs`, `HtSoundingFs`, `VhtSoundingFs`, `VhtDlMuTxOpFs`, `EhtDlMuTxOpFs`) are built by composing these primitives, which closely parallels the IEEE standard's notation for frame exchange sequences. This is a genuinely innovative design.

#### S4 — HE/EHT OFDMA scheduling framework
The [scheduler/](src/inet/linklayer/ieee80211/mac/scheduler) directory implements a **pluggable scheduling architecture** for HE/EHT DL and UL multi-user operation. There are concrete schedulers based on equal-sized RUs, backlog-based allocation, and HoL-minimum-delay, all behind clean interfaces (`IIeee80211HeDlScheduler`, `IIeee80211HeUlScheduler`). The separation of scheduling policy from the coordination function is excellent for research extensibility.

#### S5 — Standards-referenced comments
Throughout the codebase, normative behavior is annotated with **IEEE Std 802.11-2024 clause references** (e.g., "IEEE Std 802.11-2024, 10.23.2.2", "IEEE Std 802.11-2024, 26.5.5"). This dramatically improves auditability and makes it possible to trace implementation decisions back to the standard. The [HeHcf.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc) header block at lines 42–70 is exemplary, listing relevant clauses and documenting known deviations.

#### S6 — Comprehensive test and example corpus
- **538 unit tests** and **106 module-level tests** covering HE block-ack windows, multi-TID BA, HE STA-ID, preamble puncturing, PHY calculations, association lifecycle, Minstrel rate control, and more.
- **23 802.11ax examples** most with `walkthrough.md` guides, plus multiple 802.11n, ac, and be example directories.
- Fingerprint regression tests provide behavioral stability across refactors.

#### S7 — Mature contention and backoff model
[`Contention.cc`](src/inet/linklayer/ieee80211/mac/contention/Contention.cc) uses a careful FSM (IDLE → DEFER → IFS_AND_BACKOFF → SUSPENDED) with backoff optimization, EIFS handling, and correct scheduling-priority sequencing for internal-collision detection. The model faithfully implements the EDCA backoff procedure including the subtle channel-busy‐during-backoff transitions.

#### S8 — Multi-generation rate control
The rate-control subsystem provides AARF, Onoe, and **Minstrel** (with HT, VHT, and HE variants), closely following Linux `mac80211` algorithms. The HT Minstrel implementation includes explicit CSI/sounding integration via the `HtCsiCache` and the MCS request/feedback mechanism.

---

### 2.2 Weaknesses 🟡

#### W1 — God-class coordination functions
[`Hcf.cc`](src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc) is **2 302 lines** in a single file. [`HeHcfUl.cc`](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfUl.cc) is **1 989 lines**. Together with [`HeHcf.cc`](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc) (690 lines) and [`HeHcfDl.cc`](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfDl.cc), the `HeHcf` class spans **four implementation files and ~4 700 lines of C++**. `Hcf` inherits from 6 interfaces and owns ~30 member pointers, making it a textbook "God object." The class is simultaneously an `ICoordinationFunction`, `IFrameSequenceHandler::ICallback`, `IChannelAccess::ICallback`, `ITx::ICallback`, `IProcedureCallback`, and `IBlockAckAgreementHandlerCallback`. This impedes comprehension, testing, and safe modification.

#### W2 — 108 open TODO/KLUDGE markers in MAC source
Many are non-trivial, including:
- `// TODO + non-QoS frames` (incomplete QoS classification)
- `// KLUDGE` in `Contention.cc` for scheduling priority
- `// TODO always call processResponse?` in `Hcf::processLowerFrame`
- `details.setLimit(-1); // TODO` in retry-limit-reached paths
- `// TODO review` on `sendDownPendingRadioConfigMsg()` calls
- `// TODO Ieee80211ControlFrame` in originator processing

These indicate **incomplete or acknowledged-but-unaddressed behavior gaps** scattered across critical code paths.

#### W3 — Excessive `dynamic_cast` / `dynamicPtrCast` usage
The coordination functions rely heavily on runtime type checks to classify frames:
```cpp
if (auto dataHeader = dynamicPtrCast<const Ieee80211DataHeader>(header))
    ...
else if (auto mgmtHeader = dynamicPtrCast<const Ieee80211MgmtHeader>(header))
    ...
else // TODO + NonQoSDataFrame
    throw cRuntimeError("Unknown frame");
```
This pattern recurs extensively in `[Hcf.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc)`. Traversing runtime class inheritance structures via RTTI is slow. Since IEEE 802.11 headers explicitly identify frame categories and subtypes via enums (retrieved via `header->getType()`), this should be replaced with constant-time dispatch tables and zero-cost compile-time `static_cast` constructs (see [O2](#o2---introduce-a-frame-type-visitor-or-dispatch-table)).

#### W4 — PCF, MCF, and HCCA are stub implementations
[`Pcf.cc`](src/inet/linklayer/ieee80211/mac/coordinationfunction/Pcf.cc), [`Mcf.cc`](src/inet/linklayer/ieee80211/mac/coordinationfunction/Mcf.cc), and the HCCA path in Hcf all throw `cRuntimeError("...is unimplemented!")`. These are advertised as coordination function options in the NED module hierarchy but silently crash at runtime. The `Hcf` class checks `hcca->isOwning()` in multiple paths and throws, creating dead code paths that add complexity without value.

#### W5 — Header file include bloat in Hcf.h
[`Hcf.h`](src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h) directly `#include`s **37 headers**, including concrete implementation headers (not just interfaces). This couples the header to specific implementations (`CtsProcedure`, `RecipientAckProcedure`, `OriginatorQosMacDataService`, `QosAckHandler`, etc.), significantly increasing compile-time coupling and making incremental builds slower. Forward declarations + pointer-only members could eliminate most of these (see [O6](#o6---reduce-header-coupling-in-hcfh)).

#### W6 — Inconsistent error handling strategy
The codebase mixes three error-handling paradigms:
1. `throw cRuntimeError(...)` for unimplemented features (327 sites)
2. `ASSERT(...)` for precondition checks
3. Silent `else ;` branches that swallow unhandled cases (e.g., line 1462 in Hcf.cc: `else ; // Beacon, etc`)

There is no consistent philosophy for when to throw, when to assert, and when to silently ignore. Some `throw cRuntimeError("Unknown frame")` sites are reachable with valid (but not-yet-supported) frame types, making the simulation fragile to protocol extensions.

#### W7 — Mode/amendment layering in PHY
The PHY mode files are enormous monoliths:
- [`Ieee80211VhtMode.h`](src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.h): **40 855 bytes**
- [`Ieee80211HtMode.h`](src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtMode.h): **26 478 bytes**
- [`Ieee80211ModeSet.cc`](src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.cc): **212 723 bytes** (~1 700 lines)
- [`Ieee80211VhtMode.cc`](src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.cc): **103 546 bytes**

The `Ieee80211ModeSet.cc` file contains hardcoded lookup tables for timing profiles and MCS parameters constructed via complex preprocessor macros (`EHT_MODE_ENTRIES_FOR_BW`). These should be data-driven (loaded from JSON/CSV files or constexpr arrays) to improve maintainability (see [O3](#o3---data-driven-phy-mode-tables)).

---

### 2.3 Opportunities 🟢

#### O1 — Extract services from Hcf to reduce God-class size
The `Hcf` class mixes four distinct responsibilities:
1. **Frame dispatch** (originator/recipient classification and routing)
2. **BA agreement lifecycle** (ADDBA, DELBA, inactivity)
3. **HT sounding coordination** (NDP announcement, CSI feedback, MFB)
4. **TXOP scheduling and frame-sequence lifecycle**

Each could be factored into a dedicated delegate class (some refactoring has already begun with `HcfResponseService`, `HcfExchangeCoordinator`, `HcfAggregationService`). Completing this decomposition would bring `Hcf.cc` below 800–1000 lines and make each concern independently testable.

#### O2 — Introduce a frame-type visitor or dispatch table
Replace cascading RTTI type checks with an $O(1)$ switch statement on the frame Type enum. Once the type is matched, cast the pointer using a zero-overhead compile-time `static_cast`.

**Example implementation**:
```cpp
switch (header->getType()) {
    case ST_DATA:
    case ST_DATA_WITH_QOS: {
        auto dataHeader = static_cast<const Ieee80211DataHeader*>(header.get());
        processDataFrame(packet, dataHeader);
        break;
    }
    case ST_BEACON:
    case ST_ASSOCIATION_REQ: {
        auto mgmtHeader = static_cast<const Ieee80211MgmtHeader*>(header.get());
        processManagementFrame(packet, mgmtHeader);
        break;
    }
    default:
        // Handle control frames and stubs...
        break;
}
```
This eliminates runtime type traversal completely and prevents "Unknown frame" crash-on-extension fragility.

#### O3 — Data-driven PHY mode tables
Timing, modulation, and MCS parameter maps can be defined inside `constexpr` static tables or parsed dynamically from external data resources (such as CSV/JSON files) loaded at module initialization.

**Example implementation**:
```cpp
struct EhtModeConfig {
    uint8_t mcs;
    uint8_t nss;
    uint16_t bandwidthMhz;
    bool isMandatory;
    double netBitrateBps;
};

static constexpr EhtModeConfig ehtMcsTable[] = {
    // mcs, nss, bw, mandatory, bitrate
    { 0,   1,   20,  true,      8.6e6 },
    { 1,   1,   20,  false,     17.2e6 },
    // ...
};
```
This reduces the file size of the mode classes by 50–70% and makes adding new rates a data modification rather than a C++ structural code change.

#### O4 — Complete or remove PCF/MCF/HCCA stubs
Leaving unimplemented coordination functions in the NED module hierarchy creates false API promises. Either:
- Remove the stubs entirely and document them as out-of-scope, or
- Complete a minimal functional implementation for research use cases

#### O5 — Strengthen HE/EHT MAC-PHY integration testing
The UL/DL OFDMA paths are complex (HeHcfUl.cc is ~2000 lines) but have limited integration coverage. Adding small deterministic end-to-end simulation tests that verify complete trigger → response → Multi-STA BA flows would catch regressions early.

#### O6 — Reduce header coupling in Hcf.h
Replace concrete header `#include`s in `[Hcf.h](src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h)` with forward declarations when the member variable is declared as a pointer.

**Example implementation**:
```cpp
// In Hcf.h (replace #include statements)
namespace inet {
namespace ieee80211 {
class Edca;
class ITx;
}
}
```
The implementation files (`.cc`) then include the concrete files directly. This isolates code changes and significantly accelerates incremental compile times.

#### O7 — Per-AC statistics completion
The [`__TODO`](src/inet/linklayer/ieee80211/__TODO) file lists extensive per-AC statistics that are planned but not implemented (per-AC packet drops, retry counts, aggregation counts, per-reason collision counts, TXOP duration/packet-count histograms). Implementing these would significantly improve the framework's value for research and analysis.

#### O8 — Unified logging strategy
Current logging uses a mix of `EV_INFO`, `EV_DETAIL`, `EV_TRACE`, `EV_WARN`, and bare `std::endl` vs `endl`. Standardizing log levels and adding structured event identifiers (especially in HeHcf UL/DL paths) would improve debuggability of complex multi-user scenarios.

---

### 2.4 Threats ⚠️

#### T1 — Maintainability pressure from standard complexity
Each new IEEE 802.11 amendment (be, bn) adds exponential interaction complexity (new MCS tables, new PPDU formats, new frame exchanges, new scheduling rules). The current God-class architecture amplifies this: every new feature must be woven into the already-2300-line Hcf.cc or its HE/VHT derivatives. Without the refactoring in O1, adding EHT/MLO features risks making the coordination functions unmaintainable.

#### T2 — Risk of "Unknown frame" runtime crashes in new scenarios
The 327 `throw cRuntimeError(...)` sites include many on reachable code paths (HCCA, unknown control frames, non-QoS data). As users compose more complex scenarios or mix protocol generations, the probability of hitting an unhandled branch increases. This could manifest as silent simulation crashes that are difficult to debug.

#### T3 — Simulation performance at scale
Simulation speed degrades in dense topologies (e.g., 100+ active nodes) due to hot paths.

**Performance Solutions**:
1. **Stateful Queue Byte Tracking**: Replace the $O(N)$ recursive byte summation in `calculateBufferedTrafficServiceBytes` with a stateful counter on each queue. Increment the counter on `enqueue()` and decrement on `dequeue()` to achieve constant-time $O(1)$ reads instead of traversing all queued packets on every scheduling loop.
2. **Batch Tag Lookups**: Instead of querying `findTag` multiple times per packet traversal (which performs list traversals inside the `Packet` class), extract all metadata tags into a lightweight context struct upon packet reception and pass it downstream.
3. **Type Dispatch**: Replace the RTTI-based type cast chains with enum-based dispatch tables (described in [O2](#o2---introduce-a-frame-type-visitor-or-dispatch-table)).

#### T4 — Coupling between MAC and PHY mode implementation details
The MAC layer directly references concrete PHY mode classes (`Ieee80211HeMode`, `Ieee80211HtMode`) in several places rather than going through the `IIeee80211Mode` interface. This creates tight coupling that could break when PHY implementations are refactored or replaced.

#### T5 — Limited layered (bit-level) PHY model support
The [`__TODO`](src/inet/physicallayer/wireless/ieee80211/__TODO) file notes: "layered physical model is broken." The bit-level PHY model for IEEE 802.11 appears to be in a degraded state. If researchers need sub-packet-level fidelity (e.g., for interference or coding studies), the packet-level model is insufficient and the bit-level model is acknowledged-broken.

---

## 3. Code Quality Assessment

### 3.1 Readability

| Aspect | Rating | Notes |
|--------|--------|-------|
| Naming conventions | ⭐⭐⭐⭐ | Classes, methods, and files follow clear conventions (`Hcf`, `HeHcf`, `Edcaf`, `OriginatorBlockAckAgreementHandler`). Interface names use `I` prefix. |
| Comments quality | ⭐⭐⭐⭐ | Standards references are excellent. Inline rationale comments are helpful. Some areas (particularly `HeHcfUl.cc`) could use more high-level block comments. |
| Function length | ⭐⭐⭐ | Most functions are reasonable (<50 lines), but some are very long: `recipientProcessReceivedFrame` (~140 lines), `processLowerFrame` (~90 lines), `channelGranted` (~40 lines with nested control flow). |
| File organization | ⭐⭐⭐ | The directory structure mirrors IEEE standard concepts well. However, spreading HeHcf across 4 `.cc` files without a clear boundary contract is confusing. |
| Header discipline | ⭐⭐ | Excessive includes in major headers. Forward declarations underused. |

### 3.2 Efficiency (Static Analysis)

| Concern | Severity | Location |
|---------|----------|----------|
| Cascading `dynamicPtrCast` chains | Medium | `Hcf.cc` lines 1142–1376, 1498–1580 |
| Per-frame `findTag<>` lookups in hot paths | Low-Medium | `Hcf::processLowerFrame`, `HeHcfUl.cc` trigger processing |
| O(N) frame iteration in buffer status | Medium | `calculateBufferedTrafficServiceBytes` (line 92–170 in Hcf.cc) |
| `std::vector` by value in `handleInternalCollision` | Low | Line 1098 (should be `const&`) |
| String construction in `getFrameSequenceInfo()` every refresh | Low | `refreshDisplay()` constructs string on every call |
| `HcfResponseService::Actions` struct of lambdas created per call | Low | `makeResponseServiceActions()` creates closures on every invocation |

### 3.3 Overall Code Quality Grade

| Criterion | Grade |
|-----------|-------|
| Architecture & Design | **B+** — Excellent interface decomposition, marred by God-class coordination functions |
| Standards Compliance | **A−** — Broad coverage with clause references; some acknowledged gaps (PCF, HCCA, non-QoS data) |
| Testability | **B** — Good unit test count; integration tests for complex HE paths need strengthening |
| Readability | **B** — Clean naming and conventions; hurt by very large files and include bloat |
| Maintainability | **B−** — The Hcf/HeHcf size and 108 TODOs create long-term risk |
| Efficiency | **B** — Adequate for typical scenarios; potential bottlenecks in dense deployments |

---

## 4. Priority Improvement Areas

### Tier 1 — High Impact, Moderate Effort

| # | Area | Rationale |
|---|------|-----------|
| 1 | **Decompose Hcf into focused delegates** | The single largest code-quality lever. Extract BA lifecycle, HT sounding, frame dispatch, and TXOP management into separate classes. |
| 2 | **Replace `dynamicPtrCast` cascades with dispatch** | Reduces crash risk, improves performance, and makes frame-type extension localized. |
| 3 | **Reduce Hcf.h include set** | Forward-declare types, move includes to `.cc`. Cuts incremental build time. |

### Tier 2 — Medium Impact, Moderate Effort

| # | Area | Rationale |
|---|------|-----------|
| 4 | **Complete or remove PCF/MCF/HCCA stubs** | Removes false API surface and dead code. |
| 5 | **Implement planned per-AC statistics** | High research value, already designed in the `__TODO` file. |

### Tier 3 — Long-term Structural

| # | Area | Rationale |
|---|------|-----------|
| 6 | **Data-driven PHY mode tables** | Reduce 200K+ byte mode files; make MCS addition a data change. |
| 7 | **Fix layered (bit-level) PHY model** | Prerequisite for sub-packet interference research. |
| 8 | **HE/EHT MAC-PHY integration test suite** | Protect complex OFDMA paths from regression as EHT/MLO expands. |
| 9 | **Unified logging and error-handling policy** | Replace ad-hoc mix of throw/assert/ignore with documented strategy. |
