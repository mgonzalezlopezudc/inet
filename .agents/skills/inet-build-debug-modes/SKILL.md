---
name: inet-build-debug-modes
description: Diagnose INET build modes, generated code, and model libraries. Use when Codex needs to build or troubleshoot INET release/debug artifacts, stale objects, generated message code, opp_makemake or make issues, library naming, custom project libraries, or release/debug mismatches before running tests or LLDB.
---

# INET build and debug modes

Use this skill when simulation, unit-test, or LLDB behavior may be caused by build artifacts rather than model logic.

## Core rule

Match the runner, libraries, and build mode. Do not debug C++ behavior with stale, optimized, or mismatched binaries.

## Workflow

1. Record repository root, dirty state, build mode, command, and relevant library paths.
2. Verify that the expected INET library exists before running simulations:
   * release: `$INET_ROOT/src/libINET.so`
   * debug: `$INET_ROOT/src/libINET_dbg.so`
3. Use `opp_run` with release libraries and `opp_run_dbg` with debug libraries.
4. If the project has custom C++ modules, load matching release/debug project libraries as well.
5. When source changes affect messages, packets, or NED-generated artifacts, check whether generated code and dependent objects were rebuilt.
6. If LLDB cannot resolve source lines, locals, or breakpoints, verify debug symbols, optimization level, loaded image path, and source/binary commit match.
7. Run the smallest build or test command that proves the artifact is fresh; use `inet-unit-tests` for unit-test execution and `inet-simulation-run` for simulation validation.

## Useful checks

```sh
git status --short
find src -maxdepth 1 -name 'libINET*.so' -ls
command -v opp_run opp_run_dbg inet_run_unit_tests
opp_run_dbg -h | head
```

For loaded-library suspicion under LLDB, inspect:

```text
(lldb) image list
(lldb) image lookup --name '<symbol>'
```

## Common failure modes

* `opp_run_dbg` loads `libINET.so` instead of `libINET_dbg.so`.
* `opp_run` loads a debug library or project debug library by accident.
* A project-specific model library is missing, stale, or built in the wrong mode.
* Generated message or NED-related code was not regenerated after definition changes.
* Breakpoints bind to a different source copy or remain unresolved because the wrong library was loaded.
* A previous build artifact masks a source change.
* ccache or incremental build behavior obscures whether a file was rebuilt; follow repository-specific test guidance when present.

## Do not

* Assume a compile succeeded because an old library exists.
* Mix release and debug binaries to save time during source-level debugging.
* Treat missing LLDB locals as proof of program state before checking debug info and optimization.
* Rebuild broadly without first identifying the smallest artifact that must be fresh, unless the user asks for a clean rebuild.

## Reporting

Include build command, working directory, mode, library paths, project libraries, generated-code considerations, exact runner command, loaded libraries when inspected, failure output, and whether the final run used release or debug artifacts consistently.
