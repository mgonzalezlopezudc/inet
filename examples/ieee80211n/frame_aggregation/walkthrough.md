# Walkthrough: 802.11n Frame Aggregation

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260803T185849Z`
- PCAP: `NOT RUN`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough demonstrates IEEE 802.11n High Throughput (HT) frame aggregation mechanisms, comparing un-aggregated transmission against MSDU aggregation (A-MSDU), MPDU aggregation (A-MPDU), and two-level combined aggregation.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- Explain how IEEE 802.11n frame aggregation reduces PHY overhead (preambles, headers, DIFS, and BACK SIFS intervals).
- Differentiate A-MSDU (packing multiple network layer SDUs into one MAC SDU) from A-MPDU (packing multiple MAC SDUs into one physical frame with subframe delimiters).
- Understand how two-level aggregation achieves maximum channel utilization under high offered load.
- Observe MAC queueing, subframe encapsulation, and throughput gains in simulation captures.

## [agent] Scenario description

The topology uses the common single-BSS network [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned) with an Access Point (`ap`), two client stations (`host[0]`, `host[1]`), and a wired server (`server`).

```text
server === wired link === AP -- 802.11n HT wireless --> host[0]
                                                     `--> host[1]
```

High-rate UDP traffic (1000-byte packets every 0.2 ms) is transmitted from `server` to `host[0]` over a 2.4 GHz 802.11n channel (`opMode = "n(mixed-2.4Ghz)"`). Four configurations are evaluated:

1. `NoAggregation`: Standard un-aggregated 802.11 transmission.
2. `AMsduOnly`: MSDU aggregation up to 3839 bytes.
3. `AMpduOnly`: MPDU aggregation up to 65535 bytes with Block ACK.
4. `TwoLevelAggregation`: Combined A-MSDU inside A-MPDU aggregation.

## [agent] Standards and INET model boundary

- **IEEE Std 802.11n-2009 / 802.11-2020 Clause 19 & 9.8**: Defines A-MSDU and A-MPDU frame structures, delimiter checks, and Block Ack requirements.
- **INET Model Boundary**: A-MSDU subframes and A-MPDU subframe delimiters are encapsulated as chunk fields inside `Packet` objects (`Ieee80211MsduAggregationPolicy` and `Ieee80211MpduAggregationPolicy`). Bit-level channel corruption is evaluated analytically via NIST error models.

## [agent] Scalar and vector analysis

<!-- BEGIN GENERATED: ieee80211-scalar-vector-frame_aggregation -->
### [script] Generated scalar/vector plot and table

![frame_aggregation scalar/vector analysis](results/20260803T185849Z/frame-aggregation-delivery-delay.png)

Figure provenance: [`results/20260803T185849Z/frame-aggregation-delivery-delay.png.json`](results/20260803T185849Z/frame-aggregation-delivery-delay.png.json). Run-level metric source: [`../../ieee80211ax/analysis/metrics.json`](../../ieee80211ax/analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)<br>vector / **.app[*] / endToEndDelay:vector
- Window / per-run aggregation / exclusions: [0.3, 0.9) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| A-MPDU Only / goodput mbps | 19.4533 | 0.157059 |
| A-MSDU Only / goodput mbps | 19.4533 | 0.157059 |
| No Aggregation / goodput mbps | 19.4533 | 0.157059 |
| Two-Level Aggregation / goodput mbps | 19.4533 | 0.157059 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **INCONCLUSIVE** | Compare application delivered bytes across frame aggregation policies | No manifest acceptance threshold defined for frame aggregation delivery comparison |
<!-- END GENERATED: ieee80211-scalar-vector-frame_aggregation -->
