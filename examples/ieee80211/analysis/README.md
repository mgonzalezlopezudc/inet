# IEEE 802.11 analysis machinery

This directory contains generation-neutral result and packet-capture analysis
components shared by IEEE 802.11 examples. A capture observation has a common
MAC envelope and exactly one typed PHY profile (`legacy`, `ht`, `vht`, `he`, or
`eht`). Feature-specific evidence is layered on top; unavailable PHY facts stay
unknown instead of being filled with configuration-derived defaults.

Suite descriptors live in `suites/`. They declare example roots, configurations,
capture interface patterns, and the walkthrough generated-section marker. The
existing `examples/ieee80211ax/analysis` commands remain compatibility entry
points while they migrate onto these common modules.

Walkthrough-facing analyses publish presentation bundles: a compact Markdown
table, a deterministic figure (or an explicit no-plot rationale), and a JSON
provenance sidecar. Scalar/vector bundles belong under `## Scalar and vector
analysis`; PCAP bundles belong under `## PCAP statistics`. Exhaustive
packet-type rows stay subordinate to the compact explanatory summary.

Run a suite through the shared PCAP pipeline:

```sh
python3 examples/ieee80211/analysis/analyze_pcap.py \
  --suite examples/ieee80211/analysis/suites/be-eht-features.json \
  --generate --subdir eht_features --run 0
```

The PCAP command generates and inserts a compact cross-configuration table,
the packet count-versus-airtime figure, its `.png.json` capture-session
sidecar, representative timelines, and exhaustive packet-type tables in a
marker-bounded block.

The existing AX scalar/vector suite uses the shared campaign primitives and a
manifest-driven presentation renderer:

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
