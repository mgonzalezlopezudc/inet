# Walkthrough: 802.11n Channel Widths & Channel Bonding

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260803T235205Z`
- PCAP: `20260803T235205Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough demonstrates IEEE 802.11n 20 MHz and 40 MHz channel bonding operations, highlighting throughput scaling and subchannel offset configurations.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- Explain how 802.11n bonds two adjacent 20 MHz subchannels into a 40 MHz channel to double peak PHY data rates.
- Identify primary and secondary 20 MHz subchannel relationships.
- Compare transmission duration and throughput between 20 MHz and 40 MHz channel widths.

## [agent] Scenario description

The topology uses [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned). UDP traffic is sent from `server` to `host[0]`.

Four configurations are evaluated:

1. `Ht20MHz`: 20 MHz HT channel in 2.4 GHz.
2. `Ht40MHz`: 40 MHz HT channel bonding in 2.4 GHz.
3. `Ht40MHzSecondaryAbove`: 40 MHz channel with secondary subchannel positioned +20 MHz above primary.
4. `Ht40MHzSecondaryBelow`: 40 MHz channel with secondary subchannel positioned -20 MHz below primary.

The two directional configurations deliberately use different primary channels: `Ht40MHzSecondaryAbove`
uses INET channel index 0 (IEEE channel 1) with IEEE channel 5 as the secondary, while
`Ht40MHzSecondaryBelow` uses index 4 (IEEE channel 5) with IEEE channel 1 as the secondary.
Their bonded center is therefore the same, but the primary/secondary roles are visibly reversed.

## [agent] Standards and INET model boundary

- **IEEE Std 802.11n-2009 / 802.11-2020 Clause 19.3.3 & 19.3.11**: Specifies 20 MHz and 40 MHz OFDM subcarrier layouts (52 data subcarriers in 20 MHz vs 108 data subcarriers in 40 MHz).
- **INET Model Boundary**: Channel width and subchannel frequency spectrum allocation are set via radio and radioMedium parameters (`channelWidth = 40MHz`).
- The PHY carries the HT secondary-channel offset (`none`, `above`, or `below`) through the radio channel object; HT40 transmissions/listening use the bonded center between the primary and secondary 20 MHz channels.

## [agent] Scalar and vector analysis

<!-- BEGIN GENERATED: ieee80211-scalar-vector-channel_widths -->
### [script] Generated scalar/vector plot and table

![channel_widths scalar/vector analysis](results/20260803T235205Z/channel-widths-delivery-delay.png)

Figure provenance: [`results/20260803T235205Z/channel-widths-delivery-delay.png.json`](results/20260803T235205Z/channel-widths-delivery-delay.png.json). Run-level metric source: [`../../ieee80211ax/analysis/metrics.json`](../../ieee80211ax/analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)<br>vector / **.app[*] / endToEndDelay:vector
- Window / per-run aggregation / exclusions: [0.3, 0.9) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| HT 20 MHz / goodput mbps | 5.1488 | 0.00888462 |
| HT 40 MHz / goodput mbps | 14.0544 | 0.0266539 |
| HT 40 MHz (Secondary Above) / goodput mbps | 14.0544 | 0.0266539 |
| HT 40 MHz (Secondary Below) / goodput mbps | 14.0544 | 0.0266539 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **INCONCLUSIVE** | Compare application delivered bytes across 20 MHz and 40 MHz channel widths | No manifest acceptance threshold defined for channel width comparison |
<!-- END GENERATED: ieee80211-scalar-vector-channel_widths -->

