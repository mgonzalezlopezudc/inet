# Walkthrough: 802.11ac Wide Channels (80 MHz, 160 MHz & 80+80 MHz)

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260809T100318Z`
- PCAP: `NOT RUN`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough demonstrates IEEE 802.11ac Very High Throughput (VHT) wide channel bonding operations, comparing 20, 40, 80, 160, and 80+80 MHz subchannels in the 5 GHz band.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- Explain how 802.11ac introduces 80 MHz, contiguous 160 MHz, and non-contiguous 80+80 MHz channel bonding in 5 GHz.
- Observe subcarrier scaling (234 data subcarriers in 80 MHz vs 468 data subcarriers in 160 MHz).
- Compare peak PHY bitrates and packet transmission durations across 20, 40, 80, 160, and 80+80 MHz channel widths.

## [agent] Scenario description

The topology uses [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned). High-throughput UDP streams flow from `server` to `host[0]`.

Five configurations are evaluated:

1. `Vht20MHz`: 20 MHz 5 GHz baseline.
2. `Vht40MHz`: 40 MHz channel bonding.
3. `Vht80MHz`: 80 MHz wide channel bonding.
4. `Vht160MHz`: 160 MHz contiguous wide channel bonding.
5. `Vht80Plus80MHz`: 80+80 MHz non-contiguous wide channel bonding.

## [agent] Standards and INET model boundary

- **IEEE Std 802.11ac-2013 / 802.11-2020 Clause 21.3**: Specifies VHT 20, 40, 80, 160, and 80+80 MHz OFDM subcarrier layouts and channelization rules.
- **INET Model Boundary**: Channel width spectrum allocation is controlled via `bandName` parameter and handled in `Ieee80211VhtMode`.

## [agent] Scalar and vector analysis

<!-- BEGIN GENERATED: ieee80211-scalar-vector-wide_channels -->
### [script] Generated scalar/vector plot and table

![wide_channels scalar/vector analysis](results/20260809T100318Z/wide-channels-delivery-delay.png)

Figure provenance: [`results/20260809T100318Z/wide-channels-delivery-delay.png.json`](results/20260809T100318Z/wide-channels-delivery-delay.png.json). Run-level metric source: [`../../ieee80211ax/analysis/metrics.json`](../../ieee80211ax/analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)<br>vector / **.app[*] / endToEndDelay:vector
- Window / per-run aggregation / exclusions: [0.3, 0.5) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| VHT 20 MHz / goodput mbps | 63.6048 | 0.572119 |
| VHT 40 MHz / goodput mbps | 117.757 | 1.89878 |
| VHT 80 MHz / goodput mbps | 199.998 | 3.58099 |
| VHT 80+80 MHz / goodput mbps | 270.883 | 79.0624 |
| VHT 160 MHz / goodput mbps | 285.566 | 2.5908 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **INCONCLUSIVE** | Compare application delivered bytes across VHT 20, 40, 80, 160, and 80+80 MHz channel widths | No manifest acceptance threshold defined for wide channel comparison |
<!-- END GENERATED: ieee80211-scalar-vector-wide_channels -->
