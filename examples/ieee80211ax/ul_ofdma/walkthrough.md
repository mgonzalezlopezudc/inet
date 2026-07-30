# Walkthrough: IEEE 802.11ax scheduled uplink OFDMA

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260730T172503Z`
- PCAP: `20260730T172503Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `20260729T231416Z`.

This four-configuration experiment tests the mechanism and its application
outcomes in one bounded campaign. The three scheduled configurations show
Trigger-led HE-TB exchanges; the EDCA control remains station-contended
HE-SU. The result supports the exchange distinction and the allocation join,
but does not establish a generally best scheduler.

## [agent] Learning objectives and feature primer

The learning question is: does enabling UL-MU-OFDMA change the observed MAC
exchange, and what does that change mean for delivery and tail delay in this
workload?

Uplink OFDMA lets the AP divide a channel into resource units (RUs). The AP
wins channel access and sends a Basic Trigger whose User Info fields identify
scheduled stations and their RU/PHY parameters. Qualifying stations answer
with aligned HE trigger-based (HE-TB) PPDUs one SIFS later; a response may be
QoS Data or QoS Null. INET completes the modeled exchange with a delayed
Multi-STA Block Ack. Ordinary EDCA instead produces station-originated HE-SU
traffic without the Trigger prefix.

## [agent] Scenario description

The [network](Lan80211AxUlOfdma.ned) extends
[`HeSingleBssNetwork`](../common/HeSingleBssNetwork.ned) with three stationary
hosts sending UDP traffic through the AP to `server.app[0]`:

```text
host[0] --\
host[1] ---- 5 GHz / 20 MHz --> AP === Ethernet === server
host[2] --/                    ^
                               +-- Basic Trigger / HE-TB responses
```

The [INI file](omnetpp.ini) fixes the radios to AX 20 MHz, HE MCS 1/NSS 1,
zero random-access RUs, Block Ack, and MPDU aggregation. The common measured
workload sends 100-byte packets every 0.5 ms in `[0.3,0.95)` s. The
asymmetric case deliberately changes packet size, station intervals, and
measurement window, so it is a stress case rather than a clean scheduler
comparison.

The baseline SU EDCA values are `2/3/7`; the scheduled configurations use
explicit MU-EDCA values `10/255/1023` with a 2.089 s timer. These are
configuration inputs. The PCAP and aligned result evidence establish which
exchange and allocations actually occurred.

## [agent] Standards and INET model boundary

IEEE Std 802.11-2024 defines the Basic Trigger format in Clause 9.3.1.22.2
and UL-MU solicitation in Clause 26.5.2.2. Clauses 26.5.2.2.4 and 26.5.2.3.1
specify the allocation fields and SIFS response conditions; Clause 26.5.2.4
permits QoS Null when no pending frame fits. These rules define the expected
exchange, not INET's scheduler policy.

INET implements the path with `HeHcf`, an UL coordinator, and replaceable
`HeUlScheduler*` policies. IEEE defines legal fields, response conditions, and
timing; it does not define INET's backlog score, allocation order, or
tie-breaking.

The PCAP is AP `wlan[0]` observation evidence. It counts transmission
observations, not unique deliveries; A-MPDU expansion and parallel RU
observations affect totals. Goodput and delay are derived application
outcomes, while the joined Trigger table is the evidence for per-user
allocations.

## [agent] Evidence status

The suite and INI statements below are configuration input evidence; decoded
PCAP fields and result records are direct observations; goodput and delay are
derived measurements from those records; scheduler explanations are
inferences unless aligned telemetry establishes them.

| Claim | Status | Script-generated evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| Scheduled configurations use a Trigger-led UL-MU exchange while the EDCA control does not | `PASS` | PCAP statistics and representative timelines | run 0 / seed 0 per configuration | Direct AP-capture observation in this session |
| All four configurations record delivered goodput and p95 delay | `PASS` | Scalar/vector plot and table | runs 0–4 / seeds 0–4 | Per-run aggregation over the stated windows |
| One scheduler is generally better than the others | `INCONCLUSIVE` | Scalar/vector plot and table | runs 0–4 / seeds 0–4 | No acceptance threshold; one scenario and five seeds |
| Each committed Basic Trigger per-user allocation matches the transmitted Trigger User Info | `PASS` | Joined scheduler decision vectors and decoded Trigger AID/RU fields | matched run 0 / seed 0 in the three scheduled configurations | 756 per-user allocations matched one-to-one |

## [agent] Configuration matrix

