# Walkthrough: IEEE 802.11ax scheduled uplink OFDMA

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260729T083529Z`
- PCAP: `20260729T083529Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `20260729T083529Z`.

This four-configuration experiment shows that enabling scheduled uplink OFDMA
changes the observed exchange from station-contended HE-SU traffic to
Trigger-led HE-TB responses. In session `20260729T083529Z`, the scheduled
configurations contain that exchange while the EDCA control does not. The
application results compare delivery and tail delay, but do not by themselves
explain each scheduler decision.

## [agent] Learning objectives and feature primer

The learning question is: does the example expose the scheduled uplink OFDMA
exchange, and how do its application outcomes compare with ordinary EDCA?

Uplink Orthogonal Frequency-Division Multiple Access (OFDMA) lets an access
point (AP) divide a channel into resource units (RUs). The AP wins channel
access and sends a Basic Trigger whose User Info fields identify scheduled
stations and their RU/PHY parameters. Addressed stations that satisfy the
response conditions start aligned HE trigger-based (HE-TB) responses one SIFS
after the triggering PPDU. A response can carry scheduled data or QoS Null.
INET completes this example's modeled exchange with a SIFS-delayed Multi-STA
Block Ack. An ordinary EDCA transmission is instead a station-originated HE
single-user (HE-SU) exchange with no preceding Basic Trigger.

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

The [INI file](omnetpp.ini) places the AP at the center of a 50 m × 50 m
area, fixes the radios to AX 20 MHz operation and HE MCS 1/NSS 1, disables
random-access RUs, and enables Block Ack and MPDU aggregation. The common
warm-up app offers one 100-byte packet per host at 0.2 s; the measured app
starts at 0.3 s and sends 100-byte packets every 0.5 ms. The default
measurement window is `[0.3,0.95)` s. The asymmetric case changes both apps
to 256-byte packets, uses 1/2/4 ms measured-app intervals for hosts 0/1/2, and
measures `[0.3,0.8)` s.

Port 5000 is classified as UP_VO by `ExampleQosClassifier`. The baseline SU
EDCA values are therefore the HE VO defaults (AIFSN/CWmin/CWmax `2/3/7`).
The current MU-EDCA settings are explicitly `muAifsn=10`, `muCwMin=255`,
`muCwMax=1023`, and `muEdcaTimer=100ms` in the
[INI configuration](omnetpp.ini). `AsymmetricBacklog` also sets the ordinary
SU values to `10/255/1023` so queues remain available to the AP scheduler.
These assignments are requested configuration; the campaign results and
captures establish what occurred.

## [agent] Standards and INET model boundary

IEEE Std 802.11-2024 defines the Basic Trigger format in Clause 9.3.1.22.2 and
UL-MU solicitation in Clause 26.5.2.2. Clause 26.5.2.2.4 constrains the Common
Info and User Info fields used to construct an HE-TB PPDU. Under Clause
26.5.2.3.1, a qualifying non-AP station starts that response one SIFS after
the triggering PPDU; Clause 26.5.2.4 permits a QoS Null response when no
pending frame fits. Immediate acknowledgment behavior is specified in Clause
10.3.2.13.3 and Clauses 26.4.4.5–26.4.4.6.

INET concretizes the exchange with `HeHcf`, the UL coordinator, and
replaceable `HeUlScheduler*` policies. This configured path collects HE-TB
responses and sends a SIFS-delayed Multi-STA Block Ack. IEEE defines legal
fields, response conditions, and timing, but not INET's scheduler objective,
backlog scoring, allocation order, or tie-breaking policy.

The PCAP is AP `wlan[0]` observation evidence. It counts capture observations,
not necessarily unique application deliveries; A-MPDU expansion and parallel
RU observations can affect totals. Radiotap fields are reported only when the
capture marks them known. Goodput and delay are application outcomes and do not
by themselves prove scheduler decisions or per-user RU assignments.

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
| Outcome differences identify individual RU decisions | `INCONCLUSIVE` | PCAP and scalar/vector outputs | matched run 0 plus five-run outcomes | Requires decoded Trigger user allocations aligned with scheduler telemetry |

## [agent] Configuration matrix

