#!/usr/bin/env bash

set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
readonly OFDMA_EXAMPLE_DIR="${REPO_ROOT}/examples/ieee80211ax/dl_ofdma"
readonly OFDMA_RESULTS_FILE="${OFDMA_EXAMPLE_DIR}/results/General-#0.sca"

fail()
{
    echo "ofdma_example_validation: ERROR: $*" >&2
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

assert_config_line()
{
    local expected_line="$1"
    local result_file="$2"

    grep -qxF "$expected_line" "$result_file" || fail "expected config line '${expected_line}' in ${result_file}"
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

run_ofdma_example()
{
    rm -f "$OFDMA_RESULTS_FILE"

    (
        cd "$OFDMA_EXAMPLE_DIR"
        inet -u Cmdenv -f omnetpp.ini -c General -r 0
    )

    require_file "$OFDMA_RESULTS_FILE"
    assert_config_line "config sim-time-limit 0.6s" "$OFDMA_RESULTS_FILE"
    assert_zero_scalar "edcaCollisionDetected:count" "$OFDMA_RESULTS_FILE"
}

cd "$REPO_ROOT"

require_file "/home/user/omnetpp-6.4.0aipre2/setenv"
require_file "setenv"
require_executable "bin/inet_run_unit_tests"
require_file "tests/unit/Ieee80211HeMuSeqAck_1.test"
test -f examples/ieee80211ax/dl_ofdma/omnetpp.ini || fail "required file is missing: examples/ieee80211ax/dl_ofdma/omnetpp.ini"

export IN_NIX_SHELL="${IN_NIX_SHELL:-}"

set +u
# shellcheck disable=SC1090
source /home/user/omnetpp-6.4.0aipre2/setenv -f
# shellcheck disable=SC1091
source setenv -q
set -u

command -v inet >/dev/null || fail "required command is missing from PATH after environment bootstrap: inet"

bin/inet_run_unit_tests -m release -f Ieee80211HeMuSeqAck_1.test
run_ofdma_example
