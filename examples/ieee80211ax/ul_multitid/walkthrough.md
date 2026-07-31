# Walkthrough: HE Multi-TID Block Ack

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260731T104140Z`
- PCAP: `20260731T104140Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `20260731T102044Z`.

The fresh five-run uplink session shows application delivery for two HE cases
and an HT implicit-BlockAck case. Its representative run-0 capture directly
decodes UL-MU Multi-STA Block Ack BA Type 11 entries and, for the HT case,
Block Ack observations with no on-air BAR. The Type 11 fields establish only
the UL-MU acknowledgment context.

## [agent] Learning objectives and feature primer

A traffic identifier (TID) distinguishes QoS traffic streams. An ack-enabled
multi-TID A-MPDU can carry frames from several TIDs; its receiver must respond
in a way that identifies the acknowledged contexts. In an uplink multi-user
exchange, the AP first sends a Trigger and then acknowledges station responses
with a Multi-STA Block Ack. HT can instead use an implicit Block Ack exchange
after an A-MPDU. The learning question is whether the configured HE UL-SU,
HE UL-MU, and HT UL-SU cases deliver their offered traffic while exposing their
respective acknowledgment evidence.

## [agent] Scenario description

The scenario is [omnetpp.ini](omnetpp.ini), using a stationary 5 GHz, 20 MHz
HE basic service set: stations send UDP to the wired server through the AP.
`UlSuMultiTidBlockAck` runs two offered flows from `host[0]` with UL MU
disabled. `UlMuMultiTidBlockAck` enables the backlog-based UL scheduler and
offers one flow from each of `host[0]` and `host[1]`; `host[2]` is idle. Both
start their traffic at 0.3 s in a 1 s simulation. The HT
`UlSUHTAMpduCompressedBlockAck` condition inherits the UL-SU topology, selects
mixed 2.4 GHz HT mode, uses one flow, and enables HT implicit Block Ack. The
shared analysis uses the `[0.3,0.88)` s traffic window.

## [agent] Standards and INET model boundary

IEEE Std 802.11-2024 §26.6.3.4 specifies ack-enabled multi-TID A-MPDUs and
requires peer HE capability before their transmission
(`80211ax-2024:chunk:09842`). Section 9.3.1.8.6 defines the Multi-STA Block
Ack and its repeated per-AID/TID information
(`80211ax-2024:chunk:01597`). The standard is the normative definition; this
INET example requests its relevant TX/RX capability with
`heMultiTidAggregation{Tx,Rx}`. The capture now directly decodes QoS TIDs,
Trigger, BA Control Type 11, and the repeated Multi-STA per-AID/TID
information.

## [agent] Evidence status

| Claim | Status | Script-generated evidence | Runs | Scope or gap |
|---|---|---|---|---|
| All three uplink configurations deliver application traffic | `PASS` | scalar/vector goodput table | 0–4 | delivery outcome, not BA contents |
| UL-SU carries the two offered QoS TIDs | `PASS` | representative frame timeline | 0 | TIDs 6 and 7 are directly decoded |
| UL-MU has Trigger and Block Ack exchanges | `PASS` | representative frame timeline and PCAP table | 0 | observations are not de-duplicated transmissions |
| A UL-MU response acknowledges multiple AID/TID contexts | `PASS` | decoded BA Type 11 table and executable evidence check | 0 | direct frame evidence; not an end-to-end delivery claim |
| HT implicit Block Ack has Block Ack observations and no on-air BAR | `PASS` | PCAP evidence check | 0 | control-observation evidence, not a decoded compressed-BA field |

The goodput rows are derived measurements; the timeline fields are direct
observations; the Multi-STA Block Ack table is direct frame-field evidence.

## [agent] Configuration matrix

| Configuration | Role | Causal delta | Runs | Expected invariant |
|---|---|---|---|---|
| `UlSuMultiTidBlockAck` | UL-SU | UL MU OFDMA disabled; two ports/TIDs at one station | 0–4 | two TIDs are offered by one transmitter |
| `UlMuMultiTidBlockAck` | UL-MU | scheduler and UL MU OFDMA enabled; one port/TID at each of two stations | 0–4 | Trigger precedes scheduled station responses |
| `UlSUHTAMpduCompressedBlockAck` | HT UL-SU | mixed 2.4 GHz HT mode, one flow, implicit Block Ack enabled | 0–4 | Block Ack observations occur without an on-air BAR |

