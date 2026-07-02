---
name: inet-lldb-debugging
description: Debug INET and OMNeT++ simulations at the C++ source level using LLDB. Use when a simulation raises a runtime error, crashes, aborts, segfaults, hangs in a known code path, or requires breakpoints, watchpoints, stack inspection, or local-variable analysis. Use after ordinary Cmdenv logs, Qtenv inspection, packet captures, or event logs are insufficient.
---

# Debugging INET simulations with LLDB

Use this skill for source-level C++ debugging of an INET simulation.

Prefer launching the simulation directly under LLDB. This is more reliable
than attaching after an error and allows breakpoints to be configured before
simulation initialization.

## Important behavior of `debug-on-errors`

The command-line option:

```sh
--debug-on-errors=true
```

causes OMNeT++ to trigger a debugger trap when it detects a runtime error.

It does not start LLDB. The simulation must already be running under LLDB.

When LLDB stops at the debugger trap:

1. Do not immediately continue.
2. Record the stop reason.
3. Capture the complete backtrace.
4. Find the first relevant frame in project or INET source code.
5. Inspect its arguments and local variables.
6. Continue only after preserving the relevant state.

Continuing may allow exception handling and stack unwinding to obscure the
original context.

---

## Preconditions

Run from the same working directory used by a known-good simulation command.
Relative INI includes, input files, result directories, and NED paths may
depend on the working directory.

Verify the tools and debug library:

```sh
: "${INET_ROOT:?INET_ROOT must point to the INET project root}"

command -v lldb >/dev/null ||
    { echo "lldb is not available on PATH" >&2; exit 1; }

command -v opp_run_dbg >/dev/null ||
    { echo "opp_run_dbg is not available on PATH" >&2; exit 1; }

test -f "$INET_ROOT/src/libINET_dbg.so" ||
    {
        echo "Missing INET debug library:" >&2
        echo "  $INET_ROOT/src/libINET_dbg.so" >&2
        exit 1
    }
```

Assume OMNeT++, INET, and related tools such as `opp_run`, `opp_run_dbg`, and
`opp_repl` are already sourced when they are available on `PATH`. Do not prefix
every command with `source "$OMNETPP_ROOT/setenv"` or `source setenv`. Source an
environment script only as an explicit recovery step after validation fails, and
state why it was necessary.

Print tool versions for the investigation record:

```sh
lldb --version
opp_run_dbg -h | head
```

The INET and project libraries must contain debug information. If LLDB cannot
resolve source lines, local variables, or breakpoints, verify that the relevant
library was built in debug mode.

Do not use:

```text
opp_run_dbg + libINET.so
opp_run     + libINET_dbg.so
```

Use matching debug components:

```text
opp_run_dbg + libINET_dbg.so
```

If the project has custom C++ modules, also load its debug library, for example:

```sh
-l "$PROJECT_ROOT/src/libMyProject_dbg.so"
```

Do not substitute a release project library while diagnosing project code.

---

## Define the simulation arguments safely

Use a Bash array so that semicolon-separated NED paths and other arguments
remain single arguments:

```sh
INI_FILE="${INI_FILE:-omnetpp.ini}"
PROJECT_NED_ROOT="${PROJECT_NED_ROOT:-.}"
CONFIG="${CONFIG:?Set CONFIG to the OMNeT++ configuration name}"
RUN="${RUN:-0}"

LLDB_TARGET="$(command -v opp_run_dbg)"

SIM_ARGS=(
  -u Cmdenv
  -f "$INI_FILE"
  "--image-path=$INET_ROOT/images"
  "--ned-path=$PROJECT_NED_ROOT;$INET_ROOT/src;$INET_ROOT/examples;$INET_ROOT/tutorials;$INET_ROOT/showcases"
  --debug-on-errors=true
  -l "$INET_ROOT/src/libINET_dbg.so"
  -c "$CONFIG"
  -r "$RUN"
)
```

