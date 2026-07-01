# Working Effectively With Coding Agents On IEEE 802.11 In OMNeT++ INET

**Date:** 2026-07-01
**Scope:** 802.11ax/HE, early 802.11be/EHT, and supporting INET/OMNeT++ workflow
**Repository:** `/home/user/omnetpp_ws/inet`
**HEAD inspected:** `e42814991b1d6cd067a611d8746d6c44eab5ade8`

## Executive Summary

The most effective way to use a coding agent for implementing IEEE 802.11 in INET is to treat the agent as a fast, disciplined engineering collaborator, not as an oracle for the standard. The successful pattern in this project was:

1. Convert an ambiguous standards feature into a small implementation slice.
2. Anchor that slice in the actual IEEE text and existing INET architecture.
3. Write or extend focused tests before or alongside the change.
4. Run the real INET/OMNeT++ build and simulation commands.
5. Ask for a skeptical review pass.
6. Commit with a detailed implementation and verification summary.
7. Repeat.

This loop worked especially well for HE OFDMA, BlockAck behavior, LDPC/FEC constraints, BSR/BSRP, UORA, rate adaptation, frame-sequence refactoring, standards annotations, and the new standards preprocessing pipeline.

The weakest outcomes came from broad prompts such as "make this standard compliant" without an explicit compliance level, from trusting example results without seeded repetitions, from allowing documentation to drift after code changes, and from letting debugging/instrumentation changes grow without clear acceptance criteria.

The headline recommendation for other developers is simple: make every agent task produce one of four durable artifacts: a test, a standards citation, an executable example, or a scoped refactor that reduces future risk. If a task produces none of those, it is probably agent churn.

## Evidence Base

This report is based on local artifacts available in the workspace:

- `git log` over the 802.11-related paths, especially from 2026-06-16 through 2026-07-01.
- 110 project-relevant Codex session records found in `/home/user/.codex/session_index.jsonl` and the matching session JSONL files.
- Existing plans under `plans/`, including HE refactor, rate adaptation/ER SU/BSR, frame-sequence refactor, and EHT plans.
- Existing reports under `reports/`, especially the 2026-06-24, 2026-06-25, and 2026-06-30 compliance and implementation reviews.
- Commit bodies for representative implementation slices.

Important limitation: the session logs contain structured user prompts, final answers, tool calls, and large tool output, but not always compact human-readable reasoning. I used thread names, prompts, final answers, commits, plans, and reports as the reliable conversation record. I ignored unrelated personal/session metadata and did not treat encrypted reasoning blobs as evidence.

The inspected 802.11-related diff from the pre-HE baseline named in the prior report to current HEAD is large: about `341 files changed, 41888 insertions(+), 644 deletions(-)` across the main 802.11 implementation, examples, tests, plans, reports, and standards tooling. Git shows 50 commits since 2026-06-17 touching the main 802.11 paths after the rebase history shape, and the session index shows a broader sequence starting on 2026-06-16 with ADDBA/BlockAck planning and validation.

## Development Timeline And What It Shows

| Period | Main activity | Workflow lesson |
|---|---|---|
| 2026-06-16 | ADDBA and BlockAck phase planning, pattern mapping, validation plans, review/revision loops | Planning artifacts helped constrain the agent and prevented vague edits. Review passes caught contradictions before implementation. |
| 2026-06-17 | HE DL MU OFDMA migration, single PPDU model, packet tag fixes, unit coverage, validation scripts | Complex PHY/MAC changes needed immediate tests and executable scenarios. |
| 2026-06-18 | Per-STA queue architecture, UL MU OFDMA implementation, Qtenv crash fixes, UL timing review | Running simulations exposed integration bugs that unit tests did not. |
| 2026-06-19 | PHY correctness plan, RU/MCS/NSS/GI/DCM tests, management elements, LDPC, DL MU compliance, examples | Standards features became tractable when decomposed into PHY calculators, serializers, schedulers, and examples. |
| 2026-06-20 | MU-MIMO and sounding work | High-level packet model decisions needed explicit boundaries to avoid overclaiming PHY realism. |
| 2026-06-21 | Qtenv debugging, environment setup, `he_features` runs | GUI/runtime defects need the same verification discipline as source-code changes. |
| 2026-06-23 | TWT support, debug asserts/logging | Logging and assertions helped find integration defects, but they later needed cleanup. |
| 2026-06-24 | Broad 802.11ax review, compliance fixes, serializer work | A skeptical review pass was very productive, especially when grounded in file/line references. |
| 2026-06-25 | On-wire compliance improvements, spatial reuse, UORA, HE rate adaptation, ER SU, BSR accounting | Strongest feature growth happened as small vertical slices with examples and tests. |
| 2026-06-26 | TWT/BSR fixes and walkthroughs | Example walkthroughs improved usability, but result claims can become stale quickly. |
| 2026-06-27 | WATCH instrumentation and cleanup, Qtenv `any_ptr`/descriptor issues | Instrumentation is valuable, but broad instrumentation is itself a risky feature. |
| 2026-06-29 | Standards annotation passes, `HeHcf` split, frame-sequence grammar refactor | Documentation inside code was most useful when it corrected and constrained behavior, not when it merely praised compliance. |
| 2026-06-30 | Implementation review, HE test additions, `HeDlMuPackingPlanner`, standards preprocessing pipeline, EHT plan | The project matured once standards lookup became a first-class local tool and large files were split around testable responsibilities. |
| 2026-07-01 | Early EHT slices: EHT modes, 6 GHz bands, capabilities, error model tests | Starting EHT after building the standards corpus and HE lessons produced better initial boundaries. |