Both configurations enable HE Multi-TID aggregation transmit and receive
capability at all listed AP and station WLAN interfaces.

## [agent] Expected invariants and diagnostic map

| Invariant | Script-generated evidence | Failure symptom | First diagnostic |
|---|---|---|---|
| UL-SU exposes both offered TIDs | UL-SU frame timeline | only one decoded TID | inspect the QoS classifier and per-AC queues |
| UL-MU schedules a response | UL-MU Trigger/response timeline | no Trigger or HE-TB response | inspect the UL scheduler decision |
| UL-MU Block Ack has multiple AID/TID contexts | decoded BA Type 11 table | no Type 11 record or fewer than two entries | inspect the MAC header serializer and capture decoder |
| HT implicit Block Ack avoids an on-air BAR | HT PCAP evidence check | BAR observed or no Block Ack observed | inspect the HT block-ack policy and control-frame serialization |

## [agent] Reproduction

Run from the INET repository root. The fresh session is publication-ready:
five scalar/vector runs (`[0,5)`) and representative run-0 PCAP evidence. The
following `inspect` and `report` commands completed with exit status 0 during
this revision; `publish` refreshes the generated blocks.

The retained PCAP is run 0 with seed set 0. The suite does not publish a seed
mapping for scalar/vector runs 1–4, so this walkthrough makes no seed claim
for them. Session inputs include
[`UlMuMultiTidBlockAck-#0.sca`](results/20260731T102044Z/UlMuMultiTidBlockAck/UlMuMultiTidBlockAck-#0.sca)
and
[`UlSUHTAMpduCompressedBlockAck-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`](results/20260731T102044Z/UlSUHTAMpduCompressedBlockAck/UlSUHTAMpduCompressedBlockAck-#0Lan80211AxUlOfdma.ap.wlan[0].pcap).

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py inspect ul_multitid \
  --session-id 20260731T102044Z
python3 examples/ieee80211/analysis/wifi_analysis.py report ul_multitid \
  --session-id 20260731T102044Z
python3 examples/ieee80211/analysis/wifi_analysis.py publish ul_multitid \
  --session-id 20260731T102044Z --update
```

## [agent] Scalar and vector analysis

The generated five-run table answers whether offered uplink traffic reaches
the receiver over the shared measurement window. All three configurations have
small reported 95% CI half-widths in this deterministic session. This is useful
outcome evidence. Goodput itself does not identify the Block Ack variant or the
TIDs it covers; those claims come from the companion PCAP evidence.

<!-- BEGIN GENERATED: ieee80211-scalar-vector-multi_tid -->
### [script] Generated scalar/vector plot and table

![multi_tid scalar/vector analysis](results/20260731T104140Z/multi-tid-delivery.png)

Figure provenance: [`results/20260731T104140Z/multi-tid-delivery.png.json`](results/20260731T104140Z/multi-tid-delivery.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)
- Window / per-run aggregation / exclusions: [0.3, 0.88) s; observation=one aggregate goodput value per run; uncertainty=95% Student-t CI
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| Uplink MU Multi-TID BA / goodput mbps | 3.2 | 0 |
| Uplink SU HT A-MPDU compressed BA / goodput mbps | 39.9669 | 0.011165 |
| Uplink SU Multi-TID BA / goodput mbps | 1.76 | 0 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **INCONCLUSIVE** | Acknowledgment exchanges identify the TIDs covered by each Multi-STA Block Ack | Application delivery is retained for the five-run comparison, while decisive per-TID Block Ack fields remain PCAP evidence. |
<!-- END GENERATED: ieee80211-scalar-vector-multi_tid -->

## [agent] PCAP statistics

The script analyzes AP `wlan[0]` PCAPng from representative run 0. Its counts
are capture observations, not de-duplicated transmissions or delivered
packets. Read the UL-MU Trigger/HE-TB/Block Ack pattern and the HE UL-SU
BAR/Block Ack rows as exchange evidence. The HT condition instead has Block Ack
observations and no BAR. The dedicated decoded Multi-STA Block Ack table then
supplies the BA Type 11 and per-AID/TID fields for the UL-MU claim.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260731T104140Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260731T104140Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260731T104140Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260731T104140Z.json` (SHA-256 `ee48d96c67abc0f68531c4b45a4062b1b36e2b711ce4e15479e7b8baaa1bf27d`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `UlMuMultiTidBlockAck` | `none (all decoded frames)` | 1231 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (292), Control: Ack (280), Control: Trigger (279) | 22.24% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UlSuMultiTidBlockAck` | `none (all decoded frames)` | 381 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (210), Control: Ack (141), Control: Block Ack Request (BAR) (13) | 10.44% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UlSUHTAMpduCompressedBlockAck` | `none (all decoded frames)` | 8724 | Data: QoS Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC, A-MPDU] (6986), Control: Block Ack (BA) [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC] (1716), Control: Ack [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC] (11) | 79.30% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | UlMuMultiTidBlockAck produced protocol-visible wireless observations | 1231 AP/global transmission observations |
| **PASS** | UlSUHTAMpduCompressedBlockAck produced protocol-visible wireless observations | 8724 AP/global transmission observations |
| **PASS** | UlSuMultiTidBlockAck produced protocol-visible wireless observations | 381 AP/global transmission observations |
| **PASS** | BA Type 11 and per-AID/TID entries are decoded from Multi-STA Block Ack frames | UlMuMultiTidBlockAck: 278 BA Type 11 frame(s) with multiple AID/TID entries, UlSuMultiTidBlockAck: 13 BA Type 11 frame(s) with multiple AID/TID entries |
| **PASS** | HT implicit Block Ack has Block Ack observations and no on-air BAR | UlSUHTAMpduCompressedBlockAck: 1716 Block Ack observation(s), 0 BAR observation(s) |

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
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **8724**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#13aa1d" /></svg> | Data: QoS Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC, A-MPDU] | 6986 | 80.08% | 567.0 B | 1.7 B | 105.8 us | 0.2 us | 5010 MHz | -60.0 dBm | - | 93.19% | 73.90% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#3ad440" /></svg> | Data: QoS Data [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC] | 9 | 0.10% | 570.0 B | 0.0 B | 106.2 us | 0.0 us | 5010 MHz | -60.0 dBm | - | 0.12% | 0.10% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1537a8" /></svg> | Control: Block Ack (BA) [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC] | 1716 | 19.67% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 6.64% | 5.26% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#71a9ea" /></svg> | Control: Ack [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC] | 11 | 0.13% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -60.0 dBm | 10.0 dBm | 0.03% | 0.03% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#e9453f" /></svg> | Management: Action [HT, HT-MCS 7, 20 MHz, GI 0.8 us, BCC] | 2 | 0.02% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -60.0 dBm | 10.0 dBm | 0.02% | 0.01% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.300108000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / Legacy/HT/VHT | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 2 | 0.300158000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 3 | 0.300212000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Management: Action / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=0, frag=0, more-frag=0, TID=- | Provides frame-order context for the representative exchange. |
| 4 | 0.300262000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 5 | 0.300380000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / Legacy/HT/VHT | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 6 | 0.300430000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 7 | 0.300548000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / Legacy/HT/VHT | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 8 | 0.300598000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 9 | 0.300716000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / Legacy/HT/VHT | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 10 | 0.300766000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 11 | 0.300884000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / Legacy/HT/VHT | direction=to DS, retry=0, seq=4, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 12 | 0.300934000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 13 | 0.301052000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / Legacy/HT/VHT | direction=to DS, retry=0, seq=5, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 14 | 0.301102000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 15 | 0.301220000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / Legacy/HT/VHT | direction=to DS, retry=0, seq=6, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 16 | 0.301270000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |

Frame numbers are local to capture `UlSUHTAMpduCompressedBlockAck-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded HT Compressed Block Ack records

| Frame | Simulation time (s) | Starting sequence | Bitmap | Acknowledged MPDU sequence numbers |
|---:|---:|---:|---|---|
| 33 | 0.302682000 | 9 | ff03000000000000 | 9, 10, 11, 12, 13, 14, 15, 16, 17, 18 |
| 42 | 0.303347000 | 19 | ff00000000000000 | 19, 20, 21, 22, 23, 24, 25, 26 |
| 50 | 0.303943000 | 27 | 7f00000000000000 | 27, 28, 29, 30, 31, 32, 33 |
| 58 | 0.304639000 | 34 | 7f00000000000000 | 34, 35, 36, 37, 38, 39, 40 |
| 65 | 0.305163000 | 41 | 3f00000000000000 | 41, 42, 43, 44, 45, 46 |
| 71 | 0.305619000 | 47 | 1f00000000000000 | 47, 48, 49, 50, 51 |
| 77 | 0.306115000 | 52 | 1f00000000000000 | 52, 53, 54, 55, 56 |
| 83 | 0.306571000 | 57 | 1f00000000000000 | 57, 58, 59, 60, 61 |
| 88 | 0.306955000 | 62 | 0f00000000000000 | 62, 63, 64, 65 |
| 93 | 0.307339000 | 66 | 0f00000000000000 | 66, 67, 68, 69 |
| 99 | 0.307876000 | 70 | 1f00000000000000 | 70, 71, 72, 73, 74 |
| 104 | 0.308260000 | 75 | 0f00000000000000 | 75, 76, 77, 78 |
| 109 | 0.308644000 | 79 | 0f00000000000000 | 79, 80, 81, 82 |
| 114 | 0.309028000 | 83 | 0f00000000000000 | 83, 84, 85, 86 |
| 120 | 0.309584000 | 87 | 1f00000000000000 | 87, 88, 89, 90, 91 |
| 125 | 0.309968000 | 92 | 0f00000000000000 | 92, 93, 94, 95 |
| 130 | 0.310352000 | 96 | 0f00000000000000 | 96, 97, 98, 99 |
| 135 | 0.310736000 | 100 | 0f00000000000000 | 100, 101, 102, 103 |
| 141 | 0.311252000 | 104 | 1f00000000000000 | 104, 105, 106, 107, 108 |
| 146 | 0.311636000 | 109 | 0f00000000000000 | 109, 110, 111, 112 |
| 151 | 0.312021000 | 113 | 0f00000000000000 | 113, 114, 115, 116 |
| 156 | 0.312405000 | 117 | 0f00000000000000 | 117, 118, 119, 120 |
| 162 | 0.312961000 | 121 | 1f00000000000000 | 121, 122, 123, 124, 125 |
| 167 | 0.313345000 | 126 | 0f00000000000000 | 126, 127, 128, 129 |
| 172 | 0.313729000 | 130 | 0f00000000000000 | 130, 131, 132, 133 |
| 177 | 0.314113000 | 134 | 0f00000000000000 | 134, 135, 136, 137 |
| 183 | 0.314669000 | 138 | 1f00000000000000 | 138, 139, 140, 141, 142 |
| 188 | 0.315053000 | 143 | 0f00000000000000 | 143, 144, 145, 146 |
| 193 | 0.315437000 | 147 | 0f00000000000000 | 147, 148, 149, 150 |
| 198 | 0.315822000 | 151 | 0f00000000000000 | 151, 152, 153, 154 |
| 204 | 0.316378000 | 155 | 1f00000000000000 | 155, 156, 157, 158, 159 |
| 209 | 0.316762000 | 160 | 0f00000000000000 | 160, 161, 162, 163 |
| 214 | 0.317146000 | 164 | 0f00000000000000 | 164, 165, 166, 167 |
| 219 | 0.317530000 | 168 | 0f00000000000000 | 168, 169, 170, 171 |
| 225 | 0.318046000 | 172 | 1f00000000000000 | 172, 173, 174, 175, 176 |
| 230 | 0.318430000 | 177 | 0f00000000000000 | 177, 178, 179, 180 |
| 235 | 0.318814000 | 181 | 0f00000000000000 | 181, 182, 183, 184 |
| 240 | 0.319198000 | 185 | 0f00000000000000 | 185, 186, 187, 188 |
| 246 | 0.319754000 | 189 | 1f00000000000000 | 189, 190, 191, 192, 193 |
| 251 | 0.320139000 | 194 | 0f00000000000000 | 194, 195, 196, 197 |
| 256 | 0.320523000 | 198 | 0f00000000000000 | 198, 199, 200, 201 |
| 261 | 0.320907000 | 202 | 0f00000000000000 | 202, 203, 204, 205 |
| 266 | 0.321351000 | 206 | 0f00000000000000 | 206, 207, 208, 209 |
| 271 | 0.321735000 | 210 | 0f00000000000000 | 210, 211, 212, 213 |
| 276 | 0.322119000 | 214 | 0f00000000000000 | 214, 215, 216, 217 |
| 281 | 0.322503000 | 218 | 0f00000000000000 | 218, 219, 220, 221 |
| 286 | 0.322967000 | 222 | 0f00000000000000 | 222, 223, 224, 225 |
| 291 | 0.323351000 | 226 | 0f00000000000000 | 226, 227, 228, 229 |
| 296 | 0.323736000 | 230 | 0f00000000000000 | 230, 231, 232, 233 |
| 301 | 0.324120000 | 234 | 0f00000000000000 | 234, 235, 236, 237 |
| 306 | 0.324564000 | 238 | 0f00000000000000 | 238, 239, 240, 241 |
| 311 | 0.324948000 | 242 | 0f00000000000000 | 242, 243, 244, 245 |
| 316 | 0.325332000 | 246 | 0f00000000000000 | 246, 247, 248, 249 |
| 321 | 0.325716000 | 250 | 0f00000000000000 | 250, 251, 252, 253 |
| 327 | 0.326252000 | 254 | 1f00000000000000 | 254, 255, 256, 257, 258 |
| 332 | 0.326636000 | 259 | 0f00000000000000 | 259, 260, 261, 262 |
| 337 | 0.327020000 | 263 | 0f00000000000000 | 263, 264, 265, 266 |
| 342 | 0.327404000 | 267 | 0f00000000000000 | 267, 268, 269, 270 |
| 348 | 0.327961000 | 271 | 1f00000000000000 | 271, 272, 273, 274, 275 |
| 353 | 0.328345000 | 276 | 0f00000000000000 | 276, 277, 278, 279 |
| 358 | 0.328729000 | 280 | 0f00000000000000 | 280, 281, 282, 283 |
| 363 | 0.329113000 | 284 | 0f00000000000000 | 284, 285, 286, 287 |
| 369 | 0.329649000 | 288 | 1f00000000000000 | 288, 289, 290, 291, 292 |
| 374 | 0.330033000 | 293 | 0f00000000000000 | 293, 294, 295, 296 |
| 379 | 0.330417000 | 297 | 0f00000000000000 | 297, 298, 299, 300 |
| 384 | 0.330801000 | 301 | 0f00000000000000 | 301, 302, 303, 304 |
| 389 | 0.331225000 | 305 | 0f00000000000000 | 305, 306, 307, 308 |
| 394 | 0.331610000 | 309 | 0f00000000000000 | 309, 310, 311, 312 |
| 399 | 0.331994000 | 313 | 0f00000000000000 | 313, 314, 315, 316 |
| 403 | 0.332306000 | 317 | 0700000000000000 | 317, 318, 319 |
| 408 | 0.332690000 | 320 | 0f00000000000000 | 320, 321, 322, 323 |
| 413 | 0.333114000 | 324 | 0f00000000000000 | 324, 325, 326, 327 |
| 418 | 0.333498000 | 328 | 0f00000000000000 | 328, 329, 330, 331 |
| 422 | 0.333810000 | 332 | 0700000000000000 | 332, 333, 334 |
| 427 | 0.334194000 | 335 | 0f00000000000000 | 335, 336, 337, 338 |
| 431 | 0.334506000 | 339 | 0700000000000000 | 339, 340, 341 |
| 436 | 0.334950000 | 342 | 0f00000000000000 | 342, 343, 344, 345 |
| 441 | 0.335335000 | 346 | 0f00000000000000 | 346, 347, 348, 349 |
| 446 | 0.335719000 | 350 | 0f00000000000000 | 350, 351, 352, 353 |
| 451 | 0.336103000 | 354 | 0f00000000000000 | 354, 355, 356, 357 |
| 456 | 0.336567000 | 358 | 0f00000000000000 | 358, 359, 360, 361 |
| 461 | 0.336951000 | 362 | 0f00000000000000 | 362, 363, 364, 365 |
| 466 | 0.337335000 | 366 | 0f00000000000000 | 366, 367, 368, 369 |
| 471 | 0.337719000 | 370 | 0f00000000000000 | 370, 371, 372, 373 |
| 476 | 0.338143000 | 374 | 0f00000000000000 | 374, 375, 376, 377 |
| 481 | 0.338527000 | 378 | 0f00000000000000 | 378, 379, 380, 381 |
| 486 | 0.338912000 | 382 | 0f00000000000000 | 382, 383, 384, 385 |
| 491 | 0.339296000 | 386 | 0f00000000000000 | 386, 387, 388, 389 |
| 496 | 0.339720000 | 390 | 0f00000000000000 | 390, 391, 392, 393 |
| 501 | 0.340104000 | 394 | 0f00000000000000 | 394, 395, 396, 397 |
| 506 | 0.340488000 | 398 | 0f00000000000000 | 398, 399, 400, 401 |
| 510 | 0.340800000 | 402 | 0700000000000000 | 402, 403, 404 |
| 515 | 0.341184000 | 405 | 0f00000000000000 | 405, 406, 407, 408 |
| 520 | 0.341608000 | 409 | 0f00000000000000 | 409, 410, 411, 412 |
| 525 | 0.341992000 | 413 | 0f00000000000000 | 413, 414, 415, 416 |
| 529 | 0.342304000 | 417 | 0700000000000000 | 417, 418, 419 |
| 534 | 0.342689000 | 420 | 0f00000000000000 | 420, 421, 422, 423 |
| 538 | 0.343001000 | 424 | 0700000000000000 | 424, 425, 426 |
| 543 | 0.343425000 | 427 | 0f00000000000000 | 427, 428, 429, 430 |
| 548 | 0.343809000 | 431 | 0f00000000000000 | 431, 432, 433, 434 |

Showing the first 100 of 1716 decoded HT Compressed Block Ack frames; the script-owned packet metrics JSON preserves every row.

### [script] Analysis of Packet Distribution
**PASS: decoded Multi-STA Block Ack fields.** UlMuMultiTidBlockAck: 278 BA Type 11 frame(s) with multiple AID/TID entries, UlSuMultiTidBlockAck: 13 BA Type 11 frame(s) with multiple AID/TID entries. The table above is direct TShark decoding of BA Control Type 11 and its per-AID/TID entries, as specified by IEEE Std 802.11-2024 Clause 9.3.1.8.6. It establishes the acknowledged recipient/TID identities in the captured frames, not payload delivery or end-to-end reliability.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->

## [agent] Frame exchange analysis

The generated timelines give the causal order available in the capture. In
UL-SU, QoS Data frames for TIDs 6 and 7 are directly visible before later BAR
and Block Ack observations. In UL-MU, a Trigger precedes simultaneous HE-TB
responses and a Type 11 Multi-STA Block Ack. The adjacent decoded-fields table
shows the response's AID/TID entries; the timeline remains representative
run-0 evidence rather than a delivery measurement.

## [agent] Cross-layer findings and verdict

The configuration requests Multi-TID capability, five independent runs retain
application delivery, and the representative captures show the expected
uplink control/data patterns. The UL-MU capture additionally contains 278
decoded BA Type 11 frames with multiple AID/TID entries, resolving the
Multi-STA acknowledgment-context claim for that configuration. The HT capture
has 1,716 Block Ack observations and zero BAR observations, supporting its
configured implicit Block Ack exchange. These fields and counts do not prove
successful application delivery.

## [agent] Limitations and inconclusive claims

- The decoded Multi-STA Block Ack fields are direct representative run-0
  evidence for `UlMuMultiTidBlockAck`; they do not establish an analogous
  Multi-STA form for the UL-SU control configuration.
- The HT result establishes observed Block Ack and absent BAR control frames;
  it does not decode a compressed-BlockAck-specific field.
- PCAP is representative run-0 mechanism evidence; it does not make a
  population claim across the five scalar/vector runs.
