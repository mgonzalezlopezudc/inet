---
name: inet-simulation-run
description: Run and diagnose INET simulations using the INET launcher (`inet`) with Cmdenv or Qtenv. Use for normal simulation execution, short diagnostic runs, initialization failures, runtime errors, or requests for interactive graphical debugging.
---

# Running INET simulations

Use Cmdenv by default. Use Qtenv only when interactive inspection is useful or the user explicitly asks for it.

## Common Syntax Pitfalls

* **Root Executable Missing**: Do NOT execute `./run` from the repository root directory (fails with `No such file or directory`). Source both OMNeT++ and INET's `setenv` scripts, then use `inet` (or `inet --debug`) as shown in the templates below.
* **Working-directory mismatch**: Keep every relative `-f`, result, NED-path, and library path consistent with the selected working directory. If the command runs from an example directory, use paths relative to that directory; if it runs from the repository root, use repository-root-relative paths.
* **Undocumented query flags**: Do not improvise `inet -q` or `-h` category arguments. Use the run templates below for execution and consult the underlying simulator's help only when a supported option is genuinely needed.

## Inputs

Adapt these inputs to the scenario:

```sh
INI_FILE="${INI_FILE:-omnetpp.ini}"
CONFIG=<configuration-name>
RUN=<run-number>
```

The commands below assume that `setenv` has been sourced from both the OMNeT++ and INET roots. The `inet` launcher supplies INET's standard NED folders and image path and selects the matching executable or shared library. Use `inet --printcmd` (or `inet --release --printcmd`/`inet --debug --printcmd` when selecting a mode explicitly) when the resolved runner and library paths must be recorded.

For a project with additional NED roots or model libraries, pass the project-specific paths and `-l` options in addition to the launcher defaults.

When adding custom NED roots manually, OMNeT++ NED-path entries are separated with semicolons. Always quote the complete argument in a Unix shell.

## Run with Cmdenv

Use Cmdenv for automated and reproducible execution:

```sh
inet --release \
  -u Cmdenv \
  -f "$INI_FILE" \
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
inet --release \
  -u Qtenv \
  -f "$INI_FILE" \
  --debug-on-errors=true \
  -c "$CONFIG" \
  -r "$RUN"
```

### Debug build

```sh
inet --debug \
  -u Qtenv \
  -f "$INI_FILE" \
  --debug-on-errors=true \
  -c "$CONFIG" \
  -r "$RUN"
```

The `--release` and `--debug` launcher modes select the corresponding runner and INET library. Do not mix release and debug binaries. For LLDB, use the direct `opp_run_dbg` form described by `inet-lldb-debugging`, because LLDB must target the actual executable rather than this shell wrapper.

`--debug-on-errors=true` requests a debugger trap; it does not launch a debugger. Use `inet-lldb-debugging` for source-level investigation.

## Diagnostic routing

* Use `inet-cmdenv-log-analysis` for module decisions and runtime context.
* Use `inet-pcap-tshark-analysis` for protocol-visible packets. For a diagnostic PCAP run, make the first command capture-ready with `--**.checksumMode="computed"` and `--**.fcsMode="computed"` unless both are already effective.
* Use `omnetpp-eventlog-analysis` for scheduling and message causality.
* Use `omnetpp-result-analysis` for recorded statistics.
* Use `inet-lldb-debugging` for source-level state.

Report the selected environment, build mode, loaded model libraries, and relevant module, event number, simulation time, or error.
