---
phase: 03
slug: phy-layer-ru-behavior-attenuation-auditing
status: finalized
nyquist_compliant: true
wave_0_complete: true
created: 2026-06-17
---

# Phase 03 - Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | OMNeT++ `opp_test` plus INET Python runners |
| **Config file** | Per-test `%inifile` sections in `.test` files |
| **Quick run command** | `source setenv && export PYTHONPATH=/home/user/omnetpp-6.4.0/python:$PYTHONPATH && python3 -c "from inet.test.opp import run_opp_tests; run_opp_tests('tests/unit', filter='Ieee80211HeMuRuAttenuation_1.test', full_match=True)"` |
| **Full suite command** | `source setenv && export PYTHONPATH=/home/user/omnetpp-6.4.0/python:$PYTHONPATH && ./bin/inet_run_unit_tests` |
| **Estimated runtime** | Quick: under 60 seconds after build cache warmup; full suite: project-dependent |

## Sampling Rate

- **After every task commit:** Run the appropriate quick command for the task that just changed: baseline RU/receiver checks after Task 1, RU attenuation regression after Task 2, and RU noise isolation regression after Task 3.
- **After every plan wave:** Run the full suite command.
- **Before `$gsd-verify-work`:** New RU attenuation and noise isolation tests must be green.
- **Max feedback latency:** Keep focused RU assertions under 60 seconds where possible.

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 03-01-01 | 01 | 1 | PHY-01 | T-03-01 | RU band derivation in the medium is scheduler-authoritative, invalid mappings fail fast, and per-RU power audits are observable. | source assertion + unit `.test` baseline | Quick run command | verified by Plan 03 Task 1 and extended by Plan 03 Task 2 | planned |
| 03-01-02 | 01 | 1 | PHY-02 | T-03-03 | Same-MU sub-transmissions remain isolated, the main MU transmission stays physically suppressed, and RU-local noise does not leak across sub-transmissions. | unit `.test` / source assertion | Quick run command | created by Plan 03 Task 3 | planned |

## Wave 0 Requirements

No standalone Wave 0 plan is required after the finalized plan revision. The phase plan applies the medium-side production change first, then extends the focused RU attenuation and noise-isolation tests, and every implementation task has an `<automated>` verification command.

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| None | PHY-01, PHY-02 | The finalized plan uses automated `.test` coverage plus source-level assertions for the medium-side audit path. | The plan's quick command covers the focused RU attenuation regression; the full suite covers broader HE MU receiver/path interplay. |

## Validation Sign-Off

- [x] All tasks have `<automated>` verify commands.
- [x] Sampling continuity: no 3 consecutive tasks without automated verify.
- [x] No standalone Wave 0 gaps remain; test scaffolding starts before production changes.
- [x] No watch-mode flags.
- [x] Feedback latency < 60s for focused RU validation where build cache allows.
- [x] `nyquist_compliant: true` set in frontmatter for the finalized plan structure.

**Approval:** finalized for planning; implementation verification occurs during `$gsd-execute-phase 03`.
