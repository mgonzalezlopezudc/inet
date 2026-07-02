## INET and OMNeT++ workflows

Use repository skills for detailed simulation, test, result-analysis, and standards workflows.

### General rules

* Use Cmdenv by default for automated runs, batch execution, log analysis, packet capture, and reproducible debugging.
* Use Qtenv when Cmdenv is insufficient for interactive debugging or when the user explicitly requests Qtenv.
* Do not modify `omnetpp.ini` solely to enable temporary logging, tracing, packet capture, or result inspection. Prefer command-line overrides.
* Run one configuration and run number at a time unless a simulation campaign is explicitly requested.
* Preserve the exact command, working directory, configuration, run number, exit status, and generated artifact paths in reports.
* Do not infer packet delivery, loss, retransmission, or protocol behavior without supporting logs, captures, event logs, or recorded results.
* When a simulation error requires source-level C++ debugging, use a debug build with `opp_run_dbg`, the corresponding debug model libraries, and the `inet-lldb-debugging` skill. Do not mix release and debug binaries.

### Available skills

* `inet-simulation-run`: Run INET simulations with Cmdenv or Qtenv and diagnose startup or runtime failures.
* `inet-cmdenv-log-analysis`: Find text and investigate module behavior in Cmdenv output.
* `inet-pcap-tshark-analysis`: Record and analyze INET packet exchanges with PcapRecorder and TShark.
* `omnetpp-eventlog-analysis`: Reconstruct simulator-level message and event causality.
* `omnetpp-result-analysis`: Query and export scalar and vector simulation results with `opp_scavetool`.
* `inet-lldb-debugging`: Debug INET and OMNeT++ C++ code using LLDB, including runtime errors, crashes, breakpoints, watchpoints, stack inspection, and automated backtrace capture.
* `inet-unit-tests`: Build and run INET unit tests with repository-specific requirements.
* `ieee80211-standards`: Search the generated IEEE 802.11 standards corpus and consult source PDFs when necessary.

### IEEE 802.11 standards

The source standards documents are under `standards/`.

Before reading or processing the PDFs directly, use the generated corpus through the `ieee80211-standards` skill. Rebuild the corpus when it is missing or stale.
