# Walkthrough: 802.11ac Extended A-MPDU Aggregation Limits

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260809T181633Z`
- PCAP: `20260809T181633Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough compares two VHT A-MPDU policy limits, 65,535 and 1,048,575 bytes, under deliberately saturated offered load. Three streams target the same station so INET can form A-MSDUs before assembling the A-MPDU. HCF records the assembled A-MPDU length and MPDU count, while PCAP evidence confirms the corresponding A-MPDU framing.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- Explain the configured VHT A-MPDU policy limits of 65,535 and 1,048,575 bytes.
- Check whether the HCF actually builds aggregates large enough to cross the 65,535-byte boundary.
- Keep the packet-level model boundary visible: the 64-entry Block ACK window and the 4,065-byte A-MSDU limit constrain the aggregate even when the VHT policy ceiling is 1,048,575 bytes.

## [agent] Scenario description

The topology uses [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned). Three saturated 1,400-byte UDP streams are pushed from `server` to `host[0]`. Keeping the streams on one receiver makes them eligible for same-RA, same-TID A-MSDU aggregation; the resulting larger MPDUs are then eligible for A-MPDU aggregation.

Two configurations are evaluated:

1. `StandardVhtAmpduLimit`: VHT A-MPDU policy capped at 65,535 bytes.
2. `ExtendedVhtAmpduLimit`: A-MPDU size extended up to 1,048,575 bytes (VHT maximum limit).

## [agent] Standards and INET model boundary

- **IEEE Std 802.11ac-2013 / 802.11-2020 Clause 9.8 & 21.3.14**: Defines extended VHT A-MPDU frame limits and VHT EOF subframe delimiter structures.
- **INET Model Boundary**: HCF owns the active A-MPDU construction and applies the negotiated VHT cap plus the configured policy cap. The new `ampduCreated` telemetry records the model’s assembled delimiter-plus-MPDU content length; it is not a complete PHY PPDU length because PHY EOF padding is modeled separately.

## [agent] Evidence status

Evidence basis: direct observations from HCF telemetry and derived run-level maxima, cross-checked against direct PCAP observations.

| Claim | Status | Evidence | Scope or gap |
|---|---|---|---|
| HCF emits assembled A-MPDU length and MPDU-count telemetry | `PASS` | `ampduCreated:vector(packetBytes)` and `ampduNumMpdus:vector` in the session-bound `.vec` files | HCF model boundary; includes delimiter and modeled padding, not PHY EOF padding |
| Both policy configurations create multi-MPDU aggregates | `PASS` | Five matched runs per configuration; every run has qualifying samples | Minimum evidence threshold is 1,400 bytes and 2 MPDUs |
| The extended policy produces aggregates larger than the 65,535-byte standard boundary | `PASS` | Maximum observed aggregate is 95,102 bytes in the five-run campaign | Larger same-peer A-MSDUs create 64-member A-MPDUs |
| PCAP confirms VHT A-MPDU transmissions and Block ACK exchanges | `PASS` | AP PCAP statistics and representative frame-exchange timelines | PCAP provides over-the-air framing evidence, while HCF telemetry provides assembled model length |

## [agent] Configuration matrix

| Configuration | Role | Causal delta | Runs/seeds | Expected invariant |
|---|---|---|---|---|
| `StandardVhtAmpduLimit` | Baseline | `maxAmpduLengthExponent = 3` | 5 runs (seeds 0..4) | No assembled aggregate exceeds 65,535 bytes |
| `ExtendedVhtAmpduLimit` | Treatment | `maxAmpduLengthExponent = 7` | 5 runs (seeds 0..4) | No assembled aggregate exceeds 1,048,575 bytes; aggregates cross 65,535 bytes |

## [agent] Expected invariants and diagnostic map

| Invariant | Failure symptom | First diagnostic |
|---|---|---|
| `ampduCreated` and `ampduNumMpdus` have aligned timestamps | Length and MPDU count cannot be paired | Query both vectors for `**.ap.wlan[0].mac.hcf` and compare event times |
| Aggregate byte length respects the selected policy limit | Observed maximum exceeds the configured limit | Check `VhtMpduAggregationPolicy.maxAmpduLengthExponent` and HCF assembly path |
| The treatment crosses 65,535 bytes when the scenario is sufficiently loaded | Baseline and treatment maxima are identical below the boundary | Check same-peer destination mapping, A-MSDU size, and Block ACK eligibility threshold |

## [agent] Reproduction

Run from the INET repository root:

```sh
MPLCONFIGDIR=/tmp/matplotlib python3 examples/ieee80211/analysis/wifi_analysis.py run extended_ampdu --suite ac --evidence both --runs 5
MPLCONFIGDIR=/tmp/matplotlib python3 examples/ieee80211/analysis/wifi_analysis.py report extended_ampdu --suite ac --session-id 20260809T181633Z
MPLCONFIGDIR=/tmp/matplotlib python3 examples/ieee80211/analysis/wifi_analysis.py publish extended_ampdu --suite ac --session-id 20260809T181633Z --update
```

Session: `20260809T181633Z`; five independent seeds (`0..4`) per configuration. Result artifacts are under `examples/ieee80211ac/extended_ampdu/results/20260809T181633Z/`, including `.sca`, `.vec`, and `.pcapng` files.

## [agent] Scalar and vector analysis

<!-- BEGIN GENERATED: ieee80211-scalar-vector-extended_ampdu -->
### [script] Generated scalar/vector plot and table

![extended_ampdu scalar/vector analysis](results/20260809T181633Z/extended-ampdu-delivery-delay.png)

Figure provenance: [`results/20260809T181633Z/extended-ampdu-delivery-delay.png.json`](results/20260809T181633Z/extended-ampdu-delivery-delay.png.json). Run-level metric source: [`../../ieee80211ax/analysis/metrics.json`](../../ieee80211ax/analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)<br>vector / **.app[*] / endToEndDelay:vector
- Window / per-run aggregation / exclusions: [0.3, 0.5) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| Extended VHT A-MPDU Limit / goodput mbps | 286.72 | 0 |
| Standard VHT A-MPDU Policy Limit / goodput mbps | 257.734 | 1.67574 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **PASS** | Compare application delivered bytes while changing the VHT A-MPDU policy limit | Observed HCF aggregates respect the configured 65535- and 1048575-byte limits, and the treatment crosses the baseline boundary. |

#### [script] Observed assembled HCF A-MPDU telemetry

| Run | Standard samples / max bytes / max MPDUs | Extended samples / max bytes / max MPDUs | Configured limits |
|---:|---:|---:|---|
| 0 | 105 / 65382 / 44 | 80 / 95102 / 64 | 65535 / 1048575 bytes |
| 1 | 105 / 65382 / 44 | 80 / 95102 / 64 | 65535 / 1048575 bytes |
| 2 | 106 / 65382 / 44 | 79 / 95102 / 64 | 65535 / 1048575 bytes |
| 3 | 104 / 65382 / 44 | 80 / 95102 / 64 | 65535 / 1048575 bytes |
| 4 | 105 / 65382 / 44 | 80 / 95102 / 64 | 65535 / 1048575 bytes |
<!-- END GENERATED: ieee80211-scalar-vector-extended_ampdu -->

## [agent] PCAP statistics

