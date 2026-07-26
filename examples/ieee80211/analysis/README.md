# IEEE 802.11 analysis machinery

This directory contains the authoritative generation-neutral IEEE 802.11
analysis interface. Start with one scenario:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py inspect twt
python3 examples/ieee80211/analysis/wifi_analysis.py run twt \
  --evidence both --runs 5
python3 examples/ieee80211/analysis/wifi_analysis.py report twt \
  --session-id <YYYYMMDDTHHMMSSZ>
python3 examples/ieee80211/analysis/wifi_analysis.py publish twt \
  --session-id <YYYYMMDDTHHMMSSZ> --update
```

`inspect` is fully read-only. With the default `--evidence both`, `run`
creates one multi-run result set. All runs record the selected scalar/vector
data, and run 0 additionally records PCAPng files in the same configuration
directory. It does not analyze the results or edit a walkthrough. `report`
reuses that exact result set to create
metrics, figures, an evidence ledger, and a PCAP report, but never edits a
walkthrough. `publish` is the only walkthrough-mutating command and requires
the explicit `--update` flag.

Scalar/vector and PCAP therefore share the same run-0 simulation trajectory;
the PCAP indexer does not launch a second simulation. Generated scalar/vector
and PCAP figures are stored at the root of that timestamped result set, above
its per-configuration directories.

Pass `--session-id <YYYYMMDDTHHMMSSZ>` to `inspect` to display the selected
logical session and whether it meets the publication run-count policy.

Publication sessions use five independent runs by default. `--runs 1` is
allowed for diagnosis and is labeled diagnostic; any session with fewer than
five scalar/vector runs is rejected by `publish`. PCAP is representative
run-0 mechanism evidence within a combined result session. Run selection follows `[start, end)`
notation, so `--runs 5` covers run numbers `[0, 5)`. `--config` may be
repeated to focus a diagnostic run.

A capture observation has a common MAC envelope and exactly one typed PHY
profile (`legacy`, `ht`, `vht`, `he`, or `eht`). Feature-specific evidence is
layered on top; unavailable PHY facts stay unknown instead of being filled
with configuration-derived defaults.

Suite descriptors live in `suites/`. They declare example roots,
configurations, capture interface patterns, the walkthrough generated-section
marker, and scalar/vector manifest and group mappings. Every declared AX and
BE/EHT scenario has a mapping, so the analyzer only indexes captures from the
shared campaign; it has no standalone simulation mode.

Walkthrough-facing analyses publish presentation bundles: a compact Markdown
table, a deterministic figure (or an explicit no-plot rationale), and a JSON
provenance sidecar. The figure's parent directory identifies its raw result
session, so sidecars retain analysis semantics without repeating raw input
paths and hashes. Scalar/vector bundles belong under `## Scalar and vector
analysis`; PCAP bundles belong under `## PCAP statistics`. Exhaustive
packet-type rows stay subordinate to the compact explanatory summary.

The existing AX scalar/vector suite uses the shared campaign primitives and a
manifest-driven presentation renderer. New workflows should use
`wifi_analysis.py`:

```sh
python3 examples/ieee80211ax/analysis/summarize_results.py
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211ax/analysis/first_tranche.py <group>
python3 examples/ieee80211ax/analysis/render_walkthrough_results.py \
  <group> --update
```

Run the profile tests from the repository root:

```sh
python3 -m unittest discover \
  -s examples/ieee80211/analysis/tests \
  -p 'test_*.py'
```
