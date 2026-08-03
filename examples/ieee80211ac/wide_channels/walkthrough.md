# Walkthrough: 802.11ac Wide Channels (80 MHz & 160 MHz)

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `NOT RUN`
- PCAP: `NOT RUN`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough demonstrates IEEE 802.11ac Very High Throughput (VHT) wide channel bonding operations, comparing 20, 40, 80, and 160 MHz subchannels in the 5 GHz band.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- Explain how 802.11ac introduces 80 MHz and contiguous 160 MHz channel bonding in 5 GHz.
- Observe subcarrier scaling (234 data subcarriers in 80 MHz vs 468 data subcarriers in 160 MHz).
- Compare peak PHY bitrates and packet transmission durations across 20, 40, 80, and 160 MHz channel widths.

## [agent] Scenario description

The topology uses [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned). High-throughput UDP streams flow from `server` to `host[0]`.

Four configurations are evaluated:

1. `Vht20MHz`: 20 MHz 5 GHz baseline.
2. `Vht40MHz`: 40 MHz channel bonding.
3. `Vht80MHz`: 80 MHz wide channel bonding.
4. `Vht160MHz`: 160 MHz contiguous wide channel bonding.

## [agent] Standards and INET model boundary

- **IEEE Std 802.11ac-2013 / 802.11-2020 Clause 21.3**: Specifies VHT 20, 40, 80, and 160 MHz OFDM subcarrier layouts and channelization rules.
- **INET Model Boundary**: Channel width spectrum allocation is controlled via `bandName` parameter and handled in `Ieee80211VhtMode`.
