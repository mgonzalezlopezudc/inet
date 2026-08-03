# Walkthrough: 802.11ac Extended Multi-Stream MIMO (Up to 8x8)

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `NOT RUN`
- PCAP: `NOT RUN`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough demonstrates IEEE 802.11ac Very High Throughput (VHT) multi-stream spatial multiplexing, expanding spatial stream capacity from 4 streams (HT limit) up to 8 spatial streams.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- Explain how 802.11ac extends spatial stream support up to 8 spatial streams.
- Understand peak throughput calculations ($8 \times \text{stream bitrate}$, reaching up to 3.466 Gbps in 80 MHz channels).
- Compare transmission duration scaling across 1x1, 4x4, and 8x8 antenna configurations.

## [agent] Scenario description

The topology uses [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned). High-volume traffic is sent from `server` to `host[0]`.

Three configurations are evaluated:

1. `SingleStreamVht`: 1x1 VHT antenna configuration (433.3 Mbps in 80 MHz).
2. `FourStreamVht`: 4x4 VHT antenna configuration (1.733 Gbps in 80 MHz).
3. `EightStreamVht`: 8x8 VHT antenna configuration (3.466 Gbps in 80 MHz).

## [agent] Standards and INET model boundary

- **IEEE Std 802.11ac-2013 / 802.11-2020 Clause 21.3.2 & Table 21-26**: Defines VHT 1-8 spatial stream parameters.
- **INET Model Boundary**: Antenna element scaling (`numAntennas = 8`) and spatial demux processing are modeled in `inet::ieee80211::Ieee80211Radio`.
