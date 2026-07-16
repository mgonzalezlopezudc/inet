# IEEE 802.11ax (Wi-Fi 6) examples

These examples demonstrate High Efficiency (HE) PHY and MAC mechanisms. Each
directory is independently runnable; use this table to choose a scenario.

The examples are organized as controlled experiments: change one mechanism,
keep the topology and offered load fixed, and compare both protocol events and
end-to-end results. They model the behavior relevant to INET experiments; they
are not waveform implementations or interoperability certification tests.

| Area | Example | Main mechanisms |
|------|---------|-----------------|
| Downlink multi-user access | [dl_ofdma](dl_ofdma/walkthrough.md) | RU scheduling, SU baseline, wide channels, DL MU-MIMO, Multi-TID Block Ack |
| Uplink multi-user access | [ul_ofdma](ul_ofdma/walkthrough.md) | Scheduled OFDMA, UORA, UL MU-MIMO, fragmentation, NDP feedback, OM Control |
| Buffer reporting | [he_bsr](he_bsr/walkthrough.md) | Explicit, stale, and implicit Buffer Status Reports |
| Channel bandwidth | [he_channel_widths](he_channel_widths/walkthrough.md) | 20, 40, 80, and 160 MHz operation |
| Frequency-selective channels | [frequency_selective_channel](frequency_selective_channel/walkthrough.md) | Dimensional radio, per-RU isolation, TGax static/dynamic SISO Model B, opt-in RBIR, notch sweeps, and preamble puncturing |
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

For a fair comparison, use the same configuration run and seed, and inspect
more than packet counts. Depending on the feature, useful evidence includes RU
allocations, PPDU format, Trigger/Block Ack counts, selected MCS, latency,
fairness, radio awake time, and consumed energy. Numeric results printed in a
walkthrough describe the documented deterministic run; regenerate them after
changing the model, seed, traffic, or simulation duration.

The controlled short scenarios use a common timing convention: a low-rate
warm-up trigger from `0.2 s` to `0.25 s`, followed by normal traffic from
`0.3 s`. The analysis manifest records measurement windows from `0.3 s` unless
a scenario explicitly needs settling time or a different time scale.

## Reading the terminology

- **HE SU**, **HE MU**, **HE TB**, and **HE ER SU** name PPDU formats. OFDMA
  divides frequency into resource units (RUs); MU-MIMO separates users by
  spatial streams. A scenario may combine the two.
- Downlink MU scheduling is performed by the AP. Uplink HE TB transmission is
  initiated by an AP Trigger frame; scheduled RUs name a station, whereas
  UORA RUs permit contention.
- BSS color identifies the BSS of an HE PPDU. Spatial reuse is the separate
  decision to apply an OBSS/PD rule to an eligible inter-BSS PPDU.
- A configured bitrate is a model input, not by itself proof that a particular
  MCS, coding mode, or PPDU format was transmitted. Confirm the transmitter or
  HCF statistics/watches named by the walkthrough.

## Shared topology

Conventional single-BSS examples derive from
[`common/HeSingleBssNetwork.ned`](common/HeSingleBssNetwork.ned). The base keeps
the `server`, `ap`, `host[]`, `radioMedium`, and `configurator` paths stable, so
scenario INI files remain explicit. Examples with materially different
topology or management behavior retain dedicated networks.
