# Walkthrough: 802.11n Channel Widths & Channel Bonding

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `NOT RUN`
- PCAP: `NOT RUN`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough demonstrates IEEE 802.11n 20 MHz and 40 MHz channel bonding operations, highlighting throughput scaling and subchannel offset configurations.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- Explain how 802.11n bonds two adjacent 20 MHz subchannels into a 40 MHz channel to double peak PHY data rates.
- Identify primary and secondary 20 MHz subchannel relationships.
- Compare transmission duration and throughput between 20 MHz and 40 MHz channel widths.

## [agent] Scenario description

The topology uses [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned). UDP traffic is sent from `server` to `host[0]`.

Four configurations are evaluated:

1. `Ht20MHz`: 20 MHz HT channel in 2.4 GHz.
2. `Ht40MHz`: 40 MHz HT channel bonding in 2.4 GHz.
3. `Ht40MHzSecondaryAbove`: 40 MHz channel with secondary subchannel positioned +20 MHz above primary.
4. `Ht40MHzSecondaryBelow`: 40 MHz channel with secondary subchannel positioned -20 MHz below primary.

## [agent] Standards and INET model boundary

- **IEEE Std 802.11n-2009 / 802.11-2020 Clause 19.3.3 & 19.3.11**: Specifies 20 MHz and 40 MHz OFDM subcarrier layouts (52 data subcarriers in 20 MHz vs 108 data subcarriers in 40 MHz).
- **INET Model Boundary**: Channel width and subchannel frequency spectrum allocation are set via radio and radioMedium parameters (`channelWidth = 40MHz`).
