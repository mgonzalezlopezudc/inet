## INET and OMNeT++ workflows

Use repository skills for detailed simulation, test, result-analysis, and standards workflows.

### Agent routing and orchestration

Use the `inet-agent-orchestration` skill for nontrivial INET work that benefits from specialist agents. Delegate proactively when a task has two or more independent evidence lanes, crosses IEEE 802.11 normative behavior and INET implementation, investigates unexplained runtime behavior, changes production C++/NED/MSG code, requires nontrivial statistical analysis, or needs independent regression/review evidence. Keep simple lookups, one-command checks, and obvious one-file edits in the root thread when delegation would cost more than the work.

Optimize routing for correctness first and token/credit consumption second:

* Use GPT-5.6 Sol for correctness-critical judgment: ambiguous Wi-Fi MAC/PHY behavior, standards-to-model comparison, difficult runtime causality, production implementation, and final review.
* Use GPT-5.6 Terra for bounded engineering work: repository/configuration tracing, deterministic regression work, and result analysis with established methods.
* Use GPT-5.6 Luna only for clear, repeatable work with an explicit output contract: artifact inventory, targeted extraction, filtering, and structured summarization. Do not ask Luna to establish causality, interpret ambiguous standards language, design a fix, or approve a change.
* Use the lowest reasoning effort adequate for the assignment, but never use an effort higher than `high`. Use `high` for correctness-critical work, `medium` for bounded analysis, and `low` only for mechanical transformations with independently checkable output.
* If a configured model is unavailable, preserve or increase capability: route Luna work to Terra and Terra work to Sol at no more than `high`. Do not silently downgrade Sol work; report the limitation and either keep the work in the capable root thread or use Terra at `high` with independent verification.

Keep orchestration shallow and evidence-driven:

* The root agent owns requirements, decomposition, agent selection, handoffs, conflict resolution, and the final answer. Delegate bounded outcomes, not vague topics.
* Start independent read-only lanes in parallel when they can materially reduce latency or protect the root context. Do not assign two agents the same question merely to seek consensus.
* Use at most one production-code writer at a time. Other agents may inspect, reproduce, test, or review concurrently only when their work does not race with the writer or rely on an unstable worktree.
* Preserve `max_depth = 1`: specialist agents must return to the root instead of recursively delegating.
* Give every agent the exact question, scope, exclusions, relevant paths/configuration/run, permitted writes, required evidence, and definition of done. Require concise conclusions with file references and artifact paths rather than raw logs.
* Reuse an existing specialist for follow-up work when it already has the relevant context. Stop opening lanes once the decisive evidence is established.
* Resolve disagreements by evidence strength: reproducible runtime/debugger evidence, packet/event/result evidence, effective configuration, source behavior, and then hypothesis. IEEE text governs normative claims; checked-out INET source and observed runs govern implemented behavior.
* Before implementation, establish the failure mechanism and smallest change surface. After implementation, use an independent regression or review lane for nontrivial Wi-Fi changes.

### Links to files in user-facing explanations
* Always use relative (to the project workspace) and not absolute links to files in your user-facing explanations. For example, use `[some-file](src/some-file)` instead of `[some-file]/(home/user/omnetpp_ws/inet/src/some-file)`.

### General rules

* Use Cmdenv by default for automated runs, batch execution, log analysis, packet capture, and reproducible debugging.
* Use Qtenv when Cmdenv is insufficient for interactive debugging or when the user explicitly requests Qtenv.
* Assume the required OMNeT++ and INET development tools are installed, sourced, and available. Invoke them directly; do not preflight executables, print tool versions routinely, or source environment scripts unless a concrete failure indicates an environment problem. Still validate task artifacts such as selected libraries, captures, result files, and generated corpora when they are evidence for the task.
* Do not modify `omnetpp.ini` solely to enable temporary logging, tracing, packet capture, or result inspection. Prefer command-line overrides.
* Run one configuration and run number at a time unless a simulation campaign is explicitly requested.
* Preserve the exact command, working directory, configuration, run number, exit status, and generated artifact paths in reports.
* Do not infer packet delivery, loss, retransmission, or protocol behavior without supporting logs, captures, event logs, or recorded results.
* Never update fingerprint CSV files without explicit user approval, even when the changed trajectory is explained and `.UPDATED` files have been generated.
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

* `inet-agent-orchestration`: Route nontrivial INET and IEEE 802.11 work across the project-scoped Sol, Terra, and Luna specialist agents.
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
