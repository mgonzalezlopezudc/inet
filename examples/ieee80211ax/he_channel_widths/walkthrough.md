# Walkthrough: HE channel widths

This example compares four contiguous High Efficiency (HE) channel-width
configurations: 20, 40, 80, and 160 MHz. A fresh, co-recorded session combines
five application-result runs per width with run-0 PCAPng evidence. It
demonstrates that the configured width bundle reaches the transmitted
TXVECTOR and that this saturated four-station workload is served faster as the
bundle widens. It does not isolate width from every other PHY parameter.

## Learning objectives and feature primer

After completing this walkthrough, the reader can:

- distinguish operating channel width from application bitrate and resource
  unit (RU) size;
- find the width-setting chain in the INI file and the decoded width in HE-SU
  radiotap metadata;
- recognize the setup HE-SU/Data-Ack exchange and the measured
  HE-MU Data/MU-BAR Trigger/HE-TB Block Ack cycle; and
- reproduce the representative run, five-seed campaign, and first diagnostics.

Channel width is the occupied radio-frequency bandwidth. Wider channels expose
more subcarriers, which can support larger RUs or more simultaneous RUs, but
they do not promise a particular throughput ratio: preambles, acknowledgments,
scheduling, aggregation, modulation and coding scheme (MCS), contention, and
offered load all matter.

The run has two useful exchanges. At `0.2 s`, one setup packet per station
creates an HE single-user (HE-SU) Data/Ack exchange intended to establish
Block Ack state. Starting at `0.3 s`, four saturated downlink flows exercise
the HE hybrid coordination function (`HeHcf`) and equal-sized-RU scheduler.
The dominant measured cycle is AP HE multi-user (HE-MU) QoS Data, an AP
multi-user Block Ack Request (MU-BAR) Trigger, and simultaneous HE
trigger-based (HE-TB) Block Ack responses.

## Scenario description

[HeChannelWidthsNetwork.ned](HeChannelWidthsNetwork.ned) extends the shared
single-BSS network with four wireless hosts. The topology contains one wired
UDP server, an access point (AP), and four stationary stations. Each station
is about 94 m from the AP; there is no mobility or configured external
interferer.

[omnetpp.ini](omnetpp.ini) sends one 1,000-byte setup datagram to each host at
`0.2 s`, then starts four downlink UDP flows at `0.3 s`, each offering a
1,000-byte datagram every `0.25 ms`. The analysis window is `[0.3, 0.43) s`;
the simulation ends at `0.45 s`.

```text
server -- AP ~~ {host[0], host[1], host[2], host[3]}
```

## Standards and INET model boundary

IEEE Std 802.11-2024 Clause 27.2.5 defines the HE
`CHANNEL_WIDTH` PHY configuration parameter, including 20, 40, 80, and
160 MHz (`80211ax-2024:chunk:10020`). Table 27-1 defines `CH_BANDWIDTH` as a
TXVECTOR and RXVECTOR parameter for HE-SU, with `CBW20`, `CBW40`, `CBW80`,
and `CBW160` values (`80211ax-2024:chunk:10000`). Clause 26.17.1 also
constrains a transmission not to exceed peer capability or BSS width
(`80211ax-2024:chunk:09956`). These are normative width semantics; the
standard does not require goodput to scale linearly with width.

INET models the exchange at packet level. The INI requests AX operation,
fixed topology and traffic, `HeHcf`, an equal-sized-RU downlink scheduler, and
a width-specific PHY bundle. PCAP radiotap fields are direct observation only
when the recorder marks the corresponding HE field as present and known.
Application vectors are model outcomes. The explanation connecting them is an
inference unless the artifacts expose the same decision directly.

## Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| Each primary configuration transmits HE-SU data at its named width | `PASS` | AP run-0 PCAPng; radiotap HE bandwidth with known bits | run/seed `0` per width | four setup frames plus the first measured-flow frame per width |
| The saturated phase exercises HE multi-user scheduling | `PASS` | AP run-0 PCAPng; Trigger, HE-MU QoS Data, and HE-TB Block Ack rows | run/seed `0` per width | protocol-visible exchange |
| Aggregate sink goodput rises and per-run p95 delay falls across the four bundles | `PASS` | `packetReceived:vector(packetBytes)` and `endToEndDelay:vector` | runs/seeds `0–4` | `[0.3, 0.43) s`; five independent runs |
| Machine-readable TXVECTOR and capacity acceptance contracts | `INCONCLUSIVE` | session evidence ledger | runs/seeds `0–4` | no scalar per-PPDU width and no encoded numerical acceptance envelope |
| Width alone caused the outcome ordering | `INCONCLUSIVE` | configuration changes several PHY inputs together | same runs | no single-parameter counterfactual |
| A wider channel improves coverage | `NOT RUN` | no matched distance/control pair in the analyzed suite | none | `WidthCellEdge` is 160 MHz only |

