# Walkthrough: 802.11ac Extended A-MPDU Aggregation Limits

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `NOT RUN`
- PCAP: `NOT RUN`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough demonstrates IEEE 802.11ac Very High Throughput (VHT) extended A-MPDU maximum frame length scaling (up to 1,048,575 bytes), comparing standard 802.11n HT limits (65,535 bytes) against VHT limits under high offered load.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- Explain how 802.11ac extends the maximum A-MPDU length limit from $2^{16}-1$ (65,535 bytes) to $2^{20}-1$ (1,048,575 bytes).
- Understand why large A-MPDUs are required to maintain high MAC protocol efficiency in gigabit-per-second physical channels (80/160 MHz with 256-QAM).
- Measure MAC efficiency gains and channel contention overhead reductions.

## [agent] Scenario description

The topology uses [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned). High-frequency 1400-byte UDP packets are pushed from `server` to `host[0]`.

Two configurations are evaluated:

1. `StandardHtAmpduLimit`: A-MPDU size capped at 65,535 bytes (HT default limit).
2. `ExtendedVhtAmpduLimit`: A-MPDU size extended up to 1,048,575 bytes (VHT maximum limit).

## [agent] Standards and INET model boundary

- **IEEE Std 802.11ac-2013 / 802.11-2020 Clause 9.8 & 21.3.14**: Defines extended VHT A-MPDU frame limits and VHT EOF subframe delimiter structures.
- **INET Model Boundary**: A-MPDU size capping and subframe aggregation are managed in `inet::ieee80211::Ieee80211MpduAggregationPolicy`.
