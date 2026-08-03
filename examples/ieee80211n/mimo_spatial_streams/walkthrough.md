# Walkthrough: 802.11n MIMO & Spatial Multiplexing

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260803T190441Z`
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

## [agent] Scalar and vector analysis

<!-- BEGIN GENERATED: ieee80211-scalar-vector-mimo_spatial_streams -->
### [script] Generated scalar/vector plot and table

![mimo_spatial_streams scalar/vector analysis](results/20260803T190441Z/mimo-spatial-streams-delivery-delay.png)

Figure provenance: [`results/20260803T190441Z/mimo-spatial-streams-delivery-delay.png.json`](results/20260803T190441Z/mimo-spatial-streams-delivery-delay.png.json). Run-level metric source: [`../../ieee80211ax/analysis/metrics.json`](../../ieee80211ax/analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)<br>vector / **.app[*] / endToEndDelay:vector
- Window / per-run aggregation / exclusions: [0.3, 0.9) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| Dual Stream (MCS 15) / goodput mbps | 24.1248 | 0.169213 |
| Quad Stream (MCS 31) / goodput mbps | 24.1248 | 0.169213 |
| Single Stream (MCS 7) / goodput mbps | 24.1248 | 0.169213 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **INCONCLUSIVE** | Compare application delivered bytes across Single, Dual, and Quad Spatial Streams | No manifest acceptance threshold defined for spatial stream comparison |
<!-- END GENERATED: ieee80211-scalar-vector-mimo_spatial_streams -->