<!-- BEGIN GENERATED: ieee80211-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260809T181633Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260809T181633Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260809T181633Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ac/capture_manifests/20260809T181633Z.json` (SHA-256 `906a13c9872f5e3a95a9f1bc6be838f57712992730775f7f0fe8153177ac3f78`). VHT PPDU format, MCS, coding, bandwidth, GI, and NSTS are decoded directly from standards-compliant radiotap VHT fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. VHT SU and VHT MU use modeled preambles; per-user VHT MU signaling remains approximate because radiotap carries common MU metadata alongside each logical user record.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

<small>

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `StandardVhtAmpduLimit` | `none (all decoded frames)` | 7398 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] (7072), Control: Block Ack Request (BAR) (160), Control: Block Ack (BA) (160) | 50.67% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `ExtendedVhtAmpduLimit` | `none (all decoded frames)` | 8028 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] (7780), Control: Block Ack Request (BAR) (121), Control: Block Ack (BA) (121) | 55.20% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

</small>

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | ExtendedVhtAmpduLimit produced protocol-visible wireless observations | 8028 AP/global transmission observations |
| **PASS** | StandardVhtAmpduLimit produced protocol-visible wireless observations | 7398 AP/global transmission observations |

### [script] Configuration: `StandardVhtAmpduLimit`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **7398**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | - | 7072 | 95.59% | 1481.0 B | 19.0 B | 69.4 us | 0.4 us | 5040 MHz | - | 13.0 dBm | 96.83% | 49.06% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28d733" /></svg> | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC] | - | 1 | 0.01% | 1466.0 B | 0.0 B | 69.1 us | 0.0 us | 5040 MHz | - | 13.0 dBm | 0.01% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 160 | 2.16% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.88% | 0.45% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 160 | 2.16% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 5010 MHz | -53.0 dBm | - | 2.23% | 1.13% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 3 | 0.04% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -53.0 dBm | 13.0 dBm | 0.01% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Req | - | 1 | 0.01% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Resp | - | 1 | 0.01% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -53.0 dBm | - | 0.01% | 0.01% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28d733" /></svg> | 1 | 0.200076000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200136000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 3 | 0.200255000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200315000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 5 | 0.200443000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200503000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 7 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 8 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 9 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 10 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 11 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 12 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 13 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 14 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 15 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 16 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 17 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 18 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 19 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 20 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 21 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 22 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=8, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 23 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 24 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 25 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 26 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=10, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 27 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 28 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=11, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 29 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 30 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=12, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 31 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 32 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=13, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 33 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 34 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=14, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 35 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 36 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=15, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 37 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 38 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=16, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 39 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 40 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=17, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 41 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 42 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=18, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 43 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 44 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=19, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 45 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 46 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=20, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 47 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 48 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=21, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 49 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 50 | 0.201948000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=22, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 51 | 0.202173000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 52 | 0.202417000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 53 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 54 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=23, frag=1, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 55 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=24, frag=0, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 56 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=24, frag=1, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 57 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=25, frag=0, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 58 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=25, frag=1, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 59 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=26, frag=0, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 60 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=26, frag=1, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 61 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=27, frag=0, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 62 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=27, frag=1, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 63 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=28, frag=0, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 64 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=28, frag=1, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 65 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=29, frag=0, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 66 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=29, frag=1, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 67 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=30, frag=0, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 68 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=30, frag=1, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 69 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=31, frag=0, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 70 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=31, frag=1, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 71 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=32, frag=0, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 72 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=32, frag=1, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 73 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=33, frag=0, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 74 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=33, frag=1, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 75 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=34, frag=0, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 76 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=34, frag=1, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 77 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=35, frag=0, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 78 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=35, frag=1, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 79 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=36, frag=0, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 80 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=36, frag=1, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 81 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=37, frag=0, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 82 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=37, frag=1, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 83 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=38, frag=0, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 84 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=38, frag=1, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 85 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=39, frag=0, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 86 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=39, frag=1, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 87 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=40, frag=0, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 88 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=40, frag=1, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 89 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=41, frag=0, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 90 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=41, frag=1, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 91 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=42, frag=0, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 92 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=42, frag=1, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 93 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=43, frag=0, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 94 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=43, frag=1, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 95 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=44, frag=0, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 96 | 0.203925000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=44, frag=1, more-frag=0, TID=0, A-MPDU=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 97 | 0.204087000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 98 | 0.204331000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=5337 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 99 | 0.205776000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=45, frag=0, more-frag=0, TID=0, A-MPDU=8997 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 100 | 0.205776000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=45, frag=1, more-frag=0, TID=0, A-MPDU=8997 |

</small>

Frame numbers are local to capture `StandardVhtAmpduLimit-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `ExtendedVhtAmpduLimit`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **8028**

<small>

| Color | Frame Type & Subtype | BSS Color | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | - | 7780 | 96.91% | 1481.0 B | 19.0 B | 69.4 us | 0.4 us | 5040 MHz | - | 13.0 dBm | 97.79% | 53.97% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28d733" /></svg> | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC] | - | 1 | 0.01% | 1466.0 B | 0.0 B | 69.1 us | 0.0 us | 5040 MHz | - | 13.0 dBm | 0.01% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | - | 121 | 1.51% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.61% | 0.34% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | - | 121 | 1.51% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 5010 MHz | -53.0 dBm | - | 1.55% | 0.86% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | - | 3 | 0.04% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -53.0 dBm | 13.0 dBm | 0.01% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Req | - | 1 | 0.01% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | - | 13.0 dBm | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action: Block Ack: ADDBA Resp | - | 1 | 0.01% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -53.0 dBm | - | 0.01% | 0.01% |

</small>

#### [script] Representative frame-exchange timeline

<small>

