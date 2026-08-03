# Walkthrough: 802.11n Rate Adaptation (HT Minstrel)

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260803T190458Z`
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

## [agent] Scalar and vector analysis

<!-- BEGIN GENERATED: ieee80211-scalar-vector-rate_adaptation -->
### [script] Generated scalar/vector plot and table

![rate_adaptation scalar/vector analysis](results/20260803T190458Z/rate-adaptation-delivery-delay.png)

Figure provenance: [`results/20260803T190458Z/rate-adaptation-delivery-delay.png.json`](results/20260803T190458Z/rate-adaptation-delivery-delay.png.json). Run-level metric source: [`../../ieee80211ax/analysis/metrics.json`](../../ieee80211ax/analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)<br>vector / **.app[*] / endToEndDelay:vector
- Window / per-run aggregation / exclusions: [0.3, 1.9) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| Fixed MCS 7 / goodput mbps | 8 | 0 |
| HT Minstrel Adaptation / goodput mbps | 8 | 0 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **INCONCLUSIVE** | Compare application delivered bytes between Fixed MCS 7 and HT Minstrel dynamic adaptation | No manifest acceptance threshold defined for rate adaptation comparison |
<!-- END GENERATED: ieee80211-scalar-vector-rate_adaptation -->