| Configuration | Role | Effective scheduler / UL mode | Contention delta | Workload | Expected invariant |
|---|---|---|---|---|---|
| `EdcaBaseline` | Negative control | UL MU-OFDMA disabled; HE-SU EDCA | SU `2/3/7`; MU settings inactive for the disabled trigger path | matched | no Basic Trigger or HE-TB response |
| `EqualSizedRUs` | Scheduled policy control | `HeUlSchedulerEqualSizedRUs`; UL MU-OFDMA enabled | same | matched | Trigger-led exchange with equal-sized RU policy |
| `BacklogBased` | Scheduled treatment | `HeUlSchedulerBacklogBased`; UL MU-OFDMA enabled | SU `2/3/7`; MU `10/255/1023`, 2.089 s | 3 × 100 B every 0.5 ms | Trigger-led HE-TB exchange |
| `AsymmetricBacklog` | Multi-factor stress case | inherits `BacklogBased`; UL MU-OFDMA enabled | SU `10/255/1023`; MU `10/255/1023`, 2.089 s | both apps 256 B; measured app every 1/2/4 ms | unequal backlog inputs remain observable |

All four configurations use AX 20 MHz, fixed HE MCS 1/NSS 1, three stations,
zero random-access RUs, and the same topology. `BacklogBased`,
`EqualSizedRUs`, and `EdcaBaseline` share the measured workload. The
asymmetric condition also changes payload size, offered load, SU contention,
and measurement duration, so it is not an isolated scheduler comparison.

## [agent] Expected invariants and diagnostic map

| Invariant | Script-generated evidence | Failure symptom | First diagnostic |
|---|---|---|---|
| Scheduled cases show Trigger followed by HE-TB responses | Representative PCAP timelines | Trigger or solicited response is absent | inspect effective `enableUlMuOfdma` and AP scheduler initialization |
| EDCA remains non-triggered | EDCA PCAP statistics and timeline | Trigger or HE-TB appears in the control | inspect INI precedence and selected MAC type |
| Delivery metrics use only sink packets in the measurement window | Scalar/vector provenance | empty or off-path vectors | inspect recorded module paths, vector units, and window bounds |
| Committed per-user allocations match transmitted Trigger fields | Executable joined-allocation check | count, order, AID, RU size, or RU offset differs | inspect the decision projection vectors, AP capture, and join diagnostics |

## [agent] Reproduction

Run from the INET repository root. First inspect the suite-owned options:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py inspect ul_ofdma

MPLCONFIGDIR=/tmp/matplotlib \
python3 examples/ieee80211/analysis/wifi_analysis.py run ul_ofdma \
  --suite ax --evidence both --runs 5 --jobs "$(nproc)" \
  --session-id 20260729T231416Z
```

The session contains runs `[0,5)` for each of the four configurations. Run 0
also records the AP `wlan[0]` PCAP from the same simulation trajectory. Report
and publish that retained session with:

```sh
MPLCONFIGDIR=/tmp/matplotlib \
python3 examples/ieee80211/analysis/wifi_analysis.py report ul_ofdma \
  --suite ax --session-id 20260729T231416Z

MPLCONFIGDIR=/tmp/matplotlib \
python3 examples/ieee80211/analysis/wifi_analysis.py publish ul_ofdma \
  --suite ax --session-id 20260729T231416Z --update
