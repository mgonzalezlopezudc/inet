# Walkthrough: 802.11n Guard Interval (SGI vs LGI)

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `NOT RUN`
- PCAP: `NOT RUN`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough demonstrates IEEE 802.11n Guard Interval configurations, comparing standard Long Guard Interval (800 ns) against Short Guard Interval (400 ns).

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- Explain how Guard Intervals prevent Inter-Symbol Interference (ISI) caused by multipath delay spread.
- Understand how reducing the GI from 800 ns to 400 ns decreases OFDM symbol duration from 4.0 µs to 3.6 µs (~11% throughput increase).
- Identify PHY transmission duration variations in packet traces.

## [agent] Scenario description

The topology uses [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned). UDP data is sent from `server` to `host[0]`.

Two configurations are evaluated:

1. `LongGI800ns`: Standard 800 ns Guard Interval (symbol time = 4.0 µs).
2. `ShortGI400ns`: Short 400 ns Guard Interval (symbol time = 3.6 µs).

## [agent] Standards and INET model boundary

- **IEEE Std 802.11n-2009 / 802.11-2020 Clause 19.3.2.4**: Specifies short and long GI timing rules.
- **INET Model Boundary**: PHY symbol durations are computed according to the configured radio `guardInterval` mode in `Ieee80211HtMode`.
