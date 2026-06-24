#!/usr/bin/env bash

set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
readonly OFDMA_EXAMPLE_DIR="${REPO_ROOT}/examples/ieee80211/dl_ofdma"
readonly OFDMA_RESULTS_FILE="${OFDMA_EXAMPLE_DIR}/results/General-#0.sca"
readonly OFDMA_LOAD_FAST="--**.server.app[*].sendInterval=0.2ms"
readonly OFDMA_LOAD_BASELINE="--**.server.app[*].sendInterval=1ms"

fail()
{
    echo "he_mu_command_contract: ERROR: $*" >&2
    exit 1
}

require_file()
{
    test -f "$1" || fail "required file is missing: $1"
}

require_executable()
{
    test -x "$1" || fail "required executable is missing or not executable: $1"
}

assert_positive_scalar()
{
    local scalar_name="$1"
    local result_file="$2"

    awk -v scalar_name="$scalar_name" '
        $1 == "scalar" && $3 == scalar_name {
            found = 1
            if ($4 + 0 > 0)
                positive = 1
        }
        END {
            if (!found)
                exit 2
            if (!positive)
                exit 3
        }
    ' "$result_file" || fail "expected positive scalar ${scalar_name} in ${result_file}"
}

assert_zero_scalar()
{
    local scalar_name="$1"
    local result_file="$2"

    awk -v scalar_name="$scalar_name" '
        $1 == "scalar" && $3 == scalar_name {
            found = 1
            if ($4 + 0 != 0)
                nonzero = 1
        }
        END {
            if (!found)
                exit 2
            if (nonzero)
                exit 3
        }
    ' "$result_file" || fail "expected zero scalar ${scalar_name} in ${result_file}"
}

run_ofdma_load()
{
    local send_interval_override="$1"

    rm -f "$OFDMA_RESULTS_FILE"
    echo "Running OFDMA General run 0 with ${send_interval_override}"
    (
        cd "$OFDMA_EXAMPLE_DIR"
        inet -u Cmdenv -f omnetpp.ini -c General -r 0 "$send_interval_override"
    )

    require_file "$OFDMA_RESULTS_FILE"
    assert_positive_scalar "blockAckAgreementAdded:count" "$OFDMA_RESULTS_FILE"
    assert_zero_scalar "edcaCollisionDetected:count" "$OFDMA_RESULTS_FILE"
}

cd "$REPO_ROOT"

test -f /home/user/omnetpp-6.4.0aipre2/setenv || fail "required file is missing: /home/user/omnetpp-6.4.0aipre2/setenv"
test -f setenv || fail "required file is missing: setenv"
test -x bin/inet_run_unit_tests || fail "required executable is missing or not executable: bin/inet_run_unit_tests"
test -f tests/unit/Ieee80211HeMuAddbaValidation_1.test || fail "required file is missing: tests/unit/Ieee80211HeMuAddbaValidation_1.test"
test -f tests/unit/Ieee80211HeMuSeqAck_1.test || fail "required file is missing: tests/unit/Ieee80211HeMuSeqAck_1.test"
require_file "examples/ieee80211/dl_ofdma/omnetpp.ini"

export IN_NIX_SHELL="${IN_NIX_SHELL:-}"

set +u
# shellcheck disable=SC1090
source /home/user/omnetpp-6.4.0aipre2/setenv -f
# shellcheck disable=SC1091
source setenv -q
set -u

bin/inet_run_unit_tests -m release -f Ieee80211HeMuAddbaValidation_1.test
bin/inet_run_unit_tests -m release -f Ieee80211HeMuSeqAck_1.test

run_ofdma_load "$OFDMA_LOAD_FAST"
run_ofdma_load "$OFDMA_LOAD_BASELINE"
