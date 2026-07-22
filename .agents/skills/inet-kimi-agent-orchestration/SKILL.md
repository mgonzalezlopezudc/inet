---
name: inet-kimi-agent-orchestration
description: Route and coordinate Kimi K3 and K2.7 sub-agents for nontrivial OMNeT++/INET and IEEE 802.11 work on Kimi Code CLI. Use for multi-stage debugging, standards-to-implementation analysis, C++/NED/MSG changes, Wi-Fi packet or PHY/MAC investigations, regression design, result analysis, patch review, or any task with multiple independent evidence lanes or specialist handoffs.
---

# INET Kimi Agent Orchestration

Kimi counterpart of `inet-agent-orchestration`, which targets GPT-5.6 Sol/Terra/Luna under Codex. The specialist roles, evidence lanes, assignment rules, and gating are the same; the model bindings and sub-agent mechanics differ.

Preserve correctness while limiting duplicate work and model cost. Keep requirements and synthesis in the root thread, delegate narrow evidence or execution outcomes, and use the cheapest model that is reliable for each bounded assignment.

## Apply hard constraints

- Keep delegation depth at one. Specialists must not spawn children — the `coder` sub-agent type is able to nest agents, so every delegated prompt must forbid it explicitly.
- Allow only one production-code writer at a time.
- Do not delegate a simple lookup, one-command check, or obvious one-file edit when orchestration overhead exceeds the task.
- Do not use multiple agents to answer the same question unless independent validation is itself the requested outcome.
- Never let a lower-cost lane turn extracted facts into unsupported causal, normative, or correctness claims.
- Reserve K3 at `max` effort for Sol-tier roles. Never spend `max` on bounded or mechanical lanes.

## Select the model tier

Select models for the judgment required, not the amount of text or number of files.

| Tier | Model and effort | Use | Do not use |
| --- | --- | --- | --- |
| Sol-tier | K3 (`kimi-code/k3`), effort `max` | Ambiguous 802.11 MAC/PHY or standards reasoning; difficult event causality; risky production implementation; final correctness review | Mechanical inventory or bulk extraction |
| Terra-tier | K3 (`kimi-code/k3`), effort `high` | Architecture and NED/INI tracing; established build/test workflows; deterministic regression work; result analysis with known semantics | Resolving genuinely ambiguous normative or causal questions without Sol-tier review |
| Luna-tier | K2.7 (`kimi-code/kimi-for-coding`), thinking on | Exact searches, artifact inventory, fixed-filter log/PCAP/result extraction, structured summaries, mechanical checks | Causality, standards interpretation, fix design, statistical judgment, or approval decisions |

If a model is unavailable, move upward in capability: K2.7 (thinking on) to K3 `high`, then K3 `high` to K3 `max`. Do not silently move Sol-tier work downward. If K3 is unavailable, keep the work in a capable root thread or use K2.7 with thinking on plus an independent verification lane and disclose the substitution.

### CLI mechanics and honest tier claims

Kimi Code CLI sub-agents (`coder`, `explore`, `plan`) run on the session model and effort; the `Agent` tool has no per-subagent model or effort parameter. Realize the tier bindings as follows:

- Run the root session on K3. Sol-tier and Terra-tier lanes then inherit K3; keep the session effort at `max` when Sol-tier lanes are planned, and accept `high` for purely Terra-tier work.
- For a Luna-tier lane that must actually run on K2.7, launch a separate headless session (for example `kimi -p -m kimi-code/kimi-for-coding`) where the CLI binary is available, or keep the lane on the session model and compensate with the persona's strict, mechanically checkable output contract.
- Wherever the runtime does not expose the intended model or effort, state which model and effort the lane actually ran on instead of claiming the tier was used. Persona, scoping, and evidence rules apply regardless of the model a lane ran on.

## Route to project agents

Read the persona file for the assigned role from `agents/<agent-name>.md` next to this SKILL.md and inject it verbatim at the top of the `Agent` prompt, followed by the bounded assignment. Use `explore` for read-only roles and `coder` for roles that run simulations, create artifacts, or edit files.

