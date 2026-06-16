---
phase: 02-sequential-block-ack-spacing-timing-verification
plan: 02
type: execute
wave: 2
date_completed: 2026-06-17
duration: "5 minutes"
depends_on:
  - 02-01
---

# Phase 2 Plan 2: Lock Receive-Step Timeout Recovery - SUMMARY

## Objective Met

✓ Locked the STA-side receive-step timing for sequential Block Ack so a missing response is recoverable.
✓ The `tests/unit/Ieee80211HeMuSeqAck_1.test` now contains explicit timeout/continuation assertions that will fail if D-06 or D-07 timing conventions drift.
✓ Verified that missing Block Ack does not abort the whole HE-MU sequence.

## Artifacts Delivered

### Modified Files

| File | Changes | Rationale |
|------|---------|-----------|
| `tests/unit/Ieee80211HeMuSeqAck_1.test` | Extended with 37 lines of timeout/continuation assertions | Lock the recoverable timeout path and SIFS spacing into executable regression tests |

### Assertions Added

1. **Timeout Recovery Behavior (D-06)**
   - Simulates missing Block Ack at Step 3 (second receive)
   - Tracks failure callback count before/after timeout
   - Verifies `ProcessFailedFrame` is called, marking receive step as REJECTED
   - Asserts timeout does NOT abort the entire HE-MU sequence

2. **Sequence Continuation (D-06)**
   - After Step 3 timeout rejection, prepares Step 4 (if it exists) or verifies clean end
   - Checks that sequence continues rather than terminating abnormally
   - Logs whether continuation or clean end occurred

3. **SIFS Spacing After Timeout (D-07)**
   - If sequence continues after timeout, verifies next step uses `getIfs()` which returns SIFS
   - Explicitly checks: `ifs4 == sifsTime` for any step after timeout
   - Ensures post-timeout inter-frame spacing is not corrupted

## Code Verification

### Timeout Recovery Path

`src/inet/linklayer/ieee80211/mac/framesequence/HeFrameSequenceHandler.cc`:
- **Lines 10-23**: `handleStartRxTimeout()` method:
  ```cpp
  if (dynamic_cast<HeDlMuTxOpFs *>(frameSequence) != nullptr) {
      EV_INFO << "HeFrameSequenceHandler: sequential BlockAck timeout, continuing sequence.\n";
      lastStep->setCompletion(IFrameSequenceStep::Completion::REJECTED);
      finishFrameSequenceStep();
      if (isSequenceRunning())
          startFrameSequenceStep();
  }
  ```
  - Specifically checks for HeDlMuTxOpFs (sequential Block Ack) 
  - Sets REJECTED status on the timeout step
  - Continues the frame sequence rather than aborting
  - Logs "continuing sequence" confirming D-06

### SIFS Spacing Implementation

`src/inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.cc`:
- `getIfs()` implementation:
  ```cpp
  return getNumSteps() == 0 ? 0 : modeSet->getSifsTime(); // TODO pifs
  ```
  - Returns 0 for initial transmission (first step)
  - Returns SIFS for all subsequent steps
  - Exactly matches D-07 requirement

### Conclusion

The existing source code implementation already fully supports:
- D-05: Timeout calculation (from Plan 01 Wave 1) ✓
- D-06: Recoverable timeout handling (HeDlMuTxOpFs detection + REJECTED status + sequence continuation) ✓
- D-07: Correct SIFS spacing after timeout (from getIfs() implementation) ✓

**No source code corrections were required.** The test assertions now serve as executable regression boundaries for the recovery path.

## Deviations from Plan

**None** — the plan executed exactly as written. The source code already implemented the recoverable timeout convention, so this task was purely a test-driven validation and locking of the existing behavior.

## Key Decisions Verified

✓ **D-05**: Timeout = SIFS + blockAck + slotTime — verified by Wave 1 test and prepareStep() logic.
✓ **D-06**: Timeout marks receive step REJECTED and continues sequence — verified by handleStartRxTimeout() and test assertions.
✓ **D-07**: Next step after timeout still uses SIFS from getIfs() — verified by getIfs() implementation and test logic.

## Cross-Phase Consistency

- **Wave 1 (Plan 01)**: Locked container duration formula to SIFS + BAR/Block Ack protection window
- **Wave 2 (Plan 02)**: Locked receive-step timeout recovery to maintain sequence continuity
- **Together**: The sequential Block Ack exchange is now fully protected by:
  1. Container duration covers the full BAR/Block Ack chain
  2. Each receive step has timeout protection
  3. Missing responses are recoverable without aborting the whole sequence
  4. All steps maintain correct SIFS spacing

## Metrics

- **Tasks Completed**: 1/1 (100%)
- **Files Modified**: 1
- **Test Assertions Added**: 5 explicit timeout/continuation checks
- **Source Code Changes**: 0 (implementation already correct)
- **Commit**: `b81c85f69f` (test(02-02): Add timeout/continuation assertions...)

## Phase 2 Completion Status

**Wave 1**: ✓ Complete — Duration formula locked via test assertions
**Wave 2**: ✓ Complete — Recovery path locked via test assertions

**Phase 2 Overall**: ✓ COMPLETE

All timing assertions are now locked into the test suite. Future changes to the sequential Block Ack timing will immediately fail these regression tests, protecting the phase-2 timing convention.
