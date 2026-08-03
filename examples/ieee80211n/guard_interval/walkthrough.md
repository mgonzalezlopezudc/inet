# Walkthrough: 802.11n Guard Interval (SGI vs LGI)

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260803T185939Z`
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

## [agent] Scalar and vector analysis

<!-- BEGIN GENERATED: ieee80211-scalar-vector-guard_interval -->
### [script] Generated scalar/vector plot and table

![guard_interval scalar/vector analysis](results/20260803T185939Z/guard-interval-delivery-delay.png)

Figure provenance: [`results/20260803T185939Z/guard-interval-delivery-delay.png.json`](results/20260803T185939Z/guard-interval-delivery-delay.png.json). Run-level metric source: [`../../ieee80211ax/analysis/metrics.json`](../../ieee80211ax/analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)<br>vector / **.app[*] / endToEndDelay:vector
- Window / per-run aggregation / exclusions: [0.3, 0.9) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| Long GI (800 ns) / goodput mbps | 19.4533 | 0.157059 |
| Short GI (400 ns) / goodput mbps | 19.4533 | 0.157059 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **INCONCLUSIVE** | Compare application delivered bytes across Long (800 ns) and Short (400 ns) Guard Intervals | No manifest acceptance threshold defined for Guard Interval comparison |
<!-- END GENERATED: ieee80211-scalar-vector-guard_interval -->
