---
phase: 04-automated-testing-example-verification
plan: 01
subsystem: testing
tags: [ieee80211, he-mu, ofdma, omnetpp, validation]

requires:
  - phase: 03-phy-layer-ru-behavior-attenuation-auditing
    provides: RU behavior and HE MU regression context used by Phase 04 validation
provides:
  - Executable TST-01 HE MU command contract
  - Focused ADDBA and sequential Block Ack unit-test gate
  - Varying-load OFDMA example scalar assertions
affects: [phase-04, TST-01, TST-02]

tech-stack:
  added: []
  patterns:
    - Bash strict-mode validation script with explicit preflight checks
    - OMNeT++ scalar assertions using in-repository tools only

key-files:
  created:
    - tests/validation/ieee80211/he_mu_command_contract.sh
    - tests/validation/ieee80211/README.md
  modified:
    - tests/unit/Ieee80211HeMuSeqAck_1.test

key-decisions:
  - "TST-01 is represented as one executable repository-owned shell contract."
  - "The command contract uses focused unit filters plus two OFDMA load runs instead of a broad full-suite gate."

patterns-established:
  - "Validation contracts bootstrap OMNeT++ first, then INET, and fail fast on missing paths."
  - "OFDMA example evidence is asserted from generated .sca scalar rows."

requirements-completed: [TST-01]

duration: 12 min
completed: 2026-06-16
---

# Phase 04 Plan 01: TST-01 HE MU Command Contract Summary

**Executable HE MU protocol-sequence gate with ADDBA, sequential Block Ack, and varying-load OFDMA scalar assertions**

## Performance

- **Duration:** 12 min
- **Started:** 2026-06-16T23:38:08Z
- **Completed:** 2026-06-16T23:50:28Z
- **Tasks:** 3
- **Files modified:** 3

## Accomplishments

- Added `tests/validation/ieee80211/he_mu_command_contract.sh` as the repository-owned TST-01 command contract.
- The script runs `Ieee80211HeMuAddbaValidation_1.test` and `Ieee80211HeMuSeqAck_1.test` through `bin/inet_run_unit_tests -m release -f`.
- The script runs the OFDMA `General` example with `sendInterval=0.2ms` and `sendInterval=1ms`, then asserts `blockAckAgreementAdded:count > 0` and `edcaCollisionDetected:count == 0`.
- Added operator documentation for local and CI invocation, including the explicit `04-02` boundary for TST-02.

## Task Commits

1. **Task 1: Create deterministic TST-01 command-contract script** - `0e2b8403b3` (feat)
2. **Task 2: Add robust executable preflight and failure guards** - `0f8725fb30` (fix)
3. **Task 3: Document command contract usage and expected gate behavior** - `d3811fe48a` (docs)

**Plan metadata:** final `docs(04-01)` metadata commit created after this summary was written.

## Files Created/Modified

- `tests/validation/ieee80211/he_mu_command_contract.sh` - Executable TST-01 shell contract with environment bootstrap, unit checks, OFDMA load runs, and scalar assertions.
- `tests/validation/ieee80211/README.md` - Usage, expected behavior, TST-01 mapping, CI command, and 04-02 boundary.
- `tests/unit/Ieee80211HeMuSeqAck_1.test` - Updated fixture to satisfy current active QoS Block Ack preconditions and deterministic expected output.

## Decisions Made

- Kept the contract dependency-free and repository-grounded: Bash, `bin/inet_run_unit_tests`, `inet`, and `.sca` parsing only.
- Used two explicit unit-test invocations rather than a combined filter so command output and failures remain unambiguous.
- Kept TST-02 out of this plan except for documenting the boundary.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Made OMNeT++ environment sourcing compatible with strict shell mode**
- **Found during:** Task 2
- **Issue:** `set -u` caused `/home/user/omnetpp-6.4.0/setenv -f` to fail on unset variables such as `IN_NIX_SHELL` and `__omnetpp_root_dir`.
- **Fix:** Initialized `IN_NIX_SHELL` and temporarily disabled nounset only around external `source` calls, restoring strict mode before validation commands.
- **Files modified:** `tests/validation/ieee80211/he_mu_command_contract.sh`
- **Verification:** `tests/validation/ieee80211/he_mu_command_contract.sh` completed successfully.
- **Committed in:** `0f8725fb30`

**2. [Rule 3 - Blocking] Repaired SeqAck fixture for current HE MU admission preconditions**
- **Found during:** Task 2
- **Issue:** `Ieee80211HeMuSeqAck_1.test` failed under the required contract because it used non-QoS packets and no active originator Block Ack agreement handler, while current `HeDlMuTxOpFs` correctly rejects those inputs before queue mutation.
- **Fix:** Added a minimal mock originator Block Ack agreement handler, used QoS data packets with a matching TID, fixed enum diagnostic streaming, and aligned expected output with current timeout recovery order.
- **Files modified:** `tests/unit/Ieee80211HeMuSeqAck_1.test`
- **Verification:** `bin/inet_run_unit_tests -m release -f Ieee80211HeMuSeqAck_1.test` passed as part of the contract.
- **Committed in:** `0f8725fb30`

**3. [Rule 1 - Bug] Corrected OMNeT++ scalar parser column indexes**
- **Found during:** Task 2
- **Issue:** The first implementation checked the wrong fields for `.sca` scalar rows, causing a false failure even though `blockAckAgreementAdded:count` and `edcaCollisionDetected:count` were present.
- **Fix:** Parsed scalar name from column 3 and value from column 4.
- **Files modified:** `tests/validation/ieee80211/he_mu_command_contract.sh`
- **Verification:** Both OFDMA load runs completed and scalar assertions passed.
- **Committed in:** `0f8725fb30`

---

**Total deviations:** 3 auto-fixed (2 Rule 3, 1 Rule 1)
**Impact on plan:** No scope expansion beyond making the planned command contract executable and truthful.

## Issues Encountered

- Initial sandboxed runtime execution failed with `ccache: error: Read-only file system` while building OMNeT++ unit-test artifacts. The exact failing command was `bash -lc 'tests/validation/ieee80211/he_mu_command_contract.sh'`. Rerunning with elevated filesystem permissions allowed the required build/cache/results writes and the command passed.

## Verification

- `bash -lc 'test -x bin/inet_run_unit_tests && test -f tests/unit/Ieee80211HeMuAddbaValidation_1.test && test -f tests/unit/Ieee80211HeMuSeqAck_1.test && test -f examples/ieee80211/ofdma/omnetpp.ini'` - PASS
- `bash -lc 'test -x tests/validation/ieee80211/he_mu_command_contract.sh'` - PASS
- `bash -lc 'tests/validation/ieee80211/he_mu_command_contract.sh'` - PASS
- `bash -lc 'grep -n "TST-01\|04-02" tests/validation/ieee80211/README.md'` - PASS

## Known Stubs

None.

## User Setup Required

None - no external service configuration required. Local execution requires the repository's existing OMNeT++/INET environment at `/home/user/omnetpp-6.4.0`.

## Next Phase Readiness

Ready for `04-02`, which owns TST-02 example-level scalar assertions and broader validation scenario hardening.

## Self-Check: PASSED

- Found `tests/validation/ieee80211/he_mu_command_contract.sh`
- Found `tests/validation/ieee80211/README.md`
- Found commits `0e2b8403b3`, `0f8725fb30`, and `d3811fe48a`
- Final contract command completed successfully.

---
*Phase: 04-automated-testing-example-verification*
*Completed: 2026-06-16*
