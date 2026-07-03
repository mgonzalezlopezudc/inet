## Contents

* Important behavior of `debug-on-errors`
* Preconditions
* Define the simulation arguments safely
* Launch the simulation interactively
* Use Cmdenv or Qtenv under LLDB

---

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