```

## [agent] Scalar and vector analysis

The native OMNeT++ result API queried one `packetReceived:vector(packetBytes)`
and one `endToEndDelay:vector` result per condition and run. Sink modules were
selected with `\.server\.app\[`. For each run, delivered bytes and delay
samples were cropped to the condition's measurement window. Goodput is the
sum of delivered bytes × 8 divided by the window duration. Delay is the pooled
95th percentile of delivered-packet delay samples within that run. The five
run-level values per condition are summarized with a two-sided 95% Student-t
interval; vector samples are not treated as independent repetitions.

The retained inputs are the `.sca` and `.vec` files under
`results/20260729T231416Z/{configuration}/` for runs 0–4.

The figure answers two separate questions: how much application traffic
reached the sink and what p95 delay the delivered packets experienced. In this
campaign, EDCA is the strongest application control, while the scheduled
cases establish the UL-MU mechanism rather than a delivery or delay advantage.
The uncertainty and the asymmetric workload also prevent a general scheduler
ranking.

<!-- BEGIN GENERATED: ieee80211-scalar-vector-ul_ofdma -->
### [script] Generated scalar/vector plot and table

![ul_ofdma scalar/vector analysis](results/20260730T172503Z/ul-ofdma-delivery-delay.png)

Figure provenance: [`results/20260730T172503Z/ul-ofdma-delivery-delay.png.json`](results/20260730T172503Z/ul-ofdma-delivery-delay.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)<br>vector / **.app[*] / endToEndDelay:vector / unit=s
- Window / per-run aggregation / exclusions: [0.3, 0.8) s, [0.3, 0.95) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| EDCA baseline / delay p95 ms | 26.5 | 1.17524 |
| EDCA baseline / goodput mbps | 1.25735 | 0.0120622 |
| Equal-sized RUs / delay p95 ms | 28.9086 | 1.16755 |
| Equal-sized RUs / goodput mbps | 1.25588 | 0.0116685 |
| Backlog scheduler / delay p95 ms | 29.5865 | 1.18046 |
| Backlog scheduler / goodput mbps | 1.22782 | 0.0623255 |
| Asymmetric backlog / delay p95 ms | 35.3981 | 10.7317 |
| Asymmetric backlog / goodput mbps | 2.51331 | 0.62728 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **INCONCLUSIVE** | Every committed Basic Trigger per-user allocation matches the decoded PCAP AID and RU allocation | BacklogBased: found 3 committed Basic Triggers and 2 decoded PCAP Triggers |
| **INCONCLUSIVE** | Scheduled uplink OFDMA and EDCA are compared using delivered application bytes | The five-run delivery comparison has no manifest-defined acceptance threshold. |

#### [script] Joined Basic Trigger allocation evidence

| Config | Time (s) / Trigger / user | AID | Reported / planned bytes | Model RU | PCAP RU field → decoded RU | Match |
|---|---|---:|---:|---|---|---|
| AsymmetricBacklog | 0.32 / 17 / 0 | 2 | 2576 / 2576 | 52@0 | 37 → 52@0 | PASS |
| AsymmetricBacklog | 0.3824447728460001 / 19 / 0 | 7 | 5152 / 5152 | 106@0 | 53 → 106@0 | PASS |
| AsymmetricBacklog | 0.39343323053100004 / 20 / 0 | 4 | 2576 / 2576 | 52@0 | 37 → 52@0 | PASS |
| AsymmetricBacklog | 0.40156715611200006 / 21 / 0 | 5 | 2576 / 2576 | 52@0 | 37 → 52@0 | PASS |
| AsymmetricBacklog | 0.40156715611200006 / 21 / 1 | 2 | 15456 / 9040 | 106@136 | 54 → 106@136 | PASS |
| AsymmetricBacklog | 0.4116024040900001 / 22 / 0 | 6 | 5152 / 4216 | 52@0 | 37 → 52@0 | PASS |
| AsymmetricBacklog | 0.4116024040900001 / 22 / 1 | 7 | 5152 / 4216 | 52@54 | 38 → 52@54 | PASS |
| AsymmetricBacklog | 0.4116024040900001 / 22 / 2 | 3 | 5152 / 5152 | 106@136 | 54 → 106@136 | PASS |
| AsymmetricBacklog | 0.4236663994060001 / 23 / 0 | 1 | 36064 / 20864 | 242@0 | 61 → 242@0 | PASS |
| AsymmetricBacklog | 0.43 / 24 / 0 | 4 | 2576 / 2576 | 52@0 | 37 → 52@0 | PASS |
| AsymmetricBacklog | 0.56 / 37 / 0 | 1 | 257600 / 2064 | 26@0 | 0 → 26@0 | PASS |
| AsymmetricBacklog | 0.56 / 37 / 1 | 3 | 90160 / 2064 | 26@26 | 1 → 26@26 | PASS |

Showing 12 of 31 joined users; the session-bound evidence ledger retains every observation.
<!-- END GENERATED: ieee80211-scalar-vector-ul_ofdma -->
## [agent] PCAP statistics

The shared AX PCAP analyzer used the four AP captures from session
`20260729T231416Z`, with PCAPng/radiotap input and TShark decoding. The
generated statistics, trigger-type tables, evidence checks, and representative
timelines are the authoritative packet evidence below. Counts are capture
observations; unknown PHY fields remain unknown.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260730T172503Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260730T172503Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260730T172503Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260730T172503Z.json` (SHA-256 `b9b138fe5733852ae296e131bf54c7a7dcd7a211291125b83171bd2504d9ddbb`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `EdcaBaseline` | `none (all decoded frames)` | 4541 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (3945), Control: Block Ack Request (BAR) (220), Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (212) | 41.68% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `EqualSizedRUs` | `none (all decoded frames)` | 4643 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (3717), Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (218), Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] (208) | 48.65% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `BacklogBased` | `none (all decoded frames)` | 4399 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (3448), Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] (228), Control: Block Ack Request (BAR) (213) | 46.83% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `AsymmetricBacklog` | `none (all decoded frames)` | 2328 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (1769), Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] (144), Control: Block Ack (BA) (121) | 41.10% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | AsymmetricBacklog produced protocol-visible wireless observations | 2328 AP/global transmission observations |
| **PASS** | BacklogBased produced protocol-visible wireless observations | 4399 AP/global transmission observations |
| **PASS** | EdcaBaseline produced protocol-visible wireless observations | 4541 AP/global transmission observations |
| **PASS** | EqualSizedRUs produced protocol-visible wireless observations | 4643 AP/global transmission observations |

