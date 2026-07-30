# Walkthrough: IEEE 802.11ax scheduled uplink OFDMA

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260729T231416Z`
- PCAP: `20260729T231416Z`
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
| `BacklogBased` | Scheduled treatment | `HeUlSchedulerBacklogBased`; UL MU-OFDMA enabled | SU `2/3/7`; MU `10/255/1023`, 2.089 s | 3 × 100 B every 0.5 ms | Trigger-led HE-TB exchange |
| `EqualSizedRUs` | Scheduled policy control | `HeUlSchedulerEqualSizedRUs`; UL MU-OFDMA enabled | same | matched | Trigger-led exchange with equal-sized RU policy |
| `EdcaBaseline` | Negative control | UL MU-OFDMA disabled; HE-SU EDCA | SU `2/3/7`; MU settings inactive for the disabled trigger path | matched | no Basic Trigger or HE-TB response |
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

![ul_ofdma scalar/vector analysis](results/20260729T231416Z/ul-ofdma-delivery-delay.png)

Figure provenance: [`results/20260729T231416Z/ul-ofdma-delivery-delay.png.json`](results/20260729T231416Z/ul-ofdma-delivery-delay.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.server.app[*] / packetReceived:vector(packetBytes)<br>vector / **.server.app[*] / endToEndDelay:vector / unit=s
- Window / per-run aggregation / exclusions: [0.3, 0.8) s, [0.3, 0.95) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| Asymmetric backlog / delay p95 ms | 204.981 | 163.506 |
| Asymmetric backlog / goodput mbps | 2.69435 | 1.04681 |
| Backlog scheduler / delay p95 ms | 95.0476 | 2.72524 |
| Backlog scheduler / goodput mbps | 1.44985 | 0.0355451 |
| EDCA baseline / delay p95 ms | 7.5023 | 0.0619875 |
| EDCA baseline / goodput mbps | 1.59212 | 0.00920734 |
| Equal-sized RUs / delay p95 ms | 207.65 | 33.6225 |
| Equal-sized RUs / goodput mbps | 0.916185 | 0.385238 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **PASS** | Every committed Basic Trigger per-user allocation matches the decoded PCAP AID and RU allocation | All 1353 committed Basic Trigger user allocations match decoded PCAP AID/RU fields one-to-one. |
| **INCONCLUSIVE** | Scheduled uplink OFDMA and EDCA are compared using delivered application bytes | The five-run delivery comparison has no manifest-defined acceptance threshold. |

#### [script] Joined Basic Trigger allocation evidence

| Config | Time (s) / Trigger / user | AID | Reported / planned bytes | Model RU | PCAP RU field → decoded RU | Match |
|---|---|---:|---:|---|---|---|
| AsymmetricBacklog | 0.328 / 6 / 0 | 1 | 5152 / 5152 | 106@0 | 53 → 106@0 | PASS |
| AsymmetricBacklog | 0.33146807362100006 / 7 / 0 | 1 | 5152 / 5152 | 106@0 | 53 → 106@0 | PASS |
| AsymmetricBacklog | 0.334 / 8 / 0 | 1 | 5152 / 5152 | 106@0 | 53 → 106@0 | PASS |
| AsymmetricBacklog | 0.3403871570110001 / 9 / 0 | 2 | 5152 / 5152 | 106@0 | 53 → 106@0 | PASS |
| AsymmetricBacklog | 0.379 / 10 / 0 | 1 | 2576 / 2576 | 52@0 | 37 → 52@0 | PASS |
| AsymmetricBacklog | 0.41500000000000004 / 11 / 0 | 1 | 2576 / 2576 | 52@0 | 37 → 52@0 | PASS |
| AsymmetricBacklog | 0.4182020333560001 / 12 / 0 | 1 | 2576 / 2576 | 52@0 | 37 → 52@0 | PASS |
| AsymmetricBacklog | 0.421 / 13 / 0 | 3 | 2576 / 2576 | 52@0 | 37 → 52@0 | PASS |
| AsymmetricBacklog | 0.423 / 14 / 0 | 3 | 2576 / 2576 | 52@0 | 37 → 52@0 | PASS |
| AsymmetricBacklog | 0.425 / 15 / 0 | 3 | 2576 / 2576 | 52@0 | 37 → 52@0 | PASS |
| AsymmetricBacklog | 0.427 / 16 / 0 | 3 | 2576 / 2576 | 52@0 | 37 → 52@0 | PASS |
| AsymmetricBacklog | 0.429 / 17 / 0 | 3 | 2576 / 2576 | 52@0 | 37 → 52@0 | PASS |

Showing 12 of 1353 joined users; the session-bound evidence ledger retains every observation.
<!-- END GENERATED: ieee80211-scalar-vector-ul_ofdma -->
## [agent] PCAP statistics

The shared AX PCAP analyzer used the four AP captures from session
`20260729T231416Z`, with PCAPng/radiotap input and TShark decoding. The
generated statistics, trigger-type tables, evidence checks, and representative
timelines are the authoritative packet evidence below. Counts are capture
observations; unknown PHY fields remain unknown.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260729T231416Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260729T231416Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260729T231416Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260729T231416Z.json` (SHA-256 `d0c64c973a0f8b93f8974c04ed54af54db6e22cf63041261211de5cb90cef07f`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `AsymmetricBacklog` | `none (all decoded frames)` | 2179 | Data: QoS Data [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (617), Control: Block Ack (BA) (349), Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (332) | 65.08% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `BacklogBased` | `none (all decoded frames)` | 2217 | Data: QoS Data [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (726), Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (495), Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (407) | 32.98% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `EdcaBaseline` | `none (all decoded frames)` | 3729 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (2667), Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (474), Control: Block Ack Request (BAR) (350) | 33.83% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `EqualSizedRUs` | `none (all decoded frames)` | 1349 | Data: QoS Data [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, BCC, A-MPDU] (585), Control: Block Ack (BA) (315), Control: Trigger (301) | 59.20% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | AsymmetricBacklog produced protocol-visible wireless observations | 2179 AP/global transmission observations |
| **PASS** | BacklogBased produced protocol-visible wireless observations | 2217 AP/global transmission observations |
| **PASS** | EdcaBaseline produced protocol-visible wireless observations | 3729 AP/global transmission observations |
| **PASS** | EqualSizedRUs produced protocol-visible wireless observations | 1349 AP/global transmission observations |

### [script] Configuration: `AsymmetricBacklog`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2179**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 332 | 15.24% | 324.1 B | 2.0 B | 184.0 us | 14.6 us | 5010 MHz | -50.0 dBm | - | 9.39% | 6.11% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 186 | 8.54% | 326.0 B | 0.0 B | 214.3 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 6.13% | 3.99% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39c224" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 617 | 28.32% | 323.3 B | 1.9 B | 417.8 us | 19.3 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 39.61% | 25.78% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24a331" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 290 | 13.31% | 326.0 B | 0.0 B | 905.3 us | 0.0 us | 5003 MHz, 5007 MHz | -75.0 dBm | - | 40.34% | 26.25% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 9 | 0.41% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.55% | 0.36% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 298 | 13.68% | 40.0 B | 5.1 B | 33.3 us | 1.7 us | 5010 MHz | - | 10.0 dBm | 1.53% | 0.99% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 68 | 3.12% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 0.29% | 0.19% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#9f532d" /></svg> | Control: Block Ack Request (BAR) [HE-TB, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 15 | 0.69% | 24.0 B | 0.0 B | 100.0 us | 0.0 us | 5003 MHz | -75.0 dBm | - | 0.23% | 0.15% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 349 | 16.02% | 42.9 B | 6.1 B | 34.3 us | 2.0 us | 5010 MHz | - | 10.0 dBm | 1.84% | 1.20% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 3 | 0.14% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.28% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.28% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.06% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.003064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=220 | Responds without MAC payload while preserving QoS control information. |
| 3 | 0.003064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=224 | Responds without MAC payload while preserving QoS control information. |
| 4 | 0.003064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=228 | Responds without MAC payload while preserving QoS control information. |
| 5 | 0.003133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 6 | 0.104048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 7 | 0.106064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=382 | Responds without MAC payload while preserving QoS control information. |
| 8 | 0.106064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=386 | Responds without MAC payload while preserving QoS control information. |
| 9 | 0.106064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=390 | Responds without MAC payload while preserving QoS control information. |
| 10 | 0.106133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 11 | 0.200244000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 12 | 0.200597000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 13 | 0.200962000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 14 | 0.201010000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 16 | 0.201106000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 17 | 0.201384000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |

Frame numbers are local to capture `AsymmetricBacklog-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded HE Trigger user allocations

| Frame | Simulation time (s) | Trigger type | Ordered user allocations |
|---:|---:|---:|---|
| 1 | 0.001048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 6 | 0.104048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 32 | 0.301048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 36 | 0.304048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 58 | 0.309015000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 134 | 0.328036000 | 0 | #0: AID=1, RU=53, MCS=1, target RSSI=35 |
| 145 | 0.331504000 | 0 | #0: AID=1, RU=53, MCS=1, target RSSI=35 |
| 159 | 0.334036000 | 0 | #0: AID=1, RU=53, MCS=1, target RSSI=35 |
| 196 | 0.340423000 | 0 | #0: AID=2, RU=53, MCS=1, target RSSI=35 |
| 305 | 0.379036000 | 0 | #0: AID=1, RU=37, MCS=1, target RSSI=35 |
| 413 | 0.415036000 | 0 | #0: AID=1, RU=37, MCS=1, target RSSI=35 |
| 431 | 0.418238000 | 0 | #0: AID=1, RU=37, MCS=1, target RSSI=35 |

Showing the first 12 of 298 decoded Trigger frames; the script-owned packet metrics JSON preserves every row.

### [script] Configuration: `BacklogBased`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2217**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 495 | 22.33% | 168.4 B | 2.0 B | 98.5 us | 14.2 us | 5010 MHz | -50.0 dBm | - | 14.79% | 4.88% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 407 | 18.36% | 170.0 B | 0.0 B | 129.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 15.92% | 5.25% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39c224" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 726 | 32.75% | 166.7 B | 1.5 B | 215.2 us | 15.3 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 47.36% | 15.62% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31d831" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 10 | 0.45% | 166.4 B | 1.2 B | 94.6 us | 11.5 us | 5010 MHz | -75.0 dBm | - | 0.29% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#18a523" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 22 | 0.99% | 170.0 B | 0.0 B | 942.7 us | 0.0 us | 5002 MHz, 5004 MHz | -75.0 dBm | - | 6.29% | 2.07% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24a331" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 54 | 2.44% | 167.3 B | 1.9 B | 458.2 us | 22.0 us | 5003 MHz, 5007 MHz | -75.0 dBm | - | 7.50% | 2.47% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 21 | 0.95% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 2.54% | 0.84% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 121 | 5.46% | 42.6 B | 10.4 B | 34.2 us | 3.5 us | 5010 MHz | - | 10.0 dBm | 1.26% | 0.41% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 114 | 5.14% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 0.97% | 0.32% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#8d5134" /></svg> | Control: Block Ack Request (BAR) [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 14 | 0.63% | 24.0 B | 0.0 B | 164.0 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz, 5010 MHz | -75.0 dBm | - | 0.70% | 0.23% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 218 | 9.83% | 39.7 B | 8.5 B | 33.2 us | 2.8 us | 5010 MHz | - | 10.0 dBm | 2.20% | 0.72% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 3 | 0.14% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.02% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.27% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.04% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.27% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.13% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.003064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=220 | Responds without MAC payload while preserving QoS control information. |
| 3 | 0.003064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=224 | Responds without MAC payload while preserving QoS control information. |
| 4 | 0.003064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=228 | Responds without MAC payload while preserving QoS control information. |
| 5 | 0.003133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 6 | 0.104048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 7 | 0.106064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=382 | Responds without MAC payload while preserving QoS control information. |
| 8 | 0.106064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=386 | Responds without MAC payload while preserving QoS control information. |
| 9 | 0.106064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=390 | Responds without MAC payload while preserving QoS control information. |
| 10 | 0.106133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 11 | 0.200164000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 12 | 0.200437000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 13 | 0.200722000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 14 | 0.200770000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 16 | 0.200866000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 17 | 0.201064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |

Frame numbers are local to capture `BacklogBased-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded HE Trigger user allocations

| Frame | Simulation time (s) | Trigger type | Ordered user allocations |
|---:|---:|---:|---|
| 1 | 0.001048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 6 | 0.104048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 32 | 0.301048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 43 | 0.304224000 | 0 | #0: AID=1, RU=0, MCS=1, target RSSI=35; #1: AID=2, RU=1, MCS=1, target RSSI=35; #2: AID=3, RU=2, MCS=1, target RSSI=35 |
| 54 | 0.306895000 | 0 | #0: AID=1, RU=0, MCS=1, target RSSI=35; #1: AID=3, RU=1, MCS=1, target RSSI=35 |
| 468 | 0.378036000 | 0 | #0: AID=1, RU=0, MCS=1, target RSSI=35 |
| 471 | 0.380036000 | 0 | #0: AID=1, RU=0, MCS=1, target RSSI=35 |
| 476 | 0.382347000 | 0 | #0: AID=1, RU=0, MCS=1, target RSSI=35 |
| 486 | 0.384780000 | 0 | #0: AID=1, RU=0, MCS=1, target RSSI=35 |
| 501 | 0.387858000 | 0 | #0: AID=1, RU=0, MCS=1, target RSSI=35 |
| 511 | 0.390418000 | 0 | #0: AID=1, RU=37, MCS=1, target RSSI=35 |
| 516 | 0.392096000 | 0 | #0: AID=1, RU=0, MCS=1, target RSSI=35 |

Showing the first 12 of 121 decoded Trigger frames; the script-owned packet metrics JSON preserves every row.

### [script] Configuration: `EdcaBaseline`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **3729**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 2667 | 71.52% | 168.5 B | 1.9 B | 97.4 us | 13.1 us | 5010 MHz | -50.0 dBm | - | 76.82% | 25.99% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 474 | 12.71% | 170.0 B | 0.0 B | 129.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 18.07% | 6.11% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 350 | 9.39% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 2.90% | 0.98% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 223 | 5.98% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 2.02% | 0.68% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 3 | 0.08% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.02% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.16% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.04% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.16% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.12% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.200164000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 2 | 0.200419000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 3 | 0.200467000 | ? → 0a:aa:00:00:00:02 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 5 | 0.200563000 | ? → 0a:aa:00:00:00:02 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 7 | 0.200677000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 8 | 0.200911000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 9 | 0.201262000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 10 | 0.201310000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 12 | 0.201406000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 14 | 0.201547000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 15 | 0.201745000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 16 | 0.201793000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 18 | 0.201889000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 20 | 0.202003000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 21 | 0.300164000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 22 | 0.301664000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |

Frame numbers are local to capture `EdcaBaseline-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded HE Trigger user allocations

No HE Trigger User Info fields were decoded.

### [script] Configuration: `EqualSizedRUs`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1349**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 18 | 1.33% | 168.0 B | 2.0 B | 105.9 us | 18.4 us | 5010 MHz | -50.0 dBm | - | 0.32% | 0.19% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 81 | 6.00% | 170.0 B | 0.0 B | 129.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 1.76% | 1.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28af31" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 585 | 43.37% | 170.0 B | 0.0 B | 942.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 93.15% | 55.15% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 12 | 0.89% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.81% | 0.48% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 301 | 22.31% | 40.5 B | 3.8 B | 33.5 us | 1.3 us | 5010 MHz | - | 10.0 dBm | 1.70% | 1.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 15 | 1.11% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 0.07% | 0.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#946938" /></svg> | Control: Block Ack Request (BAR) [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 7 | 0.52% | 24.0 B | 0.0 B | 164.0 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.19% | 0.11% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 315 | 23.35% | 45.7 B | 3.7 B | 35.2 us | 1.2 us | 5010 MHz | - | 10.0 dBm | 1.87% | 1.11% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 3 | 0.22% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.44% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.03% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.44% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.07% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.003064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=220 | Responds without MAC payload while preserving QoS control information. |
| 3 | 0.003064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=224 | Responds without MAC payload while preserving QoS control information. |
| 4 | 0.003064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=228 | Responds without MAC payload while preserving QoS control information. |
| 5 | 0.003133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 6 | 0.104048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 7 | 0.106064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=382 | Responds without MAC payload while preserving QoS control information. |
| 8 | 0.106064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=386 | Responds without MAC payload while preserving QoS control information. |
| 9 | 0.106064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=390 | Responds without MAC payload while preserving QoS control information. |
| 10 | 0.106133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 11 | 0.200164000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 12 | 0.200437000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 13 | 0.200722000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 14 | 0.200770000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 16 | 0.200866000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 17 | 0.201064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |

Frame numbers are local to capture `EqualSizedRUs-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded HE Trigger user allocations

| Frame | Simulation time (s) | Trigger type | Ordered user allocations |
|---:|---:|---:|---|
| 1 | 0.001048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 6 | 0.104048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 32 | 0.301048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 43 | 0.304224000 | 0 | #0: AID=1, RU=0, MCS=1, target RSSI=35; #1: AID=2, RU=1, MCS=1, target RSSI=35; #2: AID=3, RU=2, MCS=1, target RSSI=35 |
| 50 | 0.306403000 | 0 | #0: AID=1, RU=0, MCS=1, target RSSI=35; #1: AID=2, RU=1, MCS=1, target RSSI=35; #2: AID=3, RU=2, MCS=1, target RSSI=35 |
| 53 | 0.308117000 | 0 | #0: AID=1, RU=0, MCS=1, target RSSI=35; #1: AID=2, RU=1, MCS=1, target RSSI=35; #2: AID=3, RU=2, MCS=1, target RSSI=35 |
| 62 | 0.310315000 | 0 | #0: AID=1, RU=0, MCS=1, target RSSI=35; #1: AID=2, RU=1, MCS=1, target RSSI=35 |
| 162 | 0.412048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 169 | 0.415040000 | 0 | #0: AID=1, RU=0, MCS=1, target RSSI=35; #1: AID=2, RU=1, MCS=1, target RSSI=35; #2: AID=3, RU=2, MCS=1, target RSSI=35 |
| 175 | 0.417036000 | 0 | #0: AID=1, RU=0, MCS=1, target RSSI=35; #1: AID=2, RU=1, MCS=1, target RSSI=35 |
| 178 | 0.419036000 | 0 | #0: AID=1, RU=0, MCS=1, target RSSI=35; #1: AID=2, RU=1, MCS=1, target RSSI=35 |
| 183 | 0.421036000 | 0 | #0: AID=1, RU=0, MCS=1, target RSSI=35; #1: AID=2, RU=1, MCS=1, target RSSI=35 |

Showing the first 12 of 301 decoded Trigger frames; the script-owned packet metrics JSON preserves every row.

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
