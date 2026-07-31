# Walkthrough: HE Multi-TID Block Ack

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260731T211915Z`
- PCAP: `20260731T211915Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

The published session shows application delivery in all three configurations.
Its representative run-0 captures also decode Multi-STA Block Ack Type 11
per-AID/TID entries and show the HT implicit Block Ack condition without an
on-air Block Ack Request (BAR). These are separate outcome and mechanism
observations: neither alone proves the other.

## [agent] Learning objectives and feature primer

A traffic identifier (TID) distinguishes QoS traffic streams. A Multi-TID
A-MPDU can include frames from more than one TID, so the Block Ack needs to
identify the acknowledged contexts. In an HE uplink MU exchange, the AP sends
a Trigger, stations respond, and the AP returns a Multi-STA Block Ack. The HT
comparison instead uses an implicit Block Ack after an A-MPDU. This walkthrough
checks delivery and the corresponding on-air acknowledgment evidence.

## [agent] Scenario description

The scenario in [omnetpp.ini](omnetpp.ini) has stationary stations sending
UDP to a wired server through an AP. `UlSuMultiTidBlockAck` uses one HE station
with two TIDs and UL MU disabled. `UlMuMultiTidBlockAck` enables the
backlog-based uplink scheduler, with one offered TID from each of two stations.
Both start traffic at 0.3 s. `UlSUHTAMpduCompressedBlockAck` inherits the
single-user topology, changes to mixed 2.4 GHz HT mode, and enables implicit
Block Ack. The shared delivery measurement window is `[0.3, 0.88)` s.

## [agent] Standards and INET model boundary

IEEE Std 802.11-2024 §26.6.3.4 specifies ack-enabled Multi-TID A-MPDUs and
requires peer HE capability before transmission
(`80211ax-2024:chunk:09842`). Section 9.3.1.8.6 defines Multi-STA Block Ack
and its repeated per-AID/TID information (`80211ax-2024:chunk:01597`). INET
requests the relevant transmit and receive capability through
`heMultiTidAggregation{Tx,Rx}`. That configuration is an input; the decoded
Type 11 fields and per-AID/TID entries in the published PCAP table are the
direct observation.

## [agent] Evidence status

| Claim | Status | Script-generated evidence | Runs | Scope or gap |
|---|---|---|---|---|
| All configurations deliver offered traffic | `PASS` | scalar/vector goodput table | 0-4 | outcome only; does not identify an acknowledgment format |
| HE captures expose the offered QoS TIDs | `PASS` | representative frame timelines | 0 | direct capture observation only |
| HE UL-MU includes the Trigger, response, and Block Ack pattern | `PASS` | UL-MU timeline and PCAP statistics | 0 | observations are not de-duplicated transmissions |
| Multi-STA Block Ack identifies multiple AID/TID contexts | `PASS` | decoded Type 11 records and PCAP evidence check | 0 | direct frame-field evidence, not delivery proof |
| HT implicit Block Ack has no on-air BAR | `PASS` | HT PCAP evidence check | 0 | direct control-observation evidence, not a decoded policy setting |

The goodput rows are derived measurements; the timeline fields are direct
observations; the Multi-STA Block Ack table is direct frame-field evidence.

## [agent] Configuration matrix

| Configuration | Role | Causal delta | Runs | Expected invariant |
|---|---|---|---|---|
| `UlSuMultiTidBlockAck` | HE UL-SU | UL MU OFDMA disabled; two TIDs at one station | 0-4 | the capture exposes both TIDs |
| `UlMuMultiTidBlockAck` | HE UL-MU | scheduler and UL MU OFDMA enabled; one TID at each of two stations | 0-4 | Trigger precedes scheduled responses |
| `UlSUHTAMpduCompressedBlockAck` | HT UL-SU | mixed 2.4 GHz HT mode and implicit Block Ack enabled | 0-4 | Block Ack is observed without a BAR |

Both configurations enable HE Multi-TID aggregation transmit and receive
capability at all listed AP and station WLAN interfaces.

## [agent] Expected invariants and diagnostic map

| Invariant | Script-generated evidence | Failure symptom | First diagnostic |
|---|---|---|---|
| UL-SU exposes both offered TIDs | UL-SU timeline | only one decoded TID | inspect the QoS classifier and per-AC queues |
| UL-MU schedules station responses | UL-MU timeline | no Trigger or HE-TB response | inspect the uplink scheduler decision |
| Multi-STA Block Ack has multiple AID/TID contexts | decoded Type 11 records | no Type 11 record or fewer than two entries | inspect MAC-header serialization and the capture decoder |
| HT implicit Block Ack has no BAR | HT PCAP evidence check | BAR observed or no Block Ack observed | inspect the HT block-ack policy |

## [agent] Reproduction