### [script] Configuration: `EdcaBaseline`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **4541**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 3945 | 86.88% | 167.9 B | 2.0 B | 95.8 us | 11.6 us | 5010 MHz | -56.2 dBm | - | 90.64% | 37.78% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 212 | 4.67% | 170.0 B | 0.0 B | 129.0 us | 0.0 us | 5010 MHz | -53.9 dBm | - | 6.56% | 2.73% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 220 | 4.84% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -55.1 dBm | - | 1.48% | 0.62% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 124 | 2.73% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.91% | 0.38% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 8 | 0.18% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.05% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 16 | 0.35% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -56.6 dBm | 10.0 dBm | 0.09% | 0.04% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 16 | 0.35% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -56.6 dBm | 10.0 dBm | 0.27% | 0.11% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.200164000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 2 | 0.200410000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 3 | 0.200458000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 5 | 0.200554000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 6 | 0.200761000 | 0a:aa:00:00:00:05 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 7 | 0.200809000 | ? → 0a:aa:00:00:00:05 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 9 | 0.200905000 | ? → 0a:aa:00:00:00:05 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 11 | 0.201028000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 13 | 0.201124000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 14 | 0.201340000 | 0a:aa:00:00:00:04 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 15 | 0.201625000 | 0a:aa:00:00:00:06 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 16 | 0.201673000 | ? → 0a:aa:00:00:00:06 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 18 | 0.201769000 | ? → 0a:aa:00:00:00:06 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 19 | 0.201976000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 20 | 0.202024000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 22 | 0.202120000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |

Frame numbers are local to capture `EdcaBaseline-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded HE Trigger user allocations

No HE Trigger User Info fields were decoded.

### [script] Configuration: `EqualSizedRUs`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **4643**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 3717 | 80.06% | 168.0 B | 2.0 B | 96.2 us | 12.1 us | 5010 MHz | -56.6 dBm | - | 73.49% | 35.75% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 218 | 4.70% | 170.0 B | 0.0 B | 129.0 us | 0.0 us | 5010 MHz | -54.9 dBm | - | 5.78% | 2.81% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 208 | 4.48% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz, 5008 MHz, 5010 MHz, 5012 MHz, 5014 MHz, 5016 MHz | -75.0 dBm | - | 17.05% | 8.29% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 65 | 1.40% | 71.9 B | 6.2 B | 44.0 us | 2.1 us | 5010 MHz | - | 10.0 dBm | 0.59% | 0.29% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 203 | 4.37% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -54.5 dBm | - | 1.17% | 0.57% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 192 | 4.14% | 59.9 B | 40.2 B | 40.0 us | 13.4 us | 5010 MHz | - | 10.0 dBm | 1.58% | 0.77% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 8 | 0.17% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.04% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 16 | 0.34% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -56.6 dBm | 10.0 dBm | 0.08% | 0.04% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 16 | 0.34% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -56.6 dBm | 10.0 dBm | 0.23% | 0.11% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.010048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.012064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=455 | Responds without MAC payload while preserving QoS control information. |
| 3 | 0.012064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=459 | Responds without MAC payload while preserving QoS control information. |
| 4 | 0.012064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=463 | Responds without MAC payload while preserving QoS control information. |
| 5 | 0.012064000 | 0a:aa:00:00:00:06 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=469 | Responds without MAC payload while preserving QoS control information. |
| 6 | 0.012064000 | 0a:aa:00:00:00:07 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=475 | Responds without MAC payload while preserving QoS control information. |
| 7 | 0.012064000 | 0a:aa:00:00:00:05 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=481 | Responds without MAC payload while preserving QoS control information. |
| 8 | 0.012064000 | 0a:aa:00:00:00:04 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=487 | Responds without MAC payload while preserving QoS control information. |
| 9 | 0.012064000 | 0a:aa:00:00:00:08 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=493 | Responds without MAC payload while preserving QoS control information. |
| 10 | 0.012153000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 11 | 0.030048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 12 | 0.032064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1177 | Responds without MAC payload while preserving QoS control information. |
| 13 | 0.032064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1181 | Responds without MAC payload while preserving QoS control information. |
| 14 | 0.032064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1185 | Responds without MAC payload while preserving QoS control information. |
| 15 | 0.032064000 | 0a:aa:00:00:00:06 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1191 | Responds without MAC payload while preserving QoS control information. |
| 16 | 0.032064000 | 0a:aa:00:00:00:07 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1197 | Responds without MAC payload while preserving QoS control information. |

Frame numbers are local to capture `EqualSizedRUs-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded HE Trigger user allocations