<!-- BEGIN GENERATED: ieee80211-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260803T235205Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260803T235205Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260803T235205Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/n/capture_manifests/20260803T235205Z.json` (SHA-256 `08858a0caa976d876b0e963edcdd8da4abdea6d7b91a29ada78932bde6902693`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `Ht20MHz` | `none (all decoded frames)` | 783 | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC] (430), Control: Ack (294), Control: Block Ack Request (BAR) (27) | 36.40% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `Ht40MHz` | `none (all decoded frames)` | 2119 | QoS Data [HT, HT-MCS 1, 40 MHz, GI 0.8 us, BCC] (1171), Control: Ack (791), Control: Block Ack Request (BAR) (76) | 51.37% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `Ht40MHzSecondaryAbove` | `none (all decoded frames)` | 2119 | QoS Data [HT, HT-MCS 1, 40 MHz, GI 0.8 us, BCC] (1171), Control: Ack (791), Control: Block Ack Request (BAR) (76) | 51.37% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `Ht40MHzSecondaryBelow` | `none (all decoded frames)` | 2119 | QoS Data [HT, HT-MCS 1, 40 MHz, GI 0.8 us, BCC] (1171), Control: Ack (791), Control: Block Ack Request (BAR) (76) | 51.37% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | Ht20MHz produced protocol-visible wireless observations | 783 AP/global transmission observations |
| **PASS** | Ht40MHz produced protocol-visible wireless observations | 2119 AP/global transmission observations |
| **PASS** | Ht40MHzSecondaryAbove produced protocol-visible wireless observations | 2119 AP/global transmission observations |
| **PASS** | Ht40MHzSecondaryBelow produced protocol-visible wireless observations | 2119 AP/global transmission observations |

### [script] Configuration: `Ht20MHz`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **783**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | QoS Data | - | 1 | 0.13% | 84.0 B | 0.0 B | 48.0 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.01% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#35e01f" /></svg> | QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.8 us, BCC] | - | 430 | 54.92% | 1278.5 B | 313.7 B | 822.8 us | 193.0 us | 2412 MHz | -47.0 dBm | 13.0 dBm | 97.19% | 35.38% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 27 | 3.45% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.21% | 0.08% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 27 | 3.45% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 2412 MHz | -47.0 dBm | - | 0.52% | 0.19% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 294 | 37.55% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -47.0 dBm | 13.0 dBm | 1.99% | 0.73% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 4 | 0.51% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 2412 MHz | -47.0 dBm | 13.0 dBm | 0.08% | 0.03% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 1 | 0.200058000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 2 | 0.200308000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 3 | 0.200368000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 4 | 0.201442000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 5 | 0.201502000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 6 | 0.201634000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 7 | 0.201694000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 8 | 0.203026000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 9 | 0.203086000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 10 | 0.203178000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 11 | 0.203238000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 12 | 0.205286000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.205346000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 14 | 0.207394000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=1, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.207454000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 16 | 0.208754000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=1, frag=2, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 17 | 0.208814000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 18 | 0.210922000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 19 | 0.210982000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 20 | 0.213090000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=2, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 21 | 0.213150000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 22 | 0.214450000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=2, frag=2, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 23 | 0.214510000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 24 | 0.214602000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action | direction=direct/IBSS, retry=0, seq=1, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 25 | 0.214662000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 26 | 0.214854000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=1, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 27 | 0.214914000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 28 | 0.216750000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 29 | 0.216810000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 30 | 0.218878000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=3, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 31 | 0.218938000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 32 | 0.220198000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=3, frag=2, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 33 | 0.222266000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 34 | 0.222326000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 35 | 0.224434000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=4, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 36 | 0.224494000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 37 | 0.225734000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=4, frag=2, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 38 | 0.227782000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 39 | 0.227842000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 40 | 0.229890000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=5, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 41 | 0.229950000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 42 | 0.231250000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=5, frag=2, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 43 | 0.233318000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 44 | 0.233378000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 45 | 0.235426000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=6, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 46 | 0.235486000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 47 | 0.236766000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=6, frag=2, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 48 | 0.238834000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 49 | 0.238894000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 50 | 0.241002000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=7, frag=1, more-frag=0, TID=6 |

Frame numbers are local to capture `Ht20MHz-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `Ht40MHz`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2119**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | QoS Data | - | 1 | 0.05% | 84.0 B | 0.0 B | 48.0 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.01% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf2c" /></svg> | QoS Data [HT, HT-MCS 1, 40 MHz, GI 0.8 us, BCC] | - | 1171 | 55.26% | 1280.4 B | 309.9 B | 415.4 us | 91.8 us | 2412 MHz | -47.0 dBm | 13.0 dBm | 94.68% | 48.64% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 76 | 3.59% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.41% | 0.21% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 76 | 3.59% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 2412 MHz | -47.0 dBm | - | 1.05% | 0.54% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 791 | 37.33% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -47.0 dBm | 13.0 dBm | 3.80% | 1.95% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 4 | 0.19% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 2412 MHz | -47.0 dBm | 13.0 dBm | 0.05% | 0.03% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 1 | 0.200058000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 2 | 0.200288000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 3 | 0.200348000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 4 | 0.200882000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 5 | 0.200942000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 6 | 0.201304000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 7 | 0.201364000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 8 | 0.201556000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 9 | 0.201616000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 10 | 0.202512000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 11 | 0.202572000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 12 | 0.202934000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=1, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.202994000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 14 | 0.203086000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.203146000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 16 | 0.204234000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 17 | 0.204294000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 18 | 0.204788000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=2, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 19 | 0.204848000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 20 | 0.205146000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=2, frag=2, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 21 | 0.205206000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 22 | 0.206294000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 23 | 0.206354000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 24 | 0.206848000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=3, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 25 | 0.206908000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 26 | 0.207206000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=3, frag=2, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 27 | 0.207266000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 28 | 0.207610000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action | direction=direct/IBSS, retry=0, seq=1, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 29 | 0.207670000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 30 | 0.208164000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 31 | 0.208224000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 32 | 0.208718000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=4, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 33 | 0.208778000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 34 | 0.208970000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=1, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 35 | 0.209030000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 36 | 0.209474000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=4, frag=2, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 37 | 0.209968000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 38 | 0.210028000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 39 | 0.210522000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=5, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 40 | 0.210582000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 41 | 0.211338000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=5, frag=2, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 42 | 0.211832000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 43 | 0.211892000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 44 | 0.212386000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=6, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 45 | 0.212446000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 46 | 0.213162000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=6, frag=2, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 47 | 0.213656000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 48 | 0.213716000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 49 | 0.214210000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=7, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 50 | 0.214270000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

