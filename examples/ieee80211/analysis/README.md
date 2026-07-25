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

Run a suite through the shared PCAP pipeline:

```sh
python3 examples/ieee80211/analysis/analyze_pcap.py \
  --suite examples/ieee80211/analysis/suites/be-eht-features.json \
  --generate --subdir eht_features --run 0
```

Run the profile tests from the repository root:

```sh
python3 -m unittest discover \
  -s examples/ieee80211/analysis/tests \
  -p 'test_*.py'
```