| Color | Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields |
|:---:|---:|---:|---|---|---|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28d733" /></svg> | 1 | 0.200076000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC] | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=0 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 2 | 0.200136000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 3 | 0.200255000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action: Block Ack: ADDBA Req | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 4 | 0.200315000 | ? → 10:00:00:00:00:00 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | 5 | 0.200443000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action: Block Ack: ADDBA Resp | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | 6 | 0.200503000 | ? → 0a:aa:00:00:00:01 | Control: Ack | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 7 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 8 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=1, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 9 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 10 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=2, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 11 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 12 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=3, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 13 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 14 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=4, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 15 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 16 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=5, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 17 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 18 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=6, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 19 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 20 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=7, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 21 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=8, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 22 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=8, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 23 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 24 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=9, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 25 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=10, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 26 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=10, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 27 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=11, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 28 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=11, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 29 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=12, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 30 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=12, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 31 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=13, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 32 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=13, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 33 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=14, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 34 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=14, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 35 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=15, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 36 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=15, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 37 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=16, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 38 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=16, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 39 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=17, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 40 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=17, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 41 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=18, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 42 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=18, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 43 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=19, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 44 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=19, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 45 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=20, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 46 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=20, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 47 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=21, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 48 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=21, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 49 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=22, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 50 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=22, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 51 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=23, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 52 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=23, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 53 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=24, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 54 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=24, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 55 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=25, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 56 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=25, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 57 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=26, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 58 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=26, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 59 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=27, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 60 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=27, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 61 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=28, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 62 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=28, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 63 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=29, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 64 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=29, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 65 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=30, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 66 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=30, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 67 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=31, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 68 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=31, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 69 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=32, frag=0, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 70 | 0.202556000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=32, frag=1, more-frag=0, TID=0, A-MPDU=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | 71 | 0.202763000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | 72 | 0.203007000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=-, A-MPDUs acknowledged=1443 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 73 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=33, frag=0, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 74 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=33, frag=1, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 75 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=34, frag=0, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 76 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=34, frag=1, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 77 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=35, frag=0, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 78 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=35, frag=1, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 79 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=36, frag=0, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 80 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=36, frag=1, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 81 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=37, frag=0, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 82 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=37, frag=1, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 83 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=38, frag=0, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 84 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=38, frag=1, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 85 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=39, frag=0, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 86 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=39, frag=1, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 87 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=40, frag=0, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 88 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=40, frag=1, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 89 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=41, frag=0, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 90 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=41, frag=1, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 91 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=42, frag=0, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 92 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=42, frag=1, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 93 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=43, frag=0, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 94 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=43, frag=1, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 95 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=44, frag=0, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 96 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=44, frag=1, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 97 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=45, frag=0, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 98 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=45, frag=1, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 99 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=46, frag=0, more-frag=0, TID=0, A-MPDU=6375 |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24b616" /></svg> | 100 | 0.205051000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | QoS Data [VHT, VHT-MCS 8, 80 MHz, GI 0.4 us, BCC, A-MPDU] | direction=from DS, retry=0, seq=46, frag=1, more-frag=0, TID=0, A-MPDU=6375 |

</small>

Frame numbers are local to capture `ExtendedVhtAmpduLimit-#0SingleBssNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Analysis of Packet Distribution
Across these configurations, **QoS Data** frames constitute the primary payload delivery mechanism, while **Block Ack (BA)** and **Block Ack Request (BAR)** control frames ensure reliable transport via the MAC-level acknowledgment protocol. Management frames, specifically **Beacons**, are transmitted periodically by the Access Point to maintain BSS time synchronization and broadcast network capabilities. The ratio of control/management overhead to actual data frames indicates the relative MAC efficiency of the chosen configurations.
<!-- END GENERATED: ieee80211-pcap-statistics -->

## [agent] Frame exchange analysis

The generated timelines show ADDBA negotiation followed by VHT QoS Data A-MPDU transmissions and BAR/BA exchanges. The HCF telemetry is the authoritative signal for the assembled model packet: PCAP confirms that the corresponding wireless data is transmitted as A-MPDU-framed VHT traffic, but PCAP frame sizes describe individual MPDUs after dissection and do not replace the HCF aggregate-length measurement.

## [agent] Cross-layer findings and verdict

The production telemetry closes the previous observability gap: the HCF now reports both assembled aggregate bytes and the number of MPDUs at the point where the A-MPDU is built. The campaign proves that aggregation is active and that both policy configurations are respected. The standard configuration reaches 65,382 bytes but remains below its 65,535-byte cap, while the extended configuration reaches 95,102 bytes with 64 MPDUs. The policy-boundary claim is therefore `PASS`.

## [agent] Limitations and inconclusive claims

- The current scenario demonstrates crossing the 65,535-byte boundary, but it does not approach the theoretical 1,048,575-byte VHT ceiling because the model uses a 64-member Block ACK window and 4,065-byte A-MSDU limit.
- `ampduCreated` measures the HCF-assembled delimiter-plus-MPDU content length. It excludes PHY-level EOF padding and is not a complete PPDU airtime measurement.
- The campaign uses five seeds per configuration and one AP PCAP observation point; PCAP counts are transmission observations, not de-duplicated application packets.
