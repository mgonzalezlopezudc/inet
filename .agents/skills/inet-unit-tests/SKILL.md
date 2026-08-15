---
name: inet-unit-tests
description: Build INET and run unit tests in this repository. Use when asked to build for, execute, filter, diagnose, or report INET C++ unit tests, including IEEE 802.11 HE tests.
---

# Running INET unit tests

Run unit-test commands from the repository root.

Do not use `./runtest` or infer a test runner path from another project. The repository-supported entry point is `inet_run_unit_tests` from the repository root.

## Rebuild INET before testing changed code

After changing compiled INET source or an input that generates compiled code, rebuild INET explicitly before running unit tests. Use the same mode for the build and test:

```sh
make MODE=release -j$(nproc)
inet_run_unit_tests \
  -m release \
  -f '<filter>'
```

Use `MODE=debug` with `-m debug` for debug tests. Do not run the test command if the matching INET build fails.

The test runners default to `debug` when `-m` is omitted. After changing compiled INET source, always specify `-m` explicitly; never rely on the runner default. The same build/library rule applies to module tests, which use the same test-task machinery:

```sh
make MODE=debug -j$(nproc)
inet_run_unit_tests -m debug -f '<filter>'
inet_run_module_tests -m debug -f '<filter>'
```

For release validation, use `make MODE=release -j$(nproc)` together with `-m release` on the selected test runner.

`inet_run_unit_tests` generates and builds the selected test executables, but it does not rebuild `src/libINET.so` or `src/libINET_dbg.so`. Its test-local build is not evidence that the INET library contains the current source changes.

Editing only a `.test` file does not require rebuilding INET when no compiled INET source, generated-code input, or test support library changed. The test runner still rebuilds that test's generated executable.

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
3. Record the preceding INET build command and exit status when compiled inputs changed.
4. Record the regex filter.
5. Identify failing test names.
6. Separate INET build failures, test-executable build failures, and test assertion failures.
7. Search earlier output for the first relevant error.
8. Do not treat subsequent cascading failures as independent root causes without evidence.
9. Rerun a narrower regex only when it helps isolate the failure.

Report the build mode, INET build command and status when required, regex filter, test exit status, test summary, first relevant failure, and captured log path.