| Frame | Simulation time (s) | Trigger type | Ordered user allocations |
|---:|---:|---:|---|
| 1 | 0.010048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 11 | 0.030048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 21 | 0.050048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 31 | 0.070048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 41 | 0.090048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 51 | 0.110048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 61 | 0.130048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 71 | 0.150048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 81 | 0.170048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 91 | 0.190048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 150 | 0.220048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 160 | 0.240048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 170 | 0.260048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 180 | 0.280048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 190 | 0.300048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 193 | 0.310048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 196 | 0.320048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 229 | 0.330048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 332 | 0.341384000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 405 | 0.350048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 477 | 0.360048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 594 | 0.372610000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 642 | 0.380048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 717 | 0.390048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 891 | 0.408933000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 936 | 0.430048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 984 | 0.440048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1066 | 0.450048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1244 | 0.470738000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1293 | 0.480036000 | 0 | #0: AID=5, RU=0, MCS=1, target RSSI=35 |
| 1332 | 0.490048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1393 | 0.500048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1490 | 0.514692000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1552 | 0.530048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1626 | 0.540048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1747 | 0.554331000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1817 | 0.570048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1959 | 0.587865000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2188 | 0.623830000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2246 | 0.640048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2304 | 0.650048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2460 | 0.668260000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2512 | 0.690048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2589 | 0.700048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2694 | 0.720048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2731 | 0.730048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2808 | 0.740048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2926 | 0.753576000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2960 | 0.770048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 3025 | 0.780048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 3109 | 0.790048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 3215 | 0.802487000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 3266 | 0.810048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 3346 | 0.820048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 3529 | 0.842531000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 3631 | 0.860048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 3721 | 0.870048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 3792 | 0.880048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 3970 | 0.902330000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 4045 | 0.920048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 4156 | 0.932614000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 4223 | 0.942718000 | 0 | #0: AID=3, RU=0, MCS=1, target RSSI=35; #1: AID=4, RU=1, MCS=1, target RSSI=35 |
| 4313 | 0.954856000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 4486 | 0.980048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 4643 | 0.999520000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |

### [script] Configuration: `BacklogBased`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **4399**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 3448 | 78.38% | 168.1 B | 2.0 B | 96.3 us | 12.1 us | 5010 MHz | -56.0 dBm | - | 70.89% | 33.20% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 210 | 4.77% | 170.0 B | 0.0 B | 129.0 us | 0.0 us | 5010 MHz | -54.8 dBm | - | 5.78% | 2.71% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 228 | 5.18% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz, 5008 MHz, 5010 MHz, 5012 MHz, 5014 MHz, 5016 MHz | -75.0 dBm | - | 19.41% | 9.09% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 65 | 1.48% | 71.9 B | 6.2 B | 44.0 us | 2.1 us | 5010 MHz | - | 10.0 dBm | 0.61% | 0.29% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 213 | 4.84% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -54.7 dBm | - | 1.27% | 0.60% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 195 | 4.43% | 59.9 B | 40.2 B | 40.0 us | 13.4 us | 5010 MHz | - | 10.0 dBm | 1.66% | 0.78% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 8 | 0.18% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.04% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 16 | 0.36% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -56.6 dBm | 10.0 dBm | 0.08% | 0.04% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 16 | 0.36% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -56.6 dBm | 10.0 dBm | 0.24% | 0.11% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.010048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.012064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=455 | Responds without MAC payload while preserving QoS control information. |
| 3 | 0.012064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=459 | Responds without MAC payload while preserving QoS control information. |
| 4 | 0.012064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=463 | Responds without MAC payload while preserving QoS control information. |
| 5 | 0.012064000 | 0a:aa:00:00:00:06 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=469 | Responds without MAC payload while preserving QoS control information. |
| 6 | 0.012064000 | 0a:aa:00:00:00:07 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=475 | Responds without MAC payload while preserving QoS control information. |
| 7 | 0.012064000 | 0a:aa:00:00:00:05 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=481 | Responds without MAC payload while preserving QoS control information. |
| 8 | 0.012064000 | 0a:aa:00:00:00:04 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=487 | Responds without MAC payload while preserving QoS control information. |
| 9 | 0.012064000 | 0a:aa:00:00:00:08 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=493 | Responds without MAC payload while preserving QoS control information. |
| 10 | 0.012153000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 11 | 0.030048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 12 | 0.032064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1177 | Responds without MAC payload while preserving QoS control information. |
| 13 | 0.032064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1181 | Responds without MAC payload while preserving QoS control information. |
| 14 | 0.032064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1185 | Responds without MAC payload while preserving QoS control information. |
| 15 | 0.032064000 | 0a:aa:00:00:00:06 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1191 | Responds without MAC payload while preserving QoS control information. |
| 16 | 0.032064000 | 0a:aa:00:00:00:07 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1197 | Responds without MAC payload while preserving QoS control information. |

