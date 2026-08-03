# Walkthrough: 802.11n Preamble Modes (Mixed-Mode vs Greenfield)

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `NOT RUN`
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
