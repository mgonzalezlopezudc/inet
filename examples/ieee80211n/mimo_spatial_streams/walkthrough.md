# Walkthrough: 802.11n MIMO & Spatial Multiplexing

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `NOT RUN`
- PCAP: `NOT RUN`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough demonstrates IEEE 802.11n Multi-Input Multi-Output (MIMO) spatial multiplexing across single-stream, dual-stream, and quad-stream configurations.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- Explain how spatial multiplexing sends independent data streams simultaneously over multiple antennas.
- Understand the HT MCS indexing scheme (MCS 0–7 for 1 spatial stream, MCS 8–15 for 2 streams, MCS 16–23 for 3 streams, and MCS 24–31 for 4 streams).
- Observe how scaling from 1x1 to 4x4 spatial streams linearly increases channel capacity and throughput.

## [agent] Scenario description

The topology uses [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned). UDP streams are sent from `server` to `host[0]`.

Three configurations are evaluated:

1. `SingleStreamMcs7`: 1x1 antenna configuration operating at HT MCS 7 (65.0 Mbps PHY rate).
2. `DualStreamMcs15`: 2x2 antenna configuration operating at HT MCS 15 (130.0 Mbps PHY rate).
3. `QuadStreamMcs31`: 4x4 antenna configuration operating at HT MCS 31 (260.0 Mbps PHY rate).

## [agent] Standards and INET model boundary

- **IEEE Std 802.11n-2009 / 802.11-2020 Clause 19.3.2**: Defines HT MCS parameters and spatial stream mapping rules.
- **INET Model Boundary**: Antenna array size (`numAntennas`) and spatial stream decoding are modeled in `Ieee80211Radio` and `Ieee80211ScalarRadioMedium`.