Frame numbers are local to capture `BacklogBased-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded HE Trigger user allocations

| Frame | Simulation time (s) | Trigger type | Ordered user allocations |
|---:|---:|---:|---|
| 1 | 0.010048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 11 | 0.030048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 21 | 0.050048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 31 | 0.070048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 41 | 0.090048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 51 | 0.110048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 61 | 0.130048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 71 | 0.150048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 81 | 0.170048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 91 | 0.190048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 150 | 0.220048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 160 | 0.240048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 170 | 0.260048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 180 | 0.280048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 190 | 0.300048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 193 | 0.310048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 196 | 0.320048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 229 | 0.330048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 332 | 0.341384000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 405 | 0.350048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 477 | 0.360048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 594 | 0.372610000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 642 | 0.380048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 717 | 0.390048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 891 | 0.408933000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 936 | 0.430048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 984 | 0.440048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1066 | 0.450048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1244 | 0.470738000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1293 | 0.480036000 | 0 | #0: AID=5, RU=0, MCS=1, target RSSI=35 |
| 1324 | 0.490048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1395 | 0.500048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1513 | 0.513694000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1590 | 0.530048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1771 | 0.552127000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1809 | 0.570048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1933 | 0.584386000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1992 | 0.600048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2106 | 0.614505000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2151 | 0.630048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2193 | 0.640048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2320 | 0.654565000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2385 | 0.670048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2533 | 0.688866000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2643 | 0.710048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2804 | 0.729932000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2850 | 0.740036000 | 0 | #0: AID=3, RU=0, MCS=1, target RSSI=35; #1: AID=7, RU=1, MCS=1, target RSSI=35 |
| 2859 | 0.750048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2907 | 0.760048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 3072 | 0.779131000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 3097 | 0.790048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 3138 | 0.800048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 3324 | 0.824145000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 3395 | 0.840048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 3501 | 0.853335000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 3560 | 0.870048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 3691 | 0.887732000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 3754 | 0.900048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 3877 | 0.914281000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 3892 | 0.920048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 3927 | 0.930048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 3976 | 0.940048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 4078 | 0.951130000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 4184 | 0.970048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 4371 | 0.994008000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |

### [script] Configuration: `AsymmetricBacklog`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2328**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 1769 | 75.99% | 323.8 B | 2.0 B | 181.0 us | 11.7 us | 5010 MHz | -61.6 dBm | - | 77.93% | 32.03% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 71 | 3.05% | 326.0 B | 0.0 B | 214.3 us | 0.0 us | 5010 MHz | -57.0 dBm | - | 3.70% | 1.52% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39c224" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 2 | 0.09% | 326.0 B | 0.0 B | 445.1 us | 0.0 us | 5015 MHz | -75.0 dBm | - | 0.22% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31d831" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 5 | 0.21% | 322.8 B | 1.6 B | 183.8 us | 15.3 us | 5010 MHz | -75.0 dBm | - | 0.22% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24a331" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 3 | 0.13% | 326.0 B | 0.0 B | 905.3 us | 0.0 us | 5003 MHz, 5007 MHz | -75.0 dBm | - | 0.66% | 0.27% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 144 | 6.19% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz, 5008 MHz, 5010 MHz, 5012 MHz, 5014 MHz, 5016 MHz | -75.0 dBm | - | 13.97% | 5.74% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 80 | 3.44% | 69.5 B | 10.4 B | 43.2 us | 3.5 us | 5010 MHz | - | 10.0 dBm | 0.84% | 0.35% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 93 | 3.99% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -58.9 dBm | - | 0.63% | 0.26% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 121 | 5.20% | 83.7 B | 41.3 B | 47.9 us | 13.8 us | 5010 MHz | - | 10.0 dBm | 1.41% | 0.58% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 8 | 0.34% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.05% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 16 | 0.69% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -56.6 dBm | 10.0 dBm | 0.10% | 0.04% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 16 | 0.69% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -56.6 dBm | 10.0 dBm | 0.27% | 0.11% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.010048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.012064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=455 | Responds without MAC payload while preserving QoS control information. |
| 3 | 0.012064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=459 | Responds without MAC payload while preserving QoS control information. |
| 4 | 0.012064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=463 | Responds without MAC payload while preserving QoS control information. |
| 5 | 0.012064000 | 0a:aa:00:00:00:06 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=469 | Responds without MAC payload while preserving QoS control information. |
| 6 | 0.012064000 | 0a:aa:00:00:00:07 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=475 | Responds without MAC payload while preserving QoS control information. |
| 7 | 0.012064000 | 0a:aa:00:00:00:05 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=481 | Responds without MAC payload while preserving QoS control information. |
| 8 | 0.012064000 | 0a:aa:00:00:00:04 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=487 | Responds without MAC payload while preserving QoS control information. |
| 9 | 0.012064000 | 0a:aa:00:00:00:08 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=493 | Responds without MAC payload while preserving QoS control information. |
| 10 | 0.012153000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 11 | 0.030048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 12 | 0.032064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1177 | Responds without MAC payload while preserving QoS control information. |
| 13 | 0.032064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1181 | Responds without MAC payload while preserving QoS control information. |
| 14 | 0.032064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1185 | Responds without MAC payload while preserving QoS control information. |
| 15 | 0.032064000 | 0a:aa:00:00:00:06 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1191 | Responds without MAC payload while preserving QoS control information. |
| 16 | 0.032064000 | 0a:aa:00:00:00:07 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1197 | Responds without MAC payload while preserving QoS control information. |

Frame numbers are local to capture `AsymmetricBacklog-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded HE Trigger user allocations