| Configuration | Role | Effective scheduler / UL mode | Contention delta | Workload | Expected invariant |
|---|---|---|---|---|---|
| `BacklogBased` | Scheduled treatment | `HeUlSchedulerBacklogBased`; UL MU-OFDMA enabled | SU `2/3/7`; MU `10/255/1023`, 100 ms | 3 × 100 B every 0.5 ms | Trigger-led HE-TB exchange |
| `EqualSizedRUs` | Scheduled policy control | `HeUlSchedulerEqualSizedRUs`; UL MU-OFDMA enabled | same | matched | Trigger-led exchange with equal-sized RU policy |
| `EdcaBaseline` | Negative control | UL MU-OFDMA disabled; HE-SU EDCA | SU `2/3/7`; MU settings inactive for the disabled trigger path | matched | no Basic Trigger or HE-TB response |
| `AsymmetricBacklog` | Multi-factor stress case | inherits `BacklogBased`; UL MU-OFDMA enabled | SU `10/255/1023`; MU `10/255/1023`, 100 ms | both apps 256 B; measured app every 1/2/4 ms | unequal backlog inputs remain observable |

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
| RU attribution remains bounded by decoded fields | Trigger/HE-TB fields in generated timelines | scheduler conclusions exceed visible fields | add aligned Trigger-user and scheduler telemetry |

## [agent] Reproduction

Run from the INET repository root. First inspect the suite-owned options:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py inspect ul_ofdma

MPLCONFIGDIR=/tmp/matplotlib \
python3 examples/ieee80211/analysis/wifi_analysis.py run ul_ofdma \
  --suite ax --evidence both --runs 5 --jobs "$(nproc)" \
  --session-id 20260729T083529Z
```

The session contains runs `[0,5)` for each of the four configurations. Run 0
also records the AP `wlan[0]` PCAP from the same simulation trajectory. Report
and publish that retained session with:

```sh
MPLCONFIGDIR=/tmp/matplotlib \
python3 examples/ieee80211/analysis/wifi_analysis.py report ul_ofdma \
  --suite ax --session-id 20260729T083529Z

MPLCONFIGDIR=/tmp/matplotlib \
python3 examples/ieee80211/analysis/wifi_analysis.py publish ul_ofdma \
  --suite ax --session-id 20260729T083529Z --update
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
`results/20260729T083529Z/{configuration}/` for runs 0–4.

The figure answers two separate questions: how much application traffic
reached the sink and what p95 delay the delivered packets experienced. In this
session the EDCA control has the highest goodput and lowest p95 delay. The
scheduled cases still prove the UL-MU mechanism, but this workload does not
show a delivery or delay advantage for it. The wide confidence interval for
the equal-sized scheduler also cautions against ranking the two schedulers
from five runs.

<!-- BEGIN GENERATED: ieee80211-scalar-vector-ul_ofdma -->
### [script] Generated scalar/vector plot and table

![ul_ofdma scalar/vector analysis](results/20260729T083529Z/ul-ofdma-delivery-delay.png)

