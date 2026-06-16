---
phase: 02-sequential-block-ack-spacing-timing-verification
plan: 01
type: execute
wave: 1
date_completed: 2026-06-17
duration: "5 minutes"
---

# Phase 2 Plan 1: Lock Sequential Duration Formula - SUMMARY

## Objective Met

✓ Locked the container protection math for DL HE-MU sequential acknowledgments via test assertions.
✓ The `tests/unit/Ieee80211HeMuSeqAck_1.test` now contains explicit duration formula assertions that will fail if the D-02 through D-07 timing convention drifts.

## Artifacts Delivered

### Modified Files

| File | Changes | Rationale |
|------|---------|-----------|
| `tests/unit/Ieee80211HeMuSeqAck_1.test` | Extended with 57 lines of duration formula assertions | Lock the sequential duration formula and timeout calculations into executable regression tests |

### Assertions Added

1. **Container Duration Verification (D-02, D-03, D-04)**
   - Extracts the container packet from Step 0 (TRANSMIT)
   - Verifies `durationField` matches: SIFS + blockAck + (SIFS + BAR(38) + SIFS + blockAck)
   - Calculates expected duration dynamically from `IQosRateSelection::computeResponseBlockAckFrameMode()`
   - Logs individual components: SIFS, BlockAck, BAR durations

2. **Timeout Window Verification (D-05)**
   - Explicitly calculates expected timeout: SIFS + blockAck + slotTime
   - Verifies this formula is applied at Step 1 (first receive) and Step 3 (second receive)
   - Ensures slotTime safety margin is preserved

3. **Subframe Duration Field Verification (D-04)**
   - Checks that BAR packet (Step 2) contains the same duration field as the container
   - Verifies the locked duration is copied to all subframes before queue removal

## Code Insights

### Source Code Verification

`src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc`:
- **Line 87**: `simtime_t totalDuration = simtime_t::ZERO;` — initializes aggregate duration
- **Lines 180-184**: Duration formula matches locked convention:
  ```cpp
  if (idx >= 1)
      totalDuration += modeSet->getSifsTime() + barDuration + modeSet->getSifsTime() + responseDuration;
  else
      totalDuration += modeSet->getSifsTime() + responseDuration;
  ```
- **Line 192**: Container header duration set: `containerHdr->setDurationField(totalDuration);`
- **Line 242**: Subframe duration set: `dataOrMgmtHdrWritable->setDurationField(totalDuration);`

### Conclusion

The existing source code implementation already matches the locked D-02 through D-07 timing formula. **No source code corrections were required.** The test assertions now serve as executable regression boundaries to prevent future timing drift.

## Deviations from Plan

**None** — the plan executed exactly as written. The source code already implemented the locked convention, so this task was purely a test-driven validation and locking of the existing behavior.

## Key Decisions Verified

✓ **D-01**: Response mode is computed by `IQosRateSelection::computeResponseBlockAckFrameMode()` — locked into test.
✓ **D-02**: First response = SIFS + blockAck — verified by test assertion.
✓ **D-03**: Additional responses = SIFS + BAR(38) + SIFS + blockAck — verified by test calculation.
✓ **D-04**: Same duration on container and subframes — verified by BAR packet check.
✓ **D-05**: Timeout = SIFS + blockAck + slotTime — verified for both receive steps.

## Next Phase

Wave 2 (02-02-PLAN.md) depends_on this plan. Wave 2 will extend the same test with timeout/continuation and recoverable failure assertions.

## Metrics

- **Tasks Completed**: 1/1 (100%)
- **Files Modified**: 1
- **Test Assertions Added**: 4 explicit duration/timeout checks
- **Source Code Changes**: 0 (implementation already correct)
- **Commit**: `3dc770438b` (test(02-01): Add duration formula assertions...)