| Frame | Simulation time (s) | Trigger type | Ordered user allocations |
|---:|---:|---:|---|
| 1 | 0.010048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 11 | 0.030048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 21 | 0.050048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 31 | 0.070048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 41 | 0.090048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 51 | 0.110048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 61 | 0.130048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 71 | 0.150048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 81 | 0.170048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 91 | 0.190048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 150 | 0.220048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 160 | 0.240048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 170 | 0.260048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 180 | 0.280048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 190 | 0.300048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 210 | 0.310048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 257 | 0.320036000 | 0 | #0: AID=2, RU=37, MCS=1, target RSSI=35 |
| 515 | 0.370048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 560 | 0.382480000 | 0 | #0: AID=7, RU=53, MCS=1, target RSSI=35 |
| 609 | 0.393469000 | 0 | #0: AID=4, RU=37, MCS=1, target RSSI=35 |
| 653 | 0.401603000 | 0 | #0: AID=5, RU=37, MCS=1, target RSSI=35; #1: AID=2, RU=54, MCS=1, target RSSI=35 |
| 704 | 0.411642000 | 0 | #0: AID=6, RU=37, MCS=1, target RSSI=35; #1: AID=7, RU=38, MCS=1, target RSSI=35; #2: AID=3, RU=54, MCS=1, target RSSI=35 |
| 768 | 0.423702000 | 0 | #0: AID=1, RU=61, MCS=1, target RSSI=35 |
| 778 | 0.430036000 | 0 | #0: AID=4, RU=37, MCS=1, target RSSI=35 |
| 781 | 0.440048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 797 | 0.450048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 832 | 0.460048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 875 | 0.470048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 926 | 0.480048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 962 | 0.490048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 965 | 0.500048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 974 | 0.510048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1005 | 0.520048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1044 | 0.530048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1080 | 0.540048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1138 | 0.550615000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1152 | 0.560044000 | 0 | #0: AID=1, RU=0, MCS=1, target RSSI=35; #1: AID=3, RU=1, MCS=1, target RSSI=35; #2: AID=5, RU=2, MCS=1, target RSSI=35; #3: AID=6, RU=3, MCS=1, target RSSI=35; #4: AID=7, RU=4, MCS=1, target RSSI=35; #5: AID=2, RU=54, MCS=1, target RSSI=35 |
| 1155 | 0.570048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1170 | 0.580048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1203 | 0.590048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1244 | 0.600048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1293 | 0.610048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1328 | 0.620048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1331 | 0.630048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1340 | 0.640048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1371 | 0.650048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1410 | 0.660048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1445 | 0.670048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1479 | 0.680048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1482 | 0.690048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1491 | 0.700048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1522 | 0.710048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1553 | 0.720048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1589 | 0.730048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1650 | 0.741151000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1661 | 0.750044000 | 0 | #0: AID=2, RU=0, MCS=1, target RSSI=35; #1: AID=3, RU=1, MCS=1, target RSSI=35; #2: AID=5, RU=2, MCS=1, target RSSI=35; #3: AID=6, RU=3, MCS=1, target RSSI=35; #4: AID=7, RU=4, MCS=1, target RSSI=35; #5: AID=1, RU=54, MCS=1, target RSSI=35 |
| 1664 | 0.760048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1679 | 0.770048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1712 | 0.780048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1743 | 0.790048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1781 | 0.800048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1816 | 0.810048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1819 | 0.820048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1828 | 0.830048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1859 | 0.840048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1889 | 0.850048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1936 | 0.860048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1970 | 0.870048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1973 | 0.880048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 1982 | 0.890048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2013 | 0.900048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2043 | 0.910048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2090 | 0.920048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2151 | 0.931115000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2162 | 0.940044000 | 0 | #0: AID=2, RU=0, MCS=1, target RSSI=35; #1: AID=3, RU=1, MCS=1, target RSSI=35; #2: AID=5, RU=2, MCS=1, target RSSI=35; #3: AID=6, RU=3, MCS=1, target RSSI=35; #4: AID=7, RU=4, MCS=1, target RSSI=35; #5: AID=1, RU=54, MCS=1, target RSSI=35 |
| 2165 | 0.950048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2180 | 0.960048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2213 | 0.970048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2245 | 0.980048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 2294 | 0.990048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=4, RU=3, MCS=0, target RSSI=35; #4: AID=5, RU=4, MCS=0, target RSSI=35; #5: AID=6, RU=5, MCS=0, target RSSI=35; #6: AID=7, RU=6, MCS=0, target RSSI=35; #7: AID=8, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |

