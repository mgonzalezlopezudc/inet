---
name: inet-unit-tests
description: Run INET unit tests in this repository. Use when asked to execute, filter, diagnose, or report INET C++ unit tests, including IEEE 802.11 HE tests.
---

# Running INET unit tests

Run unit-test commands from the repository root.

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

When saving diagnostic output through `tee`, enable `pipefail` and preserve the test runner's exit status.

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

Report the build mode, regex filter, exit status, test summary, first relevant failure, and captured log path.