Frame numbers are local to capture `Ht40MHz-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `Ht40MHzSecondaryAbove`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2119**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | QoS Data | - | 1 | 0.05% | 84.0 B | 0.0 B | 48.0 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.01% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf2c" /></svg> | QoS Data [HT, HT-MCS 1, 40 MHz, GI 0.8 us, BCC] | - | 1171 | 55.26% | 1280.4 B | 309.9 B | 415.4 us | 91.8 us | 2412 MHz | -47.0 dBm | 13.0 dBm | 94.68% | 48.64% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 76 | 3.59% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.41% | 0.21% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 76 | 3.59% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 2412 MHz | -47.0 dBm | - | 1.05% | 0.54% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 791 | 37.33% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -47.0 dBm | 13.0 dBm | 3.80% | 1.95% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 4 | 0.19% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 2412 MHz | -47.0 dBm | 13.0 dBm | 0.05% | 0.03% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 1 | 0.200058000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 2 | 0.200288000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 3 | 0.200348000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 4 | 0.200882000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 5 | 0.200942000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 6 | 0.201304000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 7 | 0.201364000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 8 | 0.201556000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 9 | 0.201616000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 10 | 0.202512000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 11 | 0.202572000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 12 | 0.202934000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=1, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.202994000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 14 | 0.203086000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.203146000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 16 | 0.204234000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 17 | 0.204294000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 18 | 0.204788000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=2, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 19 | 0.204848000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 20 | 0.205146000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=2, frag=2, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 21 | 0.205206000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 22 | 0.206294000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 23 | 0.206354000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 24 | 0.206848000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=3, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 25 | 0.206908000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 26 | 0.207206000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=3, frag=2, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 27 | 0.207266000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 28 | 0.207610000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action | direction=direct/IBSS, retry=0, seq=1, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 29 | 0.207670000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 30 | 0.208164000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 31 | 0.208224000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 32 | 0.208718000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=4, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 33 | 0.208778000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 34 | 0.208970000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=1, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 35 | 0.209030000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 36 | 0.209474000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=4, frag=2, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 37 | 0.209968000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 38 | 0.210028000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 39 | 0.210522000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=5, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 40 | 0.210582000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 41 | 0.211338000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=5, frag=2, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 42 | 0.211832000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 43 | 0.211892000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 44 | 0.212386000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=6, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 45 | 0.212446000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 46 | 0.213162000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=6, frag=2, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 47 | 0.213656000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 48 | 0.213716000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 49 | 0.214210000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=7, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 50 | 0.214270000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

Frame numbers are local to capture `Ht40MHzSecondaryAbove-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `Ht40MHzSecondaryBelow`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2119**

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | QoS Data | - | 1 | 0.05% | 84.0 B | 0.0 B | 48.0 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.01% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf2c" /></svg> | QoS Data [HT, HT-MCS 1, 40 MHz, GI 0.8 us, BCC] | - | 1171 | 55.26% | 1280.4 B | 309.9 B | 415.4 us | 91.8 us | 2412 MHz | -47.0 dBm | 13.0 dBm | 94.68% | 48.64% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 76 | 3.59% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 2412 MHz | - | 13.0 dBm | 0.41% | 0.21% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 76 | 3.59% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 2412 MHz | -47.0 dBm | - | 1.05% | 0.54% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 791 | 37.33% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 2412 MHz | -47.0 dBm | 13.0 dBm | 3.80% | 1.95% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | - | 4 | 0.19% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 2412 MHz | -47.0 dBm | 13.0 dBm | 0.05% | 0.03% |

