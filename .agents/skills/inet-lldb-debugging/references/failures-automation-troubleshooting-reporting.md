## Contents

* Debug a segmentation fault or abort
* Debug initialization failures
* Capture a first-stop backtrace non-interactively
* Add an automatic backtrace to an interactive session
* Attach LLDB to an already running simulation
* Inspect a core dump
* Troubleshoot unresolved breakpoints
* Troubleshoot missing local variables
* Investigation procedure
* Reporting requirements

---

## Debug a segmentation fault or abort

LLDB normally stops automatically on fatal signals such as a segmentation
fault or abort.

At the stop:

```text
(lldb) process status
(lldb) thread backtrace all
(lldb) bt
(lldb) frame variable
```

Identify:

* Stop signal.
* Faulting instruction.
* First project or INET frame.
* Pointer values.
* Relevant packet or message pointer.
* Object lifetime.
* Container or iterator state.
* Calling event or callback.

Common evidence to check includes:

```text
(lldb) frame variable this
(lldb) frame variable msg
(lldb) frame variable packet
(lldb) frame variable pointer
```

Do not assume a null pointer is the root cause merely because it appears in a
later frame. Follow the call chain to determine where the invalid value was
created or accepted.

---

## Debug initialization failures

Initialization errors may occur before the first simulation event.

Launch under LLDB normally:

```text
(lldb) run
```

When stopped:

```text
(lldb) bt
(lldb) thread backtrace all
```

Look for the first frame involving:

* `initialize()`.
* Module construction.
* Parameter parsing.
* NED type creation.
* Gate or interface lookup.
* Module-path lookup.
* XML or external-file loading.
* INET configurators.

Initialization failures may have no useful event number. Report the
initialization stage and module path instead.

---

## Capture a first-stop backtrace non-interactively

For an automated first-pass diagnosis, use LLDB batch mode with stop hooks:

```sh
mkdir -p logs
set -o pipefail

lldb \
  --no-lldbinit \
  --batch \
  --one-line 'target stop-hook add --one-liner "process status"' \
  --one-line 'target stop-hook add --one-liner "thread backtrace all"' \
  --one-line run \
  -- "$LLDB_TARGET" "${SIM_ARGS[@]}" \
  2>&1 | tee logs/lldb-first-stop.log
```

This is useful for:

* Capturing a backtrace from a reproducible crash.
* Capturing the `debug-on-errors` trap.
* Determining which source frame needs interactive inspection.
* Preserving debugger output in automated environments.

After reviewing the batch backtrace, rerun interactively to inspect variables
or step through the code.

Do not rely only on LLDB’s own shell exit status to determine whether the
simulation succeeded. Inspect:

* Process status.
* Stop reason.
* Simulation output.
* Whether the process exited normally.
* Whether LLDB stopped on a trap, breakpoint, or fatal signal.

---

## Add an automatic backtrace to an interactive session

At the LLDB prompt:

```text
(lldb) target stop-hook add
Enter your debugger command(s). Type 'DONE' to end.
> process status
> thread backtrace all
> DONE
```

LLDB will print the process status and all-thread backtrace whenever the target
stops.

This is useful when the failure is intermittent or several breakpoints may be
hit.

---

## Attach LLDB to an already running simulation

Prefer launching under LLDB. Attach only when launching under the debugger is
impractical.

Find the process ID:

```sh
pgrep -af opp_run
```

Attach:

```sh
lldb -p <pid>
```

or inside LLDB:

```text
(lldb) process attach --pid <pid>
```

After attaching:

```text
(lldb) process status
(lldb) thread list
(lldb) thread backtrace all
```

Attaching to an unrelated or non-child process may be restricted by the
operating system.

If attach fails:

* Report the exact LLDB error.
* Prefer relaunching the simulation under LLDB.
* Do not weaken system-wide tracing or security settings without explicit
  authorization.

After attaching before an OMNeT++ error, `--debug-on-errors=true` must already
be active in the simulation configuration for the OMNeT++ debugger trap to
occur.

