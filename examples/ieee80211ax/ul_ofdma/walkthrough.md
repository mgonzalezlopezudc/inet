# Walkthrough: IEEE 802.11ax scheduled uplink OFDMA

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260729T165048Z`
- PCAP: `20260729T165048Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `20260729T103037Z`.

This four-configuration experiment shows that enabling scheduled uplink OFDMA
changes the observed exchange from station-contended HE-SU traffic to
Trigger-led HE-TB responses. In session `20260729T103037Z`, the scheduled
configurations contain that exchange while the EDCA control does not. The
application results compare delivery and tail delay. Aligned run-0 telemetry
and decoded Trigger User Info additionally verify each emitted per-user RU
allocation, but do not establish that one scheduler policy is generally best.

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
`muCwMax=1023`, and `muEdcaTimer=2.089s` in the
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
  --session-id 20260729T103037Z
```

The session contains runs `[0,5)` for each of the four configurations. Run 0
also records the AP `wlan[0]` PCAP from the same simulation trajectory. Report
and publish that retained session with:

```sh
MPLCONFIGDIR=/tmp/matplotlib \
python3 examples/ieee80211/analysis/wifi_analysis.py report ul_ofdma \
  --suite ax --session-id 20260729T103037Z

MPLCONFIGDIR=/tmp/matplotlib \
python3 examples/ieee80211/analysis/wifi_analysis.py publish ul_ofdma \
  --suite ax --session-id 20260729T103037Z --update
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
`results/20260729T103037Z/{configuration}/` for runs 0–4.

The figure answers two separate questions: how much application traffic
reached the sink and what p95 delay the delivered packets experienced. In this
session the EDCA control has the highest goodput and lowest p95 delay. The
scheduled cases still prove the UL-MU mechanism, but this workload does not
show a delivery or delay advantage for it. The wide confidence interval for
the equal-sized scheduler also cautions against ranking the two schedulers
from five runs.

<!-- BEGIN GENERATED: ieee80211-scalar-vector-ul_ofdma -->
### [script] Generated scalar/vector plot and table

![ul_ofdma scalar/vector analysis](results/20260729T165048Z/ul-ofdma-delivery-delay.png)