## What Worked Well

### 1. Small Vertical Slices

The best agent tasks were not "implement 802.11ax." They were slices like:

- Enforce active BlockAck admission before HE MU packing.
- Add RU/MCS/NSS/GI/DCM unit coverage.
- Implement UL Trigger response collection.
- Add BSR parsing/accounting and a focused example.
- Add HE Minstrel rate control behind an optional module.
- Extract DL MU packing into `HeDlMuPackingPlanner`.
- Add EHT SU mode support before EHT management serialization.

Each slice crossed enough layers to be useful, but not so many that the agent had to invent a whole architecture in one step.

### 2. Standards Grounding Before Code

The later workflow is clearly stronger than the earlier workflow because the agent was forced to use local standards sources:

- Initially, standards references were manual and sometimes stale.
- Later, `standards/80211ax-2024.txt` and then `bin/inet_process_standards` became the source of truth.
- The EHT plan explicitly says to use `bin/inet_process_standards status`, `search`, and `show`, and to avoid rereading PDFs unless the corpus is stale.

This matters because 802.11 is too broad for an agent's memory. The agent can reason well once it has the right clause/table/chunk in context; without that, it tends to produce plausible but overconfident summaries.

### 3. Review Passes With Findings First

The review sessions were among the highest-leverage uses of the agent. The 2026-06-24 report identified real issues around FEC rules, BCC timing, puncturing validation, stale scripts, weak experiment design, and overclaims. The 2026-06-25 update then confirmed which findings had been fixed and which remained partially true.

The important practice is to make the review adversarial but precise:

- Ask for defects, risks, behavioral regressions, and missing tests.
- Require file/line evidence.
- Separate "standard defect" from "simulation model simplification."
- Re-run after fixes, because the first review becomes stale quickly.

### 4. Tests As The Agent's Memory

The strongest progress came when the agent left executable tests behind:

- HE OFDMA unit tests early in the work.
- RU attenuation and noise isolation regressions.
- HE scheduler tests.
- Trigger, BlockAck, management, serializer, LDPC, puncturing, spatial reuse, UORA, BSR, ER SU, and Minstrel tests.
- Fingerprint rows for HE examples added later.

Tests turned conversation state into repository state. This is essential because agent conversations are ephemeral, but tests survive rebases, model switches, and future contributors.

### 5. Real Simulation Runs

Unit tests were not enough. Several bugs only appeared in example simulations, Qtenv, or debug runs:

- UL OFDMA startup crashes.
- TWT example lifecycle issues.
- Qtenv teardown/assertion problems.
- WATCH descriptor/type problems.
- BSR example crashes.

For INET, "build passes" is not a sufficient done state. A good agent task should specify at least one of:

- A focused unit test command.
- A fingerprint test.
- A short Cmdenv example run.
- A Qtenv/debug run when the bug is visual or GUI-only.

### 6. Detailed Commit Messages

The git history is unusually useful because many commits explain:

- What changed.
- Why it changed.
- Which files matter.
- What was verified.
- What was deliberately left out.

For agent-driven development, this is not cosmetic. It is the durable audit trail that lets later developers reconstruct the agent's intent without reading a full transcript.