Run the following from the INET repository root. The session contains five
scalar/vector run summaries and representative run-0 PCAP evidence. The
published capture manifest records its capture points and input provenance.
The PCAP is run-0 evidence; the session does not publish seed mappings for
runs 1-4, so this walkthrough makes no seed-specific claim.
The published scalar/vector view uses retained input such as
[`UlMuMultiTidBlockAck-#0.sca`](results/20260731T102044Z/UlMuMultiTidBlockAck/UlMuMultiTidBlockAck-#0.sca);
the displayed plots and tables are from the published `20260731T211915Z`
session.

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py report ul_multitid \
  --session-id 20260731T211915Z
python3 examples/ieee80211/analysis/wifi_analysis.py publish ul_multitid \
  --session-id 20260731T211915Z --update
```

## [agent] Scalar and vector analysis

The generated five-run table measures goodput over the shared window. Its
nonzero configuration rows establish the delivery outcome for this session;
the reported 95% confidence intervals describe variation among run summaries.
The accompanying scalar/vector check is deliberately `INCONCLUSIVE` for
per-TID Block Ack contents because those are PCAP fields, not application
measurements.

<!-- BEGIN GENERATED: ieee80211-scalar-vector-multi_tid -->
### [script] Generated scalar/vector plot and table

![multi_tid scalar/vector analysis](results/20260731T211915Z/multi-tid-delivery.png)

Figure provenance: [`results/20260731T211915Z/multi-tid-delivery.png.json`](results/20260731T211915Z/multi-tid-delivery.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)
- Window / per-run aggregation / exclusions: [0.3, 0.88) s; observation=one aggregate goodput value per run; uncertainty=95% Student-t CI
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| Uplink MU Multi-TID BA / goodput mbps | 3.2 | 0 |
| Uplink SU HT A-MPDU compressed BA / goodput mbps | 9.15421 | 0.00306366 |
| Uplink SU Multi-TID BA / goodput mbps | 1.76 | 0 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **INCONCLUSIVE** | Acknowledgment exchanges identify the TIDs covered by each Multi-STA Block Ack | Application delivery is retained for the five-run comparison, while decisive per-TID Block Ack fields remain PCAP evidence. |
<!-- END GENERATED: ieee80211-scalar-vector-multi_tid -->

## [agent] PCAP statistics

The script analyzes AP `wlan[0]` PCAPng from representative run 0. Counts are
capture observations, not de-duplicated transmissions or delivered packets.
The UL-MU timeline supplies the Trigger, HE-TB response, and Block Ack order;
the decoded Type 11 records identify per-AID/TID contexts in the two HE
configurations. The HT evidence check separately observes Block Ack and no
on-air BAR.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260731T211915Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260731T211915Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260731T211915Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260731T211915Z.json` (SHA-256 `c694781ae2af8ed6a9a46d666b9c64da065c987692f8e73d2c72f1f7ef43d37c`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `UlMuMultiTidBlockAck` | `none (all decoded frames)` | 1231 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (292), Control: Ack (280), Control: Trigger (279) | 22.24% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UlSuMultiTidBlockAck` | `none (all decoded frames)` | 381 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (210), Control: Ack (141), Control: Block Ack Request (BAR) (13) | 10.44% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UlSUHTAMpduCompressedBlockAck` | `none (all decoded frames)` | 4152 | Data: QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.4 us, BCC, A-MPDU] (4002), Control: Block Ack (BA) [HT, HT-MCS 1, 20 MHz, GI 0.4 us, BCC] (134), Data: QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.4 us, BCC] (6) | 73.95% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | UlMuMultiTidBlockAck produced protocol-visible wireless observations | 1231 AP/global transmission observations |
| **PASS** | UlSUHTAMpduCompressedBlockAck produced protocol-visible wireless observations | 4152 AP/global transmission observations |
| **PASS** | UlSuMultiTidBlockAck produced protocol-visible wireless observations | 381 AP/global transmission observations |
| **PASS** | BA Type 11 and per-AID/TID entries are decoded from Multi-STA Block Ack frames | UlMuMultiTidBlockAck: 278 BA Type 11 frame(s) with multiple AID/TID entries, UlSuMultiTidBlockAck: 13 BA Type 11 frame(s) with multiple AID/TID entries |
| **PASS** | HT implicit Block Ack has Block Ack observations and no on-air BAR | UlSUHTAMpduCompressedBlockAck: 134 Block Ack observation(s), 0 BAR observation(s) |

### [script] Configuration: `UlMuMultiTidBlockAck`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1231**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 292 | 23.72% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -63.9 dBm | - | 81.59% | 18.14% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0a480c" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 93 | 7.55% | 34.0 B | 0.0 B | 121.3 us | 0.0 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 5.07% | 1.13% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 9 | 0.73% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 1.61% | 0.36% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 279 | 22.66% | 40.4 B | 3.4 B | 33.5 us | 1.1 us | 5010 MHz | - | 10.0 dBm | 4.20% | 0.93% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 278 | 22.58% | 46.1 B | 1.2 B | 35.4 us | 0.4 us | 5010 MHz | - | 10.0 dBm | 4.42% | 0.98% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 280 | 22.75% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 3.11% | 0.69% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.003064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=211 | Responds without MAC payload while preserving QoS control information. |
| 3 | 0.003064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=219 | Responds without MAC payload while preserving QoS control information. |
| 4 | 0.003064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=227 | Responds without MAC payload while preserving QoS control information. |
| 5 | 0.003133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 6 | 0.104048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 7 | 0.106064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=403 | Responds without MAC payload while preserving QoS control information. |
| 8 | 0.106064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=411 | Responds without MAC payload while preserving QoS control information. |
| 9 | 0.106064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=419 | Responds without MAC payload while preserving QoS control information. |
| 10 | 0.106133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 11 | 0.207048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 12 | 0.209064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=595 | Responds without MAC payload while preserving QoS control information. |
| 13 | 0.209064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=603 | Responds without MAC payload while preserving QoS control information. |
| 14 | 0.209064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=611 | Responds without MAC payload while preserving QoS control information. |
| 15 | 0.209133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 16 | 0.300644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |

Frame numbers are local to capture `UlMuMultiTidBlockAck-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded Multi-STA Block Ack records

| Frame | Simulation time (s) | BA Control | Decoded per-AID/TID entries |
|---:|---:|---|---|
| 5 | 0.003133000 | 0x0016 (type 0x000b) | AID=1, TID=0; AID=2, TID=0; AID=3, TID=0 |
| 10 | 0.106133000 | 0x0016 (type 0x000b) | AID=1, TID=0; AID=2, TID=0; AID=3, TID=0 |
| 15 | 0.209133000 | 0x0016 (type 0x000b) | AID=1, TID=0; AID=2, TID=0; AID=3, TID=0 |
| 21 | 0.303136000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=2, TID=7 |
| 23 | 0.305621000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=2, TID=7 |
| 28 | 0.308059000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=2, TID=7 |
| 32 | 0.310621000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=2, TID=7 |
| 37 | 0.313041000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=2, TID=7 |
| 41 | 0.315621000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=2, TID=7 |
| 46 | 0.318050000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=2, TID=7 |
| 50 | 0.320621000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=2, TID=7 |
| 55 | 0.323086000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=2, TID=7 |
| 59 | 0.325621000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=2, TID=7 |
| 65 | 0.328794000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=2, TID=7 |
| 67 | 0.330621000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=2, TID=7 |
| 72 | 0.333068000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=2, TID=7 |
| 76 | 0.335621000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=2, TID=7 |
| 82 | 0.338812000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=2, TID=7 |
| 84 | 0.340621000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=2, TID=7 |
| 89 | 0.343050000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=2, TID=7 |
| 93 | 0.345621000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=2, TID=7 |
| 98 | 0.348050000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=2, TID=7 |
| 102 | 0.350621000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=2, TID=7 |
| 107 | 0.353068000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=2, TID=7 |
| 111 | 0.355621000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=2, TID=7 |

Showing the first 25 of 278 decoded Multi-STA Block Ack frames; the script-owned packet metrics JSON preserves every row.

### [script] Configuration: `UlSuMultiTidBlockAck`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **381**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 210 | 55.12% | 803.3 B | 377.1 B | 475.4 us | 206.3 us | 5010 MHz | -60.0 dBm | - | 95.67% | 9.98% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 13 | 3.41% | 30.0 B | 0.0 B | 30.0 us | 0.0 us | 5010 MHz | -60.0 dBm | - | 0.37% | 0.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 13 | 3.41% | 46.0 B | 0.0 B | 35.3 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.44% | 0.05% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 141 | 37.01% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 3.33% | 0.35% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 2 | 0.52% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -60.0 dBm | 10.0 dBm | 0.05% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 2 | 0.52% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -60.0 dBm | 10.0 dBm | 0.13% | 0.01% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.300644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 2 | 0.300692000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 3 | 0.300920000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=7 | Carries protocol-visible MAC payload in the representative exchange. |
| 4 | 0.300968000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 5 | 0.301020000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 6 | 0.301064000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 7 | 0.301134000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 8 | 0.301178000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 9 | 0.305644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 10 | 0.305692000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 11 | 0.310212000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=7 | Carries protocol-visible MAC payload in the representative exchange. |
| 12 | 0.310872000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 13 | 0.310920000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 14 | 0.315644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 15 | 0.315692000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 16 | 0.320212000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=7 | Carries protocol-visible MAC payload in the representative exchange. |

Frame numbers are local to capture `UlSuMultiTidBlockAck-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded Multi-STA Block Ack records

