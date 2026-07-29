---
name: inet-80211-walkthrough-writer
description: Create, revise, or review concise, evidence-backed walkthrough.md files for INET IEEE 802.11 examples. Use for explaining a Wi-Fi feature from current configuration and script-generated scalar/vector and PCAP evidence, including plots, tables, packet statistics, and frame exchanges.
---

# Write concise IEEE 802.11 walkthroughs

Explain the feature and what the current evidence proves. Prefer plain language,
short paragraphs, compact sections, and only details that help the reader
understand, reproduce, or debug the example.

Read:

- [references/walkthrough-contract.md](references/walkthrough-contract.md)
- [references/analysis-machinery.md](references/analysis-machinery.md) before
  generating or publishing evidence

Start new documents from
[assets/walkthrough-template.md](assets/walkthrough-template.md).

## Hard boundary: scripts generate analysis content

Use only `examples/ieee80211/analysis/wifi_analysis.py` and its suite-owned
analysis components to generate and publish:

- scalar/vector plots and tables;
- PCAP statistics plots and tables; and
- frame-exchange timelines and tables.

Do not recreate, supplement, or alter these elements with `opp_scavetool`,
TShark, ad hoc Python, manual calculations, or hand-written tables. If an
output is missing or inadequate, extend the shared suite, feature plugin, or
analysis machinery; do not work around it in the walkthrough.

Preserve every script-owned marker block and ledger entry. Agent-owned text
may cite and interpret generated outputs but must not copy their data into a
second presentation.

## Workflow

1. Identify the example, configurations, current walkthrough, and generated
   sessions. Reuse accurate existing prose, but never discuss previous states
   unless the user asks.
2. State one learning question and a small set of testable claims.
3. Use the shared analyzer to inspect, run, report, and publish the required
   evidence. Do not mix sessions.
4. Write only the explanation around the generated blocks:
   - what the feature does and why the scenario exposes it;
   - what each important generated result means;
   - how scalar/vector and packet evidence support or limit each claim;
   - the bounded verdict and the first useful diagnostic for a failure.
5. Remove repetition, raw inventories, obvious statements, speculative
   detail, and jargon that is not needed to understand the feature.
6. Validate:

```sh
python3 .agents/skills/inet-80211-walkthrough-writer/scripts/validate_walkthrough.py \
  --require-analysis-visuals path/to/walkthrough.md
```

Fix errors and assess warnings.

## Evidence rules

- Use `PASS`, `FAIL`, `INCONCLUSIVE`, and `NOT RUN` exactly as defined in the
  contract.
- Treat configuration as requested behavior, not proof of runtime behavior.
- Treat unknown or absent decoded fields as unknown.
- Do not infer mechanism from throughput or frame counts alone.
- Distinguish direct observation, script-derived measurement, and inference.
- Keep run, seed, session, window, capture point, and limitation near the
  supported claim.
- Use relative paths and directly runnable commands.
- Never change fingerprint expectations without explicit user approval.

Use the matching repository skills for configuration tracing, standards
claims, simulations, regression design, or unresolved debugging. Use
`inet-agent-orchestration` when independent specialist lanes are warranted.
These skills may inform interpretation, but must not produce substitute
walkthrough analysis tables, plots, packet statistics, or frame exchanges.