## Configuration matrix

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| `Width20MHz` | Control | 20 MHz; channel 2; 14.625 Mbps; −85 dBm sensitivity | four saturated downlink flows | `0–4` | decoded 20 MHz HE-SU; baseline outcome |
| `Width40MHz` | Treatment | 40 MHz; channel 2; 29.25 Mbps; −82 dBm | matched topology/traffic | `0–4` | decoded 40 MHz; higher goodput/lower delay |
| `Width80MHz` | Treatment | 80 MHz; channel 2; 61.25 Mbps; −79 dBm | matched topology/traffic | `0–4` | decoded 80 MHz; higher goodput/lower delay |
| `Width160MHz` | Treatment | 160 MHz; channel 1; 122.5 Mbps; −76 dBm | matched topology/traffic | `0–4` | decoded 160 MHz; highest goodput/lowest delay |

`Width40MHz`, `Width80MHz`, and `Width160MHz` extend `Width20MHz`, so their
more specific assignments win for `bandName`, `channelNumber`, receiver
bandwidth, rate-selection bandwidth, interface bitrate, and receiver
sensitivity. Topology, traffic, scheduler, power, run length, and the other
common inputs remain inherited. This is a coordinated-width comparison, not a
single raw-parameter toggle. The extra `WidthCellEdge` configuration is
exploratory and is not part of the shared four-row analysis suite.

## Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| HE-SU setup width equals the configuration | AP PCAP radiotap HE bandwidth | missing/unknown/wrong width | rate selection, radio, or capture encoding | inspect effective `dataFrameBandwidth`, radiotap presence/known bits, and the typed HE decoder |
| Measured phase contains HE-MU Data → MU-BAR Trigger → HE-TB Block Ack | AP PCAP frame/PHY rows | one exchange class absent or wrong order | HCF scheduler, acknowledgment policy, or capture point | filter QoS Data, Trigger, and Block Ack; then enable focused HCF logs |
| Goodput rises and p95 delay falls across bundles | sink application vectors | empty match or reversed ordering | workload, queueing, MAC, or result filter | verify module/result/unit, `[0.3, 0.43)` window, and each run separately |
| Results and PCAP share run-0 provenance | session manifest and capture manifest | different sessions or seeds | campaign plumbing | rerun `wifi_analysis.py run ... --evidence both` |

## Reproduction

Run from the repository root. This minimal release-mode Cmdenv command was
executed during this rewrite and exited with status `0`:

```sh
bin/inet -u Cmdenv -f examples/ieee80211ax/he_channel_widths/omnetpp.ini \
  -c Width20MHz -r 0 --seed-set=0 \
  --output-scalar-file=examples/ieee80211ax/he_channel_widths/results/20260726T160000Z-minimal/Width20MHz/Width20MHz-#0.sca \
  '--**.scalar-recording=true'
```

The publication session was then generated with this exact command; all 20
underlying runs completed successfully:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py run he_channel_widths \
  --suite ax --evidence both --runs 5 --session-id 20260726T160000Z
```

It records selected application/model vectors for runs 0–4 and enables PCAPng
only on run 0, so the representative capture and run-0 vectors come from the
same trajectory. Report generation and marker-bounded publication were:

```sh
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211/analysis/wifi_analysis.py report \
  he_channel_widths --suite ax --session-id 20260726T160000Z
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211/analysis/wifi_analysis.py publish \
  he_channel_widths --suite ax --session-id 20260726T160000Z --update
