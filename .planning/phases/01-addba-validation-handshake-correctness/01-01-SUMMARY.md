---
phase: 01-addba-validation-handshake-correctness
plan: 01
subsystem: mac
tags: [ieee80211, he-mu, addba, block-ack, omnetpp]

requires: []
provides:
  - Focused MAC-01 ADDBA validation coverage for HE MU admission.
  - Shared active originator Block Ack agreement predicate.
  - Final exact receiver/TID BA guard before HE MU queue mutation and tag allocation.
affects: [phase-01-plan-02, mac, ieee80211-he-mu]

tech-stack:
  added: []
  patterns:
    - Header-only active BA predicate for receiver/TID agreement checks.
    - Two-stage MU admission validation before pending-queue mutation.

key-files:
  created:
    - tests/unit/Ieee80211HeMuAddbaValidation_1.test
    - src/inet/linklayer/ieee80211/mac/blockack/BlockAckAgreementUtils.h
  modified:
    - src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc
    - src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h

key-decisions:
  - "DL MU container admission is BA-only: no normal ACK fallback is allowed for packets admitted to Ieee80211HeMuTag."
  - "HeDlMuTxOpFs prefers the QoSContext BA handler and falls back to the Hcf callback handler only when needed."

patterns-established:
  - "Use hasActiveOriginatorBlockAckAgreement(handler, receiver, tid) for active originator BA checks."
  - "Preflight scheduled HE MU allocations before any pendingQueue mutation, then revalidate immediately before removal/tag allocation."

requirements-completed: [MAC-01]

duration: 34min
completed: 2026-06-16
---

# Phase 01 Plan 01: Strict Active Block Ack Admission Summary

**HE MU container assembly now admits only QoS data packets with active receiver/TID originator Block Ack agreements.**

## Performance

- **Duration:** 34 min
- **Started:** 2026-06-16T20:21:44Z
- **Completed:** 2026-06-16T20:55:48Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments

- Added focused OMNeT++ coverage for active, inactive, missing, null-handler, non-QoS, and wrong-TID ADDBA eligibility.
- Added `hasActiveOriginatorBlockAckAgreement()` as the shared active BA predicate.
- Reworked `HeDlMuTxOpFs::buildMuContainerPacket()` to skip ineligible packets with `EV_WARN`, leave them pending, and abort before mutation if fewer than two BA-eligible allocations remain.
- Removed normal ACK fallback from active HE MU response handling; admitted allocations proceed through Block Ack response timing only.

## Task Commits

1. **Task 1: Add failing focused MAC-01 validation tests** - `78e7ae31bf` (test)
2. **Task 2: Enforce exact packet BA validation before MU container mutation** - `ea29e89d6e` (feat)

## Files Created/Modified

- `tests/unit/Ieee80211HeMuAddbaValidation_1.test` - Focused RED/GREEN coverage for strict ADDBA validation and pending-queue preservation.
- `src/inet/linklayer/ieee80211/mac/blockack/BlockAckAgreementUtils.h` - Header-only active originator BA agreement predicate.
- `src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc` - Exact packet preflight/final validation, ineligible diagnostics, and BA-only response handling.
- `src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h` - Stores admitted TID in active MU allocations.

## Decisions Made

- MU admission checks the `QoSContext` BA handler first, then falls back to the `Hcf` callback lookup for existing production paths.
- Active allocation state carries the admitted TID so a sequence that already passed admission can continue without re-querying BA state mid-sequence.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Stored admitted TID in active allocation state**
- **Found during:** Task 2 (Enforce exact packet BA validation before MU container mutation)
- **Issue:** BA-only response handling needed the admitted packet TID after admission, but Task 2's file list did not include `HeDlMuTxOpFs.h`.
- **Fix:** Added `tid` to `ActiveAllocation` and used it for BAR construction instead of re-querying agreement state mid-sequence.
- **Files modified:** `src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h`, `src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc`
- **Verification:** Focused direct `opp_test` passed after Task 2.
- **Committed in:** `ea29e89d6e`

---

**Total deviations:** 1 auto-fixed (1 missing critical)
**Impact on plan:** Required to satisfy D-10 and BA-only active MU response behavior. No scope creep beyond MAC-01.

## Issues Encountered

- The exact plan Python runner failed before test discovery because the local OMNeT++ Python environment is missing `matplotlib`:
  `ModuleNotFoundError: No module named 'matplotlib'`.
- Direct `opp_test` verification was used instead and passed.
- The default `ld.lld` linker crashed with a bus error during full relink. Rebuilding with `-fuse-ld=bfd` restored the local release library and allowed focused verification.

## Verification

- **RED:** Direct `opp_test` failed before Task 2 with `Expected 2 HE MU allocations, got 3`, proving inactive ADDBA packets were admitted by the old path.
- **GREEN:** Direct `opp_test` passed after Task 2:
  `PASS: 1 FAIL: 0 ERROR: 0 EXPECTEDFAIL: 0 SKIPPED: 0`.
- **Source checks:** `hasActiveOriginatorBlockAckAgreement`, `pendingQueue->removePacket`, and `addAllocation` are present in the expected files, and no `computeResponseAckFrameMode` path remains in `HeDlMuTxOpFs.cc`.

## Known Stubs

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Plan 01-02 can build on strict final MU admission. Ineligible packets now remain pending for the planned FIFO SU fallback and ADDBA retry/cooldown behavior.

## Self-Check: PASSED

- Found `tests/unit/Ieee80211HeMuAddbaValidation_1.test`.
- Found `src/inet/linklayer/ieee80211/mac/blockack/BlockAckAgreementUtils.h`.
- Found task commit `78e7ae31bf`.
- Found task commit `ea29e89d6e`.

---
*Phase: 01-addba-validation-handshake-correctness*
*Completed: 2026-06-16*
