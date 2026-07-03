---
name: inet-simulation-run
description: Run and diagnose INET simulations using Cmdenv or Qtenv. Use for normal simulation execution, short diagnostic runs, initialization failures, runtime errors, or requests for interactive graphical debugging.
---

# Running INET simulations

Use Cmdenv by default. Use Qtenv only when interactive inspection is useful or the user explicitly asks for it.

## Environment

The following variables are expected:

```sh
INET_ROOT=<path-to-inet>
INI_FILE="${INI_FILE:-omnetpp.ini}"
PROJECT_NED_ROOT="${PROJECT_NED_ROOT:-.}"
CONFIG=<configuration-name>
RUN=<run-number>
```

Validate the important inputs:

```sh
: "${INET_ROOT:?INET_ROOT must point to the INET project root}"

command -v opp_run >/dev/null ||
    { echo "opp_run is not available on PATH" >&2; exit 1; }

test -f "$INET_ROOT/src/libINET.so" ||
    { echo "Missing INET release library" >&2; exit 1; }
```

Assume OMNeT++, INET, and related tools such as `opp_run`, `opp_run_dbg`, and
`opp_repl` are already sourced when they are available on `PATH`. Do not prefix
every command with `source "$OMNETPP_ROOT/setenv"` or `source setenv`. Source an
environment script only as an explicit recovery step after validation fails, and
state why it was necessary.

## Common INET paths

Use:

```sh
INET_NED_PATH="$PROJECT_NED_ROOT;$INET_ROOT/src;$INET_ROOT/examples;$INET_ROOT/tutorials;$INET_ROOT/showcases"
INET_IMAGE_PATH="$INET_ROOT/images"
```

OMNeT++ NED-path entries are separated with semicolons. Always quote the complete argument in a Unix shell.

## Run with Cmdenv

Use Cmdenv for automated and reproducible execution:

```sh
opp_run \
  -u Cmdenv \
  -f "$INI_FILE" \
  "--image-path=$INET_IMAGE_PATH" \
  "--ned-path=$INET_NED_PATH" \
  -l "$INET_ROOT/src/libINET.so" \
  -c "$CONFIG" \
  -r "$RUN"
```

Use the dedicated Cmdenv, TShark, or event-log skills when more detailed diagnostics are required.

## Run with Qtenv

Use Qtenv when:

* The user explicitly requests it.
* Animation or topology visualization is needed.
* Module state must be inspected interactively.
* Event-by-event stepping is more useful than text logging.
* Cmdenv output is insufficient to locate the problem.

Do not use Qtenv for unattended batch runs.

### Release build

```sh
opp_run \
  -u Qtenv \
  -f "$INI_FILE" \
  "--image-path=$INET_IMAGE_PATH" \
  "--ned-path=$INET_NED_PATH" \
  --debug-on-errors=true \
  -l "$INET_ROOT/src/libINET.so" \
  -c "$CONFIG" \
  -r "$RUN"
```

### Debug build

Verify the debug library first:

```sh
test -f "$INET_ROOT/src/libINET_dbg.so" ||
    { echo "Missing INET debug library" >&2; exit 1; }
```

Then run:

```sh
opp_run_dbg \
  -u Qtenv \
  -f "$INI_FILE" \
  "--image-path=$INET_IMAGE_PATH" \
  "--ned-path=$INET_NED_PATH" \
  --debug-on-errors=true \
  -l "$INET_ROOT/src/libINET_dbg.so" \
  -c "$CONFIG" \
  -r "$RUN"
```

The debug runner and debug INET library must be used together. Do not mix release and debug binaries.

`--debug-on-errors=true` requests a debugger breakpoint when a runtime error occurs. It is effective when the simulation is already running under a debugger or debugger attachment is configured; it does not launch a debugger by itself.

For example, launch the debug simulation under LLDB:

```sh
lldb -- \
  opp_run_dbg \
  -u Qtenv \
  -f "$INI_FILE" \
  "--image-path=$INET_IMAGE_PATH" \
  "--ned-path=$INET_NED_PATH" \
  --debug-on-errors=true \
  -l "$INET_ROOT/src/libINET_dbg.so" \
  -c "$CONFIG" \
  -r "$RUN"
```

At the LLDB prompt, start the simulation:

```text
(lldb) run
```

When OMNeT++ encounters a runtime error, `--debug-on-errors=true` causes the process to stop in the debugger. Inspect the state before continuing:

```text
(lldb) process status
(lldb) thread backtrace all
(lldb) frame variable
```

For detailed source-level debugging procedures, use the `inet-lldb-debugging` skill.

## Source-level debugging 
When logs, Qtenv inspection, packet captures, and event logs are insufficient, use the `inet-lldb-debugging` skill. 

Source-level debugging must use: 
- `opp_run_dbg`. 
- `$INET_ROOT/src/libINET_dbg.so`. 
- Debug versions of any project-specific C++ libraries.
- `--debug-on-errors=true`. 

Do not mix `opp_run_dbg` with release model libraries, or `opp_run` with debug model libraries. Use Cmdenv under LLDB by default. Use Qtenv under LLDB only when interactive simulation visualization is also required.

## Reporting

Record:

* Whether Cmdenv or Qtenv was used.
* Whether release or debug mode was used.
* Exact command.
* Working directory.
* `INET_ROOT`.
* INI file.
* Configuration and run number.
* Loaded library.
* Exit status or displayed error.
* Relevant module, event number, and simulation time.
