# Walkthrough: IEEE 802.11ax scheduled uplink OFDMA

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260728T161000Z`
- PCAP: `20260728T161000Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `20260728T161000Z`.

This walkthrough uses the current four-configuration UL-OFDMA experiment. It
compares two AP schedulers with a single-user EDCA control and an asymmetric
backlog stress case. The scalar/vector and run-0 AP PCAP evidence were
co-recorded in session `20260728T161000Z`.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- explain how a Basic Trigger solicits simultaneous uplink HE-TB responses;
- distinguish scheduled HE-TB traffic from ordinary HE-SU EDCA traffic;
- identify the effect of the current SU and MU contention parameters; and
- compare delivered goodput and tail delay without treating either as proof of
  a particular RU allocation.

Uplink Orthogonal Frequency-Division Multiple Access (OFDMA) lets an access
point (AP) divide a channel into resource units (RUs). The AP wins channel
access, sends a Basic Trigger containing per-station user information, and
eligible stations respond after SIFS with aligned HE trigger-based (HE-TB)
PPDUs. The AP then completes the modeled exchange with acknowledgment traffic.
An ordinary EDCA transmission is instead a station-originated HE single-user
(HE-SU) exchange with no preceding Basic Trigger.

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
random-access RUs, and enables Block Ack and MPDU aggregation. A warm-up app
runs at 0.2 s; the measured app starts at 0.3 s and sends 100-byte packets
every 1 ms. The default measurement window is `[0.3,0.95)` s. The asymmetric
case uses 256-byte packets and 1/2/4 ms intervals for hosts 0/1/2 and measures
`[0.3,0.8)` s.

Port 5000 is classified as UP_VO by `ExampleQosClassifier`. The baseline SU
EDCA values are therefore the HE VO defaults (AIFSN/CWmin/CWmax `2/3/7`).
The current MU-EDCA settings are explicitly `muAifsn=10`, `muCwMin=255`,
`muCwMax=1023`, and `muEdcaTimer=100ms` at
[`omnetpp.ini:64`](omnetpp.ini:64). MU values apply while the MU-EDCA timer is
active; `EdcaBaseline` disables UL MU-OFDMA, so its ordinary EDCA path is the
relevant control. These settings are input evidence; the campaign results and
captures establish what exchanges occurred.

## [agent] Standards and INET model boundary

IEEE Std 802.11-2024 Clause 26.5.2 specifies Trigger-based solicitation and
HE-TB responses; Clause 26.5.2.3 covers the SIFS response timing and Trigger
User Info parameters. INET models this with `HeHcf`, the UL coordinator, and
replaceable `HeUlScheduler*` policies. IEEE defines legal frame fields and
timing, but not INET's scheduler objective, backlog scoring, fairness order, or
tie-breaking policy.

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

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| UL-MU feature gate changes the wireless exchange | `PASS` | Fresh AP PCAPs contain Trigger/HE-TB observations for the three scheduled configurations and none for EDCA | run 0 / seed 0 | Direct capture observation |
| Four current configurations are analyzed | `PASS` | Suite and scalar/vector manifests; fresh session has four configuration directories and 5 runs each | runs 0–4 / seeds 0–4 | The removed legacy configuration is absent |
| Delivered application performance is recorded | `PASS` | `packetReceived:vector(packetBytes)` and `endToEndDelay:vector` | 20 runs | Windowed per-run aggregation |
| Scheduler policy differences are exposed by outcomes | `INCONCLUSIVE` | Goodput and delay differ, but application outcomes do not identify each Trigger's RU decision | 5 runs/configuration | Use Trigger fields and scheduler telemetry for attribution |
| Current SU/MU contention inputs are documented | `PASS` | Effective INI assignments and current configuration matrix below | all configurations | MU timer activation is not separately recorded |

## [agent] Configuration matrix

