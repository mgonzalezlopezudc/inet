# Walkthrough: 802.11ac VHT Rate Adaptation (VHT AARF)

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `NOT RUN`
- PCAP: `NOT RUN`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough demonstrates dynamic Very High Throughput (VHT) rate adaptation using the AARF rate adaptation algorithm (`AarfRateControl`) under node mobility and distance-based path loss.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- Explain how AARF rate control adapts MCS rates (VHT MCS 0–9) across varying SNR conditions in 5 GHz channels.
- Observe dynamic rate fallback from 256-QAM (MCS 9/8) down through 64-QAM, 16-QAM, and QPSK as distance increases.
- Compare fixed rate transmission against dynamic rate adaptation.

## [agent] Scenario description

The topology uses [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned). `host[0]` moves away from `ap` at 15 m/s while receiving UDP traffic.

Two configurations are evaluated:

1. `FixedVhtMcs`: Fixed VHT MCS 9 (suffers frame loss as SNR degrades).
2. `VhtAarfAdaptation`: Dynamic `AarfRateControl` rate adaptation automatically adjusting MCS index to preserve packet delivery.

## [agent] Standards and INET model boundary

- **VHT AARF Algorithm**: Implements Adaptive Auto Rate Fallback logic for VHT MCS rates (0–9) and 80/160 MHz channel profiles.
- **INET Model Boundary**: Rate control logic is implemented in `inet::ieee80211::AarfRateControl`.
