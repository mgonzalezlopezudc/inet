# Phase 3: PHY Layer RU Behavior & Attenuation Auditing - Context

**Gathered:** 2026-06-17
**Status:** Ready for planning

<domain>
## Phase Boundary

Audit and lock how HE MU Resource Unit (RU) allocations are mapped and propagated through MAC-to-PHY boundaries, and how per-RU attenuation/noise/interference behavior is computed and validated in the packet-level 802.11ax model.

</domain>

<decisions>
## Implementation Decisions

### RU Frequency Mapping
- **D-01:** Scheduler-provided RU geometry is authoritative; do not treat medium-side reconstruction from count/channel as the source of truth.
- **D-02:** Invalid RU index or missing RU mapping is a hard failure condition.
- **D-03:** RU mapping metadata remains in both `Ieee80211HeMuTag` and HE MU PHY header allocations.
- **D-04:** Any scheduler-allocation versus packet-receiver mismatch is fail-fast and aborts MU assembly/transmission.

### Per-RU Power Split
- **D-05:** RU transmit power is proportional to each RU's bandwidth.
- **D-06:** Power normalization uses nominal transmission bandwidth (mode/channel bandwidth), not only active-allocation sum.
- **D-07:** Add power-conservation audit checks (sum of RU power versus expected mapped fraction) with logging for drift.
- **D-08:** For irregular future RU geometries, power derivation still follows each RU's actual bandwidth.

### Noise and Interference Isolation
- **D-09:** Sub-transmissions from the same MU PPDU are non-interfering with each other by default.
- **D-10:** Main MU transmission object is physically suppressed; only sub-transmissions propagate for PHY effects.
- **D-11:** Add explicit per-RU audit observability tied to center frequency and RU bandwidth.
- **D-12:** Isolated-RU behavior remains the default for v1 correctness; advanced coupling/leakage is opt-in future work.

### Allocation Validation and Failure Policy
- **D-13:** Allocation validation failures abort MU transmission (no tolerant partial-send default in this phase).
- **D-14:** Validation failures are signaled with `cRuntimeError` containing structured reason details.
- **D-15:** Ownership/memory-consistency errors in allocation handling are fatal.
- **D-16:** No tolerant fallback path is introduced in Phase 3 auditing logic.

### the agent's Discretion
- None. The discussion locked strict behavior for all selected areas.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Project and phase scope
- `.planning/ROADMAP.md` - Phase 3 scope, requirements linkage (PHY-01, PHY-02), and plan targets.
- `.planning/REQUIREMENTS.md` - Locked requirement identifiers for PHY correctness outcomes.
- `.planning/PROJECT.md` - Project constraints and scope guardrails.
- `.planning/phases/01-addba-validation-handshake-correctness/01-CONTEXT.md` - Prior MAC admission decisions that remain fixed.
- `.planning/phases/02-sequential-block-ack-spacing-timing-verification/02-CONTEXT.md` - Prior timing decisions that remain fixed.

### MAC to PHY RU handoff
- `src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc` - RU allocation selection, final validation, and `Ieee80211HeMuTag` population.
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuTag.h` - RU allocation ownership, duplication, and tag lifecycle.

### PHY medium and per-RU behavior
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211RadioMedium.cc` - MU sub-transmission split, RU power assignment, and interference isolation behavior.
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.cc` - HE MU decapsulation and RU-index allocation metadata handoff at receive side.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `Ieee80211HeMuTag::addAllocation()` and `setAllocations()` already carry RU-indexed packet clones for medium processing.
- `Ieee80211RadioMedium::calculateHeRus(...)` and per-RU analog model creation are existing hooks for RU-specific center frequency/bandwidth behavior.
- `HeDlMuTxOpFs::buildMuContainerPacket()` already performs multi-stage allocation validation and can host stricter fail-fast checks.

### Established Patterns
- Prior phases established strict fail-fast correctness for invalid MAC eligibility and sequencing constraints.
- MU handling currently separates logical main transmission tracking from physical sub-transmission propagation.
- Runtime correctness violations commonly use explicit `cRuntimeError` in this code path.

### Integration Points
- `HeDlMuTxOpFs` (scheduler output -> validated RU allocation -> tag metadata).
- `Ieee80211RadioMedium` (RU geometry/power split and interference rules).
- `Ieee80211Radio` (HE MU allocation metadata propagation at RX decapsulation).

</code_context>

<specifics>
## Specific Ideas

- Preserve deterministic, audit-grade behavior for PHY correctness (prefer explicit failure to silent tolerance).
- Keep observability high by retaining both tag-level and PHY-header allocation metadata.

</specifics>

<deferred>
## Deferred Ideas

- Optional RU leakage/coupling model as a future opt-in capability outside current phase scope.
- Configurable fatal/non-fatal fallback modes for allocation errors as future enhancement work.

</deferred>

---

*Phase: 03-phy-layer-ru-behavior-attenuation-auditing*
*Context gathered: 2026-06-17*
