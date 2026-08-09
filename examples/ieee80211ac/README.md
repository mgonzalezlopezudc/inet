# IEEE 802.11ac (Very High Throughput / VHT) Examples

These examples demonstrate Very High Throughput (VHT) PHY and MAC mechanisms introduced in the IEEE 802.11ac standard. Each directory contains an independently runnable controlled simulation scenario built upon the unified base topology [`examples/ieee80211/common/SingleBssNetwork.ned`](../ieee80211/common/SingleBssNetwork.ned).

## Scenario Overview

| Area | Feature Directory | Main Mechanisms & Configurations |
| :--- | :--- | :--- |
| **Wide Channels** | [`wide_channels`](wide_channels/walkthrough.md) | Contiguous channel bonding across 20, 40, 80, and 160 MHz operating channels in the 5 GHz band (`Vht20MHz`, `Vht40MHz`, `Vht80MHz`, `Vht160MHz`). |
| **VHT MCS & 256-QAM** | [`vht_mcs_256qam`](vht_mcs_256qam/walkthrough.md) | HT baseline versus supported fixed VHT MCS 8 256-QAM at 80 MHz, plus a 40 MHz width control (`BaselineHtMcs7`, `VhtMcs8_256QAM`, `VhtMcs8_256QAM_40Mhz`). |
| **A-MPDU policy limits** | [`extended_ampdu`](extended_ampdu/walkthrough.md) | VHT A-MPDU policy-limit configuration with assembled HCF aggregate-length and MPDU-count telemetry (`StandardVhtAmpduLimit`, `ExtendedVhtAmpduLimit`). |
| **Configured NSS scaling** | [`spatial_streams_8x8`](spatial_streams_8x8/walkthrough.md) | Packet-level VHT scaling across 1, 4, and 8 configured spatial streams (`SingleStreamVht`, `FourStreamVht`, `EightStreamVht`). |
| **VHT Rate Adaptation** | [`vht_rate_adaptation`](vht_rate_adaptation/walkthrough.md) | Fixed and peer-aware Minstrel VHT rate control under identical 5 GHz/80 MHz mobility conditions (`FixedVhtMcs`, `VhtMinstrelAdaptation`); AARF remains an optional probe. |
| **VHT Short GI** | [`short_gi_vht`](short_gi_vht/walkthrough.md) | Short Guard Interval (400 ns) timing on 80 MHz and 160 MHz subchannels (`VhtLongGI`, `VhtShortGI`). |
| **Downlink MU-MIMO** | [`dl_mu_mimo_baseline`](dl_mu_mimo_baseline/walkthrough.md) | VHT Downlink Multi-User MIMO (DL MU-MIMO) beamforming and three-station simultaneous transmissions (`VhtSingleUserBaseline`, `VhtDlMuMimoThreeUsers`). |

## Execution Instructions

From the INET project root directory, run any configuration in Cmdenv mode using:

```sh
bin/inet -u Cmdenv -c <ConfigName> examples/ieee80211ac/<example>/omnetpp.ini
```

Use `-u Qtenv` for interactive GUI visualization.

## Analysis Framework Integration

Run the generation-neutral analysis toolchain for the `ac` suite:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py inspect wide_channels --suite ac
python3 examples/ieee80211/analysis/wifi_analysis.py run wide_channels --suite ac --runs 5
```