Append a project debug library when required:

```sh
PROJECT_DEBUG_LIBRARY="${PROJECT_DEBUG_LIBRARY:-}"

if [[ -n "$PROJECT_DEBUG_LIBRARY" ]]; then
    test -f "$PROJECT_DEBUG_LIBRARY" ||
        {
            echo "Project debug library not found:" >&2
            echo "  $PROJECT_DEBUG_LIBRARY" >&2
            exit 1
        }

    SIM_ARGS+=(-l "$PROJECT_DEBUG_LIBRARY")
fi
```

Append temporary diagnostic overrides when needed:

```sh
SIM_ARGS+=(
  --cmdenv-express-mode=false
  --cmdenv-event-banners=true
  --cmdenv-event-banner-details=true
)
```

For a failure that occurs early, optionally restrict the simulation:

```sh
SIM_ARGS+=(--sim-time-limit=5s)
```

Do not add a time limit that prevents the failure from occurring.

---

## Launch the simulation interactively

Start LLDB with the executable and all simulation arguments:

```sh
lldb -- "$LLDB_TARGET" "${SIM_ARGS[@]}"
```

At the LLDB prompt, verify the target arguments:

```text
(lldb) settings show target.run-args
```

Then start the simulation:

```text
(lldb) run
```

The equivalent long form is:

```text
(lldb) process launch
```

Use `--` when supplying arguments directly to `process launch`:

```text
(lldb) process launch -- <simulation-arguments>
```

The Bash-array launch is preferred because it avoids manually reconstructing
the long command inside LLDB.

---

## Use Cmdenv or Qtenv under LLDB

Use Cmdenv by default:

```sh
SIM_ARGS=(
  -u Cmdenv
  ...
)
```

Cmdenv is preferable when:

* Reproducing an automated failure.
* Capturing debugger output in a terminal.
* Running repeatedly to a breakpoint.
* Debugging without graphical interaction.

Use Qtenv only when visualization or interactive model inspection is also
needed:

```sh
SIM_ARGS=(
  -u Qtenv
  ...
)
```

When LLDB stops, the Qtenv interface will also stop responding until the
debugged process is continued.

---

## Inspect a runtime-error stop

When the simulation stops, begin with:

```text
(lldb) process status
(lldb) thread list
(lldb) thread backtrace all
```

For the selected thread:

```text
(lldb) bt
```

Inspect the current frame:

```text
(lldb) frame select
(lldb) source list
(lldb) frame variable
```

Navigate the stack:

```text
(lldb) up
(lldb) down
(lldb) frame select <frame-number>
```

The top frame may be the OMNeT++ debugger-trap or error-handling implementation.
Move upward until reaching the first frame in:

* The project source tree.
* `$INET_ROOT/src`.
* A relevant OMNeT++ API call made by project or INET code.

Do not treat the trap function itself as the root cause.

For the relevant frame, record:

```text
(lldb) frame variable
(lldb) source list
```

Inspect one variable without evaluating arbitrary code:

```text
(lldb) frame variable <variable-name>
```

Examples:

```text
(lldb) frame variable packet
(lldb) frame variable msg
(lldb) frame variable state
(lldb) frame variable retryCount
(lldb) frame variable this
```

Prefer `frame variable` over `expression` when merely reading variables,
because it does not execute code in the debugged process.

---

## Evaluate expressions cautiously

Use:

```text
(lldb) expression -- <C++ expression>
```

or its aliases:

```text
(lldb) expr -- <C++ expression>
(lldb) p <C++ expression>
```

Examples:

```text
(lldb) expression -- retryCount
(lldb) expression -- packet == nullptr
(lldb) expression -- queue.getLength()
```

Expression evaluation may call functions or otherwise execute code in the
stopped process. Avoid expressions that:

