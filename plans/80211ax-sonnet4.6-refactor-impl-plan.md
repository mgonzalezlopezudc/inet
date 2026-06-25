# Implementation Plan: HE-SIG-B On-Wire Compliance + HeHcf Decomposition

## Goal

Fix two sets of issues identified in the 802.11ax implementation review:

1. **Section 1.9 — HE-SIG-B RU allocation field is not wire-compliant.**
   The current `encodeHeSigBRuAllocation()` emits one catalog index per RU across the whole bandwidth. The standard (Tables 27-26 and 27-27) defines a specific 8-bit `RU Allocation subfield` (B7..B0) per 40-MHz block in a per-content-channel two-subfield structure. The center-26-tone subfield and the cross-reference rule for wide-spanning RUs are also missing.

2. **Section 3.2 — Programming practices remaining concerns.**
   - `HeHcf.cc` is 1461 lines mixing six distinct responsibilities.
   - `#define DI` pollutes the global preprocessor namespace.
   - `Ieee80211HePhyCalculator.h` is 560+ lines of `inline` functions; non-trivial ones should live in a `.cc` file.

---

## Open Questions

> [!NOTE]
> All design questions were answered by the user before this plan was written. Answers are embedded below.

---

## Part 1 — HE-SIG-B Wire-Compliance

### Background

**Current behavior (`encodeHeSigBRuAllocation`):**
- Takes a flat `vector<Ieee80211HeRu>` (the full channel layout) and the channel bandwidth.
- Looks up each RU in `getHeRuAllocationCatalog()` (a flat ordered vector of all valid RU geometries).
- Stores the **linear catalog position** (0, 1, 2, …) as the allocation code.

**Standard requirement (IEEE 802.11-2024 Tables 27-26/27-27):**
- The Common field carries one or two `RU Allocation subfields` per 40-MHz half (content channel), each holding an **8-bit value (B7..B0)** that encodes a full 20-MHz RU tiling pattern.
- For 80 MHz: two content channels, each with one `RU Allocation subfield`.
- For 160 MHz: two content channels, each with two `RU Allocation subfields`.
- A `Center 26-tone RU subfield` is present when a 26-tone RU occupies the center tone gap.
- Wide RUs (≥242-tone) that span multiple content channels reference the same code in all affected subfields.
- Codes 0–255 are defined in Table 27-27 (B7..B0 notation); many values are reserved.

**Gap:** The catalog index happens to equal the Table 27-27 value for most simple entries, but the structure is wrong for wide channels (missing per-content-channel structure and center-26-tone subfield).

### Proposed Changes

#### [MODIFY] [Ieee80211HeSigCodec.h](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeSigCodec.h)

Replace the current flat-output codec with a standard-compliant one.

**New types:**

```cpp
// One content channel's allocation subfields.
// IEEE 802.11-2024 Table 27-26: up to 2 RU Allocation subfields per
// content channel (one for 20/40 MHz, two for 80/160 MHz).
struct Ieee80211HeSigBContentChannel {
    std::vector<uint8_t> ruAllocationSubfields; // Table 27-27 B7..B0 values
    bool hasCenterRu = false;                   // Center 26-tone RU present
};

// Wire-level HE-SIG-B Common field layout.
struct Ieee80211HeSigBCommonField {
    std::vector<Ieee80211HeSigBContentChannel> contentChannels; // 1 or 2
    std::vector<Ieee80211HeRu> rus;             // resolved RU objects
};
```

**New `encodeHeSigBCommonField()`:**

1. Call `validateHeRuLayout()`.
2. Determine content channel count from bandwidth (1 for 20 MHz, 2 for 40/80/160 MHz).
3. Build a `ruAllocationByByte` lookup from Table 27-27 (a `static const uint8_t[]` array keyed by RU-tiling pattern → standard byte value).
4. For each 40-MHz half, enumerate the RUs that fall within it (or span it).
5. Determine the tiling pattern of that 40-MHz half (sorted list of RU sizes in frequency order).
6. Look up the pattern in the Table 27-27 lookup to obtain the 8-bit subfield value.
7. Detect the center-26-tone gap (tones −16 to +16) and set `hasCenterRu` if occupied.
8. For wide RUs (≥242 tones) that span multiple content channels, write the same code to all affected subfield entries per Table 27-26.

**New `decodeHeSigBCommonField()`:**

Inverse operation: takes a `Ieee80211HeSigBCommonField`, resolves each 8-bit code against the Table 27-27 table to reconstruct `Ieee80211HeRu` objects, handles the center-26-tone subfield, and deduplicates wide RUs that appear in multiple subfields.

Keep the existing `Ieee80211HeSigCodecResult` result type for backward compatibility; expose an additional `Ieee80211HeSigBCommonFieldResult`.

