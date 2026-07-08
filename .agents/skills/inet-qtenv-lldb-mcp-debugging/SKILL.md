---
name: inet-qtenv-lldb-mcp-debugging
description: Use this skill when debugging an OMNeT++ or INET simulation in Qtenv from VS Code with lldb-dap and LLDB MCP, especially when Codex should inspect or control the LLDB session through lldb-mcp. Trigger on OMNeT++, INET, Qtenv, opp_run_dbg, lldb-dap, LLDB MCP, lldb-mcp, packet exchange debugging, MAC/PHY debugging, simulation crashes, or --debug-on-errors.
---

# OMNeT++ / INET Qtenv debugging with lldb-dap and LLDB MCP

## Purpose

Use this skill to make Codex work effectively with an OMNeT++ or INET simulation running in Qtenv while the process is debugged by LLDB.

The preferred architecture is:

```text
Codex ──MCP/stdin-stdout── lldb-mcp ──socket── LLDB MCP server
                                                  │
VS Code ─────DAP────────── lldb-dap ──────────────┘
                                                  │
                                          opp_run_dbg / Qtenv
```

Use `lldb-dap` as the VS Code debug adapter and use LLDB MCP as the agent-facing control surface. Do not rely on Qtenv alone to launch the debugger.

## When to use this skill

Use this workflow when the user wants to:

- Debug an OMNeT++ or INET simulation in Qtenv from VS Code.
- Run `opp_run_dbg` under LLDB.
- Use `--debug-on-errors=true` so OMNeT++ runtime errors stop in the debugger.
- Let Codex inspect the current debugger state, set breakpoints, continue, step, or collect backtraces through LLDB MCP.
- Debug packet exchange paths, especially INET MAC/PHY behavior.

Do not use this workflow for pure result analysis after a completed run; use scalar/vector/pcap analysis tools for that instead.

## Core rules

1. Prefer debug artifacts:
   - `opp_run_dbg`, not `opp_run`.
   - `libINET_dbg.so`, not `libINET.so`.
   - `make MODE=debug`, not a release-only build.
2. Launch the simulation under LLDB before expecting `--debug-on-errors=true` to be useful.
3. Prefer `lldb-dap` for the VS Code debug session.
4. Prefer LLDB MCP through `lldb-mcp` when Codex needs to inspect or control the debug session.
5. Do not patch source code before explaining the suspected root cause.
6. Capture debugger evidence before making changes:
   - full backtrace,
   - selected frame variables,
   - current thread,
   - event number,
   - simulation time,
   - module path,
   - message/packet name and class.
7. Avoid long blind stepping from `main()`. Set targeted breakpoints in OMNeT++/INET packet-processing code.

## Step 1: Verify tools and debug build

If `libINET_dbg.so` is missing, build debug artifacts:

```sh
cd "${INET_ROOT:-$PWD}"
make makefiles
make MODE=debug -j"$(nproc)"
```

If `lldb-mcp` or `protocol-server start MCP` is unavailable, the installed LLDB may be too old or built without MCP support. In that case, fall back to `lldb-dap` or plain `lldb` and tell the user that MCP control is not currently available in this toolchain.

Check MCP support with:

```sh
lldb -b -o 'help protocol-server' -o quit
```

## Step 2: Start the debug session

Preferred order:

1. Build INET in debug mode.
2. Start the VS Code launch configuration named `OMNeT++ Qtenv / lldb-dap + MCP`.
3. Confirm Qtenv appears.
4. Confirm the LLDB MCP server started.
5. Ask Codex to inspect/control the session through the `lldb` MCP server.

Useful prompt to the agent:

```text
Use the lldb MCP server to inspect the current OMNeT++ Qtenv debug session.
First check the target and process state. Then set targeted breakpoints around the relevant INET MAC/PHY packet path. When the process stops, collect a full backtrace, selected frame variables, current event context, and explain the likely simulation-level cause before suggesting any code change.
```

## Step 3: LLDB commands to use through MCP

Use the MCP `lldb_command` tool, if available, to run ordinary LLDB commands.

Initial inspection:

```lldb
target list
process status
thread list
image list
settings show target.source-map
```

When stopped:

```lldb
thread backtrace all
thread info
frame info
frame variable
frame variable --show-types
register read
```

Breakpoint examples:

