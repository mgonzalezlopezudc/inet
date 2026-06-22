# Agent Knowledgebase

## Compiling
- Use "-j$(nproc)" when building.

# Running a simulation in Qtenv
**Always use this method to run simulations in Qtenv**:
- Release mode: `source $HOME/omnetpp-6.4.0aipre2/setenv && source $HOME/omnetpp_ws/inet/setenv && opp_run -u Qtenv --image-path=$HOME/omnetpp_ws/inet/images --ned-path=$HOME/omnetpp_ws/inet/src:$HOME/omnetpp_ws/inet/examples:$HOME/omnetpp_ws/inet/tutorials:$HOME/omnetpp_ws/inet/showcases --debug-on-errors=true -l $HOME/omnetpp_ws/inet/src/libINET.so omnetpp.ini`

- Debug mode: `source $HOME/omnetpp-6.4.0aipre2/setenv && source $HOME/omnetpp_ws/inet/setenv && opp_run_dbg -u Qtenv --image-path=$HOME/omnetpp_ws/inet/images --ned-path=$HOME/omnetpp_ws/inet/src:$HOME/omnetpp_ws/inet/examples:$HOME/omnetpp_ws/inet/tutorials:$HOME/omnetpp_ws/inet/showcases --debug-on-errors=true -l $HOME/omnetpp_ws/inet/src/libINET_dbg.so omnetpp.ini`

## Controlling simulation launched previously by the user
When the user asks you to control a simulation they launched previously, use the tools provided by your MCP omnetpp server. If the MCP server is not running, notify the user before continuing. 

## Using opp_scavetool to analyze simulation results
Once a simulation has finished and .sca and .vec files are available, use opp_scavetool to analize scalar and vector results from a simulation: `opp_scavetool query -l -f <filter> <filename>` where filter follows the syntax indicated in the OMNeT++ Manual.

## Running INET Unit Tests

- Run test commands from the repository root.
- Disable ccache before building/running tests in this workspace:
  ```sh
  export CCACHE_DISABLE=1
  ```
- Source OMNeT++ first, then INET:
  ```sh
  source /home/user/omnetpp-6.4.0aipre2/setenv -f
  source setenv -q
  ```
- A known-good HE/OFDMA unit-test command is:
  ```sh
  bin/inet_run_unit_tests -m release -f "(Ieee80211He|HeDlScheduler).*\\.test"
  ```
- `setenv -q` alone is not enough for these tests; without the OMNeT++ environment, Python imports can fail with `No module named 'omnetpp'`.
- Running the test runner without the INET environment can fail with `No module named 'inet'`.
- `inet_run_unit_tests -f` takes a single regex filter. Use alternation for multiple test groups.
- The matplotlib warning about `/home/user/.config/matplotlib` not being writable is non-fatal; it falls back to a temporary cache under `/tmp`.

## Actual 802.11 standards documents
They are available in the `standards` folder.