#### [MODIFY] [Ieee80211HeSigCodec.h](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeSigCodec.h) — Table 27-27 constant table

Add a `static constexpr` lookup table encoding Table 27-27:

```cpp
// IEEE 802.11-2024 Table 27-27 — RU Allocation subfield (B7..B0).
// Each entry maps a sorted tuple of RU sizes (a 20-MHz tiling pattern)
// to its standard 8-bit code.
//   Code 0 (0b00000000): nine 26-tone RUs
//   Code 1 (0b00000001): eight 26-tone RUs (1 slot unused)
//   Code 2 (0b00000010): 26,26,26,26,26,52 pattern
//   ... (full table as constexpr array of {uint8_t code, pattern_id})
```

The full table (codes 0–255) will be generated from the standard text. Reserved entries will be marked.

#### [MODIFY] [Ieee80211Radio.cc](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.cc) and [Ieee80211PhyHeaderSerializer.cc](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeaderSerializer.cc)

Update the two call sites from `encodeHeSigBRuAllocation()` to `encodeHeSigBCommonField()`, and the corresponding decode call to `decodeHeSigBCommonField()`.

#### [NEW] Test: `Ieee80211HeWireSigBEncoding_1.test`

Verify:
- 20 MHz, all-26-tone layout → code 0 (0b00000000), single content channel, 1 subfield.
- 20 MHz, mixed 26+52 layout → correct Table 27-27 code.
- 80 MHz, MU layout across two content channels → two content channels, two subfields each.
- 80 MHz, 484-tone SU → same code in both content channels (cross-reference rule).
- Center 26-tone RU → `hasCenterRu == true`.
- Reserved code (unused Table 27-27 entry) → decode returns error.

---

## Part 2 — Section 3.2 Programming Practices

### 2a. `HeHcf` Decomposition (Full)

`HeHcf.cc` currently mixes six responsibilities. The `HeUlCoordinator` module already extracts UL trigger policy. The plan extracts the remaining four:

| Responsibility | Lines in HeHcf.cc | New home |
|---|---|---|
| NDP sounding (AP side: initiate, respond) | 680–780 (DL sounding launch), 889–1000 (STA side feedback) | **[NEW] `HeSoundingCoordinator.h/.cc/.ned`** |
| STA-side sounding state machine (NDP-A recv, NDP recv, feedback send) | 889–995 | Same new class |
| CSI freshness/leakage tracking | `csiManager` member, `hasFreshCsi()`, `updateCsi()` | **Promote `HeMuMimoCsiManager` to a proper class** with unit-testable public API |
| TWT sleep-state gating | Lines 405, 585 (two call sites to `isTwtSleeping()`) | **[NEW] `HeTwtGating.h`** — thin pure-function wrapper over `ITwtManager` |
| Preamble puncturing parsing/validation | `isValidHePreamblePuncturing()`, `parseHePreamblePuncturing()`, `overlapsHePuncturedSubchannel()` (lines 89–194 of anonymous namespace) | **[NEW] `HePreamblePuncturing.h`** — pure-function header; move out of anonymous namespace |

**`HeHcf` after decomposition:** retains only frame-sequence selection (`startFrameSequence`, `tryStartDlMuFrameSequence`, `tryStartUlMuFrameSequence`), queue bank management, and TXOP completion bookkeeping. Target size ≤ 600 lines.

#### [NEW] `HeSoundingCoordinator.h/.cc/.ned`

```cpp
class INET_API HeSoundingCoordinator : public cSimpleModule
{
  public:
    // AP: initiate NDP sounding for a set of target STAs.
    void initiateSounding(const std::vector<SoundingTarget>&, ...);
    // STA: process received NDP Announcement / NDP.
    void processNdpAnnouncement(Packet *packet, ...);
    void processNdp(Packet *packet, ...);
    // Callback: sounding complete with updated CSI.
    cSignal *soundingCompleted;
  protected:
    // State machine for the STA-side sounding protocol.
    bool ndpAnnouncementReceived = false;
    bool ndpReceived = false;
    uint8_t soundingDialogToken = 0;
    std::vector<SoundingTarget> soundingTargets;
};
```

`HeHcf` holds a `HeSoundingCoordinator *soundingCoordinator` submodule reference (injected in NED). The current `HeHcf.ned` gets a `soundingCoordinator: HeSoundingCoordinator` submodule.

#### [MODIFY] `HeMuMimoCsiManager.h` — promote to testable class

The existing struct has a few methods. Promote to a `class` with a proper `configure()` and an injectable clock interface (for testability). No semantic changes to the algorithm.

#### [NEW] `HePreamblePuncturing.h`