| Agent | Tier | Sub-agent type | Assign |
| --- | --- | --- | --- |
| `inet-navigator` | K3, `high` | `explore` | Read-only source ownership, C++/NED/MSG relationships, NED/INI inheritance, typename and feature-gate tracing, and architecture-aware pre-change mapping |
| `inet-evidence-miner` | K2.7, thinking on | `explore` | Bounded artifact discovery and exact extraction from source, logs, PCAPs, event logs, scalars, and vectors; facts only |
| `inet-wifi-specialist` | K3, `max` | `explore` | IEEE 802.11 normative behavior, MAC/PHY exchanges, HE/EHT, aggregation, interference, and normative-versus-implemented analysis |
| `inet-simulation-detective` | K3, `max` | `coder` | Reproduction, runtime divergence, packet/timing mysteries, event causality, crashes, hangs, and LLDB escalation |
| `inet-implementer` | K3, `max` | `coder` | Focused production C++/NED/MSG patch after mechanism and change surface are established |
| `inet-regression-guard` | K3, `high` | `coder` | Deterministic unit/simulation/fingerprint/Wi-Fi regression evidence and narrowly assigned test changes |
| `inet-results-analyst` | K3, `high` | `coder` | Semantically correct `.sca`/`.vec` querying, aggregation, uncertainty, and plots |
| `inet-reviewer` | K3, `max` | `explore` | Independent post-implementation correctness review and formal architecture, naming, and sealing audits |

Use the relevant repository workflow skills inside each lane. An agent role does not replace `inet-simulation-run`, `inet-80211-packet-debugging`, `ieee80211-standards`, testing, build, or result-analysis skills.

## Apply the IEEE 802.11 architecture overlay

For any proposed or actual production change under `src/inet/linklayer/ieee80211/` or `src/inet/physicallayer/wireless/ieee80211/`, require the navigator, implementer, and reviewer to use `inet-architectural-requirements` and read `.agents/skills/inet-architectural-requirements/references/ieee80211-architectural-requirements.md` in full. Map the change to the applicable `AR-WLAN-*` identifiers in addition to the general `R-*` and `AR-*` identifiers. Treat the WLAN rules as design and review constraints, not as evidence of normative IEEE behavior; normative claims still require the applicable standard revision and clause through `ieee80211-standards`.

For every diff review in those subtrees, require the reviewer to apply both semantic checklists: the general `.agents/skills/inet-architectural-requirements/references/enforcement/agent-review-checklist.md` and the WLAN-specific `.agents/skills/inet-architectural-requirements/references/enforcement/ieee80211-agent-review-checklist.md`. Emit the complete general checklist and its `REVIEW: ...` footer first, then the complete WLAN checklist and its final `WLAN REVIEW: ...` footer. Do not collapse, sample, or replace either checklist. Require the regression lane to assess the focused and legacy coverage called for by `AR-WLAN-QUAL-TESTS`.

## Decompose by evidence lane

Delegate proactively when two or more lanes below are independent. Start no more lanes than the task needs and leave thread capacity for follow-up work.

### Static architecture or configuration

Assign `inet-navigator` for ownership, dependency, composition, and effective-configuration tracing, including the pre-change seal and R-*/AR-* map and, for an 802.11 scope, the applicable `AR-WLAN-*` map. Assign `inet-reviewer` when the deliverable is a formal diff-compliance verdict, naming audit, full-scope architecture audit, or sealing audit; require the exact semantic-checklist output and ledger disposition, including both checklist contracts for an 802.11 diff. Add `inet-wifi-specialist` only when the answer depends on 802.11 semantics. Add `inet-evidence-miner` only for a large, explicitly bounded inventory. Keep the work read-only.

### Wi-Fi standards or model gap

Assign `inet-wifi-specialist` to establish the normative rule and implemented behavior and to identify relevant `AR-WLAN-*` constraints without treating them as normative evidence. In parallel, use `inet-navigator` for the relevant INET control/data path when that path is broad or unclear. Require exact IEEE clause/table/field evidence, exact INET files/symbols, and the applicable WLAN architecture identifiers. Do not start implementation until the discrepancy is demonstrated.

### Runtime failure or packet/timing mystery