#### [script] Representative frame-exchange timeline

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 1 | 0.200058000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 2 | 0.200288000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | QoS Data | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 3 | 0.200348000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 4 | 0.200882000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 5 | 0.200942000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 6 | 0.201304000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=0, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 7 | 0.201364000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 8 | 0.201556000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 9 | 0.201616000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 10 | 0.202512000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 11 | 0.202572000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 12 | 0.202934000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=1, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 13 | 0.202994000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 14 | 0.203086000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 15 | 0.203146000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 16 | 0.204234000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 17 | 0.204294000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 18 | 0.204788000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=2, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 19 | 0.204848000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 20 | 0.205146000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=2, frag=2, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 21 | 0.205206000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 22 | 0.206294000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 23 | 0.206354000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 24 | 0.206848000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=3, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 25 | 0.206908000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 26 | 0.207206000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=3, frag=2, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 27 | 0.207266000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 28 | 0.207610000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action | direction=direct/IBSS, retry=0, seq=1, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 29 | 0.207670000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 30 | 0.208164000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 31 | 0.208224000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 32 | 0.208718000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=4, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 33 | 0.208778000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 34 | 0.208970000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action | direction=direct/IBSS, retry=0, seq=1, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 35 | 0.209030000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 36 | 0.209474000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=4, frag=2, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 37 | 0.209968000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 38 | 0.210028000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 39 | 0.210522000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=5, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 40 | 0.210582000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 41 | 0.211338000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=5, frag=2, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 42 | 0.211832000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 43 | 0.211892000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 44 | 0.212386000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=6, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 45 | 0.212446000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 46 | 0.213162000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=6, frag=2, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 47 | 0.213656000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 48 | 0.213716000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#22c322" /></svg> | 49 | 0.214210000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data | direction=from DS, retry=0, seq=7, frag=1, more-frag=0, TID=6 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 50 | 0.214270000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |

Frame numbers are local to capture `Ht40MHzSecondaryBelow-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Analysis of Packet Distribution
IEEE Std 802.11-2024 Table 27-1 defines 20, 40, 80, and 160 MHz HE channel-width encodings, but the standard does not require packet count or throughput to scale linearly with width. The run-0 frame totals here are non-monotonic because aggregation, RU scheduling, and fixed overhead change the number of transmitted frames. The five-run sink goodput and delay analysis above is the appropriate capacity comparison; the radiotap bandwidth suffix is not.
<!-- END GENERATED: ieee80211-pcap-statistics -->