The EHT capability commit is a good example: it lists implemented capability maps, MIB wiring, simplified association negotiation, tests, and explicitly says management-frame serialization was deferred pending a standards-backed pass.

### 7. Explicit Model Boundaries

The most honest and useful language in the reports is "standards-aware packet-level simulator" rather than "fully compliant implementation." This distinction matters throughout 802.11:

- HE PHY headers may be packet-level metadata, not bit-exact HE-SIG.
- LDPC gain may be a model assumption, not a decoder.
- Spatial reuse may be threshold-based, not full SRG/non-SRG/dual-NAV behavior.
- Scheduler behavior is policy, not prescribed by the standard.
- MU-MIMO may be a packet-level abstraction, not a calibrated beamforming model.

The agent worked best when it named these boundaries in code comments, reports, examples, and commit messages.

### 8. Refactoring After Feature Pressure

The project did not start by inventing a perfect architecture. It first implemented enough behavior to see the pressure points, then refactored:

- `HeHcf` was split into responsibility-specific compilation units.
- HE frame sequences moved toward grammar-style steps.
- Timeout behavior and response validation moved into receive-step policy.
- DL MU packing moved into `HeDlMuPackingPlanner`.
- HE SIG-B and RU helpers moved out of header-heavy implementations.
- Per-AC UORA state replaced the earlier shared state simplification.

This is a good agent pattern: use feature work to locate complexity, then ask the agent for a targeted refactor with characterization tests.

### 9. Documentation That Runs Near The Code

The best documentation artifacts were not generic summaries. They were:

- Implementation plans tied to files.
- Compliance reports tied to commands and current output.
- Walkthroughs tied to example configs.
- Comments tied to clauses/tables.
- Commit messages tied to verification commands.

The further documentation was from executable state, the faster it drifted.

## What Did Not Work Well

### 1. Broad Compliance Prompts Produced Overclaims

Prompts like "ensure 802.11 compliance" can push an agent toward broad, confident language. The later reports had to correct this by distinguishing:

- Packet-level model compliance.
- Field/layout serialization compliance.
- Full wire/interoperability compliance.
- Research fidelity against external data.

For standards work, the prompt must specify the level of compliance. Otherwise, the agent may treat a useful simulator approximation as if it were a bit-exact implementation.

### 2. Documentation And Example Results Drifted

Several reviews found stale paths, stale environment commands, stale run durations, stale numerical packet counts, and old compliance claims. This is predictable in fast agent-driven work.

The fix is procedural: every change to an example must update or invalidate its walkthrough and result claims in the same slice. If a result is not generated by a command in the repo, it should be treated as illustrative, not authoritative.

### 3. One-Run Example Metrics Were Too Weak

Packet-count comparisons in BSS coloring and scheduler examples were useful demonstrations, but not strong experimental evidence. Without fixed seeds, repetitions, confidence intervals, offered-load sweeps, and delay/fairness metrics, the agent can accidentally write benchmark-like claims from smoke-test data.

For research-facing examples, require:

- Fixed seeds or seed sets.
- Repetitions.
- Throughput, delay, loss, fairness, retry, PPDU duration, and airtime metrics as appropriate.
- Clear separation between smoke tests and claims.

### 4. Instrumentation Caused Its Own Work

WATCH instrumentation improved visibility, but broad WATCH additions caused descriptor/type issues and needed cleanup. Instrumentation in C++ simulation frameworks is code, not free observability.

Good instrumentation tasks should define:

- Which dynamic state is worth watching.
- Which static/config values should stay out.
- Whether values are printable and descriptor-safe.
- How Qtenv and Cmdenv are both verified.

### 5. Environment Drift Consumed Time

The conversations show repeated friction around:

- OMNeT++ environment sourcing.
- Version paths such as `6.4.0` versus `6.4.0aipre2`.
- Qtenv GUI/debug execution.
- MCP availability for controlling simulations.
- ccache behavior.

The eventual `AGENTS.md` instructions are valuable because they turn that into a stable ritual. Agent tasks should not rediscover the environment every time.

### 6. Too-Broad Searches Waste Context

Searching the entire processed standards corpus or all session logs without a tight filter produced enormous output. This is a context-management failure, not an intelligence failure.

For this project, the better rule is:

- Use `bin/inet_process_standards search` before raw text/PDF search.
- Search specific directories first.
- Exclude `standards/processed/pages` unless inspecting exact page text.
- Prefer `git log -- path` over full history dumps.
- Extract structured session fields instead of grepping full JSONL logs.