| Configuration | Role | Effective scheduler / UL mode | Contention delta | Workload | Expected invariant |
|---|---|---|---|---|---|
| `BacklogBased` | Scheduled treatment | `HeUlSchedulerBacklogBased`; UL MU-OFDMA enabled | SU `2/3/7`; MU `10/255/1023`, 100 ms | 3 × 100 B every 1 ms | Trigger-led HE-TB exchange |
| `EqualSizedRUs` | Scheduled policy control | `HeUlSchedulerEqualSizedRUs`; UL MU-OFDMA enabled | same | matched | Trigger-led exchange with equal-sized RU policy |
| `EdcaBaseline` | Negative control | UL MU-OFDMA disabled; HE-SU EDCA | SU `2/3/7`; MU settings inactive for the disabled trigger path | matched | no Basic Trigger or HE-TB response |
| `AsymmetricBacklog` | Stress/discriminator | inherits `BacklogBased`; UL MU-OFDMA enabled | explicitly SU/MU `10/255/1023`; MU timer 100 ms | 256 B every 1/2/4 ms | unequal backlog inputs remain observable in outcomes and captures |

All four configurations use AX 20 MHz, fixed HE MCS 1/NSS 1, three stations,
zero random-access RUs, and the same topology. The only intended comparison
deltas are scheduler type, UL-MU enablement, and the asymmetric workload plus
its explicit SU contention values.

## [agent] Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| Scheduled configurations produce Trigger → HE-TB ordering | AP PCAP and generated frame timeline | no Trigger or no solicited HE-TB response | `HeHcf` / UL coordinator | inspect effective `enableUlMuOfdma` and AP scheduler initialization |
| EDCA control remains non-triggered | EDCA AP PCAP | Trigger/HE-TB appears in baseline | configuration precedence or coordinator enablement | inspect effective INI and AP MAC type |
| Trigger users have legal, non-overlapping RUs | decoded Trigger User Info and HE-TB PHY observations | duplicate/missing RU or mismatched response | scheduler/Trigger serialization | inspect RU telemetry and Trigger fields |
| Delay comparison uses delivered packets only | sink `endToEndDelay` vectors in `[0.3,end)` | empty or non-sink delay vectors | result recording/filtering | query module paths and units before aggregation |

## [agent] Reproduction

Run from the repository root. The minimal smoke run is:

```sh
bin/inet -u Cmdenv \
  -f examples/ieee80211ax/ul_ofdma/omnetpp.ini \
  -c BacklogBased -r 0 --seed-set=0 \
  --result-dir="$PWD/examples/ieee80211ax/ul_ofdma/results/manual/BacklogBased"
```

The evidence-producing campaign was executed in release mode with exit status
0 using Cmdenv, 20 runs total, run numbers `[0,5)`, seed sets 0–4, and AP
`wlan[0]` PCAPng for run 0:

```sh
MPLCONFIGDIR=/tmp/matplotlib \
python3 examples/ieee80211/analysis/wifi_analysis.py run ul_ofdma \
  --suite ax --evidence both --runs 5 --jobs "$(nproc)" \
  --session-id 20260728T161000Z
```

The report and publication commands were:

```sh
MPLCONFIGDIR=/tmp/matplotlib \
python3 examples/ieee80211/analysis/wifi_analysis.py report ul_ofdma \
  --suite ax --session-id 20260728T161000Z

MPLCONFIGDIR=/tmp/matplotlib \
python3 examples/ieee80211/analysis/wifi_analysis.py publish ul_ofdma \
  --suite ax --session-id 20260728T161000Z --update
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

The generated presentation bundle contains the compact table and the new
goodput-plus-delay figure:

```text
results/20260728T161000Z/ul-ofdma-delivery-delay.png
results/20260728T161000Z/ul-ofdma-delivery-delay.png.json
```

The figure answers two separate questions: how much application traffic was
delivered, and how the scheduler/contending access affected tail delay. It is
an outcome comparison, not direct RU-allocation evidence.

Input result artifacts are the `.sca`/`.vec` pairs under
`examples/ieee80211ax/ul_ofdma/results/20260728T161000Z/{BacklogBased,EqualSizedRUs,EdcaBaseline,AsymmetricBacklog}/`.
The exact native result queries are `packetReceived:vector(packetBytes)` and
`endToEndDelay:vector` on `**.app[*]`, filtered to sink modules.

<!-- BEGIN GENERATED: ieee80211-scalar-vector-ul_ofdma -->
### [script] Generated scalar/vector plot and table

![ul_ofdma scalar/vector analysis](results/20260728T161000Z/ul-ofdma-delivery-delay.png)

Figure provenance: [`results/20260728T161000Z/ul-ofdma-delivery-delay.png.json`](results/20260728T161000Z/ul-ofdma-delivery-delay.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.server.app[*] / packetReceived:vector(packetBytes)<br>vector / **.server.app[*] / endToEndDelay:vector / unit=s
- Window / per-run aggregation / exclusions: [0.3, 0.8) s, [0.3, 0.95) s; delay=pool delivered-packet delays within each run over each manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over each manifest measurement window, convert to bit/s; one value per run; uncertainty=95% Student-t CI across independent runs
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| Asymmetric backlog / delay p95 ms | 52.5255 | 27.7835 |
| Asymmetric backlog / goodput mbps | 3.43245 | 0.122114 |
| Backlog scheduler / delay p95 ms | 75.9393 | 56.424 |
| Backlog scheduler / goodput mbps | 2.29982 | 0.126359 |
| EDCA baseline / delay p95 ms | 5.59804 | 0.0331399 |
| EDCA baseline / goodput mbps | 2.38818 | 0.00635629 |
| Equal-sized RUs / delay p95 ms | 40.7444 | 7.97452 |
| Equal-sized RUs / goodput mbps | 2.32738 | 0.0265573 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.
<!-- END GENERATED: ieee80211-scalar-vector-ul_ofdma -->
## [agent] PCAP statistics

The shared AX PCAP analyzer used the four AP captures from session
`20260728T161000Z`, with PCAPng/radiotap input and TShark decoding. Generated
statistics, composition plots, evidence checks, and representative timelines
are inserted below by the shared publisher. Counts are capture observations;
unknown PHY fields remain unknown and estimated HE-TB airtime is explicitly
limited by fields not exposed in radiotap.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260728T161000Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260728T161000Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260728T161000Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260728T161000Z.json` (SHA-256 `73e359a1a20f5ca54681c444c8e549795f4ab98203431f3fbdfc9d3a903a19ea`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `AsymmetricBacklog` | `none (all decoded frames)` | 1774 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (1000), Control: Block Ack (BA) (246), Control: Trigger (127) | 27.84% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `BacklogBased` | `none (all decoded frames)` | 2966 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (1765), Control: Block Ack (BA) (372), Control: Block Ack Request (BAR) (233) | 32.07% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `EdcaBaseline` | `none (all decoded frames)` | 4795 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (3042), Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (851), Control: Block Ack Request (BAR) (515) | 43.73% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `EqualSizedRUs` | `none (all decoded frames)` | 3364 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (2121), Control: Block Ack (BA) (324), Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (288) | 47.70% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | AsymmetricBacklog produced protocol-visible wireless observations | 1774 AP/global transmission observations |
| **PASS** | BacklogBased produced protocol-visible wireless observations | 2966 AP/global transmission observations |
| **PASS** | EdcaBaseline produced protocol-visible wireless observations | 4795 AP/global transmission observations |
| **PASS** | EqualSizedRUs produced protocol-visible wireless observations | 3364 AP/global transmission observations |

### [script] Configuration: `AsymmetricBacklog`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1774**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 1000 | 56.37% | 322.6 B | 1.5 B | 182.0 us | 13.8 us | 5010 MHz | -50.0 dBm | - | 65.38% | 18.20% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 111 | 6.26% | 326.0 B | 0.0 B | 214.3 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 8.54% | 2.38% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39c224" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 32 | 1.80% | 325.6 B | 1.2 B | 441.3 us | 12.0 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 5.07% | 1.41% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31d831" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 90 | 5.07% | 323.9 B | 2.0 B | 194.0 us | 19.1 us | 5010 MHz | -75.0 dBm | - | 6.27% | 1.75% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24a331" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 24 | 1.35% | 326.0 B | 0.0 B | 905.3 us | 0.0 us | 5003 MHz | -75.0 dBm | - | 7.80% | 2.17% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 9 | 0.51% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 1.29% | 0.36% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 127 | 7.16% | 35.3 B | 6.1 B | 31.8 us | 2.0 us | 5010 MHz | - | 10.0 dBm | 1.45% | 0.40% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 120 | 6.76% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 1.21% | 0.34% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 246 | 13.87% | 33.8 B | 3.8 B | 31.3 us | 1.3 us | 5010 MHz | - | 10.0 dBm | 2.76% | 0.77% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 3 | 0.17% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.03% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.34% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.05% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.34% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.15% | 0.04% |

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
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2966**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 1765 | 59.51% | 166.8 B | 1.6 B | 97.5 us | 14.4 us | 5010 MHz | -50.0 dBm | - | 53.63% | 17.20% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 226 | 7.62% | 170.0 B | 0.0 B | 129.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 9.09% | 2.92% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39c224" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 34 | 1.15% | 169.1 B | 1.7 B | 239.7 us | 17.4 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 2.54% | 0.81% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31d831" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 32 | 1.08% | 168.1 B | 2.0 B | 111.1 us | 19.1 us | 5010 MHz | -75.0 dBm | - | 1.11% | 0.36% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#18a523" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 38 | 1.28% | 170.0 B | 0.0 B | 942.7 us | 0.0 us | 5002 MHz, 5004 MHz | -75.0 dBm | - | 11.17% | 3.58% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24a331" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 95 | 3.20% | 168.7 B | 1.9 B | 473.6 us | 22.1 us | 5003 MHz, 5007 MHz | -75.0 dBm | - | 14.03% | 4.50% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 9 | 0.30% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 1.12% | 0.36% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 147 | 4.96% | 35.9 B | 7.3 B | 32.0 us | 2.4 us | 5010 MHz | - | 10.0 dBm | 1.47% | 0.47% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 233 | 7.86% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 2.03% | 0.65% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 372 | 12.54% | 33.6 B | 4.2 B | 31.2 us | 1.4 us | 5010 MHz | - | 10.0 dBm | 3.62% | 1.16% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 3 | 0.10% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.02% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.20% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.05% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.20% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.13% | 0.04% |

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
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **4795**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 3042 | 63.44% | 168.5 B | 1.9 B | 99.0 us | 14.5 us | 5010 MHz | -50.0 dBm | - | 68.84% | 30.10% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 851 | 17.75% | 170.0 B | 0.0 B | 129.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 25.10% | 10.98% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 515 | 10.74% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 3.30% | 1.44% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 372 | 7.76% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 2.61% | 1.14% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 3 | 0.06% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.02% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.13% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.03% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.13% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.10% | 0.04% |

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
| 22 | 0.301164000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |

Frame numbers are local to capture `EdcaBaseline-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `EqualSizedRUs`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **3364**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 2121 | 63.05% | 167.1 B | 1.8 B | 96.4 us | 13.1 us | 5010 MHz | -50.0 dBm | - | 42.84% | 20.44% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 288 | 8.56% | 170.0 B | 0.0 B | 129.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 7.79% | 3.71% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28af31" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 212 | 6.30% | 170.0 B | 0.0 B | 942.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 41.89% | 19.98% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 9 | 0.27% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.75% | 0.36% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#105114" /></svg> | Data: QoS Null [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 45 | 1.34% | 34.0 B | 0.0 B | 217.3 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 2.05% | 0.98% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 117 | 3.48% | 47.2 B | 5.5 B | 35.7 us | 1.8 us | 5010 MHz | - | 10.0 dBm | 0.88% | 0.42% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 233 | 6.93% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 1.37% | 0.65% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 324 | 9.63% | 41.4 B | 12.5 B | 33.8 us | 4.2 us | 5010 MHz | - | 10.0 dBm | 2.30% | 1.09% |
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

### [script] Analysis of Packet Distribution
`EdcaBaseline` provides the non-triggered control. The two scheduled configurations contain repeated **Trigger** frames, solicited HE-TB observations, and AP **Block Ack** responses, which is the expected HE UL-MU exchange structure (IEEE Std 802.11-2024, Clause 26.5.2 and Annex G.5). Frame-subtype totals alone do not establish that queued payload was carried in the solicited responses or distinguish the two scheduler policies. Use decoded Trigger user allocations, HE-TB payload observations, and aligned scheduler/application telemetry for those decisions.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->

## [agent] Frame exchange analysis

The representative scheduled exchange should be read as a protocol sequence:

| Step | Observation | Role |
|---|---|---|
| 1 | AP Basic Trigger with per-user AID/RU information | solicits coordinated uplink |
| 2 | aligned HE-TB responses from addressed stations | carries scheduled uplink response traffic or a permitted null response |
| 3 | AP acknowledgment, including Block Ack where decoded | closes the modeled exchange |

The EDCA control instead shows HE-SU station transmissions without the Trigger
prefix. Frame numbers are local to each AP capture and are not OMNeT++ event
numbers. Use the generated PCAP timeline for exact timestamps, decoded fields,
and configuration-specific observations.

## [agent] Cross-layer findings and verdict

- `PASS`: the current suite and experiment manifest contain exactly the four
  live configurations; no analysis script depends on the removed legacy
  configuration.
- `PASS`: all 20 fresh runs completed and recorded the requested delivery and
  delay vectors; run-0 AP captures were nonempty and decoded.
- `PASS`: scheduled configurations show the expected Trigger-led UL-MU path,
  while `EdcaBaseline` provides the non-triggered control.
- `PASS`: the delay plot is bound to the same session as the scalar/vector
  metrics and uses per-run aggregation over the documented windows.
- `INCONCLUSIVE`: the outcome differences do not alone prove which station
  received which RU or why one scheduler produced a given delay. Use decoded
  Trigger User Info and aligned scheduler telemetry for that claim.

## [agent] Limitations and inconclusive claims

- Five repetitions support a bounded example comparison, not a general claim
  about all channels, loads, distances, or random seeds.
- The MU-EDCA timer is configured but not separately recorded as a state vector;
  activation is therefore configuration evidence, not a direct per-packet
  observation.
- AP PCAP counts do not equal delivered application-packet counts and cannot
  establish internal scheduler decisions by themselves.
- No external interference, mobility, channel-width sweep, or UORA condition
  is included.

## [agent] Further experiments

- Repeat with one contention parameter changed at a time and compare the delay
  panel plus the EDCA backoff/Trigger timeline.
- Add a larger station count while holding the four current contention values
  fixed; inspect RU occupancy and tail-delay changes.
- Pair a focused scheduler-telemetry run with the PCAP to attribute each
  Trigger User Info entry to a scheduler decision.

## [agent] Artifact provenance

| Artifact | Session/path | Scope |
|---|---|---|
| Logical analysis session | `examples/ieee80211/analysis/generated/sessions/20260728T161000Z/session.json` | AX suite, `ul_ofdma`, both evidence families |
| Scalar/vector metrics | `examples/ieee80211ax/analysis/metrics.json` | four configurations, five runs each, native result API |
| Scalar/vector figure | `examples/ieee80211ax/ul_ofdma/results/20260728T161000Z/ul-ofdma-delivery-delay.png` and `.json` | goodput and p95 delay |
| PCAP manifest/report | `examples/ieee80211/analysis/generated/ax/capture_manifests/20260728T161000Z.json` and `packet_metrics.json` | AP `wlan[0]`, run 0 |
| Analysis definitions | `examples/ieee80211ax/analysis/experiments.json`, `examples/ieee80211/analysis/suites/ax.json` | current four-configuration matrix |
| Configuration | `examples/ieee80211ax/ul_ofdma/omnetpp.ini` | current SU/MU contention and workload inputs |
