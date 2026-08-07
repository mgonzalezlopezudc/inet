# IEEE 802.11n (High Throughput / HT) Examples

These examples demonstrate High Throughput (HT) PHY and MAC mechanisms introduced in the IEEE 802.11n standard. Each directory contains an independently runnable controlled simulation scenario built upon the unified base topology [`examples/ieee80211/common/SingleBssNetwork.ned`](../ieee80211/common/SingleBssNetwork.ned).

## Scenario Overview

| Area | Feature Directory | Main Mechanisms & Configurations |
| :--- | :--- | :--- |
| **Frame Aggregation** | [`frame_aggregation`](frame_aggregation/walkthrough.md) | A-MPDU and A-MSDU subframe aggregation vs non-aggregated MAC data frames under heavy load (`NoAggregation`, `AMsduOnly`, `AMpduOnly`, `TwoLevelAggregation`). |
| **Block Acknowledgement** | [`block_ack`](block_ack/walkthrough.md) | ADDBA/DELBA management dialogs, Compressed Block ACK bitmaps, and SIFS HT implicit Block ACK timing (`StandardAck`, `FragBasicBlockAck`, `CompressedBlockAck`, `ImplicitBlockAck`). |
| **Channel Bonding** | [`channel_widths`](channel_widths/walkthrough.md) | 20 MHz vs 40 MHz subchannel operation, primary/secondary 20 MHz subchannel selection in 2.4 GHz and 5 GHz (`Ht20MHz`, `Ht40MHz`, `Ht40MHzSecondaryAboveWithInterferer`, `Ht40MHzSecondaryBelowWithInterferer`). |
| **Guard Interval** | [`guard_interval`](guard_interval/walkthrough.md) | Short Guard Interval (400 ns) vs Long Guard Interval (800 ns), including 40 MHz HT rates of 120 and 108 Mbps (`LongGI800ns`, `ShortGI400ns`, `LongGI40MHz108Mbps`, `ShortGI40MHz120Mbps`). |
| **MIMO & Spatial Streams** | [`mimo_spatial_streams`](mimo_spatial_streams/walkthrough.md) | Multi-antenna spatial multiplexing (1x1, 2x2, and 4x4 spatial streams) at HT MCS 1, 9, and 25 (`SingleStreamMcs1`, `DualStreamMcs9`, `QuadStreamMcs25`). |
| **Preamble Modes** | [`preamble_modes`](preamble_modes/walkthrough.md) | HT Mixed-Mode preamble (legacy L-SIG protection) versus HT Greenfield preamble PHY headers (`HtMixedMode`, `HtGreenfield`). |
| **Rate Adaptation** | [`rate_adaptation`](rate_adaptation/walkthrough.md) | Dynamic HT rate adaptation using `AarfRateControl` under node mobility and distance path loss (`FixedConfig3`, `HtAarfAdaptation`). |

## Execution Instructions

From the INET project root directory, run any configuration in Cmdenv mode using:

```sh
bin/inet -u Cmdenv -c <ConfigName> examples/ieee80211n/<example>/omnetpp.ini
```

Use `-u Qtenv` for interactive GUI visualization.

## Analysis Framework Integration

Run the generation-neutral analysis toolchain for the `n` suite:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py inspect frame_aggregation --suite n
python3 examples/ieee80211/analysis/wifi_analysis.py run frame_aggregation --suite n --runs 5
```
