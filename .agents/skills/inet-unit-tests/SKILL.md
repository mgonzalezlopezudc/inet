---
name: inet-unit-tests
description: Run INET unit tests in this repository. Use when asked to execute, filter, diagnose, or report INET C++ unit tests, including IEEE 802.11 HE tests.
---

# Running INET unit tests

## Working directory

Run all unit-test commands from the repository root.

Verify the location before running:

```sh
test -d src &&
test -d tests ||
    { echo "Run this skill from the repository root" >&2; exit 1; }
```

Adapt the directory check only when the repository layout is known to differ.

## Disable ccache

Disable ccache before building or running tests in this workspace:

```sh
export CCACHE_DISABLE=1
```

Apply this in the same shell that invokes the test command.

Do not omit this setting merely because ccache is not currently producing visible output.

## Known-good command

A known-good command for the relevant IEEE 802.11 HE unit tests is:

```sh
inet_run_unit_tests \
  -m release \
  -f '(Ieee80211He|HeDlScheduler).*\.test'
```

## Filter rules

`inet_run_unit_tests -f` accepts one regular-expression filter.

Do not pass multiple `-f` arguments for several test groups. Combine groups using regular-expression alternation:

```sh
inet_run_unit_tests \
  -m release \
  -f '(FirstTestGroup|SecondTestGroup|ThirdTestGroup).*\.test'
```

Quote the expression so the shell does not interpret regex metacharacters.

## Capturing output

For a diagnostic run:

```sh
mkdir -p logs
set -o pipefail
export CCACHE_DISABLE=1

inet_run_unit_tests \
  -m release \
  -f '(Ieee80211He|HeDlScheduler).*\.test' \
  2>&1 | tee logs/unit-tests.log

status=$?
echo "inet_run_unit_tests exit status: $status"
exit "$status"
```

Preserve the exit status from the test runner rather than the status from `tee`.

## Matplotlib warning

A warning stating that `$HOME/.config/matplotlib` is not writable is non-fatal when Matplotlib reports that it is using a temporary cache under `/tmp`.

Do not classify the test run as failed solely because of this warning. Determine success from:

* The test runner’s exit status.
* The reported pass/fail summary.
* Actual compilation or test failures.

## Failure analysis

When tests fail:

1. Preserve the complete command and output.
2. Record the build mode.
3. Record the regex filter.
4. Identify failing test names.
5. Separate build failures from test assertion failures.
6. Search earlier output for the first relevant error.
7. Do not treat subsequent cascading failures as independent root causes without evidence.
8. Rerun a narrower regex only when it helps isolate the failure.

## Reporting

Include:

* Working directory.
* `CCACHE_DISABLE` setting.
* Exact command.
* Build mode.
* Regex filter.
* Exit status.
* Number of passing, failing, skipped, or unavailable tests when reported.
* First relevant failure.
* Path to the captured log.
* Any non-fatal warnings that were intentionally ignored.

