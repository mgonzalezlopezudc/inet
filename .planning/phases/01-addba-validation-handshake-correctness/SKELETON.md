# Walking Skeleton - 802.11ax DL MU OFDMA Correctness

**Phase:** 1
**Generated:** 2026-06-16

## Capability Proven End-to-End

A focused OMNeT++ unit test can prove that AP MAC scheduling admits only active-BA DL MU OFDMA packets and uses standard SU EDCA/ADDBA fallback for missing agreements.

## Architectural Decisions

| Decision | Choice | Rationale |
|---|---|---|
| Framework | Existing INET C++17 MAC modules on OMNeT++ with `.test` unit coverage | The project is an INET simulator correctness slice, not a new application scaffold. |
| State/data layer | EDCA pending queues plus `OriginatorBlockAckAgreementHandler` | Queue state and BA agreement state are the canonical sources for MU admission. |
| Auth | Not applicable; protocol state validation via receiver/TID BA agreements | There is no user authentication surface in this simulator MAC phase. |
| Deployment target | Local OMNeT++/INET test runner via `source setenv` and `inet.test.opp` | The phase is validated through reproducible local simulation tests. |
| Directory layout | Existing `src/inet/linklayer/ieee80211/mac/**` and `tests/unit/**` | Keeps protocol behavior in MAC ownership boundaries and test coverage in INET unit tests. |

## Stack Touched in Phase 1

- [ ] Project scaffold - existing INET build/test environment is reused.
- [ ] Routing - not applicable to this MAC-only skeleton.
- [ ] State/data read and write - pending queue reads/removals and BA agreement lookups/updates.
- [ ] Interaction - TXOP scheduling chooses MU or SU fallback based on packet/BA state.
- [ ] Deployment - local full-stack run command: `source setenv && export PYTHONPATH=/home/user/omnetpp-6.4.0/python:$PYTHONPATH && python3 -c "from inet.test.opp import run_opp_tests; run_opp_tests('tests/unit', filter='Ieee80211HeMuAddbaValidation_1.test', full_match=True)"`

## Out of Scope (Deferred to Later Slices)

- Sequential Block Ack duration and SIFS spacing corrections beyond preserving active sequence behavior.
- PHY RU path loss, attenuation, and sub-channel noise auditing.
- MU-BAR support.
- Uplink OFDMA.
- Subcarrier-level fading or interference modeling.

## Subsequent Slice Plan

Each later phase adds one vertical protocol-correctness slice on top of this skeleton without changing the core MAC ownership decisions:

- Phase 2: Sequential Block Ack spacing and duration verification.
- Phase 3: PHY RU behavior and attenuation auditing.
- Phase 4: Automated testing and example verification.
