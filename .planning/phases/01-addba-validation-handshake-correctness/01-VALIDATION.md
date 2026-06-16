---
phase: 01
slug: addba-validation-handshake-correctness
status: finalized
nyquist_compliant: true
wave_0_complete: true
created: 2026-06-16
---

# Phase 01 - Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | OMNeT++ `opp_test` plus INET Python runners |
| **Config file** | Per-test `%inifile` sections in `.test` files |
| **Quick run command** | `source setenv && export PYTHONPATH=/home/user/omnetpp-6.4.0/python:$PYTHONPATH && python3 -c "from inet.test.opp import run_opp_tests; run_opp_tests('tests/unit', filter='Ieee80211HeMuAddbaValidation_1.test', full_match=True)"` |
| **Full suite command** | `source setenv && export PYTHONPATH=/home/user/omnetpp-6.4.0/python:$PYTHONPATH && ./bin/inet_run_unit_tests` |
| **Estimated runtime** | Quick: under 60 seconds after build cache warmup; full suite: project-dependent |

## Sampling Rate

- **After every task commit:** Run the quick command once `tests/unit/Ieee80211HeMuAddbaValidation_1.test` exists.
- **After every plan wave:** Run the full suite command.
- **Before `$gsd-verify-work`:** New focused test and `tests/unit/Ieee80211HeMuSeqAck_1.test` must be green.
- **Max feedback latency:** Keep focused ADDBA validation under 60 seconds where possible.

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 01-01-01 | 01 | 1 | MAC-01 | T-01-01 | Non-QoS, invalid TID, or no-active-BA packets are not inserted into an HE MU container. | unit `.test` | Quick run command | created by Plan 01 Task 1 | planned |
| 01-01-02 | 01 | 1 | MAC-01 | T-01-02 | ADDBA agreement placeholders without a received ADDBA Response are treated as inactive. | unit `.test` | Quick run command | created by Plan 01 Task 1 | planned |
| 01-02-01 | 02 | 2 | MAC-02 | T-01-07 | Fewer than two active-BA eligible STAs or earliest-SU-transmittable MU-ineligible head traffic causes `HeHcf` to delegate to `Hcf::startFrameSequence(ac)`. | unit/integration `.test` | Quick run command | extended by Plan 02 Task 1 | planned |
| 01-02-02 | 02 | 2 | MAC-02 | T-01-09 | SU fallback keeps the existing ADDBA request path available, limits inactive-agreement retries to 3 attempts per burst, and resumes retries after `addbaFailureTimeout = 1s` cooldown. | unit/integration `.test` | Quick run command | extended by Plan 02 Task 1 | planned |

## Wave 0 Requirements

No standalone Wave 0 plan is required after the finalized plan revision. The focused test file and fixtures are created by Plan 01 Task 1 before MAC-01 production changes, then extended by Plan 02 Task 1 before MAC-02 production changes. Every implementation task has an `<automated>` verification command, and there are no unresolved Wave 0 creation references in the plan set.

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| None | MAC-01, MAC-02 | The finalized plans use automated `.test` coverage and source assertions. | The ADDBA cooldown value is concrete: `addbaFailureTimeout = 1s`, verified by Plan 02 Task 1 and Task 3 automated checks. |

## Validation Sign-Off

- [x] All tasks have `<automated>` verify commands.
- [x] Sampling continuity: no 3 consecutive tasks without automated verify.
- [x] No standalone Wave 0 gaps remain; test scaffolding is the first task in each plan before production changes.
- [x] No watch-mode flags.
- [x] Feedback latency < 60s for focused ADDBA validation where build cache allows.
- [x] `nyquist_compliant: true` set in frontmatter for the finalized plan structure.

**Approval:** finalized for planning; implementation verification occurs during `$gsd-execute-phase 01`.
