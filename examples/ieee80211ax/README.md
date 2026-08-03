# IEEE 802.11ax (Wi-Fi 6) examples

These examples demonstrate High Efficiency (HE) PHY and MAC mechanisms. Each
directory is independently runnable; use this table to choose a scenario.

The examples are organized as controlled experiments: change one mechanism,
keep the topology and offered load fixed, and compare both protocol events and
end-to-end results. They model the behavior relevant to INET experiments; they
are not waveform implementations or interoperability certification tests.

The HE PHY boundary is standards-oriented but packet-level: represented
L-SIG/RL-SIG and HE-SIG fields, RU geometry, symbol counts, timing, packet
extension, recipient selection, and radiotap metadata are validated. Coding,
interleaving, modulation, pilots, samples, and RF synchronization remain
analytical rather than emitted bitstreams or waveforms. Runtime channels are
20/40/80/contiguous-160 MHz; 80+80 and Doppler/midamble timing are explicitly
rejected. See the
[supported-feature matrix](../../reports/80211ax-supported-feature-matrix.md)
for the exact contract and migration table.

| Area | Example | Main mechanisms |
|------|---------|-----------------|
| Downlink multi-user access | [dl_ofdma_sched](dl_ofdma_sched/walkthrough.md) | RU scheduling, SU baseline, wide channels, Multi-TID Block Ack |
| Downlink BAR acknowledgment | [dl_ofdma_bar](dl_ofdma_bar/walkthrough.md) | Sequential vs triggered (MU-BAR) Block Ack Request in DL OFDMA with DL SU baseline |
| Bidirectional multi-user access | [dl_ul_ofdma](dl_ul_ofdma/README.md) | Simultaneous DL and UL traffic, independent DL/UL OFDMA schedulers, matched SU baseline |
| Asymmetric downlink scheduling | [dl_ofdma_asym](dl_ofdma_asym/walkthrough.md) | Backlog-based and head-of-line minimum-delay scheduling under asymmetric load |
| Downlink MU-MIMO | [dl_mu_mimo](dl_mu_mimo/walkthrough.md) | Downlink MU-MIMO spatial multiplexing and sequential BAR acknowledgment |
| Uplink multi-user access | [ul_ofdma](ul_ofdma/walkthrough.md) | Scheduled UL OFDMA, equal RUs, and EDCA baseline |
| Uplink UORA | [ul_uora](ul_uora/walkthrough.md) | Uplink OFDMA Random Access (UORA) contention under light, heavy, and multi-RU load |
| Uplink MU-MIMO | [ul_mu_mimo](ul_mu_mimo/walkthrough.md) | Full-bandwidth HE UL MU-MIMO with spatial reuse |
| Buffer reporting | [bsr](bsr/walkthrough.md) | Explicit, stale, and implicit Buffer Status Reports |
| Channel bandwidth | [channel_widths](channel_widths/walkthrough.md) | 20, 40, 80, and 160 MHz operation |
| Frequency-selective channels | [frequency_selective_channel](frequency_selective_channel/walkthrough.md) | Dimensional radio, per-RU isolation, TGax static/dynamic SISO and static matrix channels, opt-in RBIR |
| Preamble puncturing | [preamble_puncturing](preamble_puncturing/walkthrough.md) | Subchannel puncturing, legacy interference avoidance, and dynamic puncturing |
| BCC & LDPC coding | [bcc_ldpc](bcc_ldpc/walkthrough.md) | BCC baseline vs HE LDPC timing and mixed peer LDPC capability negotiation |
| Rate selection | [rate_adaptation](rate_adaptation/walkthrough.md) | Fixed MCS and HE Minstrel, including mobility |
| Extended range | [er_su](er_su/walkthrough.md) | HE SU and HE ER SU with DCM |
| Feedback | [ndp_feedback](ndp_feedback/walkthrough.md) | NDP Feedback Report triggers and responses |
| Dynamic fragmentation | [dynamic_frag](dynamic_frag/walkthrough.md) | Negotiated HE dynamic fragmentation |
| Operating mode indication | [opmode_indication](opmode_indication/walkthrough.md) | Operating Mode Indication and OM Control |
| Multi-TID Block Ack | [ul_multitid](ul_multitid/walkthrough.md) | Downlink and uplink Multi-TID Block Ack scenarios |
| Spatial reuse | [bss_coloring](bss_coloring/walkthrough.md) | BSS coloring, OBSS/PD, and dual NAV |
| Power saving | [twt](twt/walkthrough.md) | Individual and broadcast Target Wake Time agreements |
| Dense IoT comparison | [dense_iot](dense_iot/README.md) | 128–512 STAs, UL/DL OFDMA with individual TWT versus 802.11ac |

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
[`../ieee80211/common/SingleBssNetwork.ned`](../ieee80211/common/SingleBssNetwork.ned). The base keeps
the `server`, `ap`, `host[]`, `radioMedium`, and `configurator` paths stable, so
scenario INI files remain explicit. Examples with materially different
topology or management behavior retain dedicated networks.

## 802.11 analysis

Use the generation-neutral interface one scenario at a time. For example:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py inspect ul_ofdma
python3 examples/ieee80211/analysis/wifi_analysis.py run ul_ofdma \
  --evidence both --runs 5
python3 examples/ieee80211/analysis/wifi_analysis.py report ul_ofdma \
  --session-id <YYYYMMDDTHHMMSSZ>
python3 examples/ieee80211/analysis/wifi_analysis.py publish ul_ofdma \
  --session-id <YYYYMMDDTHHMMSSZ> --update
```

`inspect` is read-only, `run` creates raw evidence, and `report` generates
scalar/vector and PCAP analyses without editing walkthroughs. Only the
explicit `publish --update` step replaces marker-bounded generated sections.
