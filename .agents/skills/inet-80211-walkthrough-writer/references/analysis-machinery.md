# Shared IEEE 802.11 analysis machinery

Use the generation-neutral machinery under `examples/ieee80211/analysis`
before creating scenario-local scripts. It provides a stable MAC observation
envelope with typed Legacy, HT, VHT, HE, and EHT PHY profiles. The existing AX
scalar/vector commands are compatibility entry points over this shared layer;
the PCAP implementation itself is shared by AX and EHT suites.

## Contents

- [Discovery](#discovery)
- [Presentation bundles](#presentation-bundles)
- [Scalar and vector presentation](#scalar-and-vector-presentation)
- [Shared PCAP entry point](#shared-pcap-entry-point)
- [Typed PHY evidence](#typed-phy-evidence)
- [EHT raw-radiotap fallback](#eht-raw-radiotap-fallback)
- [Extension decision](#extension-decision)
- [Validation](#validation)

## Discovery

1. Read `examples/ieee80211/analysis/README.md`.
2. Inspect `examples/ieee80211/analysis/suites/` for a descriptor covering the
   example.
3. Match the scenario's INI path, configurations, PHY profiles, feature
   plugins, capture-interface patterns, and generated-section marker.
4. Inspect the example's existing walkthrough markers and retained artifact
   manifests before generating anything.
5. Reuse the suite output and provenance paths. Do not silently mix sessions.

Use `suites/ax.json` for the existing IEEE 802.11ax examples and
`suites/be-eht-features.json` for the initial EHT adopter. Add a declarative
suite or scenario entry when the common pipeline applies to a new example.

## Presentation bundles

Each analysis family must publish a self-contained presentation bundle:

- a compact Markdown table for the explanatory section;
- a deterministic PNG when a comparison, distribution, or timeline benefits
  from a plot, or an explicit no-plot rationale;
- a JSON provenance sidecar for every generated PNG; and
- a marker-bounded Markdown fragment that can be regenerated without
  overwriting the author's explanation.

The scalar/vector and PCAP bundles belong inside their canonical walkthrough
sections. Exhaustive packet-type tables remain subordinate to the compact
summary. Generated images and tables are evidence views, not new evidence;
their provenance must bind them to the exact result or capture session.
Every heading emitted inside a generated block starts with `[script]`.
Whenever a publisher updates its generated block, it also updates its own
entry in the marker-bounded results-session ledger directly below the
walkthrough title. It must preserve the other script family and the separate
agent-owned results-session line.

## Scalar and vector presentation

For the manifest-driven AX analyses, generate the run-level metrics and plots,
then render the walkthrough bundle:

```sh
python3 examples/ieee80211ax/analysis/summarize_results.py \
  --session-id <YYYYMMDDTHHMMSSZ>
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211ax/analysis/first_tranche.py <group> \
  --session-id <YYYYMMDDTHHMMSSZ>
python3 examples/ieee80211ax/analysis/render_walkthrough_results.py \
  <group> --update
```

The renderer reads `metrics.json`, the experiment manifest, the selected
figure, and its `.png.json` sidecar. It writes a compact table with one row per
configuration and metric under a group-specific
`ieee80211-scalar-vector-*` marker. The table reports the independent-run
count, mean or direct value, and 95% confidence-interval half-width when
available. The surrounding authored text remains responsible for naming the
source query/module/unit, measurement window, aggregation, exclusions, and
interpretation.

For a new suite, reuse this split: compute validated run-level summaries,
produce the deterministic figure and sidecar, then render the bounded
Markdown fragment. Do not make a plotting function edit prose or make a
Markdown renderer reinterpret raw vector samples as repetitions.

## Shared PCAP workflow

Run from the repository root:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py run <scenario> \
  --suite <suite> --evidence pcap --runs 1 \
  --session-id <YYYYMMDDTHHMMSSZ>
python3 examples/ieee80211/analysis/wifi_analysis.py report <scenario> \
  --suite <suite> --session-id <YYYYMMDDTHHMMSSZ>
```

Start with one configuration and run when the command supports selecting
them. Record the exact command, suite descriptor, scenario, configuration,
run, seed, result directory, capture points, exit status, and generated
artifacts in the walkthrough.

The shared analyzer loads suite-owned paths, configurations, capture patterns,
and generated-section markers before validating the selected scenario. Do not
duplicate its campaign construction, capture
discovery, provenance binding, generated-block handling, or common PCAP
statistics in a scenario-local script. The generated PCAP bundle contains a
compact cross-configuration table, the packet count-versus-airtime plot, a
`.png.json` provenance sidecar, representative timelines, and subordinate
exhaustive packet-type tables. On first insertion it is placed under
`## PCAP statistics`; an existing marker-bounded block is updated in place or
migrated into that section.

## Typed PHY evidence

Use `inet_wifi_analysis.decode_phy_observation()` for PHY interpretation. Each
observation has one typed profile:

- `legacy`
- `ht`
- `vht`
- `he`
- `eht`

Treat the profile's `authoritative_fields` as the boundary for factual claims.
Carry its `limitations` into the evidence ledger and walkthrough when they
affect a central claim.

Apply these fail-closed rules:

- Require capture-format presence and known bits before accepting a decoded
  value.
- Keep absent, unsupported, invalid, or ambiguous fields unknown.
- Do not derive width, guard interval, NSS, coding, RU allocation, PHY format,
  or user attribution from INI defaults merely because the run requested
  them.
- For EHT per-user fields, use only the unique user entry marked as the
  captured user. Missing or duplicate markers make MCS, NSS, and coding
  unknown.
- Do not turn a missing legacy width, guard interval, or NSS into a 20 MHz,
  0.8 microsecond, single-stream observation.
- Keep generation-specific decoding inside the corresponding typed profile.

Unknown evidence yields `INCONCLUSIVE` or a scoped limitation when it is
decisive; it never becomes `PASS` because a plausible default exists.

## EHT raw-radiotap fallback

Some TShark versions recognize U-SIG and EHT radiotap presence while leaving
their exported values empty. In that case use
`inet_wifi_analysis.radiotap.extract_eht_radiotap()` through the shared
pipeline. It parses the raw radiotap bytes and fails closed on unsupported
layouts.

Record:

- the TShark version;
- whether normal field export or raw fallback supplied the values;
- the capture and frame scope;
- any fields that remain unknown.

Do not parse raw bytes in walkthrough prose or add a second scenario-local
EHT parser.

## Extension decision

Prefer these extension points in order:

1. Add or update a suite/scenario descriptor for paths, configurations,
   capture patterns, PHY profiles, and generated markers.
2. Add a feature plugin for feature-specific invariants, fields, timelines,
   or summaries that sit above the common observation envelope.
3. Extend a typed PHY profile only when the capture format exposes a new
   generation-specific fact with authoritative presence/known semantics.
4. Change the common envelope or campaign machinery only for behavior shared
   across multiple generations or suites.

Do not flatten generation-specific PHY semantics into one bag of optional
fields. Keep the common MAC/provenance/campaign layer stable and isolate PHY
differences in typed profiles.

## Validation

Run the shared profile and suite tests:

```sh
python3 -m unittest discover \
  -s examples/ieee80211/analysis/tests \
  -p 'test_*.py'
```

When AX-specific scalar/vector analysis code changes, also run:

```sh
python3 -m unittest discover \
  -s examples/ieee80211ax/analysis \
  -p 'test_*.py'
```

Validate a newly authored or migrated walkthrough with the presentation-bundle
gate enabled:

```sh
python3 .agents/skills/inet-80211-walkthrough-writer/scripts/validate_walkthrough.py \
  --require-analysis-visuals path/to/walkthrough.md
```

For a new suite or feature plugin, validate at least one real capture through
the shared entry point. Compare its configuration/run/seed binding, decoded
observation count, decisive fields, limitations, and generated marker against
the walkthrough's evidence ledger.

If the shared machinery cannot express a required invariant, document the gap
in the walkthrough's `Implementation plan`: name the missing observation,
smallest extension point, validation case, compatibility boundary, and any
architecture or sealing permission gate.