* Mutate simulation state.
* Remove or insert packets.
* Advance iterators with side effects.
* Schedule or cancel messages.
* Emit signals.
* Change module parameters.
* Continue simulation execution indirectly.

Do not “fix” a variable inside LLDB and then treat the resulting run as valid
simulation evidence unless the user explicitly requests an experimental
state modification.

---

## Set a breakpoint by source location

Set a breakpoint before running:

```text
(lldb) breakpoint set --file <source-file.cc> --line <line-number>
```

Example:

```text
(lldb) breakpoint set --file Ieee80211Mac.cc --line 420
```

Use the source filename rather than an absolute path when debug information
contains paths from a different build directory.

List breakpoints and verify that they resolved:

```text
(lldb) breakpoint list
```

A breakpoint with zero locations is pending or unresolved.

Pending breakpoints may resolve when `libINET_dbg.so` or a project library is
loaded. Check again after simulation startup:

```text
(lldb) breakpoint list
```

If the breakpoint remains unresolved after the relevant library has loaded,
check:

* Source filename.
* Line number.
* Build mode.
* Loaded library.
* Debug information.
* Whether the source was optimized or inlined.
* Whether the executing code comes from a different library copy.

---

## Set a breakpoint by function name

Use a fully qualified C++ function name when known:

```text
(lldb) breakpoint set --name 'inet::SomeClass::someMethod'
```

Example form:

```text
(lldb) breakpoint set \
  --name 'inet::physicallayer::SomeReceiver::computeReceptionResult'
```

The exact class and namespace must come from the source code. Do not guess a
symbol name when it can be found with source search.

Search loaded symbols:

```text
(lldb) image lookup --name 'inet::SomeClass::someMethod'
```

List loaded executable images and shared libraries:

```text
(lldb) image list
```

Use a function-name regular expression only when several related methods are
intentionally being investigated:

```text
(lldb) breakpoint set --func-regex 'Ieee80211.*::.*'
```

Broad regular-expression breakpoints may create many locations and severely
slow the simulation.

---

## Set a conditional breakpoint

Use a condition when the relevant function is called frequently:

```text
(lldb) breakpoint set \
  --file <source-file.cc> \
  --line <line-number> \
  --condition '<C++ boolean expression>'
```

Example:

```text
(lldb) breakpoint set \
  --file SomeScheduler.cc \
  --line 250 \
  --condition 'retryCount > 3'
```

List the resulting breakpoint:

```text
(lldb) breakpoint list
```

Conditions are evaluated each time the breakpoint location is reached and can
slow a frequently executed simulation path.

Keep conditions:

* Side-effect free.
* Narrow.
* Based on variables available in that frame.
* Independent of unstable temporary objects.

---

## Set and use watchpoints

A watchpoint stops when a selected memory value changes.

First stop in a frame where the variable is visible, then run:

```text
(lldb) watchpoint set variable <variable-name>
```

Example:

```text
(lldb) watchpoint set variable state
```

List watchpoints:

```text
(lldb) watchpoint list
```

Continue:

```text
(lldb) continue
```

When the watchpoint triggers:

```text
(lldb) process status
(lldb) bt
(lldb) frame variable
```

Watchpoints are limited hardware resources and may significantly slow
execution. Use them only for a small number of variables.

A watchpoint on a local variable becomes invalid when that variable’s stack
frame ends. For long-lived state, prefer a field of a long-lived module object.

---

## Step through the source

After stopping at a relevant breakpoint:

```text
(lldb) next
```

steps over the current source line.

```text
(lldb) step
```

steps into a called function.

```text
(lldb) finish
```

runs until the current function returns.

```text
(lldb) continue
```

continues until the next breakpoint, trap, signal, or process exit.

Avoid instruction-level stepping unless source-level stepping is unavailable:

```text
(lldb) stepi
(lldb) nexti
```

Stepping into heavily templated C++ or standard-library code may obscure the
model logic. Use `finish` to return to the relevant INET or project frame.

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
