# Walkthrough: 802.11n Rate Adaptation (HT Minstrel)

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `NOT RUN`
- PCAP: `NOT RUN`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough demonstrates dynamic High Throughput (HT) rate adaptation using the HT Minstrel algorithm (`Ieee80211HtMinstrel`) under node mobility and distance-based channel attenuation.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- Explain how the Minstrel HT algorithm maintains per-MCS transmission statistics (retry counts, throughput expectations, lookaround sampling).
- Observe dynamic fallback to lower MCS rates as a mobile station moves away from the Access Point and SNR drops.
- Compare fixed MCS transmission against adaptive rate control to prevent frame drop surges under varying channel conditions.

## [agent] Scenario description

The topology uses [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned). `host[0]` moves linearly away from `ap` at 15 m/s while receiving a UDP traffic flow from `server`.

Two configurations are evaluated:

1. `FixedMcs7`: Fixed MCS 7 rate without dynamic adaptation (experiences packet loss at longer distances).
2. `HtMinstrelAdaptation`: Dynamic `Ieee80211HtMinstrel` rate adaptation automatically stepping down MCS indices (from MCS 7 down to lower rates) as distance increases.

## [agent] Standards and INET model boundary

- **HT Minstrel Algorithm**: Implements multi-rate retry chain logic and empirical sampling for HT MCS sets.
- **INET Model Boundary**: Rate control logic is implemented in `inet::ieee80211::Ieee80211HtMinstrel`.
