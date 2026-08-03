# Walkthrough: 802.11n Preamble Modes (Mixed-Mode vs Greenfield)

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260803T190450Z`
- PCAP: `NOT RUN`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough demonstrates IEEE 802.11n preamble formats, comparing HT Mixed-Mode preambles (with legacy 802.11a/g preamble headers for backwards compatibility) against HT Greenfield preambles (optimized pure-HT PHY header).

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- Explain the structural difference between HT Mixed-Mode (L-STF, L-LTF, L-SIG, HT-SIG, HT-STF, HT-LTF) and HT Greenfield (HT-GF-STF, HT-LTF1, HT-SIG, HT-LTF).
- Understand how Mixed-Mode allows legacy 802.11a/g stations to decode L-SIG and update their Network Allocation Vector (NAV) duration even if they cannot decode the HT payload.
- Measure the overhead reduction achieved by Greenfield preambles in networks with homogeneous 802.11n hardware.

## [agent] Scenario description

The topology uses [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned). UDP data is transmitted from `server` to `host[0]`.

Two configurations are evaluated:

1. `HtMixedMode`: `opMode = "n(mixed-2.4Ghz)"` operating with legacy spoofing header fields.
2. `HtGreenfield`: `opMode = "n(greenfield-2.4Ghz)"` operating with streamlined Greenfield headers.

## [agent] Standards and INET model boundary

- **IEEE Std 802.11n-2009 / 802.11-2020 Clause 19.3.2 & 19.3.9**: Specifies HT Mixed-Mode and HT Greenfield PPDU frame structure formats.
- **INET Model Boundary**: HT preamble formats are selected via the `opMode` parameter and validated by `Ieee80211HtPreambleMode`.

## [agent] Scalar and vector analysis

<!-- BEGIN GENERATED: ieee80211-scalar-vector-preamble_modes -->
### [script] Generated scalar/vector plot and table

![preamble_modes scalar/vector analysis](results/20260803T190450Z/preamble-modes-delivery-delay.png)

Figure provenance: [`results/20260803T190450Z/preamble-modes-delivery-delay.png.json`](results/20260803T190450Z/preamble-modes-delivery-delay.png.json). Run-level metric source: [`../../ieee80211ax/analysis/metrics.json`](../../ieee80211ax/analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)<br>vector / **.app[*] / endToEndDelay:vector
- Window / per-run aggregation / exclusions: [0.3, 0.9) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| HT Greenfield / goodput mbps | 16 | 2.46598e-15 |
| HT Mixed-Mode / goodput mbps | 16 | 2.46598e-15 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **INCONCLUSIVE** | Compare application delivered bytes across Mixed-Mode and Greenfield HT preambles | No manifest acceptance threshold defined for preamble mode comparison |
<!-- END GENERATED: ieee80211-scalar-vector-preamble_modes -->
