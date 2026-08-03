# Walkthrough: 802.11ac VHT MCS Rates & 256-QAM

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `NOT RUN`
- PCAP: `NOT RUN`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough demonstrates IEEE 802.11ac Very High Throughput (VHT) Modulation and Coding Schemes (MCS 0 to MCS 9), highlighting the introduction of 256-QAM modulations (VHT MCS 8 & 9).

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- Understand the VHT MCS table (VHT MCS 0–9) and how 256-QAM (8 bits per subcarrier) increases spectral efficiency over HT 64-QAM (6 bits per subcarrier).
- Compare peak bitrates between 64-QAM (MCS 7) and 256-QAM (MCS 8 and MCS 9).
- Evaluate required SNR thresholds for error-free 256-QAM reception.

## [agent] Scenario description

The topology uses [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned). UDP data is transmitted from `server` to `host[0]`.

Four configurations are evaluated:

1. `BaselineHtMcs7`: 802.11n HT MCS 7 baseline (64-QAM 5/6, 65 Mbps).
2. `VhtMcs7`: 802.11ac VHT MCS 7 (64-QAM 5/6, 78 Mbps in 20 MHz due to 52 subcarriers).
3. `VhtMcs8_256QAM`: 802.11ac VHT MCS 8 (256-QAM 3/4, 86.7 Mbps in 20 MHz).
4. `VhtMcs9_256QAM`: 802.11ac VHT MCS 9 (256-QAM 5/6, 96.3 Mbps in 20 MHz).

## [agent] Standards and INET model boundary

- **IEEE Std 802.11ac-2013 / 802.11-2020 Clause 21.3.5 & Table 21-22**: Defines VHT MCS parameters, coding rates, and modulation schemes up to 256-QAM.
- **INET Model Boundary**: VHT MCS indexing and NIST error rate mappings are handled in `inet::ieee80211::Ieee80211VhtMode` and `Ieee80211NistErrorModel`.
