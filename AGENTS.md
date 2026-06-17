# Agent Knowledgebase

## Running INET Unit Tests

- Run test commands from the repository root.
- Disable ccache before building/running tests in this workspace:
  ```sh
  export CCACHE_DISABLE=1
  ```
- Source OMNeT++ first, then INET:
  ```sh
  source /home/user/omnetpp-6.4.0/setenv -f
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