Assign `inet-simulation-detective` as lead. It may create only named diagnostic artifacts and never edits production source; route an established source fix to `inet-implementer`. Add `inet-wifi-specialist` for MAC/PHY interpretation, `inet-navigator` for effective configuration or ownership, and `inet-evidence-miner` for a precisely specified extraction from existing artifacts. Begin with one configuration and one run/seed. Escalate through logs, PCAP, results, event log, and LLDB only as evidence requires.

### Production change

Establish mechanism and change surface first with the appropriate read-only or runtime lanes. Before implementation under src/inet, enumerate the target paths, resolve their current seal status, map the applicable R-* and AR-* identifiers, and obtain explicit current-conversation user permission for every sealed file. For either 802.11 subtree, also require the full WLAN architecture reference to be loaded, map the applicable `AR-WLAN-*` identifiers, and establish the IEEE revision and clause for new normative behavior. Then assign exactly one `inet-implementer` with file ownership and permitted test scope. After the patch stabilizes, assign `inet-regression-guard` when the risk warrants independent regression evidence, and assign `inet-reviewer` for every architecture-sensitive src/inet change as well as every nontrivial or 802.11 behavior change. For an 802.11 change, regression must address `AR-WLAN-QUAL-TESTS` and review must apply both semantic checklists. Do not have reviewer and implementer edit concurrently.

### Results or plots

Assign `inet-results-analyst`. Use `inet-evidence-miner` only for bounded discovery such as listing run IDs, metric names, modules, units, and recording attributes. Keep statistical interpretation, aggregation choices, and plot approval with the analyst.

## Write bounded assignments

Include all of the following in each delegated task:

- The persona file content, verbatim, at the top of the prompt, with an explicit instruction not to spawn further sub-agents.
- One concrete question or deliverable.
- Relevant paths, symbols, INI configuration, run number/seed, and existing artifact paths.
- Explicit inclusions and exclusions.
- Whether the agent is read-only, may create diagnostic artifacts, may edit tests, or owns named files.
- Applicable general and, for an 802.11 production scope, `AR-WLAN-*` identifiers and the exact architecture/checklist references the lane must load.
- Required evidence and the definition of done.
- Required return shape: concise conclusion, evidence references, exact commands and statuses when commands ran, uncertainty, and recommended next handoff.

Reuse the same agent for a related follow-up (via the `resume` parameter) when its accumulated context is useful. Do not make a fresh agent rediscover the same setup.

## Synthesize and gate handoffs

Keep raw logs and broad exploration out of the root context. Require specialists to return distilled facts and artifact paths.

Resolve conflicts using this order:

1. Reproducible runtime or debugger observation.
2. Packet capture, event log, and recorded result evidence.
3. Demonstrated effective NED/INI configuration.
4. Checked-out INET source behavior.
5. Hypothesis.

Use the applicable IEEE revision as authority for normative behavior and checked-out INET source plus observed runs as authority for implemented behavior. A standard requirement does not prove that INET implements or enables it.

Gate each transition:

- Diagnose to implement: require a demonstrated failure mechanism, bounded change surface, target-path seal decision, applicable R-*/AR-* map, applicable `AR-WLAN-*` map for an 802.11 scope, and explicit permission for every sealed target.
- Implement to verify: require a stable diff and exact claimed behavior.
- Verify to conclude: require tests that exercise the claim, not merely a passing unrelated suite; for 802.11 behavior, reconcile the evidence with `AR-WLAN-QUAL-TESTS`, including applicable boundary and legacy-mode coverage.
- Architecture-sensitive change to conclude: require the applicable check-architecture.sh result reconciled with the ledgers and the complete general semantic-checklist verdict; for an 802.11 diff, also require every WLAN checklist item and the `WLAN REVIEW: ...` footer.
- Audit to seal: require a complete compliant-scope audit, resolution or explicit sanction of every finding, and explicit user approval before recording the seal.
- Fingerprint update: require explicit user approval after explaining the trajectory change.

Stop spawning or running lanes once decisive evidence answers the task. The root agent must report unresolved disagreement or missing evidence instead of averaging incompatible conclusions.
