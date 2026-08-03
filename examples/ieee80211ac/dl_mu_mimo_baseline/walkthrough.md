# Walkthrough: 802.11ac Downlink MU-MIMO Baseline

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `NOT RUN`
- PCAP: `NOT RUN`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough demonstrates IEEE 802.11ac Downlink Multi-User MIMO (DL MU-MIMO) beamforming, comparing single-user sequential transmission against simultaneous spatial stream transmissions to multiple client stations.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- Explain how 802.11ac Downlink MU-MIMO allows an Access Point equipped with multiple antennas to transmit independent spatial streams to distinct stations simultaneously.
- Observe spatial stream allocation in VHT PPDUs and Group ID management.
- Measure aggregate throughput gain and medium access latency reduction compared to single-user EDCA contention.

## [agent] Scenario description

The topology uses [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned) with `ap` equipped with 4 antennas, transmitting traffic to `host[0]` and `host[1]`.

Two configurations are evaluated:

1. `VhtSingleUserBaseline`: Sequential Single-User transmission (AP serves `host[0]` then `host[1]` sequentially).
2. `VhtDlMuMimoTwoUsers`: Downlink MU-MIMO simultaneous transmission (AP serves `host[0]` and `host[1]` concurrently on separate spatial streams).

## [agent] Standards and INET model boundary

- **IEEE Std 802.11ac-2013 / 802.11-2020 Clause 21.3.2 & 21.3.12**: Specifies VHT MU PPDU frame format, Group ID field, and beamforming matrix feedback.
- **INET Model Boundary**: DL MU-MIMO spatial beamforming is coordinated in `inet::ieee80211::VhtHcf`.