### 7. Large Orchestration Files Remained Risky Too Long

The reports repeatedly identify large HE orchestration files as risky. They were productive at first, but over time they became hard to review. The later planner extraction and `HeHcf` split were the right response, but they came after considerable complexity had accumulated.

For future EHT/MLO work, extract planners and validators earlier, especially around:

- RU/MRU packing.
- Per-link MLO scheduling.
- Capability negotiation.
- Trigger/response collection.
- BlockAck/reorder state.
- Puncturing validation.

## Recommended Agent Development Cycle

Use this cycle for every substantial 802.11 feature.

### 1. Frame The Slice

Write a short task statement with:

- Feature name and standard family, for example HE BSRP or EHT MRU.
- Intended model level: packet-level, field-level, bit-exact serializer, or research approximation.
- In-scope and out-of-scope behavior.
- Existing files to read first.
- Expected tests or examples.

Bad:

```text
Implement 802.11be MLO.
```

Better:

```text
Implement the first MLO association-state slice for infrastructure AP/STA only.
Use the standards corpus to inspect Multi-Link element and TID-to-Link Mapping fields.
Do not implement STR scheduling yet.
Add serializer round-trip tests and a simplified association test.
Preserve existing ax behavior.
```

### 2. Force Standards Lookup

Before implementation, make the agent identify:

- The relevant clauses/tables/figures.
- The exact local source command used to find them.
- Which details are normative and which are AP policy or implementation choice.

For this repo, prefer:

```sh
bin/inet_process_standards status
bin/inet_process_standards search "EHT Operation element"
bin/inet_process_standards show 80211be-2024:chunk:NNNNN
```

The agent should not quote long standard text in code or reports. It should cite clauses/tables/chunks and paraphrase the implementation consequence.

### 3. Map To Existing INET Architecture

Ask the agent to identify the natural extension point:

- PHY mode/table/calculator.
- PHY header/tag/serializer.
- Radio/transmitter/receiver/error model.
- MAC frame definition/serializer.
- MIB/capability negotiation.
- HCF/DCF/coordination function.
- Frame sequence.
- Scheduler/rate control.
- Example or validation script.

This reduces "new abstraction first" behavior. The best changes reused existing INET patterns unless the old abstraction was clearly too narrow.

### 4. Define Tests Before The Final Shape

For each slice, choose test level by risk:

| Risk | Required checks |
|---|---|
| Pure helper/math/table | Unit test with standard-derived values. |
| Serializer/field layout | Round-trip test plus golden byte/bit vectors when possible. |
| Scheduler/rate/policy | Unit test with deterministic inputs and rejection reasons. |
| Frame sequence | Unit test for success, timeout, unexpected response, and partial failure. |
| Radio/medium/error model | Focused unit test plus at least one example smoke run. |
| User-facing example | Fingerprint or validation script plus walkthrough command. |
| Shared legacy path | Legacy regression test, not only HE/EHT tests. |

The agent should state why a narrower or broader test is appropriate.

### 5. Implement In One Commit-Sized Slice

The agent should make the smallest coherent change that:

- Compiles.
- Has focused tests.
- Keeps old behavior gated.
- Documents model assumptions.
- Avoids unrelated cleanup.

If the change starts touching too many layers, split it.

### 6. Verify With The Standard INET Ritual

Use the repo's actual environment instructions. At minimum:

```sh
export CCACHE_DISABLE=1
source /home/user/omnetpp-6.4.0aipre2/setenv -f
source setenv -q
bin/inet_run_unit_tests -m release -f "(Ieee80211He|HeDlScheduler|HeUlScheduler).*\\.test"
```

For feature examples, also run a representative Cmdenv/Qtenv scenario. For changes near the common radio/medium, run non-HE regression coverage as well.

### 7. Ask For A Review Pass

After implementation, ask:

```text
Review this change as a standards and regression reviewer.
Find bugs, overclaims, missing tests, stale docs, and legacy 802.11 risks.
Lead with findings and file/line evidence.
```

This pattern worked repeatedly. It caught stale docs, wrong standard assumptions, and test gaps.

### 8. Update Durable Artifacts

Before finishing, update the artifacts that future developers will read:

- Code comments where constants or model approximations are non-obvious.
- Example walkthroughs if configs or results changed.
- Compliance/model-limitations notes.
- Test lists or fingerprint CSVs.
- Commit message with verification commands and deferred work.

### 9. Commit With A Verification Ledger

A good agent-generated commit message should include:

- Summary of behavior.
- Main files/classes changed.
- Tests run.
- Example runs, if any.
- Known limitations or deliberately deferred work.

This makes `git log` a usable project memory.

## Prompting Patterns That Worked

### Research Prompt

```text
Inspect the existing INET 802.11 architecture and the local 802.11 standard corpus.
For [feature], identify the relevant clauses/tables, existing files to extend, model-level decisions, and test plan.
Do not edit code yet.
```

This worked for standards-heavy features where the right shape was not obvious.

### Implementation Prompt

```text
Implement [narrow feature slice] using the plan.
Preserve existing ax/non-HE behavior.
Add focused unit tests and run the targeted test command.
If exact bit-level behavior is not implemented, document the packet-level model boundary.
```

This worked when scope was constrained and verification was explicit.

### Review Prompt

```text
Review the current implementation for standards compliance, model limitations, stale documentation, and regression risk.
Findings first, with file/line references.
Separate true defects from intentional simulator approximations.
```

This worked because it changed the agent's posture from builder to skeptic.

### Refactor Prompt

```text
Refactor [large component] only where it reduces real risk.
Keep public behavior compatible.
Add characterization tests for the old behavior before moving logic.
Stop before changing unrelated policy.
```

This worked for `HeHcf`, frame-sequence policy, and DL MU packing.

### Debugging Prompt

```text
Reproduce [crash/warning] with the prescribed OMNeT++/INET environment.
Minimize the failing scenario.
Explain the failing state, patch the smallest owner, and rerun the same scenario.
Leave unrelated worktree changes untouched.
```

This worked for Qtenv and simulation crashes because it kept the loop concrete.

## Recommended Roles For Agents

For standards work, one agent persona is not enough. Rotate roles deliberately:

| Role | Use it for | Output |
|---|---|---|
| Researcher | Understanding standard clauses and existing architecture | Plan, citations, risk map |
| Implementer | Making the scoped code change | Patch, tests, verification |
| Reviewer | Finding defects and overclaims | Findings, stale docs, missing tests |
| Debugger | Reproducing runtime failures | Minimal reproduction, root cause, fix |
| Librarian | Updating reports, walkthroughs, commit messages | Durable docs and traceability |

The same model can play these roles, but the prompt should make the role explicit. The biggest mistakes happened when a builder was implicitly asked to judge its own compliance too early.

## Practical Definition Of Done

For an 802.11 standards slice, "done" should mean:

- Standards source identified by clause/table/chunk or an explicit statement that the behavior is implementation policy.
- Existing INET owner extended, not bypassed.
- Unit tests added or a documented reason why not.
- Shared-path regression considered.
- Build/test command run with OMNeT++ and INET environments sourced.
- Example or fingerprint updated when user-facing behavior changes.
- Documentation updated or stale claims removed.
- Model approximation named.
- Commit message includes verification and deferred work.

For high-risk slices, add:

- Golden serializer vectors.
- Seeded simulation repetitions.
- Cross-mode tests for a/g/n/ac/ax.
- Sanitizer or debug-mode run.
- External comparison against ns-3, analytical formulas, or measurements.

## Compliance Language Guide

Use precise language. The agent should not write:

```text
Fully standard compliant 802.11ax implementation.
```

Prefer one of:

- "Standards-aware packet-level model."
- "Standard-derived field layout for the modeled subset."
- "Serializer round-trip coverage for the INET representation."
- "Bit-exact encoding for this field, verified by golden vectors."
- "Simulation policy, because the standard leaves AP scheduling unspecified."
- "Approximation, not a hardware-calibrated PHY model."

This language protects users from overinterpreting results and helps reviewers focus on the real fidelity boundary.

## Recommended Improvements To The Cycle

### 1. Create A Standards Traceability Matrix

Maintain a table per feature:

| Feature | Clause/table/chunk | Code owner | Test owner | Model level | Status |
|---|---|---|---|---|---|

This should live near `reports/` or `docs/80211/`. It would prevent repeating the same compliance archaeology.

### 2. Add A Model Limitations Document

Create one central document for:

- LDPC/error-model assumptions.
- Rate-control heuristics.
- Scheduler policy choices.
- Spatial reuse simplifications.
- Packet-level versus bit-level PHY boundaries.
- Example limitations.
- EHT/MLO incomplete areas.

Examples and reports should link to it instead of each inventing their own caveats.

### 3. Treat Examples As Test Assets

Every example should have:

- A walkthrough.
- A fingerprint or validation row.
- A command that regenerates key results.
- Seed information.
- A smoke-test versus benchmark label.

Do not let examples become screenshots of past behavior.

### 4. Keep A "Stale Claim" Checklist

After any feature change, search for:

- Old paths, especially renamed examples.
- Old OMNeT++ version paths.
- Old run durations.
- Old packet counts.
- "Compliant" language.
- "TODO" comments that became false.
- Checked-in result files.

This was a recurring source of avoidable cleanup.

### 5. Run Legacy Regression Gates Earlier

HE/EHT changes often touch common 802.11 code:

- Frame serializers.
- HCF/DCF behavior.
- Radio medium/interference.
- Mode selection.
- Management association.
- BlockAck.

The agent should be required to identify which legacy path might have changed and run or add a corresponding regression.

### 6. Separate Planning From Implementation For Large Standards Features

The EHT plan is a better model than a direct implementation prompt. For features like MLO, puncturing, or EHT Trigger support:

1. Plan only.
2. Review the plan.
3. Implement the first slice.
4. Review the slice.
5. Continue.

This is slower per prompt but faster over the week because it avoids large wrong turns.

### 7. Improve Session And Context Hygiene

The session history was useful but noisy. Future agent workflows should make durable summaries by default:

- End every major task with a short "lessons learned" note in the relevant plan/report.
- Keep the current verification commands in `AGENTS.md`.
- Prefer standards chunk IDs over copied standard text.
- Avoid broad raw searches over `standards/processed/pages`.
- Use filtered `git log -- path` and structured JSON extraction when reviewing history.

### 8. Make Debug Instrumentation Testable

For WATCH/log/debug changes:

- Add a tiny scenario or Qtenv/Cmdenv smoke run.
- Avoid watching static constants.
- Avoid complex templated unit values unless descriptor-safe.
- Make cleanup part of the acceptance criteria.

### 9. Add External Validation Where Claims Need It

The current model is useful, but research-grade claims need comparisons:

- Analytical PHY duration calculations.
- ns-3 Wi-Fi model outputs for comparable scenarios.
- Hardware or trace data where available.
- Reproducible parameter sweeps.

The agent can help build the harness, but it should not invent validation credibility.

## Guidance For Other Developers

### Give The Agent The Right Inputs

Provide:

- The exact standard family and feature.
- The model fidelity expected.
- Relevant examples or tests.
- Known commands for build and simulation.
- Constraints on preserving old behavior.
- Whether you want planning, implementation, review, or debugging.

Do not provide:

- A broad standards mandate with no acceptance test.
- A huge PDF as the only context.
- A request to optimize or refactor without naming the risk.
- A benchmark claim without a measurement protocol.

### Keep Human Control Over Semantics

The agent is good at:

- Finding owner files.
- Following existing patterns.
- Writing tests.
- Running repetitive verification.
- Drafting comments and reports.
- Refactoring mechanical complexity.
- Comparing code to a provided standard excerpt.

The human should decide:

- Model fidelity boundaries.
- Research claims.
- Which standard branches are in scope.
- Whether a simplification is acceptable.
- What benchmark evidence is sufficient.
- When to stop feature growth and stabilize.

### Ask For Evidence, Not Confidence

Replace:

```text
Are you sure this is correct?
```

with:

```text
Show the standard source, the code owner, the test that would fail if this were wrong, and the remaining approximation.
```

This consistently produces better engineering output.

## Final Recommendations

1. Keep using agents aggressively for 802.11 work, but only inside a standards-grounded, test-first loop.
2. Turn the standards preprocessing pipeline into the mandatory first step for EHT/MLO work.
3. Require every feature slice to leave behind tests, examples, or traceability.
4. Use review prompts after implementation, not only before.
5. Distinguish packet-level simulation validity from bit-level protocol conformance everywhere.
6. Make examples reproducible and label smoke tests honestly.
7. Refactor when complexity becomes visible, but preserve behavior with characterization tests.
8. Keep commit messages rich; they are the most accessible long-term memory of agent-assisted work.

The project succeeded when the agent was used as a tireless engineer inside a disciplined loop. It struggled when asked to be a standards authority without source text, a benchmark scientist without experiment design, or a reviewer of undocumented assumptions. For OMNeT++ INET and IEEE 802.11, the winning style is not "agent, build the standard." It is "agent, help me turn this one clause and this one model decision into tested, reviewable simulator behavior."
