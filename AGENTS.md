# Agent Knowledgebase

## Compiling
- Use "-j$(nproc)" when building.

## Shell Commands

Do not assume the current shell has sourced OMNeT++, inet or opp_repl. For simulation commands, set the needed environment explicitly or source the relevant `setenv` scripts.

## Debugging Qtenv Simulations With opp_repl

Use opp_repl as the control plane for INET/OMNeT++ simulations. 

Assume there is a persistent opp_repl process controlled through its MCP socket and use your opp_repl_mcp_bridge MCP server that connects stdio MCP clients to the persistent opp_repl Unix-domain socket. This allows you to execute Python inside the already-running REPL and reuse loaded projects, helper variables, result objects, cached discovery, and debugging  state across calls. The default socket path is chosen automatically when  `opp_repl --mcp-socket` and `opp_repl_mcp_bridge` are both used without an explicit path; run `opp_repl_mcp_bridge --help` to see the resolved path  and client configuration examples.

Keep the two MCP layers distinct:

- `opp_repl_mcp_bridge` talks to the persistent Python opp_repl session.
- `QtenvMCPClient` talks to a live Qtenv simulation at
  `http://127.0.0.1:<port>/mcp`.

Use the bridge as the control plane and Qtenv's MCP endpoint for live
simulation inspection.
 

### Avoid Slow Full-INET Discovery

The full INET descriptor scans many examples and may spend about a minute querying dynamic run counts. For targeted debugging, prefer constructing a single `SimulationConfig` directly when the working directory/config/run are
known:

```python
from opp_repl import *
from opp_repl.simulation.config import SimulationConfig

load_opp_file("$OPP_REPL_ROOT/opp_repl/opp/omnetpp.opp")
load_opp_file("$OPP_REPL_ROOT/opp_repl/opp/inet.opp")
p = get_simulation_project("inet")

sc = SimulationConfig(
    p,
    "examples/wireless/hiddennode",
    ini_file="omnetpp.ini",
    config="General",
    num_runs=1,
    sim_time_limit="5s",
    bounded=True,
)
```

Use `run_simulations(..., simulation_configs=[sc], build=False, concurrent=False)` for a quick Cmdenv smoke run.

### Qtenv MCP Endpoint

Qtenv in this environment advertises `--mcp-server-address`, and the MCP endpoint works. However, binding to `localhost:<port>` may bind IPv6-only, while opp_repl's client polls `http://127.0.0.1:<port>/mcp`. For Qtenv MCP automation, bind Qtenv to `127.0.0.1:<port>` so the bind address and client URL match.

### Useful Qtenv/MCP Operations

For offscreen visual debugging, use module-image capture:

```python
r = capture_module_images(
    simulation_project=p,
    simulation_configs=[sc],
    output_dir="/tmp/opp_qtenv_snap",
    module_path_filter="HiddenNode",
    build=False,
    concurrent=False,
    startup_timeout=30.0,
)
print(r)
```

A successful run writes PNG files such as:

```text
/tmp/opp_qtenv_snap/examples_wireless_hiddennode__General__r0__HiddenNode.png
```

The lower-level MCP client is `opp_repl.common.mcp_client.QtenvMCPClient`. Available helpers include:

- `get_simulation_state()`
- `get_network_topology(max_depth=...)`
- `get_canvas_image(module_path, area="all_elements", margin=5)`
- `request_stop_simulation()`

### Verification

Known successful checks:

```bash
python3 -m py_compile opp_repl/test/module_image.py
```

Cmdenv smoke run should finish with `DONE` for
`examples/wireless/hiddennode` at `sim_time_limit="0s"`.

Qtenv module-image capture should finish with `DONE` and HTTP logs showing requests to `http://127.0.0.1:<port>/mcp`.

Launching Qtenv or opening MCP sockets usually requires escalated execution outside the sandbox. Ask for approval when needed.

## Running a simulation in Qtenv without opp_repl
- Release mode: `opp_run -u Qtenv --image-path=$HOME/omnetpp_ws/inet/images --ned-path=$HOME/omnetpp_ws/inet/src:$HOME/omnetpp_ws/inet/examples:$HOME/omnetpp_ws/inet/tutorials:$HOME/omnetpp_ws/inet/showcases --debug-on-errors=true -l $HOME/omnetpp_ws/inet/src/libINET.so omnetpp.ini`

- Debug mode: `opp_run_dbg -u Qtenv --image-path=$HOME/omnetpp_ws/inet/images --ned-path=$HOME/omnetpp_ws/inet/src:$HOME/omnetpp_ws/inet/examples:$HOME/omnetpp_ws/inet/tutorials:$HOME/omnetpp_ws/inet/showcases --debug-on-errors=true -l $HOME/omnetpp_ws/inet/src/libINET_dbg.so omnetpp.ini`

## Using opp_scavetool to analyze simulation results
Once a simulation has finished and .sca and .vec files are available, use opp_scavetool to analize scalar and vector results from a simulation: `opp_scavetool query -l -f <filter> <filename>` where filter follows the syntax indicated in the OMNeT++ Manual.

## Running INET Unit Tests

- Run test commands from the repository root.
- Disable ccache before building/running tests in this workspace:
  ```sh
  export CCACHE_DISABLE=1
  ```
- A known-good HE/OFDMA unit-test command is:
  ```sh
  inet_run_unit_tests -m release -f "(Ieee80211He|HeDlScheduler).*\\.test"
  ```
- `inet_run_unit_tests -f` takes a single regex filter. Use alternation for multiple test groups.
- The matplotlib warning about `$HOME/.config/matplotlib` not being writable is non-fatal; it falls back to a temporary cache under `/tmp`.

## Actual 802.11 standards documents
They are available in the `standards` folder.

Before reading or reprocessing the PDFs directly, prefer the generated standards corpus:

```sh
inet_process_standards status
inet_process_standards search "Table 27-61"
inet_process_standards show 80211ax-2024:chunk:00001
```

If the corpus is missing or stale, rebuild it with:

```sh
inet_process_standards build
```

Generated text, page files, chunks, and the SQLite FTS index live under `standards/processed/` and are intentionally ignored by git.