Figure provenance: [`results/20260729T165048Z/ul-ofdma-delivery-delay.png.json`](results/20260729T165048Z/ul-ofdma-delivery-delay.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.server.app[*] / packetReceived:vector(packetBytes)<br>vector / **.server.app[*] / endToEndDelay:vector / unit=s
- Window / per-run aggregation / exclusions: [0.3, 0.8) s, [0.3, 0.95) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| Asymmetric backlog / delay p95 ms | 51.5012 | 21.9485 |
| Asymmetric backlog / goodput mbps | 3.27434 | 0.577582 |
| Backlog scheduler / delay p95 ms | 137.162 | 150.31 |
| Backlog scheduler / goodput mbps | 3.00332 | 0.786488 |
| EDCA baseline / delay p95 ms | 4.53967 | 0.228952 |
| EDCA baseline / goodput mbps | 4.78523 | 0.00540301 |
| Equal-sized RUs / delay p95 ms | 198.84 | 162.795 |
| Equal-sized RUs / goodput mbps | 2.04972 | 0.191622 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **INCONCLUSIVE** | Every committed Basic Trigger per-user allocation matches the decoded PCAP AID and RU allocation | BacklogBased: found 205 committed Basic Triggers and 204 decoded PCAP Triggers |
| **INCONCLUSIVE** | Scheduled uplink OFDMA and EDCA are compared using delivered application bytes | The five-run delivery comparison has no manifest-defined acceptance threshold. |

#### [script] Joined Basic Trigger allocation evidence

| Config | Time (s) / Trigger / user | AID | Reported / planned bytes | Model RU | PCAP RU field → decoded RU | Match |
|---|---|---:|---:|---|---|---|
| AsymmetricBacklog | 0.328 / 6 / 0 | 1 | 5152 / 5152 | 106@0 | 53 → 106@0 | PASS |
| AsymmetricBacklog | 0.33146807362100006 / 7 / 0 | 1 | 5152 / 5152 | 106@0 | 53 → 106@0 | PASS |
| AsymmetricBacklog | 0.334 / 8 / 0 | 1 | 5152 / 5152 | 106@0 | 53 → 106@0 | PASS |
| AsymmetricBacklog | 0.33846803335600006 / 9 / 0 | 1 | 5152 / 5152 | 106@0 | 53 → 106@0 | PASS |
| AsymmetricBacklog | 0.34327509029900005 / 10 / 0 | 2 | 10304 / 10304 | 242@0 | 61 → 242@0 | PASS |
| AsymmetricBacklog | 0.368 / 11 / 0 | 1 | 5152 / 5152 | 106@0 | 53 → 106@0 | PASS |
| AsymmetricBacklog | 0.37 / 12 / 0 | 1 | 5152 / 5152 | 106@0 | 53 → 106@0 | PASS |
| AsymmetricBacklog | 0.372 / 13 / 0 | 1 | 12848 / 12848 | 242@0 | 61 → 242@0 | PASS |
| AsymmetricBacklog | 0.374 / 14 / 0 | 1 | 12848 / 12848 | 242@0 | 61 → 242@0 | PASS |
| AsymmetricBacklog | 0.37813405694300006 / 15 / 0 | 1 | 12848 / 9040 | 106@0 | 53 → 106@0 | PASS |
| AsymmetricBacklog | 0.37813405694300006 / 15 / 1 | 2 | 7728 / 7728 | 106@136 | 54 → 106@136 | PASS |
| AsymmetricBacklog | 0.3844831236550001 / 16 / 0 | 1 | 30912 / 20864 | 242@0 | 61 → 242@0 | PASS |

Showing 12 of 640 joined users; the session-bound evidence ledger retains every observation.
<!-- END GENERATED: ieee80211-scalar-vector-ul_ofdma -->
## [agent] PCAP statistics

The shared AX PCAP analyzer used the four AP captures from session
`20260729T103037Z`, with PCAPng/radiotap input and TShark decoding. Generated
statistics, composition plots, evidence checks, and representative timelines
are inserted below by the shared publisher. Counts are capture observations;
unknown PHY fields remain unknown and estimated HE-TB airtime is explicitly
limited by fields not exposed in radiotap.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260729T165048Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260729T165048Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260729T165048Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260729T165048Z.json` (SHA-256 `4caa7b20fd9b3095300383d00611bec8163561e330bfbed1783f92dfcb4bec20`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `AsymmetricBacklog` | `none (all decoded frames)` | 1890 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (1017), Control: Block Ack (BA) (238), Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (181) | 29.62% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `BacklogBased` | `none (all decoded frames)` | 4195 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (2203), Data: QoS Data [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (608), Control: Block Ack (BA) (325) | 48.74% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `EdcaBaseline` | `none (all decoded frames)` | 5990 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (3625), Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (947), Control: Block Ack Request (BAR) (723) | 52.89% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `EqualSizedRUs` | `none (all decoded frames)` | 3252 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (1522), Control: Block Ack (BA) (482), Data: QoS Null [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, BCC, A-MPDU] (314) | 46.61% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | AsymmetricBacklog produced protocol-visible wireless observations | 1890 AP/global transmission observations |
| **PASS** | BacklogBased produced protocol-visible wireless observations | 4195 AP/global transmission observations |
| **PASS** | EdcaBaseline produced protocol-visible wireless observations | 5990 AP/global transmission observations |
| **PASS** | EqualSizedRUs produced protocol-visible wireless observations | 3252 AP/global transmission observations |

### [script] Configuration: `AsymmetricBacklog`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1890**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 1017 | 53.81% | 322.8 B | 1.6 B | 181.4 us | 13.0 us | 5010 MHz | -50.0 dBm | - | 62.28% | 18.45% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 181 | 9.58% | 326.0 B | 0.0 B | 214.3 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 13.10% | 3.88% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39c224" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 50 | 2.65% | 323.8 B | 2.0 B | 422.9 us | 20.4 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 7.14% | 2.11% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31d831" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 119 | 6.30% | 323.7 B | 2.0 B | 192.8 us | 18.9 us | 5010 MHz | -75.0 dBm | - | 7.75% | 2.29% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24a331" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 12 | 0.63% | 326.0 B | 0.0 B | 905.3 us | 0.0 us | 5003 MHz | -75.0 dBm | - | 3.67% | 1.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 6 | 0.32% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.81% | 0.24% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 117 | 6.19% | 35.9 B | 7.9 B | 32.0 us | 2.6 us | 5010 MHz | - | 10.0 dBm | 1.26% | 0.37% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 135 | 7.14% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 1.28% | 0.38% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 238 | 12.59% | 33.7 B | 4.1 B | 31.2 us | 1.4 us | 5010 MHz | - | 10.0 dBm | 2.51% | 0.74% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 3 | 0.16% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.02% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.32% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.05% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.32% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.14% | 0.04% |

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
| 176 | 0.338504000 | 0 | #0: AID=1, RU=53, MCS=1, target RSSI=35 |
| 186 | 0.343311000 | 0 | #0: AID=2, RU=61, MCS=1, target RSSI=35 |
| 273 | 0.368036000 | 0 | #0: AID=1, RU=53, MCS=1, target RSSI=35 |
| 279 | 0.370036000 | 0 | #0: AID=1, RU=53, MCS=1, target RSSI=35 |

Showing the first 12 of 117 decoded Trigger frames; the script-owned packet metrics JSON preserves every row.

### [script] Configuration: `BacklogBased`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **4195**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 2203 | 52.51% | 166.3 B | 1.1 B | 92.9 us | 8.7 us | 5010 MHz | -50.0 dBm | - | 42.01% | 20.47% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 252 | 6.01% | 170.0 B | 0.0 B | 129.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 6.67% | 3.25% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39c224" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 608 | 14.49% | 166.7 B | 1.5 B | 215.6 us | 15.7 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 26.90% | 13.11% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31d831" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 17 | 0.41% | 166.7 B | 1.5 B | 97.5 us | 14.6 us | 5010 MHz | -75.0 dBm | - | 0.34% | 0.17% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#18a523" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 61 | 1.45% | 170.0 B | 0.0 B | 942.7 us | 0.0 us | 5002 MHz, 5010 MHz | -75.0 dBm | - | 11.80% | 5.75% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24a331" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 1 | 0.02% | 170.0 B | 0.0 B | 489.3 us | 0.0 us | 5003 MHz | -75.0 dBm | - | 0.10% | 0.05% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 6 | 0.14% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.49% | 0.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1a620f" /></svg> | Data: QoS Null [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 228 | 5.44% | 34.0 B | 0.0 B | 78.7 us | 0.0 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 3.68% | 1.79% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#148014" /></svg> | Data: QoS Null [HE-TB, HE-MCS 1, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 18 | 0.43% | 34.0 B | 0.0 B | 54.6 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 0.20% | 0.10% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#07400c" /></svg> | Data: QoS Null [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 64 | 1.53% | 34.0 B | 0.0 B | 217.3 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 2.85% | 1.39% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 208 | 4.96% | 43.1 B | 6.2 B | 34.4 us | 2.1 us | 5010 MHz | - | 10.0 dBm | 1.47% | 0.71% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 125 | 2.98% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 0.72% | 0.35% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 325 | 7.75% | 44.3 B | 11.7 B | 34.8 us | 3.9 us | 5010 MHz | - | 10.0 dBm | 2.32% | 1.13% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 67 | 1.60% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.34% | 0.17% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.14% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.03% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.14% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.09% | 0.04% |

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
| 33 | 0.301048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 139 | 0.313931000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 175 | 0.317907000 | 0 | #0: AID=2, RU=0, MCS=1, target RSSI=35 |
| 205 | 0.322397000 | 0 | #0: AID=3, RU=0, MCS=1, target RSSI=35 |
| 245 | 0.327857000 | 0 | #0: AID=3, RU=0, MCS=1, target RSSI=35 |
| 266 | 0.331150000 | 0 | #0: AID=3, RU=61, MCS=1, target RSSI=35 |
| 310 | 0.337070000 | 0 | #0: AID=2, RU=61, MCS=1, target RSSI=35 |
| 539 | 0.377508000 | 0 | #0: AID=2, RU=0, MCS=1, target RSSI=35 |
| 549 | 0.380036000 | 0 | #0: AID=2, RU=0, MCS=1, target RSSI=35 |
| 562 | 0.382036000 | 0 | #0: AID=2, RU=0, MCS=1, target RSSI=35 |

Showing the first 12 of 208 decoded Trigger frames; the script-owned packet metrics JSON preserves every row.

### [script] Configuration: `EdcaBaseline`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **5990**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 3625 | 60.52% | 167.2 B | 1.8 B | 100.7 us | 16.6 us | 5010 MHz | -50.0 dBm | - | 69.01% | 36.50% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 947 | 15.81% | 170.0 B | 0.0 B | 129.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 23.10% | 12.22% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 723 | 12.07% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 3.83% | 2.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 680 | 11.35% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 3.94% | 2.09% |
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

#### [script] Decoded HE Trigger user allocations

No HE Trigger User Info fields were decoded.

### [script] Configuration: `EqualSizedRUs`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **3252**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 1522 | 46.80% | 166.9 B | 1.7 B | 98.1 us | 15.0 us | 5010 MHz | -50.0 dBm | - | 32.05% | 14.94% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 229 | 7.04% | 170.0 B | 0.0 B | 129.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 6.34% | 2.95% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28af31" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 196 | 6.03% | 170.0 B | 0.0 B | 942.7 us | 0.0 us | 5002 MHz, 5004 MHz | -75.0 dBm | - | 39.64% | 18.48% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 6 | 0.18% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.51% | 0.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#105114" /></svg> | Data: QoS Null [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 314 | 9.66% | 34.0 B | 0.0 B | 217.3 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 14.64% | 6.82% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 267 | 8.21% | 40.4 B | 4.8 B | 33.5 us | 1.6 us | 5010 MHz | - | 10.0 dBm | 1.92% | 0.89% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 221 | 6.80% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 1.33% | 0.62% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 482 | 14.82% | 39.8 B | 8.2 B | 33.3 us | 2.7 us | 5010 MHz | - | 10.0 dBm | 3.44% | 1.60% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 3 | 0.09% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.02% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.18% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.03% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.18% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.09% | 0.04% |

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
| 33 | 0.301048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 139 | 0.313931000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 175 | 0.317907000 | 0 | #0: AID=2, RU=0, MCS=1, target RSSI=35 |
| 182 | 0.320055000 | 0 | #0: AID=2, RU=0, MCS=1, target RSSI=35; #1: AID=3, RU=1, MCS=1, target RSSI=35 |
| 205 | 0.323658000 | 0 | #0: AID=2, RU=0, MCS=1, target RSSI=35 |
| 263 | 0.329239000 | 0 | #0: AID=2, RU=0, MCS=1, target RSSI=35 |
| 282 | 0.332458000 | 0 | #0: AID=1, RU=0, MCS=1, target RSSI=35; #1: AID=2, RU=1, MCS=1, target RSSI=35 |
| 292 | 0.335036000 | 0 | #0: AID=2, RU=0, MCS=1, target RSSI=35 |
| 325 | 0.338518000 | 0 | #0: AID=2, RU=0, MCS=1, target RSSI=35 |
| 335 | 0.341036000 | 0 | #0: AID=2, RU=0, MCS=1, target RSSI=35 |

Showing the first 12 of 267 decoded Trigger frames; the script-owned packet metrics JSON preserves every row.

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

`PASS`: in session `20260729T103037Z`, all four configurations have five
scalar/vector runs and a matched run-0 AP capture. The three scheduled
configurations show Trigger-led HE-TB exchanges, while `EdcaBaseline` shows
the non-triggered HE-SU control. Across the scheduled configurations, all 756
committed per-user Basic Trigger allocations match the decoded transmitted
AID and RU geometry one-to-one.

`INCONCLUSIVE`: the measured application outcomes do not establish a general
scheduler ranking. EDCA performs best for delivery and p95 delay in this
matched workload, EqualSizedRUs has wide run-to-run uncertainty, and
AsymmetricBacklog changes both workload and measurement window. None of those
application outcomes alone explains why a scheduler chose an RU.

## [agent] Limitations and inconclusive claims

- Five repetitions support a bounded example comparison, not a general claim
  about all channels, loads, distances, or random seeds.
- The MU-EDCA timer is configured but not separately recorded as a state vector;
  activation is therefore configuration evidence, not a direct per-packet
  observation.
- AP PCAP counts do not equal delivered application-packet counts, and PCAP
  fields alone do not expose the scheduler's backlog inputs or rationale.
- The model-only Trigger ID is not transmitted over the air. The executable
  check therefore requires equal counts and uses an order-preserving causal
  join, accepting only a 0–1 ms commit-to-capture delay before comparing every
  user ordinal, AID, RU size, and RU offset.