| Frame | Simulation time (s) | BA Control | Decoded per-AID/TID entries |
|---:|---:|---|---|
| 33 | 0.350316000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=1, TID=7 |
| 60 | 0.400316000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=1, TID=7 |
| 87 | 0.450316000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=1, TID=7 |
| 114 | 0.500316000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=1, TID=7 |
| 141 | 0.550316000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=1, TID=7 |
| 168 | 0.600316000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=1, TID=7 |
| 195 | 0.650316000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=1, TID=7 |
| 222 | 0.700316000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=1, TID=7 |
| 249 | 0.750316000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=1, TID=7 |
| 276 | 0.800316000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=1, TID=7 |
| 303 | 0.850316000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=1, TID=7 |
| 330 | 0.900316000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=1, TID=7 |
| 357 | 0.950316000 | 0x0016 (type 0x000b) | AID=1, TID=6; AID=1, TID=7 |

### [script] Configuration: `UlSUHTAMpduCompressedBlockAck`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **4152**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1de234" /></svg> | Data: QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.4 us, BCC, A-MPDU] | 4002 | 96.39% | 266.1 B | 0.7 B | 183.4 us | 0.4 us | 5010 MHz | -60.0 dBm | - | 99.25% | 73.40% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#19c819" /></svg> | Data: QoS Data [HT, HT-MCS 1, 20 MHz, GI 0.4 us, BCC] | 6 | 0.14% | 270.0 B | 0.0 B | 185.5 us | 0.0 us | 5010 MHz | -60.0 dBm | - | 0.15% | 0.11% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#062988" /></svg> | Control: Block Ack (BA) [HT, HT-MCS 1, 20 MHz, GI 0.4 us, BCC] | 134 | 3.23% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.56% | 0.41% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3a96f2" /></svg> | Control: Ack [HT, HT-MCS 1, 20 MHz, GI 0.4 us, BCC] | 6 | 0.14% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.02% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#54b2e8" /></svg> | Control: Ack [HT, HT-MCS 7, 20 MHz, GI 0.4 us, BCC] | 2 | 0.05% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -60.0 dBm | 10.0 dBm | 0.01% | 0.00% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e8213c" /></svg> | Management: Action [HT, HT-MCS 7, 20 MHz, GI 0.4 us, BCC] | 2 | 0.05% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -60.0 dBm | 10.0 dBm | 0.02% | 0.01% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.300204000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / Legacy/HT/VHT | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 2 | 0.300262000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 3 | 0.300316000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 4 | 0.300366000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 5 | 0.300580000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / Legacy/HT/VHT | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 6 | 0.300638000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 7 | 0.300852000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / Legacy/HT/VHT | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 8 | 0.300910000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 9 | 0.301124000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / Legacy/HT/VHT | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 10 | 0.301182000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 11 | 0.301396000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / Legacy/HT/VHT | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 12 | 0.301454000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 13 | 0.301668000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / Legacy/HT/VHT | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 14 | 0.301726000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 15 | 0.301860000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Management: Action / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 16 | 0.301910000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |

Frame numbers are local to capture `UlSUHTAMpduCompressedBlockAck-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded HT Compressed Block Ack records

| Frame | Simulation time (s) | Starting sequence | Bitmap | Acknowledged MPDU sequence numbers |
|---:|---:|---:|---|---|
| 32 | 0.304670000 | 6 | ff7f000000000000 | 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20 |
| 60 | 0.309390000 | 21 | ffffff0700000000 | 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47 |
| 91 | 0.314574000 | 48 | ffffff3f00000000 | 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77 |
| 122 | 0.319818000 | 78 | ffffff3f00000000 | 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107 |
| 153 | 0.325003000 | 108 | ffffff3f00000000 | 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137 |
| 184 | 0.330207000 | 138 | ffffff3f00000000 | 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167 |
| 215 | 0.335431000 | 168 | ffffff3f00000000 | 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197 |
| 246 | 0.340635000 | 198 | ffffff3f00000000 | 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227 |
| 277 | 0.345819000 | 228 | ffffff3f00000000 | 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 256, 257 |
| 308 | 0.351023000 | 258 | ffffff3f00000000 | 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287 |
| 339 | 0.356267000 | 288 | ffffff3f00000000 | 288, 289, 290, 291, 292, 293, 294, 295, 296, 297, 298, 299, 300, 301, 302, 303, 304, 305, 306, 307, 308, 309, 310, 311, 312, 313, 314, 315, 316, 317 |
| 370 | 0.361491000 | 318 | ffffff3f00000000 | 318, 319, 320, 321, 322, 323, 324, 325, 326, 327, 328, 329, 330, 331, 332, 333, 334, 335, 336, 337, 338, 339, 340, 341, 342, 343, 344, 345, 346, 347 |
| 401 | 0.366675000 | 348 | ffffff3f00000000 | 348, 349, 350, 351, 352, 353, 354, 355, 356, 357, 358, 359, 360, 361, 362, 363, 364, 365, 366, 367, 368, 369, 370, 371, 372, 373, 374, 375, 376, 377 |
| 432 | 0.371900000 | 378 | ffffff3f00000000 | 378, 379, 380, 381, 382, 383, 384, 385, 386, 387, 388, 389, 390, 391, 392, 393, 394, 395, 396, 397, 398, 399, 400, 401, 402, 403, 404, 405, 406, 407 |
| 463 | 0.377084000 | 408 | ffffff3f00000000 | 408, 409, 410, 411, 412, 413, 414, 415, 416, 417, 418, 419, 420, 421, 422, 423, 424, 425, 426, 427, 428, 429, 430, 431, 432, 433, 434, 435, 436, 437 |
| 494 | 0.382268000 | 438 | ffffff3f00000000 | 438, 439, 440, 441, 442, 443, 444, 445, 446, 447, 448, 449, 450, 451, 452, 453, 454, 455, 456, 457, 458, 459, 460, 461, 462, 463, 464, 465, 466, 467 |
| 525 | 0.387492000 | 468 | ffffff3f00000000 | 468, 469, 470, 471, 472, 473, 474, 475, 476, 477, 478, 479, 480, 481, 482, 483, 484, 485, 486, 487, 488, 489, 490, 491, 492, 493, 494, 495, 496, 497 |
| 556 | 0.392696000 | 498 | ffffff3f00000000 | 498, 499, 500, 501, 502, 503, 504, 505, 506, 507, 508, 509, 510, 511, 512, 513, 514, 515, 516, 517, 518, 519, 520, 521, 522, 523, 524, 525, 526, 527 |
| 587 | 0.397920000 | 528 | ffffff3f00000000 | 528, 529, 530, 531, 532, 533, 534, 535, 536, 537, 538, 539, 540, 541, 542, 543, 544, 545, 546, 547, 548, 549, 550, 551, 552, 553, 554, 555, 556, 557 |
| 618 | 0.403144000 | 558 | ffffff3f00000000 | 558, 559, 560, 561, 562, 563, 564, 565, 566, 567, 568, 569, 570, 571, 572, 573, 574, 575, 576, 577, 578, 579, 580, 581, 582, 583, 584, 585, 586, 587 |
| 649 | 0.408388000 | 588 | ffffff3f00000000 | 588, 589, 590, 591, 592, 593, 594, 595, 596, 597, 598, 599, 600, 601, 602, 603, 604, 605, 606, 607, 608, 609, 610, 611, 612, 613, 614, 615, 616, 617 |
| 680 | 0.413632000 | 618 | ffffff3f00000000 | 618, 619, 620, 621, 622, 623, 624, 625, 626, 627, 628, 629, 630, 631, 632, 633, 634, 635, 636, 637, 638, 639, 640, 641, 642, 643, 644, 645, 646, 647 |
| 711 | 0.418856000 | 648 | ffffff3f00000000 | 648, 649, 650, 651, 652, 653, 654, 655, 656, 657, 658, 659, 660, 661, 662, 663, 664, 665, 666, 667, 668, 669, 670, 671, 672, 673, 674, 675, 676, 677 |
| 742 | 0.424041000 | 678 | ffffff3f00000000 | 678, 679, 680, 681, 682, 683, 684, 685, 686, 687, 688, 689, 690, 691, 692, 693, 694, 695, 696, 697, 698, 699, 700, 701, 702, 703, 704, 705, 706, 707 |
| 773 | 0.429225000 | 708 | ffffff3f00000000 | 708, 709, 710, 711, 712, 713, 714, 715, 716, 717, 718, 719, 720, 721, 722, 723, 724, 725, 726, 727, 728, 729, 730, 731, 732, 733, 734, 735, 736, 737 |
| 804 | 0.434429000 | 738 | ffffff3f00000000 | 738, 739, 740, 741, 742, 743, 744, 745, 746, 747, 748, 749, 750, 751, 752, 753, 754, 755, 756, 757, 758, 759, 760, 761, 762, 763, 764, 765, 766, 767 |
| 835 | 0.439633000 | 768 | ffffff3f00000000 | 768, 769, 770, 771, 772, 773, 774, 775, 776, 777, 778, 779, 780, 781, 782, 783, 784, 785, 786, 787, 788, 789, 790, 791, 792, 793, 794, 795, 796, 797 |
| 866 | 0.444877000 | 798 | ffffff3f00000000 | 798, 799, 800, 801, 802, 803, 804, 805, 806, 807, 808, 809, 810, 811, 812, 813, 814, 815, 816, 817, 818, 819, 820, 821, 822, 823, 824, 825, 826, 827 |
| 897 | 0.450081000 | 828 | ffffff3f00000000 | 828, 829, 830, 831, 832, 833, 834, 835, 836, 837, 838, 839, 840, 841, 842, 843, 844, 845, 846, 847, 848, 849, 850, 851, 852, 853, 854, 855, 856, 857 |
| 928 | 0.455265000 | 858 | ffffff3f00000000 | 858, 859, 860, 861, 862, 863, 864, 865, 866, 867, 868, 869, 870, 871, 872, 873, 874, 875, 876, 877, 878, 879, 880, 881, 882, 883, 884, 885, 886, 887 |
| 959 | 0.460509000 | 888 | ffffff3f00000000 | 888, 889, 890, 891, 892, 893, 894, 895, 896, 897, 898, 899, 900, 901, 902, 903, 904, 905, 906, 907, 908, 909, 910, 911, 912, 913, 914, 915, 916, 917 |
| 990 | 0.465753000 | 918 | ffffff3f00000000 | 918, 919, 920, 921, 922, 923, 924, 925, 926, 927, 928, 929, 930, 931, 932, 933, 934, 935, 936, 937, 938, 939, 940, 941, 942, 943, 944, 945, 946, 947 |
| 1021 | 0.470978000 | 948 | ffffff3f00000000 | 948, 949, 950, 951, 952, 953, 954, 955, 956, 957, 958, 959, 960, 961, 962, 963, 964, 965, 966, 967, 968, 969, 970, 971, 972, 973, 974, 975, 976, 977 |
| 1052 | 0.476182000 | 978 | ffffff3f00000000 | 978, 979, 980, 981, 982, 983, 984, 985, 986, 987, 988, 989, 990, 991, 992, 993, 994, 995, 996, 997, 998, 999, 1000, 1001, 1002, 1003, 1004, 1005, 1006, 1007 |
| 1083 | 0.481406000 | 1008 | ffffff3f00000000 | 1008, 1009, 1010, 1011, 1012, 1013, 1014, 1015, 1016, 1017, 1018, 1019, 1020, 1021, 1022, 1023, 1024, 1025, 1026, 1027, 1028, 1029, 1030, 1031, 1032, 1033, 1034, 1035, 1036, 1037 |
| 1114 | 0.486650000 | 1038 | ffffff3f00000000 | 1038, 1039, 1040, 1041, 1042, 1043, 1044, 1045, 1046, 1047, 1048, 1049, 1050, 1051, 1052, 1053, 1054, 1055, 1056, 1057, 1058, 1059, 1060, 1061, 1062, 1063, 1064, 1065, 1066, 1067 |
| 1145 | 0.491894000 | 1068 | ffffff3f00000000 | 1068, 1069, 1070, 1071, 1072, 1073, 1074, 1075, 1076, 1077, 1078, 1079, 1080, 1081, 1082, 1083, 1084, 1085, 1086, 1087, 1088, 1089, 1090, 1091, 1092, 1093, 1094, 1095, 1096, 1097 |
| 1176 | 0.497098000 | 1098 | ffffff3f00000000 | 1098, 1099, 1100, 1101, 1102, 1103, 1104, 1105, 1106, 1107, 1108, 1109, 1110, 1111, 1112, 1113, 1114, 1115, 1116, 1117, 1118, 1119, 1120, 1121, 1122, 1123, 1124, 1125, 1126, 1127 |
| 1207 | 0.502282000 | 1128 | ffffff3f00000000 | 1128, 1129, 1130, 1131, 1132, 1133, 1134, 1135, 1136, 1137, 1138, 1139, 1140, 1141, 1142, 1143, 1144, 1145, 1146, 1147, 1148, 1149, 1150, 1151, 1152, 1153, 1154, 1155, 1156, 1157 |
| 1238 | 0.507486000 | 1158 | ffffff3f00000000 | 1158, 1159, 1160, 1161, 1162, 1163, 1164, 1165, 1166, 1167, 1168, 1169, 1170, 1171, 1172, 1173, 1174, 1175, 1176, 1177, 1178, 1179, 1180, 1181, 1182, 1183, 1184, 1185, 1186, 1187 |
| 1269 | 0.512670000 | 1188 | ffffff3f00000000 | 1188, 1189, 1190, 1191, 1192, 1193, 1194, 1195, 1196, 1197, 1198, 1199, 1200, 1201, 1202, 1203, 1204, 1205, 1206, 1207, 1208, 1209, 1210, 1211, 1212, 1213, 1214, 1215, 1216, 1217 |
| 1300 | 0.517894000 | 1218 | ffffff3f00000000 | 1218, 1219, 1220, 1221, 1222, 1223, 1224, 1225, 1226, 1227, 1228, 1229, 1230, 1231, 1232, 1233, 1234, 1235, 1236, 1237, 1238, 1239, 1240, 1241, 1242, 1243, 1244, 1245, 1246, 1247 |
| 1331 | 0.523119000 | 1248 | ffffff3f00000000 | 1248, 1249, 1250, 1251, 1252, 1253, 1254, 1255, 1256, 1257, 1258, 1259, 1260, 1261, 1262, 1263, 1264, 1265, 1266, 1267, 1268, 1269, 1270, 1271, 1272, 1273, 1274, 1275, 1276, 1277 |
| 1362 | 0.528343000 | 1278 | ffffff3f00000000 | 1278, 1279, 1280, 1281, 1282, 1283, 1284, 1285, 1286, 1287, 1288, 1289, 1290, 1291, 1292, 1293, 1294, 1295, 1296, 1297, 1298, 1299, 1300, 1301, 1302, 1303, 1304, 1305, 1306, 1307 |
| 1393 | 0.533547000 | 1308 | ffffff3f00000000 | 1308, 1309, 1310, 1311, 1312, 1313, 1314, 1315, 1316, 1317, 1318, 1319, 1320, 1321, 1322, 1323, 1324, 1325, 1326, 1327, 1328, 1329, 1330, 1331, 1332, 1333, 1334, 1335, 1336, 1337 |
| 1424 | 0.538771000 | 1338 | ffffff3f00000000 | 1338, 1339, 1340, 1341, 1342, 1343, 1344, 1345, 1346, 1347, 1348, 1349, 1350, 1351, 1352, 1353, 1354, 1355, 1356, 1357, 1358, 1359, 1360, 1361, 1362, 1363, 1364, 1365, 1366, 1367 |
| 1455 | 0.543975000 | 1368 | ffffff3f00000000 | 1368, 1369, 1370, 1371, 1372, 1373, 1374, 1375, 1376, 1377, 1378, 1379, 1380, 1381, 1382, 1383, 1384, 1385, 1386, 1387, 1388, 1389, 1390, 1391, 1392, 1393, 1394, 1395, 1396, 1397 |
| 1486 | 0.549199000 | 1398 | ffffff3f00000000 | 1398, 1399, 1400, 1401, 1402, 1403, 1404, 1405, 1406, 1407, 1408, 1409, 1410, 1411, 1412, 1413, 1414, 1415, 1416, 1417, 1418, 1419, 1420, 1421, 1422, 1423, 1424, 1425, 1426, 1427 |
| 1517 | 0.554443000 | 1428 | ffffff3f00000000 | 1428, 1429, 1430, 1431, 1432, 1433, 1434, 1435, 1436, 1437, 1438, 1439, 1440, 1441, 1442, 1443, 1444, 1445, 1446, 1447, 1448, 1449, 1450, 1451, 1452, 1453, 1454, 1455, 1456, 1457 |
| 1548 | 0.559687000 | 1458 | ffffff3f00000000 | 1458, 1459, 1460, 1461, 1462, 1463, 1464, 1465, 1466, 1467, 1468, 1469, 1470, 1471, 1472, 1473, 1474, 1475, 1476, 1477, 1478, 1479, 1480, 1481, 1482, 1483, 1484, 1485, 1486, 1487 |
| 1579 | 0.564911000 | 1488 | ffffff3f00000000 | 1488, 1489, 1490, 1491, 1492, 1493, 1494, 1495, 1496, 1497, 1498, 1499, 1500, 1501, 1502, 1503, 1504, 1505, 1506, 1507, 1508, 1509, 1510, 1511, 1512, 1513, 1514, 1515, 1516, 1517 |
| 1610 | 0.570096000 | 1518 | ffffff3f00000000 | 1518, 1519, 1520, 1521, 1522, 1523, 1524, 1525, 1526, 1527, 1528, 1529, 1530, 1531, 1532, 1533, 1534, 1535, 1536, 1537, 1538, 1539, 1540, 1541, 1542, 1543, 1544, 1545, 1546, 1547 |
| 1641 | 0.575300000 | 1548 | ffffff3f00000000 | 1548, 1549, 1550, 1551, 1552, 1553, 1554, 1555, 1556, 1557, 1558, 1559, 1560, 1561, 1562, 1563, 1564, 1565, 1566, 1567, 1568, 1569, 1570, 1571, 1572, 1573, 1574, 1575, 1576, 1577 |
| 1672 | 0.580484000 | 1578 | ffffff3f00000000 | 1578, 1579, 1580, 1581, 1582, 1583, 1584, 1585, 1586, 1587, 1588, 1589, 1590, 1591, 1592, 1593, 1594, 1595, 1596, 1597, 1598, 1599, 1600, 1601, 1602, 1603, 1604, 1605, 1606, 1607 |
| 1703 | 0.585688000 | 1608 | ffffff3f00000000 | 1608, 1609, 1610, 1611, 1612, 1613, 1614, 1615, 1616, 1617, 1618, 1619, 1620, 1621, 1622, 1623, 1624, 1625, 1626, 1627, 1628, 1629, 1630, 1631, 1632, 1633, 1634, 1635, 1636, 1637 |
| 1734 | 0.590892000 | 1638 | ffffff3f00000000 | 1638, 1639, 1640, 1641, 1642, 1643, 1644, 1645, 1646, 1647, 1648, 1649, 1650, 1651, 1652, 1653, 1654, 1655, 1656, 1657, 1658, 1659, 1660, 1661, 1662, 1663, 1664, 1665, 1666, 1667 |
| 1765 | 0.596136000 | 1668 | ffffff3f00000000 | 1668, 1669, 1670, 1671, 1672, 1673, 1674, 1675, 1676, 1677, 1678, 1679, 1680, 1681, 1682, 1683, 1684, 1685, 1686, 1687, 1688, 1689, 1690, 1691, 1692, 1693, 1694, 1695, 1696, 1697 |
| 1796 | 0.601380000 | 1698 | ffffff3f00000000 | 1698, 1699, 1700, 1701, 1702, 1703, 1704, 1705, 1706, 1707, 1708, 1709, 1710, 1711, 1712, 1713, 1714, 1715, 1716, 1717, 1718, 1719, 1720, 1721, 1722, 1723, 1724, 1725, 1726, 1727 |
| 1827 | 0.606604000 | 1728 | ffffff3f00000000 | 1728, 1729, 1730, 1731, 1732, 1733, 1734, 1735, 1736, 1737, 1738, 1739, 1740, 1741, 1742, 1743, 1744, 1745, 1746, 1747, 1748, 1749, 1750, 1751, 1752, 1753, 1754, 1755, 1756, 1757 |
| 1858 | 0.611848000 | 1758 | ffffff3f00000000 | 1758, 1759, 1760, 1761, 1762, 1763, 1764, 1765, 1766, 1767, 1768, 1769, 1770, 1771, 1772, 1773, 1774, 1775, 1776, 1777, 1778, 1779, 1780, 1781, 1782, 1783, 1784, 1785, 1786, 1787 |
| 1889 | 0.617092000 | 1788 | ffffff3f00000000 | 1788, 1789, 1790, 1791, 1792, 1793, 1794, 1795, 1796, 1797, 1798, 1799, 1800, 1801, 1802, 1803, 1804, 1805, 1806, 1807, 1808, 1809, 1810, 1811, 1812, 1813, 1814, 1815, 1816, 1817 |
| 1920 | 0.622337000 | 1818 | ffffff3f00000000 | 1818, 1819, 1820, 1821, 1822, 1823, 1824, 1825, 1826, 1827, 1828, 1829, 1830, 1831, 1832, 1833, 1834, 1835, 1836, 1837, 1838, 1839, 1840, 1841, 1842, 1843, 1844, 1845, 1846, 1847 |
| 1951 | 0.627581000 | 1848 | ffffff3f00000000 | 1848, 1849, 1850, 1851, 1852, 1853, 1854, 1855, 1856, 1857, 1858, 1859, 1860, 1861, 1862, 1863, 1864, 1865, 1866, 1867, 1868, 1869, 1870, 1871, 1872, 1873, 1874, 1875, 1876, 1877 |
| 1982 | 0.632805000 | 1878 | ffffff3f00000000 | 1878, 1879, 1880, 1881, 1882, 1883, 1884, 1885, 1886, 1887, 1888, 1889, 1890, 1891, 1892, 1893, 1894, 1895, 1896, 1897, 1898, 1899, 1900, 1901, 1902, 1903, 1904, 1905, 1906, 1907 |
| 2013 | 0.638009000 | 1908 | ffffff3f00000000 | 1908, 1909, 1910, 1911, 1912, 1913, 1914, 1915, 1916, 1917, 1918, 1919, 1920, 1921, 1922, 1923, 1924, 1925, 1926, 1927, 1928, 1929, 1930, 1931, 1932, 1933, 1934, 1935, 1936, 1937 |
| 2044 | 0.643213000 | 1938 | ffffff3f00000000 | 1938, 1939, 1940, 1941, 1942, 1943, 1944, 1945, 1946, 1947, 1948, 1949, 1950, 1951, 1952, 1953, 1954, 1955, 1956, 1957, 1958, 1959, 1960, 1961, 1962, 1963, 1964, 1965, 1966, 1967 |
| 2075 | 0.648457000 | 1968 | ffffff3f00000000 | 1968, 1969, 1970, 1971, 1972, 1973, 1974, 1975, 1976, 1977, 1978, 1979, 1980, 1981, 1982, 1983, 1984, 1985, 1986, 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997 |
| 2106 | 0.653701000 | 1998 | ffffff3f00000000 | 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021, 2022, 2023, 2024, 2025, 2026, 2027 |
| 2137 | 0.658925000 | 2028 | ffffff3f00000000 | 2028, 2029, 2030, 2031, 2032, 2033, 2034, 2035, 2036, 2037, 2038, 2039, 2040, 2041, 2042, 2043, 2044, 2045, 2046, 2047, 2048, 2049, 2050, 2051, 2052, 2053, 2054, 2055, 2056, 2057 |
| 2168 | 0.664169000 | 2058 | ffffff3f00000000 | 2058, 2059, 2060, 2061, 2062, 2063, 2064, 2065, 2066, 2067, 2068, 2069, 2070, 2071, 2072, 2073, 2074, 2075, 2076, 2077, 2078, 2079, 2080, 2081, 2082, 2083, 2084, 2085, 2086, 2087 |
| 2199 | 0.669394000 | 2088 | ffffff3f00000000 | 2088, 2089, 2090, 2091, 2092, 2093, 2094, 2095, 2096, 2097, 2098, 2099, 2100, 2101, 2102, 2103, 2104, 2105, 2106, 2107, 2108, 2109, 2110, 2111, 2112, 2113, 2114, 2115, 2116, 2117 |
| 2230 | 0.674598000 | 2118 | ffffff3f00000000 | 2118, 2119, 2120, 2121, 2122, 2123, 2124, 2125, 2126, 2127, 2128, 2129, 2130, 2131, 2132, 2133, 2134, 2135, 2136, 2137, 2138, 2139, 2140, 2141, 2142, 2143, 2144, 2145, 2146, 2147 |
| 2261 | 0.679782000 | 2148 | ffffff3f00000000 | 2148, 2149, 2150, 2151, 2152, 2153, 2154, 2155, 2156, 2157, 2158, 2159, 2160, 2161, 2162, 2163, 2164, 2165, 2166, 2167, 2168, 2169, 2170, 2171, 2172, 2173, 2174, 2175, 2176, 2177 |
| 2292 | 0.685006000 | 2178 | ffffff3f00000000 | 2178, 2179, 2180, 2181, 2182, 2183, 2184, 2185, 2186, 2187, 2188, 2189, 2190, 2191, 2192, 2193, 2194, 2195, 2196, 2197, 2198, 2199, 2200, 2201, 2202, 2203, 2204, 2205, 2206, 2207 |
| 2323 | 0.690230000 | 2208 | ffffff3f00000000 | 2208, 2209, 2210, 2211, 2212, 2213, 2214, 2215, 2216, 2217, 2218, 2219, 2220, 2221, 2222, 2223, 2224, 2225, 2226, 2227, 2228, 2229, 2230, 2231, 2232, 2233, 2234, 2235, 2236, 2237 |
| 2354 | 0.695414000 | 2238 | ffffff3f00000000 | 2238, 2239, 2240, 2241, 2242, 2243, 2244, 2245, 2246, 2247, 2248, 2249, 2250, 2251, 2252, 2253, 2254, 2255, 2256, 2257, 2258, 2259, 2260, 2261, 2262, 2263, 2264, 2265, 2266, 2267 |
| 2385 | 0.700618000 | 2268 | ffffff3f00000000 | 2268, 2269, 2270, 2271, 2272, 2273, 2274, 2275, 2276, 2277, 2278, 2279, 2280, 2281, 2282, 2283, 2284, 2285, 2286, 2287, 2288, 2289, 2290, 2291, 2292, 2293, 2294, 2295, 2296, 2297 |
| 2416 | 0.705862000 | 2298 | ffffff3f00000000 | 2298, 2299, 2300, 2301, 2302, 2303, 2304, 2305, 2306, 2307, 2308, 2309, 2310, 2311, 2312, 2313, 2314, 2315, 2316, 2317, 2318, 2319, 2320, 2321, 2322, 2323, 2324, 2325, 2326, 2327 |
| 2447 | 0.711086000 | 2328 | ffffff3f00000000 | 2328, 2329, 2330, 2331, 2332, 2333, 2334, 2335, 2336, 2337, 2338, 2339, 2340, 2341, 2342, 2343, 2344, 2345, 2346, 2347, 2348, 2349, 2350, 2351, 2352, 2353, 2354, 2355, 2356, 2357 |
| 2478 | 0.716310000 | 2358 | ffffff3f00000000 | 2358, 2359, 2360, 2361, 2362, 2363, 2364, 2365, 2366, 2367, 2368, 2369, 2370, 2371, 2372, 2373, 2374, 2375, 2376, 2377, 2378, 2379, 2380, 2381, 2382, 2383, 2384, 2385, 2386, 2387 |
| 2509 | 0.721555000 | 2388 | ffffff3f00000000 | 2388, 2389, 2390, 2391, 2392, 2393, 2394, 2395, 2396, 2397, 2398, 2399, 2400, 2401, 2402, 2403, 2404, 2405, 2406, 2407, 2408, 2409, 2410, 2411, 2412, 2413, 2414, 2415, 2416, 2417 |
| 2540 | 0.726779000 | 2418 | ffffff3f00000000 | 2418, 2419, 2420, 2421, 2422, 2423, 2424, 2425, 2426, 2427, 2428, 2429, 2430, 2431, 2432, 2433, 2434, 2435, 2436, 2437, 2438, 2439, 2440, 2441, 2442, 2443, 2444, 2445, 2446, 2447 |
| 2571 | 0.732023000 | 2448 | ffffff3f00000000 | 2448, 2449, 2450, 2451, 2452, 2453, 2454, 2455, 2456, 2457, 2458, 2459, 2460, 2461, 2462, 2463, 2464, 2465, 2466, 2467, 2468, 2469, 2470, 2471, 2472, 2473, 2474, 2475, 2476, 2477 |
| 2602 | 0.737267000 | 2478 | ffffff3f00000000 | 2478, 2479, 2480, 2481, 2482, 2483, 2484, 2485, 2486, 2487, 2488, 2489, 2490, 2491, 2492, 2493, 2494, 2495, 2496, 2497, 2498, 2499, 2500, 2501, 2502, 2503, 2504, 2505, 2506, 2507 |
| 2633 | 0.742471000 | 2508 | ffffff3f00000000 | 2508, 2509, 2510, 2511, 2512, 2513, 2514, 2515, 2516, 2517, 2518, 2519, 2520, 2521, 2522, 2523, 2524, 2525, 2526, 2527, 2528, 2529, 2530, 2531, 2532, 2533, 2534, 2535, 2536, 2537 |
| 2664 | 0.747655000 | 2538 | ffffff3f00000000 | 2538, 2539, 2540, 2541, 2542, 2543, 2544, 2545, 2546, 2547, 2548, 2549, 2550, 2551, 2552, 2553, 2554, 2555, 2556, 2557, 2558, 2559, 2560, 2561, 2562, 2563, 2564, 2565, 2566, 2567 |
| 2695 | 0.752879000 | 2568 | ffffff3f00000000 | 2568, 2569, 2570, 2571, 2572, 2573, 2574, 2575, 2576, 2577, 2578, 2579, 2580, 2581, 2582, 2583, 2584, 2585, 2586, 2587, 2588, 2589, 2590, 2591, 2592, 2593, 2594, 2595, 2596, 2597 |
| 2726 | 0.758123000 | 2598 | ffffff3f00000000 | 2598, 2599, 2600, 2601, 2602, 2603, 2604, 2605, 2606, 2607, 2608, 2609, 2610, 2611, 2612, 2613, 2614, 2615, 2616, 2617, 2618, 2619, 2620, 2621, 2622, 2623, 2624, 2625, 2626, 2627 |
| 2757 | 0.763367000 | 2628 | ffffff3f00000000 | 2628, 2629, 2630, 2631, 2632, 2633, 2634, 2635, 2636, 2637, 2638, 2639, 2640, 2641, 2642, 2643, 2644, 2645, 2646, 2647, 2648, 2649, 2650, 2651, 2652, 2653, 2654, 2655, 2656, 2657 |
| 2788 | 0.768572000 | 2658 | ffffff3f00000000 | 2658, 2659, 2660, 2661, 2662, 2663, 2664, 2665, 2666, 2667, 2668, 2669, 2670, 2671, 2672, 2673, 2674, 2675, 2676, 2677, 2678, 2679, 2680, 2681, 2682, 2683, 2684, 2685, 2686, 2687 |
| 2819 | 0.773816000 | 2688 | ffffff3f00000000 | 2688, 2689, 2690, 2691, 2692, 2693, 2694, 2695, 2696, 2697, 2698, 2699, 2700, 2701, 2702, 2703, 2704, 2705, 2706, 2707, 2708, 2709, 2710, 2711, 2712, 2713, 2714, 2715, 2716, 2717 |
| 2850 | 0.779040000 | 2718 | ffffff3f00000000 | 2718, 2719, 2720, 2721, 2722, 2723, 2724, 2725, 2726, 2727, 2728, 2729, 2730, 2731, 2732, 2733, 2734, 2735, 2736, 2737, 2738, 2739, 2740, 2741, 2742, 2743, 2744, 2745, 2746, 2747 |
| 2881 | 0.784284000 | 2748 | ffffff3f00000000 | 2748, 2749, 2750, 2751, 2752, 2753, 2754, 2755, 2756, 2757, 2758, 2759, 2760, 2761, 2762, 2763, 2764, 2765, 2766, 2767, 2768, 2769, 2770, 2771, 2772, 2773, 2774, 2775, 2776, 2777 |
| 2912 | 0.789488000 | 2778 | ffffff3f00000000 | 2778, 2779, 2780, 2781, 2782, 2783, 2784, 2785, 2786, 2787, 2788, 2789, 2790, 2791, 2792, 2793, 2794, 2795, 2796, 2797, 2798, 2799, 2800, 2801, 2802, 2803, 2804, 2805, 2806, 2807 |
| 2943 | 0.794712000 | 2808 | ffffff3f00000000 | 2808, 2809, 2810, 2811, 2812, 2813, 2814, 2815, 2816, 2817, 2818, 2819, 2820, 2821, 2822, 2823, 2824, 2825, 2826, 2827, 2828, 2829, 2830, 2831, 2832, 2833, 2834, 2835, 2836, 2837 |
| 2974 | 0.799956000 | 2838 | ffffff3f00000000 | 2838, 2839, 2840, 2841, 2842, 2843, 2844, 2845, 2846, 2847, 2848, 2849, 2850, 2851, 2852, 2853, 2854, 2855, 2856, 2857, 2858, 2859, 2860, 2861, 2862, 2863, 2864, 2865, 2866, 2867 |
| 3005 | 0.805200000 | 2868 | ffffff3f00000000 | 2868, 2869, 2870, 2871, 2872, 2873, 2874, 2875, 2876, 2877, 2878, 2879, 2880, 2881, 2882, 2883, 2884, 2885, 2886, 2887, 2888, 2889, 2890, 2891, 2892, 2893, 2894, 2895, 2896, 2897 |
| 3036 | 0.810404000 | 2898 | ffffff3f00000000 | 2898, 2899, 2900, 2901, 2902, 2903, 2904, 2905, 2906, 2907, 2908, 2909, 2910, 2911, 2912, 2913, 2914, 2915, 2916, 2917, 2918, 2919, 2920, 2921, 2922, 2923, 2924, 2925, 2926, 2927 |
| 3067 | 0.815608000 | 2928 | ffffff3f00000000 | 2928, 2929, 2930, 2931, 2932, 2933, 2934, 2935, 2936, 2937, 2938, 2939, 2940, 2941, 2942, 2943, 2944, 2945, 2946, 2947, 2948, 2949, 2950, 2951, 2952, 2953, 2954, 2955, 2956, 2957 |
| 3098 | 0.820833000 | 2958 | ffffff3f00000000 | 2958, 2959, 2960, 2961, 2962, 2963, 2964, 2965, 2966, 2967, 2968, 2969, 2970, 2971, 2972, 2973, 2974, 2975, 2976, 2977, 2978, 2979, 2980, 2981, 2982, 2983, 2984, 2985, 2986, 2987 |

Showing the first 100 of 134 decoded HT Compressed Block Ack frames; the script-owned packet metrics JSON preserves every row.

### [script] Analysis of Packet Distribution
**PASS: decoded Multi-STA Block Ack fields.** UlMuMultiTidBlockAck: 278 BA Type 11 frame(s) with multiple AID/TID entries, UlSuMultiTidBlockAck: 13 BA Type 11 frame(s) with multiple AID/TID entries. The table above is direct TShark decoding of BA Control Type 11 and its per-AID/TID entries, as specified by IEEE Std 802.11-2024 Clause 9.3.1.8.6. It establishes the acknowledged recipient/TID identities in the captured frames, not payload delivery or end-to-end reliability.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->

## [agent] Frame exchange analysis

The generated timelines give the order observed in the representative capture.
The HE UL-SU timeline exposes TIDs 6 and 7; the UL-MU timeline shows the
Trigger, concurrent HE-TB responses, and following Type 11 Block Ack. The
decoded records identify the associated AID/TID contexts. This is mechanism
evidence from run 0, not a delivery measurement.

## [agent] Cross-layer findings and verdict

The published five-run delivery table passes for all three configurations.
The representative HE captures directly decode Type 11 Block Ack records with
multiple AID/TID contexts, and the HT PCAP check passes for Block Ack without
an on-air BAR. Together, these observations support the configured
acknowledgment mechanisms in their stated run-0 scope. The decoded fields do
not establish end-to-end delivery, and goodput does not identify the Block Ack
format; the verdict relies on both evidence types.

## [agent] Limitations and inconclusive claims

- The decoded Multi-STA Block Ack fields are representative run-0 evidence;
  they identify acknowledgment contexts but do not correlate each entry with a
  particular delivered application packet.
- The HT result establishes observed Block Ack and absent BAR control frames;
  it does not decode a compressed-BlockAck-specific field.
- PCAP is representative run-0 mechanism evidence; it does not make a
  population or seed-specific claim across the five scalar/vector runs.