```lldb
breakpoint set --name handleMessage
breakpoint set --name refreshDisplay
breakpoint set --name processPacket
breakpoint set --name encapsulate
breakpoint set --name decapsulate
breakpoint set --name sendUp
breakpoint set --name sendDown
breakpoint set --file SomeMacLayer.cc --line 123
```

Conditional examples:

```lldb
breakpoint set --name handleMessage --condition 'msg != nullptr'
breakpoint set --file SomeFile.cc --line 123 --condition 'packet != nullptr'
```

Execution control:

```lldb
continue
next
step
finish
process interrupt
```

Expression and object inspection:

```lldb
expression -- this->getFullPath().c_str()
expression -- simTime().str().c_str()
expression -- msg->getName()
expression -- msg->getClassName()
expression -- packet->getName()
expression -- packet->getFullName()
```

Use expressions carefully. Prefer side-effect-free inspection. Do not call methods that mutate simulation state unless the user explicitly asks.

## Step 4: OMNeT++ and INET-specific debugging checklist

When the simulation stops because of an error or breakpoint, collect:

```text
- LLDB stop reason
- full backtrace
- selected frame and local variables
- OMNeT++ event number
- simulation time
- module full path
- current message/packet name
- current message/packet C++ class
- packet protocol tags, if relevant
- gate name/direction, if relevant
- sender/receiver module, if relevant
```

Good OMNeT++/INET places to inspect:

- `handleMessage()` implementations.
- MAC queue insertion/removal.
- PHY signal transmission/reception paths.
- Packet encapsulation/decapsulation.
- Tag addition/removal.
- Gate sends and direct sends.
- Timers/self-messages.
- Initialization stages.
- Signal subscription/listener callbacks.

For 802.11 debugging, prefer targeted breakpoints around:

- MAC frame creation and classification.
- Data/control/management frame handling.
- ACK/RTS/CTS/BlockAck paths.
- EDCA queue selection.
- Backoff and contention state.
- NAV updates.
- Rate/control mode decisions.
- PHY reception state changes.
- SNIR/energy-detection/decoding decisions.
- Packet tags crossing MAC/PHY boundaries.

## Step 5: Interpret before editing

Before modifying code, produce a short diagnosis with:

```text
Observation:
Evidence:
Likely cause:
Alternative causes:
Next breakpoint or experiment:
Proposed fix, if any:
Risk of proposed fix:
```

Only patch code after the evidence points to a concrete defect. For configuration errors, prefer fixing `.ini`, NED path, module parameters, or build/debug launch settings before changing C++.

## Fallback: plain LLDB wrapper

If VS Code or `lldb-dap` is unavailable, use a wrapper script:

```sh
#!/usr/bin/env bash
set -euo pipefail

INI_FILE="${1:-$INET_ROOT/examples/wireless/omnetpp.ini}"
CONFIG="${2:-General}"
RUN="${3:-0}"

export PATH="$OMNETPP_ROOT/bin:$PATH"
export LD_LIBRARY_PATH="$OMNETPP_ROOT/lib:$INET_ROOT/src:${LD_LIBRARY_PATH:-}"

exec lldb -- "$OMNETPP_ROOT/bin/opp_run_dbg" \
  -u Qtenv \
  -f "$INI_FILE" \
  -n "$INET_ROOT/src;$INET_ROOT/examples" \
  "--image-path=$INET_ROOT/images;$OMNETPP_ROOT/images" \
  --debug-on-errors=true \
  -l "$INET_ROOT/src/libINET_dbg.so" \
  -c "$CONFIG" \
  -r "$RUN"
```

Inside LLDB:

```lldb
protocol-server start MCP listen://localhost:59999
run
thread backtrace all
frame variable
```

## Fallback: drive interactive Qtenv through its own MCP server

Qtenv is interactive: `-c` and `-r` only preselect the run in the GUI, they do
not automatically advance simulation time. When a Qtenv process is already
running under LLDB but needs a GUI action before a breakpoint can be reached,
start Qtenv with its own OMNeT++ MCP server and call `run_simulation` over
localhost:

```sh
opp_run_dbg -u Qtenv \
  -f "$INI_FILE" \
  -n "$INET_ROOT/src;$INET_ROOT/examples" \
  "--image-path=$INET_ROOT/images;$OMNETPP_ROOT/images" \
  --debug-on-errors=true \
  --mcp-server-address localhost:18766 \
  -l "$INET_ROOT/src/libINET_dbg.so" \
  -c "$CONFIG" \
  -r "$RUN"
```

