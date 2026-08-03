# Walkthrough: 802.11ac Short Guard Interval on Wide Channels

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `NOT RUN`
- PCAP: `NOT RUN`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough demonstrates IEEE 802.11ac Very High Throughput (VHT) Short Guard Interval (400 ns) timing on 80 MHz wide subchannels, comparing physical layer symbol duration and throughput gains against standard 800 ns Long Guard Interval.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- Explain the combination of wide 80 MHz channel bonding and Short Guard Interval (400 ns).
- Calculate peak PHY bitrates (e.g., 433.3 Mbps with Short GI vs 390 Mbps with Long GI for 1x1 VHT MCS 9 in 80 MHz).
- Measure overall frame transmission duration reduction.

## [agent] Scenario description

The topology uses [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned). Traffic flows from `server` to `host[0]`.

Two configurations are evaluated:

1. `VhtLongGI`: Standard 800 ns Long Guard Interval in an 80 MHz VHT channel.
2. `VhtShortGI`: Short 400 ns Guard Interval in an 80 MHz VHT channel.

## [agent] Standards and INET model boundary

- **IEEE Std 802.11ac-2013 / 802.11-2020 Clause 21.3.2.4**: Specifies short and long GI timing for 80 MHz and 160 MHz VHT PPDUs.
- **INET Model Boundary**: Guard interval timing is configured via `transmitter.guardInterval = 400ns` and evaluated in `Ieee80211VhtMode`.
