---
phase: 01
slug: addba-validation-handshake-correctness
status: draft
nyquist_compliant: false
wave_0_complete: false
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
| 01-00-01 | 01 | 0 | MAC-01 | T-01-01 | Non-QoS, invalid TID, or no-active-BA packets are not inserted into an HE MU container. | unit `.test` | Quick run command | no - W0 | pending |
| 01-00-02 | 01 | 0 | MAC-01 | T-01-02 | ADDBA agreement placeholders without a received ADDBA Response are treated as inactive. | unit `.test` | Quick run command | no - W0 | pending |
| 01-00-03 | 02 | 0 | MAC-02 | T-02-01 | Fewer than two active-BA eligible STAs causes `HeHcf` to delegate to `Hcf::startFrameSequence(ac)`. | unit/integration `.test` | Quick run command | no - W0 | pending |
| 01-00-04 | 02 | 0 | MAC-02 | T-02-02 | SU fallback keeps the existing ADDBA request path available for packets missing active BA. | unit/integration `.test` | Quick run command | partial - existing code only | pending |

## Wave 0 Requirements

- [ ] `tests/unit/Ieee80211HeMuAddbaValidation_1.test` - focused tests for MAC-01 and MAC-02.
- [ ] Test doubles or fixtures for `IOriginatorBlockAckAgreementHandler` and `OriginatorBlockAckAgreement` active/inactive states.
- [ ] Expected stdout assertions for MU eligibility, inactive ADDBA Response state, and SU fallback path.

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| ADDBA failure cooldown exact duration | MAC-02 | CONTEXT.md leaves the cooldown value to the agent's discretion. | Verify the plan either defers the exact cooldown value outside Phase 1 or assigns a concrete constant with tests. |

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies.
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify.
- [ ] Wave 0 covers all MISSING references.
- [ ] No watch-mode flags.
- [ ] Feedback latency < 60s for focused ADDBA validation.
- [ ] `nyquist_compliant: true` set in frontmatter after Wave 0 is implemented and verified.

**Approval:** pending
