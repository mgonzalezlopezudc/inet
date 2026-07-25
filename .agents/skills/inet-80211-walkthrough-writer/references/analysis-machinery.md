# Shared IEEE 802.11 analysis machinery

Use the generation-neutral machinery under `examples/ieee80211/analysis`
before creating scenario-local scripts. It provides a stable MAC observation
envelope with typed Legacy, HT, VHT, HE, and EHT PHY profiles. The existing AX
analysis commands are compatibility entry points over this shared layer.

## Contents

- [Discovery](#discovery)
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

## Shared PCAP entry point

Run from the repository root:

```sh
python3 examples/ieee80211/analysis/analyze_pcap.py \
  --suite examples/ieee80211/analysis/suites/<suite>.json \
  --generate --subdir <scenario> --run <run>
```

Start with one configuration and run when the command supports selecting
them. Record the exact command, suite descriptor, scenario, configuration,
run, seed, result directory, capture points, exit status, and generated
artifacts in the walkthrough.

The shared launcher configures the compatibility analyzer with suite-owned
paths and markers. Do not duplicate its campaign construction, capture
discovery, provenance binding, generated-block handling, or common PCAP
statistics in a scenario-local script.

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

When AX compatibility code changes, also run:

```sh
python3 -m unittest discover \
  -s examples/ieee80211ax/analysis \
  -p 'test_*.py'
```

For a new suite or feature plugin, validate at least one real capture through
the shared entry point. Compare its configuration/run/seed binding, decoded
observation count, decisive fields, limitations, and generated marker against
the walkthrough's evidence ledger.

If the shared machinery cannot express a required invariant, document the gap
in the walkthrough's `Implementation plan`: name the missing observation,
smallest extension point, validation case, compatibility boundary, and any
architecture or sealing permission gate.
