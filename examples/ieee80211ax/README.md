# IEEE 802.11ax (Wi-Fi 6) examples

These examples demonstrate High Efficiency (HE) PHY and MAC mechanisms. Each
directory is independently runnable; use this table to choose a scenario.

| Area | Example | Main mechanisms |
|------|---------|-----------------|
| Downlink multi-user access | [dl_ofdma](dl_ofdma/walkthrough.md) | RU scheduling, SU baseline, wide channels, DL MU-MIMO, Multi-TID Block Ack |
| Uplink multi-user access | [ul_ofdma](ul_ofdma/walkthrough.md) | Scheduled OFDMA, UORA, UL MU-MIMO, fragmentation, NDP feedback, OM Control |
| Buffer reporting | [he_bsr](he_bsr/walkthrough.md) | Explicit, stale, and implicit Buffer Status Reports |
| Channel bandwidth | [he_channel_widths](he_channel_widths/walkthrough.md) | 20, 40, 80, and 160 MHz operation |
| PHY and MAC feature combinations | [he_features](he_features/walkthrough.md) | LDPC, packet extension, puncturing, mixed capabilities, interference |
| Rate selection | [he_rate_adaptation](he_rate_adaptation/walkthrough.md) | Fixed MCS and HE Minstrel, including mobility |
| Extended range | [he_er_su](he_er_su/walkthrough.md) | HE SU and HE ER SU with DCM |
| Spatial reuse | [bss_coloring](bss_coloring/walkthrough.md) | BSS coloring, OBSS/PD, and dual NAV |
| Power saving | [twt](twt/walkthrough.md) | Individual and broadcast Target Wake Time agreements |
| Spatial multiplexing | [multi_user/mu_mimo](multi_user/mu_mimo/README.md) | Downlink and uplink MU-MIMO |
| Feedback | [multi_user/ndp_feedback](multi_user/ndp_feedback/README.md) | NDP Feedback Report triggers and responses |
| MAC feature | [mac_features/dynamic_fragmentation](mac_features/dynamic_fragmentation/README.md) | Negotiated HE dynamic fragmentation |
| MAC feature | [mac_features/operating_mode_indication](mac_features/operating_mode_indication/README.md) | Operating Mode Indication and OM Control |
| MAC feature | [mac_features/multi_tid_block_ack](mac_features/multi_tid_block_ack/README.md) | Downlink and uplink Multi-TID Block Ack scenarios |

## Running an example

From the INET project root, select a named configuration from the example's
`omnetpp.ini`:

```sh
bin/inet -u Cmdenv -c <ConfigName> examples/ieee80211ax/<example>/omnetpp.ini
```

Use `-u Qtenv` for interactive animation or module inspection.

## Shared topology

Conventional single-BSS examples derive from
[`common/HeSingleBssNetwork.ned`](common/HeSingleBssNetwork.ned). The base keeps
the `server`, `ap`, `host[]`, `radioMedium`, and `configurator` paths stable, so
scenario INI files remain explicit. Examples with materially different
topology or management behavior retain dedicated networks.
