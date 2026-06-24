# IEEE 802.11ax Validation Contracts

## TST-01 HE MU Command Contract

Phase 04 plan 04-01 owns the `TST-01` command contract for HE MU protocol sequence regression coverage. Run it from the repository root:

```bash
tests/validation/ieee80211/he_mu_command_contract.sh
```

The script bootstraps the required environment with:

```bash
source /home/user/omnetpp-6.4.0aipre2/setenv -f
source setenv -q
```

It then runs these focused checks through the in-repository unit-test runner:

```bash
bin/inet_run_unit_tests -m release -f Ieee80211HeMuAddbaValidation_1.test
bin/inet_run_unit_tests -m release -f Ieee80211HeMuSeqAck_1.test
```

`Ieee80211HeMuAddbaValidation_1.test` verifies active ADDBA agreement admission before HE MU container mutation. `Ieee80211HeMuSeqAck_1.test` verifies sequential Block Ack/BAR exchange behavior, timeout recovery, and SIFS-derived timing evidence.

The contract also runs the OFDMA `General` example twice, once with `--**.server.app[*].sendInterval=0.2ms` and once with `--**.server.app[*].sendInterval=1ms`. For each load run it asserts `blockAckAgreementAdded:count > 0` and `edcaCollisionDetected:count == 0` in `examples/ieee80211/dl_ofdma/results/General-#0.sca`.

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
bash -lc 'source /home/user/omnetpp-6.4.0aipre2/setenv -f && source setenv -q && bin/inet_run_all_tests -m release'
```

Run this broad gate after the focused `TST-01` and `TST-02` validation contracts when validating Phase 4 as a whole.

## TST-02 OFDMA Example Validation

Phase 04 plan 04-02 owns the `TST-02` example-level validation contract. Run it from the repository root:

```bash
tests/validation/ieee80211/ofdma_example_validation.sh
```

The script bootstraps the OMNeT++ and INET environments, runs the deterministic sequential Block Ack timing oracle, then executes the pinned OFDMA example:

```bash
bin/inet_run_unit_tests -m release -f Ieee80211HeMuSeqAck_1.test
cd examples/ieee80211/dl_ofdma
inet -u Cmdenv -f omnetpp.ini -c General -r 0
```

Expected output is a passing `Ieee80211HeMuSeqAck_1.test`, an OFDMA `General` run that reaches the 0.6s simulation limit, and a fresh `examples/ieee80211/dl_ofdma/results/General-#0.sca` artifact. Timing compliance is gated by `Ieee80211HeMuSeqAck_1.test`; the example-level timing proxy is asserted with `config sim-time-limit 0.6s`; collision compliance is gated by every `edcaCollisionDetected:count == 0`.

The command exits non-zero on missing prerequisites, failed unit timing oracle, failed simulation execution, missing `General-#0.sca`, missing `config sim-time-limit 0.6s`, missing `edcaCollisionDetected:count`, or any non-zero collision count.

## Phase Boundary

The `TST-01` command contract was completed in Phase 04 plan 04-01. The `TST-02` OFDMA example scalar assertion contract is completed in `04-02`.
