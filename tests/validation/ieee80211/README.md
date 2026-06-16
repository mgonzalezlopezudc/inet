# IEEE 802.11ax Validation Contracts

## TST-01 HE MU Command Contract

Phase 04 plan 04-01 owns the `TST-01` command contract for HE MU protocol sequence regression coverage. Run it from the repository root:

```bash
tests/validation/ieee80211/he_mu_command_contract.sh
```

The script bootstraps the required environment with:

```bash
source /home/user/omnetpp-6.4.0/setenv -f
source setenv -q
```

It then runs these focused checks through the in-repository unit-test runner:

```bash
bin/inet_run_unit_tests -m release -f Ieee80211HeMuAddbaValidation_1.test
bin/inet_run_unit_tests -m release -f Ieee80211HeMuSeqAck_1.test
```

`Ieee80211HeMuAddbaValidation_1.test` verifies active ADDBA agreement admission before HE MU container mutation. `Ieee80211HeMuSeqAck_1.test` verifies sequential Block Ack/BAR exchange behavior, timeout recovery, and SIFS-derived timing evidence.

The contract also runs the OFDMA `General` example twice, once with `--**.server.app[*].sendInterval=0.2ms` and once with `--**.server.app[*].sendInterval=1ms`. For each load run it asserts `blockAckAgreementAdded:count > 0` and `edcaCollisionDetected:count == 0` in `examples/ieee80211/ofdma/results/General-#0.sca`.

## Local And CI Commands

Quick local rerun:

```bash
tests/validation/ieee80211/he_mu_command_contract.sh
```

CI gate command:

```bash
bash -lc 'tests/validation/ieee80211/he_mu_command_contract.sh'
```

The command exits non-zero on missing prerequisites, failed unit tests, failed simulation execution, missing scalar evidence, no Block Ack agreement evidence, or any EDCA collision count above zero.

## Phase 4 Broad Gate

The Phase 4 broad gate is the roadmap-level compile/pass gate for automated tests under `tests/`: the repository-wide automated test lane after the standard environment bootstrap.

```bash
bash -lc 'source /home/user/omnetpp-6.4.0/setenv -f && source setenv -q && bin/inet_run_all_tests -m release'
```

Run this broad gate after the focused `TST-01` and `TST-02` validation contracts when validating Phase 4 as a whole.

## Phase Boundary

This contract completes `TST-01` for Phase 04 plan 04-01. `TST-02` example-level scalar assertions and broader validation scenario hardening are planned in `04-02`.
