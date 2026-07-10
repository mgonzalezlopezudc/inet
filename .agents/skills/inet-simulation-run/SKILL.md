---
name: inet-simulation-run
description: Run and diagnose INET simulations using Cmdenv or Qtenv. Use for normal simulation execution, short diagnostic runs, initialization failures, runtime errors, or requests for interactive graphical debugging.
---

# Running INET simulations

Use Cmdenv by default. Use Qtenv only when interactive inspection is useful or the user explicitly asks for it.

## Inputs

Adapt these inputs to the scenario:

```sh
INI_FILE="${INI_FILE:-omnetpp.ini}"
PROJECT_NED_ROOT="${PROJECT_NED_ROOT:-.}"
CONFIG=<configuration-name>
RUN=<run-number>
```

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

`--debug-on-errors=true` requests a debugger trap; it does not launch a debugger. Use `inet-lldb-debugging` for source-level investigation.

## Diagnostic routing

* Use `inet-cmdenv-log-analysis` for module decisions and runtime context.
* Use `inet-pcap-tshark-analysis` for protocol-visible packets.
* Use `omnetpp-eventlog-analysis` for scheduling and message causality.
* Use `omnetpp-result-analysis` for recorded statistics.
* Use `inet-lldb-debugging` for source-level state.

Report the selected environment, build mode, loaded model libraries, and relevant module, event number, simulation time, or error.