Initialize the MCP session and capture the `Mcp-Session-Id` response header:

```sh
curl -s -D /tmp/qtenv_mcp_headers.txt \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"codex","version":"1"}}}' \
  http://localhost:18766/mcp
```

Then drive the GUI simulation to LLDB breakpoints without relying on keyboard
focus:

```sh
curl -s \
  -H 'Content-Type: application/json' \
  -H 'Mcp-Session-Id: <session-id>' \
  -d '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"run_simulation","arguments":{"mode":"fast"}}}' \
  http://localhost:18766/mcp
```

Useful Qtenv MCP tools include `get_simulation_state`, `get_fes`,
`run_simulation`, `request_stop_simulation`, and, when C++ execution is
available, `set_debugger_break_condition` and `request_debugger_break`.
If the process stops in LLDB before the HTTP request returns, `curl` may report
an empty reply; inspect LLDB before treating that as a failure.

## Common failure modes

### `libINET_dbg.so` not found

Likely causes:

- INET was not built in debug mode.
- `cwd`, `INET_ROOT`, or workspace root is wrong.
- The launch config points to `libINET_dbg.so` in the wrong repository.

Fix:

```sh
cd "$INET_ROOT"
make MODE=debug -j"$(nproc)"
test -f src/libINET_dbg.so
```

### Breakpoints are not binding

Likely causes:

- Release binary or release shared library is loaded.
- Source paths in debug info do not match the workspace.
- The relevant library is loaded after the breakpoint is set.

Fixes:

```lldb
image list
breakpoint list
settings show target.source-map
```

If necessary, add `sourceMap` or `debuggerRoot` to `launch.json`.

### Qtenv starts but no MCP access works

Likely causes:

- `protocol-server start MCP ...` failed.
- The installed LLDB lacks MCP support.
- `lldb-mcp` cannot find the running LLDB MCP server.
- Codex MCP config points to the wrong `lldb-mcp`.

Fixes:

```sh
command -v lldb-mcp
lldb -b -o 'help protocol-server' -o quit
```

Check the VS Code debug console for the `protocol-server start MCP` output.

### LLDB-launched Qtenv aborts in Qt startup

Likely causes:

- LLDB or `lldb-mcp` did not inherit GUI environment variables.
- `DISPLAY`, `WAYLAND_DISPLAY`, or `XAUTHORITY` is missing in the debuggee.
- Qt cannot connect to the display even though Qtenv works from a normal shell.

Fix:

- Launch with explicit display environment, for example
  `DISPLAY=:0 XAUTHORITY=/tmp/xauth_... lldb -- opp_run_dbg ...`.
- Verify Qtenv starts outside LLDB with the same INI/config/run before
  diagnosing the model.

### `--debug-on-errors=true` does not stop in LLDB

Likely causes:

- The process was not launched under LLDB.
- The error occurs before the debugger is attached.
- The wrong OMNeT++ runner is used.

Fix:

- Launch `opp_run_dbg` from the `lldb-dap` configuration.
- Do not start Qtenv first and attach later unless there is a specific reason.

### GUI or display errors

Likely causes:

- VS Code or Codex is running in an environment without GUI display access.
- `DISPLAY` or Wayland variables are not forwarded.
- The simulation should be debugged in Cmdenv for this particular run.

Fix:

- Confirm Qtenv can start outside LLDB.
- Preserve `DISPLAY`, `WAYLAND_DISPLAY`, `XAUTHORITY`, and related environment variables.
- If GUI is impossible, switch temporarily to `-u Cmdenv` while keeping LLDB.

### Localhost control fails inside the Codex sandbox

Likely causes:

- The sandbox blocks opening sockets to `localhost`, even for a local Qtenv MCP
  server.

Fix:

- Retry the `curl http://localhost:<port>/mcp` command with escalated sandbox
  permissions and explain that it only connects to the local Qtenv process.

## Expected final answer format

When reporting findings to the user, include:

```text
What I checked:
What stopped or failed:
Debugger evidence:
Simulation-level interpretation:
Recommended next action:
Files changed, if any:
Commands to rerun:
```

Be explicit when the limitation is tooling-related, such as LLDB MCP not being available in the installed LLDB build.
