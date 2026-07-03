---
name: inet-lldb-debugging
description: Debug INET and OMNeT++ simulations at the C++ source level using LLDB. Use when a simulation raises a runtime error, crashes, aborts, segfaults, hangs in a known code path, or requires breakpoints, watchpoints, stack inspection, or local-variable analysis after Cmdenv logs, Qtenv inspection, packet captures, event logs, or results are insufficient.
---

# Debugging INET simulations with LLDB

Use this skill for source-level C++ debugging of INET or project simulation code. Prefer launching the simulation under LLDB instead of attaching after an error, because breakpoints and stop hooks can be configured before initialization.

## Non-negotiable setup

Use matching debug components:

```text
opp_run_dbg + libINET_dbg.so + debug project libraries
```

Do not use:

```text
opp_run_dbg + libINET.so
opp_run     + libINET_dbg.so
```

`--debug-on-errors=true` makes OMNeT++ trigger a debugger trap on runtime errors. It does not start LLDB. The simulation must already be running under LLDB.

Assume OMNeT++, INET, and related tools are already sourced when `opp_run_dbg` is on `PATH`. Source an environment script only as an explicit recovery step after validation fails, and state why.

## Minimal workflow

1. Reproduce the problem once outside LLDB and preserve the exact command.
2. Confirm working directory, INI file, configuration, run number, NED path, and debug libraries.
3. Launch `opp_run_dbg` under LLDB with `--debug-on-errors=true` and `-l "$INET_ROOT/src/libINET_dbg.so"`.
4. Add narrow breakpoints before `run` when the suspected code path is known.
5. On a trap, breakpoint, abort, or signal, record `process status` and `thread backtrace all` before continuing.
6. Move past OMNeT++ trap/error-handling frames to the first relevant project or INET source frame.
7. Use `frame variable` before `expression` when reading state, because expressions may execute code in the stopped process.
8. Use conditional breakpoints or watchpoints to find where invalid state first appears.
9. Correlate the debugger stop with Cmdenv event number, simulation time, packet captures, event logs, or result files when relevant.
10. Report whether the conclusion is direct debugger evidence or inference.

## Safe inspection rules

* Do not immediately continue after a `debug-on-errors` trap; stack unwinding can hide the original context.
* Do not treat the trap function itself as the root cause.
* Do not mutate simulation state from LLDB unless the user explicitly requests an experimental state change.
* Avoid expressions that schedule/cancel messages, mutate packets, change parameters, advance iterators with side effects, or emit signals.
* Do not claim a faulting source line is the root cause without explaining the invalid state and where it originated.
* Do not weaken system-wide tracing or security settings to attach LLDB without explicit authorization.

## Read references on demand

Load only the reference needed for the current problem:

* [setup-and-launch.md](references/setup-and-launch.md): `debug-on-errors`, preconditions, tool checks, debug-library checks, Bash-array simulation arguments, launch patterns, and Cmdenv/Qtenv selection under LLDB.
* [inspection-breakpoints-and-stepping.md](references/inspection-breakpoints-and-stepping.md): runtime-error inspection, variable reading, expression cautions, source/function/conditional breakpoints, watchpoints, and stepping.
* [failures-automation-troubleshooting-reporting.md](references/failures-automation-troubleshooting-reporting.md): segfaults, aborts, initialization failures, batch first-stop backtraces, stop hooks, attaching, core dumps, unresolved breakpoints, missing locals, investigation sequence, and reporting requirements.

Use `rg -n '^##|keyword'` in reference files before reading large ranges.

## Report essentials

Include exact LLDB launch command, working directory, `INET_ROOT`, INI file, configuration, run number, `opp_run_dbg` path, INET debug-library path, project debug libraries, LLDB version, stop reason or signal, relevant backtrace, selected source frame, source file and line, breakpoints/watchpoints used, relevant variables, simulation time/event number when available, and path to any saved LLDB log or core dump.