Move `isValidHePreamblePuncturing()`, `parseHePreamblePuncturing()`, and `overlapsHePuncturedSubchannel()` out of the anonymous namespace in `HeHcf.cc` into a new header. This makes them directly testable. Remove the anonymous namespace wrappers from `HeHcf.cc`.

#### [NEW] Test: `Ieee80211HePreamblePuncturing_1.test`

Verify all permitted/rejected patterns from IEEE 802.11-2024 Table 27-21 for both 80 MHz and 160 MHz.

#### [NEW] Test: `Ieee80211HeSoundingCoordinator_1.test`

Drive the STA-side sounding state machine with mock NDP-A and NDP packets; assert correct feedback generation.

### 2b. `#define DI` Macro Containment

The `#define DI DelayedInitializer` at line 10 of `Ieee80211HeMode.h` pollutes any file that includes it.

#### [MODIFY] [Ieee80211HeMode.h](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h)

Add `#undef DI` immediately after the last use of the macro (after the `Ieee80211HemcsTable` class body, before the closing of the header guard). No semantic change; safe sed-pass.

```cpp
// ... (end of Ieee80211HemcsTable)
#undef DI   // was: #define DI DelayedInitializer — limit scope to this header
```

### 2c. PHY Calculator — Move Non-Trivial Functions to `.cc`

The 560-line `Ieee80211HePhyCalculator.h` is included by 7 `.cc` files. Non-trivial functions that do not need to be inlined should move to a new `.cc`.

#### Functions to move to `Ieee80211HePhyCalculator.cc` (new file):

| Function | Why moveable |
|---|---|
| `computeHePpduParameters()` | Large, called from 3 sites; not performance-critical at call frequency |
| `getHeSigBSymbolCount()` | Called from calculator + serializer; no inlining benefit |
| `getHeRuDataSubcarrierCount()` | Table lookup; not inlined by any template |
| `getHeRuPilotSubcarrierCount()` | Same |

Functions that **stay** in the header (used in templates or hot loops):
- `getHeMcsBitsPerSubcarrier()`, `getHeMcsCodeRate()`, `getHeGuardIntervalDuration()`, `getHeLtfSymbolDuration()`, `isHeDcmCombinationSupported()`, `isHeValidMcsNssCombination()`

#### [NEW] `Ieee80211HePhyCalculator.cc`

A corresponding `.cc` in `src/inet/physicallayer/wireless/ieee80211/packetlevel/`. Add to `src/Makefile` (or rely on the wildcard glob if already present).

The header retains only the struct definitions and the small helpers listed above. The declaration (non-inline) of the moved functions is added to the header; the definition moves to the `.cc`.

---

## Verification Plan

### Automated Tests

```sh
# Existing suite — must all still pass
source /home/user/omnetpp-6.4.0aipre2/setenv -f && source setenv -q
export CCACHE_DISABLE=1
bin/inet_run_unit_tests -m release -f "Ieee80211He.*\\.test"

# New tests added by this plan
bin/inet_run_unit_tests -m release -f "(Ieee80211HeWireSigBEncoding|Ieee80211HePreamblePuncturing|Ieee80211HeSoundingCoordinator).*\\.test"
```

### Build check

```sh
make -j$(nproc) MODE=release
```

### Manual

Run the `ofdma_dl_mu` example (5-STA, 80 MHz) in Qtenv and confirm the Qtenv signal visualization still shows correct per-user RU labels after the codec change.

---

## Implementation Order (Dependencies First)

1. **`#undef DI`** in `Ieee80211HeMode.h` — trivial, no deps.
2. **`HePreamblePuncturing.h`** — move helpers, write test. No HeHcf change yet.
3. **`HeSoundingCoordinator.h/.cc/.ned`** — new module + test. Update HeHcf to delegate.
4. **PHY calculator `.cc`** — move functions, update build. Run full suite.
5. **HE-SIG-B wire compliance** — new Table 27-27 table, new codec, update callers, write test.
6. **`HeMuMimoCsiManager` promotion** — minor class promotion, no behavioral change.

---

## Estimated Scope

| Item | New LOC | Modified LOC | New Tests |
|---|---|---|---|
| HE-SIG-B wire codec | ~180 | ~80 (2 callers) | 1 test (~80 lines) |
| `HePreamblePuncturing.h` | ~110 | ~30 (HeHcf remove) | 1 test (~60 lines) |
| `HeSoundingCoordinator` | ~220 | ~120 (HeHcf delegate) | 1 test (~80 lines) |
| PHY calculator `.cc` | ~300 | ~60 (header trim) | 0 (existing tests cover) |
| `#undef DI` | 1 | 0 | 0 |
| `HeMuMimoCsiManager` promote | ~30 | ~20 | 0 |
| **Total** | **~841** | **~310** | **3 new tests** |