```

## Scalar and vector analysis

Inputs are the five `.sca` and `.vec` pairs in each configuration directory
under `results/20260726T160000Z/`. The sidecar
[channel-width-dashboard.png.json](results/20260726T160000Z/channel-width-dashboard.png.json)
binds the comparison, configuration metadata, query fields, aggregation, and
`[0.3, 0.43) s` measurement window.

This discovery query verifies the result names, sink modules, and units; it
does not compute the estimates:

```sh
opp_scavetool query -l \
  -f 'type =~ vector AND module =~ "**.host[*].app[0]" AND (name =~ "packetReceived:vector(packetBytes)" OR name =~ "endToEndDelay:vector")' \
  examples/ieee80211ax/he_channel_widths/results/20260726T160000Z/*/*.vec
```

The shared native result-analysis pipeline applies the `[0.3, 0.43)` window,
aggregates within each run, computes the five-run summaries, and regenerates
the table, dashboard, and provenance sidecar:

```sh
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211/analysis/wifi_analysis.py report \
  he_channel_widths --suite ax --session-id 20260726T160000Z
```

| Configuration | Aggregate goodput | p95 end-to-end delay |
|---|---:|---:|
| `Width20MHz` | 26.892 ± 0.000 Mbps | 97.253 ± 0.057 ms |
| `Width40MHz` | 50.708 ± 0.000 Mbps | 63.809 ± 0.077 ms |
| `Width80MHz` | 81.428 ± 0.335 Mbps | 40.060 ± 0.075 ms |
| `Width160MHz` | 118.646 ± 0.000 Mbps | 9.406 ± 0.075 ms |

These values are **derived measurements** from sink application vectors.
Goodput is the sum of received packet bytes in the window multiplied by eight
and divided by `0.13 s`; delay p95 is computed within each run. Each run is
aggregated before the mean and two-sided 95% Student-t confidence-interval
half-width are computed over five independent seeds. Vector samples are not
treated as repetitions, and no pre-`0.3 s` setup sample enters the window.
Zero-width goodput intervals mean the five run-level totals were identical;
they do not imply population certainty.
Seed indices 0–4 are matched across widths, so the comparison has a paired
design. The displayed intervals are marginal per-configuration intervals; no
paired-difference interval or hypothesis test is reported.
The strictly monotonic ordering is a scoped descriptive invariant selected for
this walkthrough. The session evidence ledger still reports its generic
capacity contract as `INCONCLUSIVE` because the experiment manifest does not
encode a numerical acceptance envelope.

<!-- BEGIN GENERATED: ieee80211-scalar-vector-width -->
### Generated scalar/vector plot and table

![width scalar/vector analysis](results/20260726T160000Z/channel-width-dashboard.png)

Figure provenance: [`results/20260726T160000Z/channel-width-dashboard.png.json`](results/20260726T160000Z/channel-width-dashboard.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

| Configuration or comparison | Metric | Source result filters / modules / units | Window / per-run aggregation / exclusions | Independent runs (n) | Mean or direct value | 95% CI half-width |
|---|---|---|---|---:|---:|---:|
| 160 MHz | delay p95 ms | vector / **.app[*] / packetReceived:vector(packetBytes) / unit=B<br>vector / **.app[*] / endToEndDelay:vector / unit=s | [0.3, 0.43) s; ECDF=representative run 0; bars=per-run values with 95% Student-t CI | 5 | 9.40625 | 0.0750894 |
| 160 MHz | goodput mbps | vector / **.app[*] / packetReceived:vector(packetBytes) / unit=B<br>vector / **.app[*] / endToEndDelay:vector / unit=s | [0.3, 0.43) s; ECDF=representative run 0; bars=per-run values with 95% Student-t CI | 5 | 118.646 | 0 |
| 20 MHz | delay p95 ms | vector / **.app[*] / packetReceived:vector(packetBytes) / unit=B<br>vector / **.app[*] / endToEndDelay:vector / unit=s | [0.3, 0.43) s; ECDF=representative run 0; bars=per-run values with 95% Student-t CI | 5 | 97.253 | 0.0573092 |
| 20 MHz | goodput mbps | vector / **.app[*] / packetReceived:vector(packetBytes) / unit=B<br>vector / **.app[*] / endToEndDelay:vector / unit=s | [0.3, 0.43) s; ECDF=representative run 0; bars=per-run values with 95% Student-t CI | 5 | 26.8923 | 0 |
| 40 MHz | delay p95 ms | vector / **.app[*] / packetReceived:vector(packetBytes) / unit=B<br>vector / **.app[*] / endToEndDelay:vector / unit=s | [0.3, 0.43) s; ECDF=representative run 0; bars=per-run values with 95% Student-t CI | 5 | 63.809 | 0.0766852 |
| 40 MHz | goodput mbps | vector / **.app[*] / packetReceived:vector(packetBytes) / unit=B<br>vector / **.app[*] / endToEndDelay:vector / unit=s | [0.3, 0.43) s; ECDF=representative run 0; bars=per-run values with 95% Student-t CI | 5 | 50.7077 | 0 |
| 80 MHz | delay p95 ms | vector / **.app[*] / packetReceived:vector(packetBytes) / unit=B<br>vector / **.app[*] / endToEndDelay:vector / unit=s | [0.3, 0.43) s; ECDF=representative run 0; bars=per-run values with 95% Student-t CI | 5 | 40.0602 | 0.075074 |
| 80 MHz | goodput mbps | vector / **.app[*] / packetReceived:vector(packetBytes) / unit=B<br>vector / **.app[*] / endToEndDelay:vector / unit=s | [0.3, 0.43) s; ECDF=representative run 0; bars=per-run values with 95% Student-t CI | 5 | 81.4277 | 0.334812 |

The table is a presentation view of the session-bound run-level summary. The source and aggregation columns reproduce the bundle-level figure provenance; the authored analysis identifies which source supports each metric and supplies the interpretation.
<!-- END GENERATED: ieee80211-scalar-vector-width -->

## PCAP statistics

Run 0 of session `20260726T160000Z` records MAC observations at every
`wlan[0]`; the compact comparison below uses the AP observation point. The
files have a `.pcap` suffix but are PCAPng containing IEEE 802.11 plus
radiotap. `recordPcap=true`, `moduleNamePatterns="mac"`,
`fileFormat="pcapng"`, computed checksums, and computed FCS were supplied on
the campaign command line. TShark and Capinfos 4.6.4 verified that all four AP
captures are nonempty, decode, and have strictly ordered timestamps.

```sh
tshark -n -r 'examples/ieee80211ax/he_channel_widths/results/20260726T160000Z/Width20MHz/Width20MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap' \
  -q -z io,stat,0,'wlan.fc.type_subtype==0x28'
```

| Configuration | AP observations | HE-SU QoS Data | Saturated-phase signature |
|---|---:|---|---|
| `Width20MHz` | 1,160 | 5 frames; 20 MHz; 619.1 µs mean | 504 HE-MU / 504 HE-TB |
| `Width40MHz` | 1,559 | 5 frames; 40 MHz; 327.6 µs mean | 943 HE-MU / 472 HE-TB |
| `Width80MHz` | 2,187 | 5 frames; 80 MHz; 175.2 µs mean | 1,523 HE-MU / 510 HE-TB |
| `Width160MHz` | 2,939 | 5 frames; 160 MHz; 105.6 µs mean | 2,215 HE-MU / 558 HE-TB |

The five HE-SU frames in each row comprise four setup frames near `0.2 s` and
the first measured-flow frame just after `0.3 s`. They are 1,066-byte capture
observations decoded as HE-SU, MCS 1, one spatial stream, 3.2 µs guard
interval, and low-density parity-check (LDPC) coding. The HE-MU and HE-TB
counts describe decoded PHY formats, not matched request/response pairs. All
totals are observations at one capture point, not de-duplicated application
deliveries. Estimated HE MU/TB airtime remains approximate because radiotap
does not expose every user-dependent signaling term.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260726T160000Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260726T160000Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260726T160000Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260726T160000Z.json` (SHA-256 `dfc51aa88408fccf72834eaefdc39fc20db21560f2907c4874f2e70d501c9e35`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %**: The sum of this frame type's estimated airtimes divided by the simulation time limit. Concurrent transmissions from multiple capture points are counted separately, so this value can exceed 100%; it is not the union of busy channel time.

#### Compact cross-configuration summary

| Configuration | Observation point / counting unit | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---|---:|---|---:|---|
| `Width160MHz` | AP interface(s); capture observations<br>`examples/ieee80211ax/he_channel_widths/results/20260726T160000Z/Width160MHz/Width160MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap` | `none (all decoded frames)` | 2939 | Data: QoS Data [HE-MU, HE-MCS 2, 484-tone RU, GI 3.2 us, LDPC, A-MPDU] (2213), Control: Block Ack (BA) [HE-TB, HE-MCS 0, 484-tone RU, GI 1.6 us, LDPC] (556), Control: Trigger (140) | 108.11% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `Width20MHz` | AP interface(s); capture observations<br>`examples/ieee80211ax/he_channel_widths/results/20260726T160000Z/Width20MHz/Width20MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap` | `none (all decoded frames)` | 1160 | Data: QoS Data [HE-MU, HE-MCS 5, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] (504), Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] (504), Control: Trigger (126) | 106.91% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `Width40MHz` | AP interface(s); capture observations<br>`examples/ieee80211ax/he_channel_widths/results/20260726T160000Z/Width40MHz/Width40MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap` | `none (all decoded frames)` | 1559 | Data: QoS Data [HE-MU, HE-MCS 4, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (943), Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] (472), Control: Trigger (118) | 110.23% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `Width80MHz` | AP interface(s); capture observations<br>`examples/ieee80211ax/he_channel_widths/results/20260726T160000Z/Width80MHz/Width80MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap` | `none (all decoded frames)` | 2187 | Data: QoS Data [HE-MU, HE-MCS 3, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] (1521), Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] (508), Control: Trigger (128) | 111.91% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | Width160MHz produced protocol-visible wireless observations | 2939 AP/global transmission observations |
| **PASS** | Width20MHz produced protocol-visible wireless observations | 1160 AP/global transmission observations |
| **PASS** | Width40MHz produced protocol-visible wireless observations | 1559 AP/global transmission observations |
| **PASS** | Width80MHz produced protocol-visible wireless observations | 2187 AP/global transmission observations |

### Configuration: `Width160MHz`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2939**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#21a130" /></svg> | Data: QoS Data [HE-MU, HE-MCS 2, 484-tone RU, GI 3.2 us, LDPC, A-MPDU] | 2213 | 75.30% | 1066.0 B | 0.0 B | 203.4 us | 15.6 us | 5240 MHz | - | 15.0 dBm | 92.53% | 100.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#26ab26" /></svg> | Data: QoS Data [HE-MU, HE-MCS 2, 996-tone RU, GI 3.2 us, LDPC, A-MPDU] | 2 | 0.07% | 1066.0 B | 0.0 B | 128.8 us | 0.0 us | 5240 MHz | - | 15.0 dBm | 0.05% | 0.06% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1bd02d" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 160 MHz, GI 3.2 us, LDPC] | 5 | 0.17% | 1066.0 B | 0.0 B | 105.6 us | 0.0 us | 5240 MHz | - | 15.0 dBm | 0.11% | 0.12% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 140 | 4.76% | 63.9 B | 1.5 B | 41.3 us | 0.5 us | 5240 MHz | - | 15.0 dBm | 1.19% | 1.28% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#194eb8" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 484-tone RU, GI 1.6 us, LDPC] | 556 | 18.92% | 32.0 B | 0.0 B | 51.8 us | 0.0 us | 5180 MHz, 5220 MHz, 5260 MHz, 5300 MHz | -71.8 dBm | - | 5.91% | 6.39% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#053c94" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 996-tone RU, GI 1.6 us, LDPC] | 2 | 0.07% | 32.0 B | 0.0 B | 43.5 us | 0.0 us | 5200 MHz, 5280 MHz | -71.5 dBm | - | 0.02% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 12 | 0.41% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5240 MHz | -72.0 dBm | 15.0 dBm | 0.06% | 0.07% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 9 | 0.31% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5240 MHz | -72.0 dBm | 15.0 dBm | 0.13% | 0.14% |

#### Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| `Width160MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:1` | 0.200132000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 160 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| `Width160MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:2` | 0.200176000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width160MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:3` | 0.200228000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| `Width160MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:4` | 0.200273000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width160MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:5` | 0.200439000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-SU, HE-MCS 1, 160 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| `Width160MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:6` | 0.200484000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width160MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:7` | 0.200536000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| `Width160MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:8` | 0.200580000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width160MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:9` | 0.200746000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-SU, HE-MCS 1, 160 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| `Width160MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:10` | 0.200791000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width160MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:11` | 0.200843000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| `Width160MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:12` | 0.200887000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width160MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:13` | 0.201062000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:04 | Data: QoS Data / HE-SU, HE-MCS 1, 160 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| `Width160MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:14` | 0.201107000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width160MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:15` | 0.201159000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:04 | Management: Action / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| `Width160MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:16` | 0.201204000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |

Frame numbers are local to the named capture, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### Configuration: `Width20MHz`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1160**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35c026" /></svg> | Data: QoS Data [HE-MU, HE-MCS 5, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 504 | 43.45% | 1066.0 B | 0.0 B | 746.7 us | 0.0 us | 5050 MHz | - | 15.0 dBm | 78.22% | 83.63% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 5 | 0.43% | 1066.0 B | 0.0 B | 619.1 us | 0.0 us | 5050 MHz | - | 15.0 dBm | 0.64% | 0.69% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 126 | 10.86% | 64.0 B | 0.0 B | 41.3 us | 0.0 us | 5050 MHz | - | 15.0 dBm | 1.08% | 1.16% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1332ae" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, LDPC] | 504 | 43.45% | 32.0 B | 0.0 B | 189.6 us | 0.0 us | 5043 MHz, 5047 MHz, 5053 MHz, 5057 MHz | -71.0 dBm | - | 19.86% | 21.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 4 | 0.34% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5050 MHz | -71.0 dBm | - | 0.02% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 8 | 0.69% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5050 MHz | -71.0 dBm | 15.0 dBm | 0.04% | 0.04% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 9 | 0.78% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5050 MHz | -71.0 dBm | 15.0 dBm | 0.13% | 0.14% |

#### Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| `Width20MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:1` | 0.200644000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| `Width20MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:2` | 0.200692000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width20MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:3` | 0.200744000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| `Width20MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:4` | 0.200789000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width20MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:5` | 0.201467000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| `Width20MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:6` | 0.201516000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width20MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:7` | 0.201568000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| `Width20MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:8` | 0.201612000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width20MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:9` | 0.202290000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| `Width20MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:10` | 0.202339000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width20MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:11` | 0.202391000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| `Width20MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:12` | 0.202435000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width20MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:13` | 0.203122000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:04 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| `Width20MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:14` | 0.203171000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width20MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:15` | 0.203223000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:04 | Management: Action / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| `Width20MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:16` | 0.203268000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |

Frame numbers are local to the named capture, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### Configuration: `Width40MHz`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1559**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#23be32" /></svg> | Data: QoS Data [HE-MU, HE-MCS 4, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 943 | 60.49% | 1066.0 B | 0.0 B | 463.9 us | 18.0 us | 5100 MHz | - | 15.0 dBm | 88.20% | 97.22% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c933" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 40 MHz, GI 3.2 us, LDPC] | 5 | 0.32% | 1066.0 B | 0.0 B | 327.6 us | 0.0 us | 5100 MHz | - | 15.0 dBm | 0.33% | 0.36% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 118 | 7.57% | 64.0 B | 0.0 B | 41.3 us | 0.0 us | 5100 MHz | - | 15.0 dBm | 0.98% | 1.08% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0933be" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, LDPC] | 472 | 30.28% | 32.0 B | 0.0 B | 108.3 us | 0.0 us | 5085 MHz, 5096 MHz, 5104 MHz, 5115 MHz | -71.0 dBm | - | 10.30% | 11.36% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 12 | 0.77% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5100 MHz | -71.0 dBm | 15.0 dBm | 0.06% | 0.07% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 9 | 0.58% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5100 MHz | -71.0 dBm | 15.0 dBm | 0.13% | 0.14% |

#### Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| `Width40MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:1` | 0.200356000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 40 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| `Width40MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:2` | 0.200400000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width40MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:3` | 0.200452000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| `Width40MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:4` | 0.200497000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width40MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:5` | 0.200887000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-SU, HE-MCS 1, 40 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| `Width40MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:6` | 0.200932000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width40MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:7` | 0.200984000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| `Width40MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:8` | 0.201028000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width40MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:9` | 0.201418000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-SU, HE-MCS 1, 40 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| `Width40MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:10` | 0.201463000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width40MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:11` | 0.201515000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| `Width40MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:12` | 0.201559000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width40MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:13` | 0.201958000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:04 | Data: QoS Data / HE-SU, HE-MCS 1, 40 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| `Width40MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:14` | 0.202003000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width40MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:15` | 0.202055000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:04 | Management: Action / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| `Width40MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:16` | 0.202100000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |

Frame numbers are local to the named capture, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### Configuration: `Width80MHz`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2187**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c335" /></svg> | Data: QoS Data [HE-MU, HE-MCS 3, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 1521 | 69.55% | 1066.0 B | 0.0 B | 303.6 us | 17.0 us | 5200 MHz | - | 15.0 dBm | 91.69% | 102.61% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#30b828" /></svg> | Data: QoS Data [HE-MU, HE-MCS 3, 484-tone RU, GI 3.2 us, LDPC, A-MPDU] | 2 | 0.09% | 1066.0 B | 0.0 B | 181.8 us | 0.0 us | 5200 MHz | - | 15.0 dBm | 0.07% | 0.08% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#27a52f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 80 MHz, GI 3.2 us, LDPC] | 5 | 0.23% | 1066.0 B | 0.0 B | 175.2 us | 0.0 us | 5200 MHz | - | 15.0 dBm | 0.17% | 0.19% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 128 | 5.85% | 63.9 B | 1.6 B | 41.3 us | 0.5 us | 5200 MHz | - | 15.0 dBm | 1.05% | 1.17% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1137b0" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 242-tone RU, GI 1.6 us, LDPC] | 508 | 23.23% | 32.0 B | 0.0 B | 67.5 us | 0.0 us | 5170 MHz, 5189 MHz, 5211 MHz, 5230 MHz | -71.5 dBm | - | 6.81% | 7.62% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#194eb8" /></svg> | Control: Block Ack (BA) [HE-TB, HE-MCS 0, 484-tone RU, GI 1.6 us, LDPC] | 2 | 0.09% | 32.0 B | 0.0 B | 51.8 us | 0.0 us | 5180 MHz, 5220 MHz | -71.5 dBm | - | 0.02% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 12 | 0.55% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5200 MHz | -71.0 dBm | 15.0 dBm | 0.06% | 0.07% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 9 | 0.41% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5200 MHz | -71.0 dBm | 15.0 dBm | 0.12% | 0.14% |

#### Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| `Width80MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:1` | 0.200196000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 80 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| `Width80MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:2` | 0.200240000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width80MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:3` | 0.200292000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| `Width80MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:4` | 0.200337000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width80MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:5` | 0.200567000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-SU, HE-MCS 1, 80 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| `Width80MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:6` | 0.200612000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width80MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:7` | 0.200664000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Management: Action / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| `Width80MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:8` | 0.200708000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width80MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:9` | 0.200938000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-SU, HE-MCS 1, 80 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| `Width80MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:10` | 0.200983000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width80MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:11` | 0.201035000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Management: Action / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| `Width80MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:12` | 0.201079000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width80MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:13` | 0.201318000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:04 | Data: QoS Data / HE-SU, HE-MCS 1, 80 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| `Width80MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:14` | 0.201363000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| `Width80MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:15` | 0.201415000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:04 | Management: Action / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| `Width80MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap:16` | 0.201460000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |

Frame numbers are local to the named capture, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### Analysis of Packet Distribution
IEEE Std 802.11-2024 Table 27-1 defines 20, 40, 80, and 160 MHz HE channel-width encodings, but the standard does not require packet count or throughput to scale linearly with width. The run-0 frame totals here are non-monotonic because aggregation, RU scheduling, and fixed overhead change the number of transmitted frames. The five-run sink goodput and delay analysis above is the appropriate capacity comparison; the radiotap bandwidth suffix is not.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->

## Frame exchange analysis

```sh
tshark -n -r 'examples/ieee80211ax/he_channel_widths/results/20260726T160000Z/Width20MHz/Width20MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap' \
  -Y 'frame.number <= 2 || (frame.number >= 27 && frame.number <= 35)' \
  -T fields -E header=y -E separator='|' \
  -e frame.number -e frame.time_epoch -e wlan.sa -e wlan.da \
  -e wlan.ta -e wlan.ra -e wlan.trigger.he.trigger_type \
  -e wlan.fc.type_subtype -e radiotap.he.data_1.ppdu_format \
  -e radiotap.he.data_3.data_mcs \
  -e radiotap.he.data_5.data_bw_ru_allocation \
  -e radiotap.he.data_6.nsts -e radiotap.he.data_5.gi
```

| Frame | Simulation time | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| Width20 frame 1 | 0.200644 s | AP → host[0] | QoS Data / HE-SU | subtype `0x28`; PPDU format `0`; MCS 1; width code `0` = 20 MHz; NSTS 1; GI code `2` | setup data |
| Width20 frame 2 | 0.200692 s | host[0] → AP | Ack | subtype `0x1d` | acknowledges frame 1 |
| Width40 frame 1 | 0.200356 s | AP → host[0] | QoS Data / HE-SU | same fields; width code `1` = 40 MHz | width counterfactual |
| Width80 frame 1 | 0.200196 s | AP → host[0] | QoS Data / HE-SU | same fields; width code `2` = 80 MHz | width counterfactual |
| Width160 frame 1 | 0.200132 s | AP → host[0] | QoS Data / HE-SU | same fields; width code `3` = 160 MHz | width counterfactual |
| Width20 frames 27–30 | 0.301497 s | AP → all four hosts | QoS Data / HE-MU | subtype `0x28`; PPDU format `2`; MCS 5; 52-tone RU code `5` | simultaneous downlink users |
| Width20 frame 31 | 0.301557 s | AP → broadcast RA | MU-BAR Trigger | subtype `0x12`; TA `10:00:00:00:00:00`; RA broadcast; trigger type `2` | solicits Block Ack responses |
| Width20 frames 32–35 | 0.301794 s | host[0..3] → AP | Block Ack / HE-TB | subtype `0x19`; four station TAs; AP RA; PPDU format `3`; MCS 0; 52-tone RU code `5` | simultaneous acknowledgment responses |

These are TShark frame numbers local to four different captures, not OMNeT++
event numbers. Direct field export supplies the numeric HE values. The shared
typed HE decoder first checks radiotap presence/known bits, maps the width
codes to labels, and leaves unsupported fields unknown. The longer generated
timelines above show all four setup Data/Ack exchanges. Frames 27–35 directly
establish one measured HE-MU Data/MU-BAR Trigger/HE-TB Block Ack cycle;
aggregate counts do not pair every cycle in the run. `wlan.ta`, `wlan.ra`,
and trigger type directly identify the AP MU-BAR and the four station
responses.

## Cross-layer findings and verdict

| Claim | Verdict | Configuration evidence | Model telemetry | Packet evidence | Outcome evidence |
|---|---|---|---|---|---|
| Named width reached HE-SU TXVECTOR | `PASS` | winning rate-selection bandwidth in each config | no scalar width telemetry | run-0 radiotap codes 0/1/2/3 decode to 20/40/80/160 MHz | n/a |
| The saturated phase exercised HE downlink multi-user operation | `PASS` | `HeHcf` and equal-sized-RU scheduler | result session retains outcomes, not a scheduler-decision join | frames 27–35: HE-MU QoS Data → MU-BAR Trigger → HE-TB Block Ack | sink traffic delivered |
| Wider configured bundles served this workload faster | `PASS` | same topology, traffic, scheduler, window, and seed policy | sink goodput and p95-delay vectors | setup-frame airtime falls; measured phase is protocol-visible | goodput rises 26.892→118.646 Mbps; p95 delay falls 97.253→9.406 ms |
| Width alone explains the outcome | `INCONCLUSIVE` | sensitivity, bitrate, band/channel bundle also changes | no single-parameter control | capture verifies requested modes, not isolation | ordering is bundle-specific |
| Wider channels improve range | `NOT RUN` | no matched 20-vs-160 cell-edge pair | none | none | none |

The bounded feature-test verdict is `PASS` for two claims: every named primary
configuration emitted protocol-visible HE-SU data at the requested width, and
the five-run outcome ordering is monotonic for this coordinated configuration
bundle. The broader causal claim that channel width alone produces the
observed goodput and delay ratios remains `INCONCLUSIVE`.

## Limitations and inconclusive claims

- Only run 0 has packet captures. It is representative mechanism evidence, not
  five-run packet-level coverage.
- Width, channel/band selection, interface bitrate, rate-selection bandwidth,
  and sensitivity change together. Sensitivity becomes 3 dB less permissive
  for each width step, so this suite cannot isolate width or establish range.
- The measured HE-MU rows use different decoded MCS/RU combinations. The
  equal-MCS duration comparison is limited to the five HE-SU frames (four
  setup and one initial measured-flow frame).
- `WidthCellEdge` has no matched 20 MHz control and is omitted from the shared
  suite; its description alone is not evidence that 20 MHz would succeed.
- HE MU/TB airtime is estimated where user-dependent signaling is unavailable,
  and summed observation airtime can exceed simulation time because concurrent
  station transmissions are counted separately.
- This is one stationary topology, payload size, scheduler, offered-load
  regime, and five-seed set. Packet counts are not a capacity estimator.

## Further experiments

- Sweep offered load while retaining the four widths; the first saturation
  point should appear in sink goodput and the delay ECDF.
- Add matched 20 and 160 MHz cell-edge controls with an explicit receive-power
  sweep; inspect reception/error results before making a coverage claim.
- Hold sensitivity and MCS policy fixed where the model permits, varying only
  width and its valid channel definition; compare that result with this
  coordinated-width bundle.
- Join every HE-MU transmission to its MU-BAR Trigger and HE-TB responses
  using a stable model decision identifier rather than aggregate counts.

## Implementation plan

No production implementation change is proposed. The retained primary
invariants pass. A future feature-test improvement could add an executable
goodput-and-delay acceptance envelope and a stable scheduler-decision
identifier for HE-MU/MU-BAR/HE-TB correlation; those are observability and
coverage improvements, not demonstrated protocol defects.

## Artifact provenance

| Artifact family | Session/path | Configurations/runs | Tool/filter/window | Integrity notes |
|---|---|---|---|---|
| Minimal run | `results/20260726T160000Z-minimal/Width20MHz/Width20MHz-#0.sca` | `Width20MHz`, run/seed 0 | release `bin/inet`, Cmdenv, scalar recording | exit 0; SHA-256 `d3715370…2021` |
| Combined scalar/vector and PCAP session | `results/20260726T160000Z` | four configs; scalar/vector runs/seeds `0–4`; PCAP run/seed `0` | shared `wifi_analysis.py`; `[0.3, 0.43) s` outcomes | one session; repository revision in sidecars |
| Session ledger | [`evidence-ledger.json`](../../ieee80211/analysis/generated/sessions/20260726T160000Z/evidence-ledger.json) | four configs | suite report | identifies unimplemented acceptance contracts |
| Scalar/vector figure | [`channel-width-dashboard.png`](results/20260726T160000Z/channel-width-dashboard.png) | five independent runs per width | run-level mean/95% Student-t CI; run-0 ECDF | [JSON sidecar](results/20260726T160000Z/channel-width-dashboard.png.json) |
| AP PCAP inputs | `results/20260726T160000Z/{configuration}/*ap.wlan[0].pcap` | four configs, run/seed 0 | TShark/Capinfos 4.6.4; typed HE profile | [capture manifest](../../ieee80211/analysis/generated/ax/capture_manifests/20260726T160000Z.json), SHA-256 `dfc51aa…c9e35` |
| Packet figure | [`packet_statistics.png`](results/20260726T160000Z/packet_statistics.png) | four AP captures | observation count and estimated airtime composition | [JSON sidecar](results/20260726T160000Z/packet_statistics.png.json) |