---

## Inspect a core dump

When a core dump is available:

```sh
lldb \
  -c <core-file> \
  "$(command -v opp_run_dbg)"
```

Then inspect:

```text
(lldb) process status
(lldb) thread list
(lldb) thread backtrace all
(lldb) frame select <frame-number>
(lldb) frame variable
(lldb) image list
```

The executable and shared libraries should correspond to the exact binaries
that produced the core. Rebuilt or replaced libraries may cause incorrect
symbols and source locations.

Preserve:

* Core file.
* Exact executable.
* Exact INET debug library.
* Exact project debug libraries.
* Build identifiers or commit.
* Original simulation command.

---

## Troubleshoot unresolved breakpoints

If a breakpoint reports zero locations:

```text
(lldb) breakpoint list
(lldb) image list
```

Check whether the relevant library is loaded.

Search for the symbol:

```text
(lldb) image lookup --name '<fully-qualified-function-name>'
```

Try a file basename rather than a full source path:

```text
(lldb) breakpoint set --file SomeModule.cc --line 123
```

If code is inlined or file-and-line resolution is problematic:

```text
(lldb) settings set target.inline-breakpoint-strategy always
```

Then recreate the breakpoint.

Also verify:

* The library is a debug build.
* The source and binary are from the same commit.
* The function was not optimized away.
* The module path actually instantiates the expected implementation.
* A different library copy is not being loaded.
* The breakpoint line contains executable code.

Do not assume that a successfully created logical breakpoint has an actual
resolved location.

---

## Troubleshoot missing local variables

A variable may appear as unavailable or optimized out when:

* The library was not built with debug information.
* Compiler optimization removed or transformed it.
* The selected stack frame is incorrect.
* The source and binary do not match.
* The variable is out of scope.

Try:

```text
(lldb) frame variable
(lldb) frame select <another-frame>
(lldb) source list
```

Verify the loaded binary:

```text
(lldb) image list
```

Do not infer a variable’s value from stale source code when LLDB cannot recover
it from the executing binary.

---

## Investigation procedure

Use the following sequence:

1. Reproduce the problem once outside LLDB and preserve the exact command.
2. Confirm the same configuration and run number.
3. Confirm the debug runner and debug libraries.
4. Launch the simulation under LLDB with `--debug-on-errors=true`.
5. Set narrow breakpoints before running when the suspected code is known.
6. Run until the debugger trap, breakpoint, or fatal signal.
7. Capture `process status`.
8. Capture `thread backtrace all`.
9. Identify the first project or INET source frame.
10. Inspect frame arguments and locals.
11. Determine what invariant, pointer, state, or input is invalid.
12. Add a conditional breakpoint or watchpoint to locate where it first becomes
    invalid.
13. Rerun from the beginning.
14. Record evidence before continuing or modifying state.
15. Correlate the debugger stop with Cmdenv logs, event numbers, simulation
    time, packet captures, or event logs when relevant.

Do not stop analysis at the frame that detects the error. Locate the earliest
supported cause that made the invalid state possible.

---

## Reporting requirements

Report:

* Exact LLDB launch command.
* Working directory.
* `INET_ROOT`.
* INI file.
* Configuration and run number.
* `opp_run_dbg` path.
* INET debug-library path.
* Project debug-library paths.
* LLDB version.
* Stop reason or signal.
* Selected thread.
* Complete or relevant backtrace.
* First relevant project or INET frame.
* Source file and line.
* Breakpoints and conditions used.
* Watchpoints used.
* Relevant arguments and local variables.
* Simulation time and event number, when available.
* Whether the failure occurred during initialization or event processing.
* Whether the conclusion is direct debugger evidence or an inference.
* Path to any saved LLDB log or core dump.

Do not report private memory contents, credentials, or unrelated environment
variables that happen to be visible in the debugger.

Do not claim that a source line is the root cause merely because it is the
faulting line. Explain the invalid state and how the available evidence shows
where it originated.
