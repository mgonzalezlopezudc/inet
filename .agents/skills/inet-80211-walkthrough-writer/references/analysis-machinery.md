# Shared IEEE 802.11 analysis machinery

`examples/ieee80211/analysis/wifi_analysis.py` is the walkthrough's
authoritative analysis interface. Read
`examples/ieee80211/analysis/README.md`, then run from the repository root:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py inspect <scenario>
python3 examples/ieee80211/analysis/wifi_analysis.py run <scenario> \
  --evidence both --runs 5 --session-id <YYYYMMDDTHHMMSSZ>
python3 examples/ieee80211/analysis/wifi_analysis.py report <scenario> \
  --session-id <YYYYMMDDTHHMMSSZ>
python3 examples/ieee80211/analysis/wifi_analysis.py publish <scenario> \
  --session-id <YYYYMMDDTHHMMSSZ> --update
```

Use suite options reported by `inspect`; do not guess them. `run` records
scalar/vector evidence for all runs and PCAP evidence for representative run
0. `report` generates evidence without editing the walkthrough. `publish` is
the only command allowed to insert or update walkthrough analysis blocks.

## Exclusive output ownership

The shared analyzer and its suite-owned components generate:

- scalar/vector metrics, tables, plots, and provenance;
- PCAP packet statistics, tables, plots, and provenance; and
- representative frame-exchange timelines/tables.

Agents interpret these outputs. They do not reproduce them with direct
`opp_scavetool`, TShark, plotting libraries, spreadsheets, manual
calculations, or hand-authored Markdown.

If output is missing, use these extension points in order:

1. suite/scenario descriptor;
2. feature plugin;
3. typed PHY profile;
4. shared machinery, only for cross-suite behavior.

Never add a scenario-local duplicate of campaign, result, PCAP, provenance,
plotting, table, or timeline logic.

## Evidence constraints

- Use one session for matched scalar/vector and PCAP claims. Separate sessions
  cannot prove event-level causality.
- Publication normally requires five independent scalar/vector runs. A
  one-run diagnostic is not publication evidence.
- Trust only fields marked authoritative by the typed PHY profile (`legacy`,
  `ht`, `vht`, `he`, or `eht`). Missing or ambiguous fields remain unknown.
- Preserve script-generated markers, provenance sidecars, hashes, counting
  semantics, capture points, run/seed scope, and session ledger entries.
- Keep generation-specific decoding in typed profiles.

## Validation

When analysis machinery changes, run:

```sh
python3 -m unittest discover \
  -s examples/ieee80211/analysis/tests -p 'test_*.py'
```

When AX scalar/vector components change, also run:

```sh
python3 -m unittest discover \
  -s examples/ieee80211ax/analysis -p 'test_*.py'
```

Then validate the walkthrough:

```sh
python3 .agents/skills/inet-80211-walkthrough-writer/scripts/validate_walkthrough.py \
  --require-analysis-visuals path/to/walkthrough.md
```