Figure provenance: [`results/20260729T083529Z/ul-ofdma-delivery-delay.png.json`](results/20260729T083529Z/ul-ofdma-delivery-delay.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.server.app[*] / packetReceived:vector(packetBytes)<br>vector / **.server.app[*] / endToEndDelay:vector / unit=s
- Window / per-run aggregation / exclusions: [0.3, 0.8) s, [0.3, 0.95) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| Asymmetric backlog / delay p95 ms | 51.2364 | 11.1349 |
| Asymmetric backlog / goodput mbps | 3.39558 | 0.0890388 |
| Backlog scheduler / delay p95 ms | 146.815 | 11.3155 |
| Backlog scheduler / goodput mbps | 3.68443 | 0.121387 |
| EDCA baseline / delay p95 ms | 4.55675 | 0.275597 |
| EDCA baseline / goodput mbps | 4.78572 | 0.00412897 |
| Equal-sized RUs / delay p95 ms | 98.3162 | 47.2918 |
| Equal-sized RUs / goodput mbps | 3.99138 | 0.970507 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.
<!-- END GENERATED: ieee80211-scalar-vector-ul_ofdma -->
## [agent] PCAP statistics

The shared AX PCAP analyzer used the four AP captures from session
`20260729T083529Z`, with PCAPng/radiotap input and TShark decoding. Generated
statistics, composition plots, evidence checks, and representative timelines
are inserted below by the shared publisher. Counts are capture observations;
unknown PHY fields remain unknown and estimated HE-TB airtime is explicitly
limited by fields not exposed in radiotap.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260729T083529Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260729T083529Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260729T083529Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260729T083529Z.json` (SHA-256 `01d29f72f485d88c220061fd238592889b5cfb4c57913191895fa4682e62d31b`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `AsymmetricBacklog` | `none (all decoded frames)` | 1756 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (893), Control: Block Ack (BA) (266), Control: Trigger (160) | 29.44% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `BacklogBased` | `none (all decoded frames)` | 4494 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (3056), Control: Block Ack (BA) (343), Control: Trigger (182) | 43.56% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `EdcaBaseline` | `none (all decoded frames)` | 5968 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (3630), Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (961), Control: Block Ack Request (BAR) (701) | 53.02% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `EqualSizedRUs` | `none (all decoded frames)` | 4951 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (3094), Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (869), Control: Block Ack (BA) (412) | 49.62% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | AsymmetricBacklog produced protocol-visible wireless observations | 1756 AP/global transmission observations |
| **PASS** | BacklogBased produced protocol-visible wireless observations | 4494 AP/global transmission observations |
| **PASS** | EdcaBaseline produced protocol-visible wireless observations | 5968 AP/global transmission observations |
| **PASS** | EqualSizedRUs produced protocol-visible wireless observations | 4951 AP/global transmission observations |

### [script] Configuration: `AsymmetricBacklog`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1756**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 893 | 50.85% | 322.6 B | 1.5 B | 182.1 us | 13.9 us | 5010 MHz | -50.0 dBm | - | 55.25% | 16.26% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 99 | 5.64% | 326.0 B | 0.0 B | 214.3 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 7.21% | 2.12% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39c224" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 89 | 5.07% | 324.1 B | 2.0 B | 425.3 us | 20.5 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 12.86% | 3.78% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31d831" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 76 | 4.33% | 324.1 B | 2.0 B | 195.7 us | 19.1 us | 5010 MHz | -75.0 dBm | - | 5.05% | 1.49% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24a331" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 41 | 2.33% | 326.0 B | 0.0 B | 905.3 us | 0.0 us | 5003 MHz, 5007 MHz | -75.0 dBm | - | 12.61% | 3.71% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 9 | 0.51% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 1.22% | 0.36% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 160 | 9.11% | 35.3 B | 5.5 B | 31.8 us | 1.8 us | 5010 MHz | - | 10.0 dBm | 1.73% | 0.51% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 108 | 6.15% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 1.03% | 0.30% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 266 | 15.15% | 34.1 B | 4.0 B | 31.4 us | 1.3 us | 5010 MHz | - | 10.0 dBm | 2.83% | 0.83% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 3 | 0.17% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.03% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.34% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.05% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.34% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.14% | 0.04% |

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
| 12 | 0.201965000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 13 | 0.204634000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 14 | 0.204682000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 16 | 0.204778000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 18 | 0.204901000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |

Frame numbers are local to capture `AsymmetricBacklog-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `BacklogBased`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **4494**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 3056 | 68.00% | 166.4 B | 1.2 B | 93.5 us | 9.7 us | 5010 MHz | -50.0 dBm | - | 65.60% | 28.58% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 158 | 3.52% | 170.0 B | 0.0 B | 129.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 4.68% | 2.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39c224" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 168 | 3.74% | 166.8 B | 1.6 B | 216.1 us | 16.1 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 8.34% | 3.63% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31d831" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 170 | 3.78% | 166.5 B | 1.3 B | 95.5 us | 12.6 us | 5010 MHz | -75.0 dBm | - | 3.73% | 1.62% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#18a523" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 9 | 0.20% | 170.0 B | 0.0 B | 942.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5010 MHz | -75.0 dBm | - | 1.95% | 0.85% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24a331" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 37 | 0.82% | 167.5 B | 1.9 B | 460.3 us | 22.6 us | 5003 MHz | -75.0 dBm | - | 3.91% | 1.70% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 54 | 1.20% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 4.94% | 2.15% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1a620f" /></svg> | Data: QoS Null [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 37 | 0.82% | 34.0 B | 0.0 B | 78.7 us | 0.0 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 0.67% | 0.29% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#148014" /></svg> | Data: QoS Null [HE-TB, HE-MCS 1, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 71 | 1.58% | 34.0 B | 0.0 B | 54.6 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 0.89% | 0.39% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 182 | 4.05% | 40.2 B | 12.4 B | 33.4 us | 4.1 us | 5010 MHz | - | 10.0 dBm | 1.40% | 0.61% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 173 | 3.85% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 1.11% | 0.48% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 343 | 7.63% | 36.2 B | 7.4 B | 32.1 us | 2.5 us | 5010 MHz | - | 10.0 dBm | 2.53% | 1.10% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 24 | 0.53% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.14% | 0.06% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.13% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.03% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.13% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.10% | 0.04% |

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

### [script] Configuration: `EdcaBaseline`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **5968**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 3630 | 60.82% | 167.2 B | 1.8 B | 100.7 us | 16.6 us | 5010 MHz | -50.0 dBm | - | 68.97% | 36.57% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 961 | 16.10% | 170.0 B | 0.0 B | 129.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 23.38% | 12.40% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 701 | 11.75% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 3.70% | 1.96% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 661 | 11.08% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 3.82% | 2.03% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 3 | 0.05% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.10% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.03% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.10% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.08% | 0.04% |

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
| 22 | 0.300664000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |

Frame numbers are local to capture `EdcaBaseline-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `EqualSizedRUs`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **4951**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 3094 | 62.49% | 166.9 B | 1.7 B | 95.8 us | 12.6 us | 5010 MHz | -50.0 dBm | - | 59.75% | 29.65% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 869 | 17.55% | 170.0 B | 0.0 B | 129.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 22.59% | 11.21% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28af31" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 32 | 0.65% | 170.0 B | 0.0 B | 942.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 6.08% | 3.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 69 | 1.39% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 5.54% | 2.75% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#105114" /></svg> | Data: QoS Null [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 15 | 0.30% | 34.0 B | 0.0 B | 217.3 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.66% | 0.33% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 50 | 1.01% | 58.6 B | 15.1 B | 39.5 us | 5.0 us | 5010 MHz | - | 10.0 dBm | 0.40% | 0.20% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 395 | 7.98% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 2.23% | 1.11% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 412 | 8.32% | 34.8 B | 7.8 B | 31.6 us | 2.6 us | 5010 MHz | - | 10.0 dBm | 2.62% | 1.30% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 3 | 0.06% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.12% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.03% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.12% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.08% | 0.04% |

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

### [script] Analysis of Packet Distribution
`EdcaBaseline` provides the non-triggered control. The three scheduled configurations contain repeated **Trigger** frames, solicited HE-TB observations, and AP **Block Ack** responses, which is the expected HE UL-MU exchange structure (IEEE Std 802.11-2024, Clause 26.5.2; see informative Annex G.5). Frame-subtype totals alone do not establish that queued payload was carried in the solicited responses or distinguish the two scheduler policies. Use decoded Trigger user allocations, HE-TB payload observations, and aligned scheduler/application telemetry for those decisions.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->

## [agent] Frame exchange analysis

The generated scheduled timelines show the decisive order: an AP Trigger,
simultaneous HE-TB responses on decoded RUs, and an AP Block Ack. Early
responses can be QoS Null while the warm-up establishes state; later scheduled
responses carry QoS Data. The asymmetric case exposes several RU sizes, while
the equal-sized policy shows 26-tone responses in its representative
timeline. These are direct run-0 observations, not proof of the scheduler's
internal reason for choosing an RU.

The EDCA timeline instead shows station-originated HE-SU data without the
Trigger prefix. Frame numbers are local to each AP capture and are not
OMNeT++ event numbers.

## [agent] Cross-layer findings and verdict

`PASS`: in session `20260729T083529Z`, all four configurations have five
scalar/vector runs and a matched run-0 AP capture. The three scheduled
configurations show Trigger-led HE-TB exchanges, while `EdcaBaseline` shows
the non-triggered HE-SU control.

`INCONCLUSIVE`: the measured application outcomes do not establish a general
scheduler ranking. EDCA performs best for delivery and p95 delay in this
matched workload, EqualSizedRUs has wide run-to-run uncertainty, and
AsymmetricBacklog changes both workload and measurement window. None of those
outcomes identifies why a scheduler chose an RU.

## [agent] Limitations and inconclusive claims

- Five repetitions support a bounded example comparison, not a general claim
  about all channels, loads, distances, or random seeds.
- The MU-EDCA timer is configured but not separately recorded as a state vector;
  activation is therefore configuration evidence, not a direct per-packet
  observation.
- AP PCAP counts do not equal delivered application-packet counts and cannot
  establish internal scheduler decisions by themselves.
- The smallest useful attribution check is aligned Trigger User Info plus AP
  scheduler telemetry for the same run-0 events.
