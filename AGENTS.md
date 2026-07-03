## INET and OMNeT++ workflows

Use repository skills for detailed simulation, test, result-analysis, and standards workflows.

### General rules

* Use Cmdenv by default for automated runs, batch execution, log analysis, packet capture, and reproducible debugging.
* Use Qtenv when Cmdenv is insufficient for interactive debugging or when the user explicitly requests Qtenv.
* Do not modify `omnetpp.ini` solely to enable temporary logging, tracing, packet capture, or result inspection. Prefer command-line overrides.
* Run one configuration and run number at a time unless a simulation campaign is explicitly requested.
* Preserve the exact command, working directory, configuration, run number, exit status, and generated artifact paths in reports.
* Do not infer packet delivery, loss, retransmission, or protocol behavior without supporting logs, captures, event logs, or recorded results.
* Always use `-j$(nproc)` for `make` and other parallel build commands unless the user explicitly requests a different job count.
* When a simulation error requires source-level C++ debugging, use a debug build with `opp_run_dbg`, the corresponding debug model libraries, and the `inet-lldb-debugging` skill. Do not mix release and debug binaries.

### Agent learning procedure

When an agent solves a problem in a way that is clearly reusable, it should consider whether the lesson should be persisted.

Reusable lessons include:

* Non-obvious commands, flags, workflows, or environment requirements.
* Repeated debugging patterns or failure modes.
* Project-specific conventions that prevented an error.
* Stable domain knowledge that future agents are likely to need.

Do not persist:

* One-off facts from a single task.
* Guesses that were not validated.
* Information that is already documented nearby.
* Large logs, temporary paths, or overly specific run artifacts.

Before editing any rules or skill file, the agent must ask the user for permission. The proposal should include:

* The reusable lesson.
* The target file, such as `AGENTS.md`, a relevant `.agents/skills/*/SKILL.md`, or a skill reference file.
* The exact text or a concise summary of the intended addition.
* Why the lesson is likely to help future agents.

Only add the knowledge after explicit user approval.

Store project-wide agent behavior in `AGENTS.md`. Store task-specific reusable workflows in the relevant skill's `SKILL.md`. Store longer domain notes, examples, command recipes, or troubleshooting details in a skill `references/` file, linked from `SKILL.md`. Prefer concise, validated, reusable lessons over broad commentary.

### Available skills

* `inet-simulation-run`: Run INET simulations with Cmdenv or Qtenv and diagnose startup or runtime failures.
* `inet-cmdenv-log-analysis`: Find text and investigate module behavior in Cmdenv output.
* `inet-pcap-tshark-analysis`: Record and analyze INET packet exchanges with PcapRecorder and TShark.
* `omnetpp-eventlog-analysis`: Reconstruct simulator-level message and event causality.
* `omnetpp-result-analysis`: Query and export scalar and vector simulation results with `opp_scavetool`.
* `inet-lldb-debugging`: Debug INET and OMNeT++ C++ code using LLDB, including runtime errors, crashes, breakpoints, watchpoints, stack inspection, and automated backtrace capture.
* `inet-unit-tests`: Build and run INET unit tests with repository-specific requirements.
* `inet-80211-packet-debugging`: Debug IEEE 802.11 PHY/MAC packet exchanges with Cmdenv, PCAPng, TShark, logs, event logs, results, source inspection, and LLDB.
* `inet-80211-regression-testing`: Design and verify focused IEEE 802.11 regression coverage across deterministic runs, seeds, captures, logs, results, and fingerprints.
* `inet-fingerprint-regression`: Diagnose fingerprint mismatches and update expected fingerprints only after the changed simulation trajectory is explained.
* `inet-ned-ini-analysis`: Trace NED inheritance, INI inheritance, wildcard precedence, module paths, `typename` selection, and effective parameter values.
* `inet-packet-tag-debugging`: Debug INET `Packet`, chunk, protocol tag, request/indication tag, region tag, encapsulation, and metadata propagation issues.
* `inet-build-debug-modes`: Diagnose release/debug builds, generated code, stale artifacts, model libraries, and runner/library mismatches.
* `ieee80211-standards`: Search the generated IEEE 802.11 standards corpus and consult source PDFs when necessary.

### IEEE 802.11 standards

The source standards documents are under `standards/`.

Before reading or processing the PDFs directly, use the generated corpus through the `ieee80211-standards` skill. Rebuild the corpus when it is missing or stale.