### [script] Analysis of Packet Distribution
`EdcaBaseline` provides the non-triggered control. The three scheduled configurations contain repeated **Trigger** frames, solicited HE-TB observations, and AP **Block Ack** responses, which is the expected HE UL-MU exchange structure (IEEE Std 802.11-2024, Clause 26.5.2; see informative Annex G.5). Frame-subtype totals alone do not establish that queued payload was carried in the solicited responses or distinguish the two scheduler policies. Use decoded Trigger user allocations, HE-TB payload observations, and aligned scheduler/application telemetry for those decisions.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->

## [agent] Frame exchange analysis

The generated scheduled timelines show the decisive order: an AP Trigger,
simultaneous HE-TB responses on decoded RUs, and an AP Block Ack. The trigger
tables add the decoded type and per-user AID/RU fields; early responses may be
QoS Null, while later responses carry QoS Data. These are direct run-0
observations, not proof of the scheduler's internal reason for choosing an RU.

The EDCA timeline instead shows station-originated HE-SU data without the
Trigger prefix. Frame numbers are local to each AP capture and are not
OMNeT++ event numbers.

## [agent] Cross-layer findings and verdict

`PASS`: in session `20260729T231416Z`, all four configurations have five
scalar/vector runs and a matched run-0 AP capture. The three scheduled
configurations show Trigger-led HE-TB exchanges, while `EdcaBaseline` shows
the non-triggered HE-SU control. The joined allocation checks match every
committed scheduled per-user allocation to the decoded transmitted AID and RU
geometry in the retained evidence.

`INCONCLUSIVE`: the application outcomes do not establish a general scheduler
ranking. EDCA is the strongest control in this workload, while
`EqualSizedRUs` retains substantial uncertainty and `AsymmetricBacklog`
changes multiple factors. None of these outcomes alone explains why a
scheduler chose an RU.

## [agent] Limitations and inconclusive claims

- Five repetitions support a bounded example comparison, not a general claim
  about other channels, loads, distances, or random seeds.
- The MU-EDCA timer is configuration evidence; it is not separately recorded
  as a per-packet state observation.
- AP PCAP counts do not equal delivered application-packet counts, and the
  decoded fields do not expose the scheduler's backlog rationale.
- The model-only Trigger ID is not transmitted over the air. The allocation
  result therefore depends on the executable order-preserving join between
  scheduler telemetry and decoded Trigger User Info.
