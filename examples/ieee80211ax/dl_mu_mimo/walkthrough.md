# Walkthrough: IEEE 802.11ax downlink MU-MIMO

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260727T121100Z`
- PCAP: `20260727T121100Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `20260727T121100Z`.

This example contrasts downlink multi-user multiple-input multiple-output
(DL MU-MIMO) with a matched orthogonal frequency-division multiple access
(OFDMA) control, then varies CSI leakage and offered load. Five paired seeds
per condition establish the modeled spatial-stream allocation and application
outcome; co-recorded run-0 packet captures show the protocol-visible HE MU
data and sequential Block Ack exchange.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- distinguish MU-MIMO spatial separation from OFDMA frequency separation;
- identify the capabilities, sounding state, scheduler gate, and result
  vectors that make DL MU-MIMO observable in INET;
- recognize a representative HE MU data, Block Ack Request (BAR), and Block
  Ack (BA) sequence;
- reproduce the treatment/control campaign and its packet analysis; and
- diagnose the first missing prerequisite when spatial multiplexing does not
  occur.

OFDMA serves users on different resource units (RUs). MU-MIMO instead serves
multiple stations on the same RU by assigning each station a disjoint range of
spatial streams. A configuration name or an HE MU packet count is therefore
not sufficient proof: the decisive INET evidence is a same-timestamp group
with multiple station IDs, the same full-band RU, and non-overlapping
`[streamStartIndex, streamStartIndex + spatialStreams)` ranges.

The access point (AP) must advertise beamformer capability, have at least two
antennas and sounding dimensions, collect channel-state information (CSI) from
capable beamformees, and enable DL MU-MIMO in its hybrid coordination function
(HCF). In this example the AP then sends per-user aggregated MAC protocol data
units (A-MPDUs) in one High Efficiency (HE) multi-user physical-layer protocol
data unit (PPDU). With `sequentialBar`, it polls recipients one at a time using
BAR/BA exchanges.

## [agent] Scenario description

The [INI configuration](omnetpp.ini) instantiates
[`Lan80211AxDlOfdma`](../dl_ofdma_sched/Lan80211AxDlOfdma.ned), which extends
the common [`HeSingleBssNetwork`](../common/HeSingleBssNetwork.ned).

```text
server === 100 Gbit/s Ethernet === 4-antenna AP
                                      ))) host[0], 4 antennas
                                      ))) host[1], 4 antennas
                                      ))) host[2], 4 antennas
```

All wireless nodes are stationary in one basic service set (BSS). The channel
is 5 GHz, 20 MHz, with a scalar radio medium, 100 mW transmit power,
`-85 dBm` sensitivity, and a `4 dB` SNIR threshold. There is no mobility,
external interferer, or fading campaign.

The six server applications are two phases for the same three stations:

- apps 0-2 each send one 100-byte warm-up packet during `[0.2 s, 0.25 s)` to
  establish per-station MAC/Block Ack state;
- apps 3-5 send 1,000-byte packets from 0.3 s onward; aggregate offered
  loads of 24, 48, 72, and 96 Mbit/s use per-flow intervals of 1 ms, 0.5 ms,
  333.333333333333 us, and 0.25 ms respectively.

The outcome window is `[0.55 s, 0.88 s)`, after sounding and association
transients. All conditions use identical topology, capabilities, channel,
acknowledgment policy, run numbers, and seeds. At each load the OFDMA/MU-MIMO
pair changes only the MU gate. The extra 24 Mbit/s MU-MIMO row reduces CSI
leakage from 0.01 to 0.001 as a separate PHY-selection sensitivity check.

## [agent] Standards and INET model boundary

IEEE Std 802.11-2024 Clause 27.3.1.1 describes HE MU transmission. Clause
27.3.16.1 defines DL MU-MIMO as spatial multiplexing of different stations
within one RU using disjoint space-time-stream subsets. Clause 27.3.2.5 and
Tables 27-21 and 27-28 describe full-bandwidth HE MU signaling and per-user
information. Clauses 26.7.1 and 26.7.2 describe HE sounding and feedback.
Clause 10.3.2.13.1 describes non-immediate acknowledgment sequences using
explicit BARs. The searchable corpus evidence is
`80211ax-2024:chunk:10040`, `:10269`, `:10062`, `:10159`, `:10184`,
`:10185`, `:09844`, `:09845`, `:05094`, and `:05095`.

INET models the allocation at packet level. `HeDlSchedulerEqualSizedRUs`
groups eligible stations using CSI freshness and configured leakage, not a
waveform channel matrix or calculated antenna steering weights. The radio
records one ordered vector sample per scheduled user. Matching timestamps are
the current PPDU correlation key.

Radiotap exposes HE format, modulation and coding scheme (MCS), coding, guard
interval, number of space-time streams, and RU/bandwidth when their known bits
are present. It does not expose INET's `heStreamStartIndex`, a complete
HE-SIG-B spatial configuration, or beamforming weights. Packet evidence can
show HE MU traffic on the same 242-tone RU and the BAR/BA exchange, but the
result vectors are authoritative for non-overlapping stream allocation.

## [agent] Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| Treatment prerequisites, load, leakage, and feature gates are effective inputs | `PASS` | `omnetpp.ini` plus retained HCF/application parameters | all configs/runs | Configuration input; not alone proof of runtime use |
| Every MU treatment serves multiple users on disjoint spatial streams | `PASS` | AP `heStaId`, `heSpatialStreams`, and `heStreamStartIndex` vectors; evidence ledger | five MU configs × runs 0-4 | 226-327 qualifying PPDUs per run depending on load; zero overlaps |
| MU packet captures contain full-band HE MU data and sequential BAR/BA | `PASS` | AP PCAPng and packet report | all five MU configs, run 0 / seed 0 | Direct packet observation |
| OFDMA controls use frequency-separated RUs | `PASS` | AP PCAPng: HE MU MPDUs decoded on 106-tone RUs | four matched loads, run 0 / seed 0 | Packet-visible RU geometry |
| Lower leakage changes MU PHY selection at matched load | `PASS` | AP PCAPng MCS and airtime summaries | paired 24 Mbit/s configs, run 0 | MCS 4/5 at leakage 0.01 versus MCS 6/7 at 0.001; estimated airtime 66.27% versus 55.81% |
| Offered-load sweep exposes different saturation plateaus | `PASS` | application receive vectors in `[0.55,0.88)` | matched loads, runs 0-4 / seeds 0-4 | MU-MIMO plateaus at 49.309 Mbit/s; OFDMA at about 42.9-43.1 Mbit/s |
| MU-MIMO improves saturated-load aggregate goodput | `PASS` | paired application receive vectors | 48/72/96 Mbit/s, runs 0-4 | approximately +5.0, +6.4, and +6.3 Mbit/s over OFDMA |
| PCAP alone proves disjoint spatial streams | `INCONCLUSIVE` | radiotap lacks stream-start index | run 0 | Use result vectors |
| 80 MHz stress configuration behaves correctly | `NOT RUN` | no retained result/capture session | none | Requires a separate run and window |

## [agent] Configuration matrix

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| `EqualSizedRUs_fBW` | Control | Equal-sized-RU scheduler; `enableDlMuMimo=false` | three measured 8 Mbit/s flows, 20 MHz | 0-4 / 0-4 | users occupy different 106-tone RUs |
| `DlMuMimo` | Baseline treatment | `enableDlMuMimo=true`; CSI leakage 0.01 | matched 24 Mbit/s load, 20 MHz | 0-4 / 0-4 | users share 242-tone RU with disjoint streams |
| `DlMuMimo24MbpsLeakage001` | Leakage treatment | baseline with CSI leakage reduced to 0.001 | matched 24 Mbit/s load, 20 MHz | 0-4 / 0-4 | disjoint streams remain valid and MCS may increase |
| `EqualSizedRUs48Mbps` | Control | OFDMA; interval 0.5 ms | three measured 16 Mbit/s flows, 20 MHz | 0-4 / 0-4 | 106-tone RUs; saturation may begin |
| `DlMuMimo48MbpsLeakage01` | Treatment | MU-MIMO; leakage 0.01; interval 0.5 ms | matched 48 Mbit/s load, 20 MHz | 0-4 / 0-4 | disjoint full-band streams |
| `EqualSizedRUs72Mbps` | Control | OFDMA; interval 333.333333333333 us | three measured 24 Mbit/s flows, 20 MHz | 0-4 / 0-4 | 106-tone RUs under overload |
| `DlMuMimo72MbpsLeakage01` | Treatment | MU-MIMO; leakage 0.01; same interval | matched 72 Mbit/s load, 20 MHz | 0-4 / 0-4 | valid spatial grouping under overload |
| `EqualSizedRUs96Mbps` | Control | OFDMA; interval 0.25 ms | three measured 32 Mbit/s flows, 20 MHz | 0-4 / 0-4 | stable OFDMA saturation plateau |
| `DlMuMimo96MbpsLeakage01` | Treatment | MU-MIMO; leakage 0.01; same interval | matched 96 Mbit/s load, 20 MHz | 0-4 / 0-4 | stable MU-MIMO saturation plateau |
| `DlMuMimo80MHz` | Stress case | treatment at 80 MHz | matched three-station traffic; warm-up period 0.7 s | `NOT RUN` | shared wide-band RU and disjoint streams |

`[General]` supplies the common `HeHcf`,
`HeDlSchedulerEqualSizedRUs`, four AP and station antennas, AP beamformer and
four sounding dimensions, station beamformee/feedback capabilities, QoS,
aggregation, and Block Ack support. Every concrete 20 MHz configuration uses
`sequentialBar`. Each matched OFDMA/MU-MIMO pair changes only the MU gate;
load changes only through the measured-flow interval. CSI leakage is explicit
at 0.01 for every load-sweep MU treatment.

## [agent] Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| All MU prerequisites are effective | AP/STA MIB, antenna, HCF parameters | no sounding or only SU/OFDMA data | NED/INI precedence or capability negotiation | inspect effective configuration and `isDlMuMimoEligible()` inputs |
| Sounding precedes spatial service | AP capture: Trigger/action feedback before HE MU data | no CSI exchange or stale CSI | sounding/CSI manager | enable targeted `HeHcfDl` and CSI-manager logs |
| One PPDU has multiple users on one RU | co-timestamped AP `heStaId` and RU vectors | distinct RUs or only one station ID | DL scheduler | query AP HE allocation vectors for first divergent timestamp |
| Stream ranges never overlap | `heSpatialStreams` plus `heStreamStartIndex` | intersecting half-open ranges | scheduler plan construction | inspect the exact timestamp's allocation rows |
| Sequential acknowledgment completes | AP PCAP: HE MU data, BAR, BA, BAR, BA | missing or reordered response | HCF acknowledgment policy/MAC exchange | filter capture by frame time, RA/TA, BAR and BA |
| Outcome comparison is matched | run attrs, seeds, receive vectors | differing load/window/seed | experiment setup | inspect `.sca` metadata before comparing values |

## [agent] Reproduction

Run from the INET repository root. The non-terminating 72 Mbit/s interval was
first checked with release `bin/inet`, run 0/seed 0, and exit status 0:

```sh
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211/analysis/wifi_analysis.py run dl_mu_mimo \
  --suite ax --evidence scalar-vector --runs 1 \
  --config DlMuMimo72MbpsLeakage01 --jobs 1 \
  --session-id 20260727T121000Z
```

Its `.sca` file records an effective per-flow interval of
`0.00033333333333333294 s` and AP CSI leakage 0.01. The publication campaign
then executed 45 simulations (nine configurations times five runs). Every
simulation child exited 0 and retained scalar/vector results; run 0 also
retained AP packet captures:

```sh
MPLCONFIGDIR=/tmp/matplotlib \
XDG_CONFIG_HOME=/tmp/codex-wireshark-config \
  python3 examples/ieee80211/analysis/wifi_analysis.py run dl_mu_mimo \
  --suite ax --evidence both --runs 5 --jobs 10 \
  --session-id 20260727T121100Z

MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211/analysis/wifi_analysis.py report dl_mu_mimo \
  --suite ax --session-id 20260727T121100Z
```

The campaign wrapper exited 2 only when its sandboxed TShark post-processing
could not traverse `/home/user`. Read-only copies of session
`20260727T121100Z` were therefore decoded under
`/tmp/dl-mu-mimo-analysis.4vATlm/inet`; capture indexing and the shared
analyzer both exited 0. The retained manifest hashes refer to the original
repository captures, not to the temporary copies.

## [agent] Scalar and vector analysis

Inputs are the 45 `.sca`/`.vec` pairs under
[`results/20260727T121100Z`](results/20260727T121100Z). The result query used
by the shared analysis is:

```sh
opp_scavetool query -l \
  -f 'module =~ "**.ap.wlan[0].radio" and (name =~ "heRuToneSize:vector" or name =~ "heRuToneOffset:vector" or name =~ "heStaId:vector" or name =~ "heSpatialStreams:vector" or name =~ "heStreamStartIndex:vector")' \
  'examples/ieee80211ax/dl_mu_mimo/results/20260727T121100Z/DlMuMimo96MbpsLeakage01/DlMuMimo96MbpsLeakage01-#0.vec'
```

Goodput is calculated independently for each run by summing station
`packetReceived:vector(packetBytes)` samples in `[0.55 s, 0.88 s)`, converting
bytes to bits, and dividing by 0.33 s. Runs are paired by run number/seed.
The reported uncertainty is a 95% Student-t confidence interval across five
independent runs, never across vector samples.

| Configuration / invariant | Source result, module, unit | Window and per-run aggregation | Independent runs | Estimate |
|---|---|---|---:|---:|
| `DlMuMimo` goodput | `packetReceived:vector(packetBytes)`, `**.app[*]`; bytes inferred from `packetBytes`, native unit absent | target `[0.55,0.88)`; sum bytes × 8 / 0.33 s | 5, none excluded | 23.9709 ± 0.0495 Mbit/s |
| `DlMuMimo24MbpsLeakage001` goodput | same | same | 5, none excluded | 24.0000 ± 0 Mbit/s |
| `DlMuMimo48MbpsLeakage01` goodput | same | same | 5, none excluded | 48.0145 ± 0.0756 Mbit/s |
| `DlMuMimo72MbpsLeakage01` goodput | same | same | 5, none excluded | 49.3091 ± 0 Mbit/s |
| `DlMuMimo96MbpsLeakage01` goodput | same | same | 5, none excluded | 49.3091 ± 0 Mbit/s |
| `EqualSizedRUs_fBW` goodput | same | same | 5, none excluded | 24.0000 ± 0 Mbit/s |
| `EqualSizedRUs48Mbps` goodput | same | same | 5, none excluded | 43.0545 ± 0 Mbit/s |
| `EqualSizedRUs72Mbps` goodput | same | same | 5, none excluded | 42.8994 ± 0.1077 Mbit/s |
| `EqualSizedRUs96Mbps` goodput | same | same | 5, none excluded | 42.9770 ± 0.1319 Mbit/s |
| Baseline MU disjoint streams | AP radio HE allocation vectors | group equal timestamps; test half-open stream intervals | 5, none excluded | 303, 303, 303, 304, 303 PPDUs; zero overlaps |
| Low-leakage MU disjoint streams | same | same | 5, none excluded | 327 PPDUs in every run; zero overlaps |
| 48 Mbit/s MU disjoint streams | same | same | 5, none excluded | 231, 230, 231, 231, 230 PPDUs; zero overlaps |
| 72 Mbit/s MU disjoint streams | same | same | 5, none excluded | 227, 226, 227, 227, 227 PPDUs; zero overlaps |
| 96 Mbit/s MU disjoint streams | same | same | 5, none excluded | 227 PPDUs in every run; zero overlaps |

The matched paired differences and per-station fairness below are
agent-derived from the same raw sink vectors, window, and seed pairing; they
are not fields emitted by `metrics.json`. For each run, Jain fairness is
`(sum(x_i))^2 / (3 * sum(x_i^2))` over the three station goodputs, followed by
a 95% Student-t confidence interval across the five runs:

| Offered load | MU-MIMO minus OFDMA goodput, 95% CI | OFDMA Jain fairness, 95% CI | MU-MIMO Jain fairness, 95% CI |
|---:|---:|---:|---:|
| 24 Mbit/s | -0.029 ± 0.049 Mbit/s | 1.000000 ± 0 | 1.000000 ± 0 |
| 48 Mbit/s | +4.960 ± 0.076 Mbit/s | 1.000000 ± 0 | 1.000000 ± 0 |
| 72 Mbit/s | +6.410 ± 0.108 Mbit/s | 0.999924 ± 0.000011 | 1.000000 ± 0 |
| 96 Mbit/s | +6.332 ± 0.132 Mbit/s | 0.999917 ± 0.000013 | 1.000000 ± 0 |

Run-0 captures decode a full-band 242-tone RU for every MU treatment. The
representative AP allocation vectors record station IDs 1-3, one stream per
user, and stream-start indices 0-2. The executable five-run contracts assert
multi-user grouping and disjoint ranges; they do not separately assert the
RU size, offset, or exact station-ID set. The PPDU counts above apply only to
the measurement window.

At 24 Mbit/s both access methods are offered-load limited. At 48 Mbit/s the
OFDMA control has already saturated at 43.0545 Mbit/s while MU-MIMO still
carries approximately its offered load. At 72 and 96 Mbit/s both are
saturated: MU-MIMO plateaus at 49.3091 Mbit/s and OFDMA at about
42.9-43.1 Mbit/s. Thus the sweep brackets both capacity knees and directly
shows different modeled aggregate-goodput plateaus. The structural allocation
invariant also passes in every MU run.

<!-- BEGIN GENERATED: ieee80211-scalar-vector-mimo -->
### [script] Generated scalar/vector plot and table

![mimo scalar/vector analysis](results/20260727T121100Z/mu-mimo-spatial-stream-matrix.png)

Figure provenance: [`results/20260727T121100Z/mu-mimo-spatial-stream-matrix.png.json`](results/20260727T121100Z/mu-mimo-spatial-stream-matrix.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.ap.wlan[0].radio / heStaId:vector<br>vector / **.ap.wlan[0].radio / heSpatialStreams:vector<br>vector / **.ap.wlan[0].radio / heStreamStartIndex:vector<br>vector / **.app[*] / packetReceived:vector(packetBytes) / unit=B
- Window / per-run aggregation / exclusions: [0.55, 0.88) s; goodput=per run with 95% Student-t CI; telemetry=all PPDUs validated; representative run 0 plotted
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| MU-MIMO, 24 Mbps, leakage 0.01 / goodput mbps | 23.9709 | 0.0494609 |
| MU-MIMO, 24 Mbps, leakage 0.001 / goodput mbps | 24 | 0 |
| MU-MIMO, 48 Mbps, leakage 0.01 / goodput mbps | 48.0145 | 0.0755528 |
| MU-MIMO, 72 Mbps, leakage 0.01 / goodput mbps | 49.3091 | 0 |
| MU-MIMO, 96 Mbps, leakage 0.01 / goodput mbps | 49.3091 | 0 |
| OFDMA, 24 Mbps / goodput mbps | 24 | 0 |
| OFDMA, 48 Mbps / goodput mbps | 43.0545 | 0 |
| OFDMA, 72 Mbps / goodput mbps | 42.8994 | 0.107692 |
| OFDMA, 96 Mbps / goodput mbps | 42.977 | 0.131896 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.
<!-- END GENERATED: ieee80211-scalar-vector-mimo -->

## [agent] PCAP statistics

Capture point: AP `wlan[0]` MAC packet recorder.  
Format/precision: PCAPng with radiotap, microsecond timestamps.  
Recorder settings: `moduleNamePatterns="mac"`, computed checksum and FCS,
non-verbose recording.  
Decoder: TShark 4.6.4 and Capinfos 4.6.4.

Rows below count AP-interface frame/MPDU observations. They are not
de-duplicated medium transmissions or application deliveries.

| Configuration | Observation point and selection | Observations | Decisive frame/PHY facts | Interpretation limit |
|---|---|---:|---|---|
| `DlMuMimo` | AP `wlan[0]`; all decoded frames | 5,801 | 2,065 HE-MU QoS MPDUs on 242-tone RU, mostly MCS 4; 1,804 BAR; 1,803 BA | stream-start indices come from vectors |
| `DlMuMimo24MbpsLeakage001` | same | 6,102 | 2,061 HE-MU QoS MPDUs, mostly MCS 6; 1,952 BAR and BA | matched-load leakage treatment |
| `DlMuMimo48MbpsLeakage01` | same | 7,180 | 4,190 HE-MU QoS MPDUs, all MCS 4; 1,464 BAR and BA | matched load-sweep treatment |
| `DlMuMimo72MbpsLeakage01` | same | 7,256 | 4,320 HE-MU QoS MPDUs, all MCS 4; 1,437 BAR and BA | saturated MU treatment |
| `DlMuMimo96MbpsLeakage01` | same | 7,257 | 4,320 HE-MU QoS MPDUs, all MCS 4; 1,438 BAR and 1,437 BA | saturated MU treatment |
| `EqualSizedRUs_fBW` | AP `wlan[0]`; all decoded frames | 4,731 | 2,095 HE-MU QoS MPDUs on 106-tone RUs; 1,308 BAR and BA | HE-MU format alone does not distinguish OFDMA |
| `EqualSizedRUs48Mbps` | same | 5,652 | 3,752 HE-MU QoS MPDUs on 106-tone RUs, MCS 8; 940 BAR and BA | matched saturated OFDMA control |
| `EqualSizedRUs72Mbps` | same | 5,657 | 3,757 HE-MU QoS MPDUs on 106-tone RUs, MCS 8; 940 BAR and BA | saturated OFDMA control |
| `EqualSizedRUs96Mbps` | same | 5,658 | 3,758 HE-MU QoS MPDUs on 106-tone RUs, MCS 8; 940 BAR and BA | saturated OFDMA control |

The generated count-versus-airtime plot materially distinguishes the
full-band spatial treatments from the narrower-RU control. At matched load,
reducing modeled CSI leakage from 0.01 to 0.001 raises the dominant MCS from 4
to 6 and lowers estimated cumulative airtime from 66.27% to 55.81%. Increasing
load at leakage 0.01 leaves the MU-MIMO MCS at 4 and the OFDMA MCS at 8. Frame
counts flatten from 48 to 96 Mbit/s for OFDMA and from 72 to 96 Mbit/s for
MU-MIMO, consistent with the scalar saturation plateaus. Airtime is estimated
from radiotap fields; HE MU/TB user-dependent signaling remains approximate,
and cumulative multi-user airtime can exceed 100%.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260727T121100Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260727T121100Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260727T121100Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260727T121100Z.json` (SHA-256 `1e33f7fe3a2be8193f1a79a0689133340be577c73667f74a3ee918ba1f24f0fd`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `DlMuMimo` | `none (all decoded frames)` | 5801 | Data: QoS Data [HE-MU, HE-MCS 4, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] (1863), Control: Block Ack Request (BAR) (1804), Control: Block Ack (BA) (1803) | 66.27% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `DlMuMimo24MbpsLeakage001` | `none (all decoded frames)` | 6102 | Control: Block Ack Request (BAR) (1952), Control: Block Ack (BA) (1952), Data: QoS Data [HE-MU, HE-MCS 6, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] (1859) | 55.81% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `DlMuMimo48MbpsLeakage01` | `none (all decoded frames)` | 7180 | Data: QoS Data [HE-MU, HE-MCS 4, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] (4190), Control: Block Ack Request (BAR) (1464), Control: Block Ack (BA) (1464) | 101.83% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `DlMuMimo72MbpsLeakage01` | `none (all decoded frames)` | 7256 | Data: QoS Data [HE-MU, HE-MCS 4, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] (4320), Control: Block Ack Request (BAR) (1437), Control: Block Ack (BA) (1437) | 104.00% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `DlMuMimo96MbpsLeakage01` | `none (all decoded frames)` | 7257 | Data: QoS Data [HE-MU, HE-MCS 4, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] (4320), Control: Block Ack Request (BAR) (1438), Control: Block Ack (BA) (1437) | 104.01% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `EqualSizedRUs48Mbps` | `none (all decoded frames)` | 5652 | Data: QoS Data [HE-MU, HE-MCS 8, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (3752), Control: Block Ack Request (BAR) (940), Control: Block Ack (BA) (940) | 96.48% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `EqualSizedRUs72Mbps` | `none (all decoded frames)` | 5657 | Data: QoS Data [HE-MU, HE-MCS 8, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (3757), Control: Block Ack Request (BAR) (940), Control: Block Ack (BA) (940) | 96.59% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `EqualSizedRUs96Mbps` | `none (all decoded frames)` | 5658 | Data: QoS Data [HE-MU, HE-MCS 8, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (3758), Control: Block Ack Request (BAR) (940), Control: Block Ack (BA) (940) | 96.61% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `EqualSizedRUs_fBW` | `none (all decoded frames)` | 4731 | Data: QoS Data [HE-MU, HE-MCS 8, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (2095), Control: Block Ack Request (BAR) (1308), Control: Block Ack (BA) (1308) | 64.49% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | DlMuMimo produced protocol-visible wireless observations | 5801 AP/global transmission observations |
| **PASS** | DlMuMimo24MbpsLeakage001 produced protocol-visible wireless observations | 6102 AP/global transmission observations |
| **PASS** | DlMuMimo48MbpsLeakage01 produced protocol-visible wireless observations | 7180 AP/global transmission observations |
| **PASS** | DlMuMimo72MbpsLeakage01 produced protocol-visible wireless observations | 7256 AP/global transmission observations |
| **PASS** | DlMuMimo96MbpsLeakage01 produced protocol-visible wireless observations | 7257 AP/global transmission observations |
| **PASS** | EqualSizedRUs48Mbps produced protocol-visible wireless observations | 5652 AP/global transmission observations |
| **PASS** | EqualSizedRUs72Mbps produced protocol-visible wireless observations | 5657 AP/global transmission observations |
| **PASS** | EqualSizedRUs96Mbps produced protocol-visible wireless observations | 5658 AP/global transmission observations |
| **PASS** | EqualSizedRUs_fBW produced protocol-visible wireless observations | 4731 AP/global transmission observations |
| **INCONCLUSIVE** | Multiple users with disjoint stream allocations in one PPDU | The packet-type table is exchange evidence only; use the recorded feature vectors/results |

### [script] Configuration: `DlMuMimo`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **5801**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36d72d" /></svg> | Data: QoS Data [HE-MU, HE-MCS 4, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 1863 | 32.12% | 1066.0 B | 0.0 B | 225.4 us | 12.4 us | 5010 MHz | - | 20.0 dBm | 63.36% | 41.99% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#23a92c" /></svg> | Data: QoS Data [HE-MU, HE-MCS 5, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 202 | 3.48% | 1066.0 B | 0.0 B | 181.4 us | 3.6 us | 5010 MHz | - | 20.0 dBm | 5.53% | 3.66% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 38 | 0.66% | 994.9 B | 242.7 B | 580.2 us | 132.8 us | 5010 MHz | - | 20.0 dBm | 3.33% | 2.20% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 7 | 0.12% | 45.1 B | 2.1 B | 35.0 us | 0.7 us | 5010 MHz | - | 20.0 dBm | 0.04% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 1804 | 31.10% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 7.62% | 5.05% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 1803 | 31.08% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 5010 MHz | -66.0 dBm | - | 19.23% | 12.74% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 38 | 0.66% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -66.0 dBm | - | 0.14% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.10% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -66.0 dBm | 20.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 13 | 0.22% | 36.8 B | 0.5 B | 69.1 us | 0.7 us | 5010 MHz | -66.0 dBm | 20.0 dBm | 0.14% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#cb1a20" /></svg> | Management: Action [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, BCC] | 2 | 0.03% | 34.0 B | 0.0 B | 112.8 us | 0.0 us | 5005 MHz, 5015 MHz | -66.0 dBm | - | 0.03% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#de1c12" /></svg> | Management: Action [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, BCC] | 18 | 0.31% | 34.0 B | 0.0 B | 199.2 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -66.0 dBm | - | 0.54% | 0.36% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#82cc33" /></svg> | A-MPDU Delimiter / Aggregation Overhead | 7 | 0.12% | 0.0 B | 0.0 B | 20.0 us | 0.0 us | 5010 MHz | - | - | 0.02% | 0.01% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 19 | 0.300644000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 20 | 0.300692000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 23 | 0.300902000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 26 | 0.301499000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 5, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=2654436768 | Carries protocol-visible MAC payload in the representative exchange. |
| 27 | 0.301499000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 5, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=2654436768 | Carries protocol-visible MAC payload in the representative exchange. |
| 28 | 0.301499000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 5, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=1013905259 | Carries protocol-visible MAC payload in the representative exchange. |
| 29 | 0.301499000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 5, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=1013905259 | Carries protocol-visible MAC payload in the representative exchange. |
| 30 | 0.301547000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 31 | 0.301636000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 32 | 0.301684000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 33 | 0.301773000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 34 | 0.302641000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 35 | 0.302690000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 36 | 0.302991000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 5, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=2654436512 | Carries protocol-visible MAC payload in the representative exchange. |
| 37 | 0.302991000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 5, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=1013905003 | Carries protocol-visible MAC payload in the representative exchange. |
| 38 | 0.303039000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |

Frame numbers are local to capture `DlMuMimo-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `DlMuMimo24MbpsLeakage001`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **6102**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39c02a" /></svg> | Data: QoS Data [HE-MU, HE-MCS 6, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 1859 | 30.47% | 1066.0 B | 0.0 B | 163.5 us | 8.4 us | 5010 MHz | - | 20.0 dBm | 54.46% | 30.40% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39d03b" /></svg> | Data: QoS Data [HE-MU, HE-MCS 7, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 202 | 3.31% | 1066.0 B | 0.0 B | 152.3 us | 3.6 us | 5010 MHz | - | 20.0 dBm | 5.51% | 3.08% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 42 | 0.69% | 1001.7 B | 231.8 B | 583.9 us | 126.8 us | 5010 MHz | - | 20.0 dBm | 4.39% | 2.45% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 7 | 0.11% | 45.1 B | 2.1 B | 35.0 us | 0.7 us | 5010 MHz | - | 20.0 dBm | 0.04% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 1952 | 31.99% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 9.79% | 5.47% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 1952 | 31.99% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 5010 MHz | -66.0 dBm | - | 24.72% | 13.79% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 42 | 0.69% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -66.0 dBm | - | 0.19% | 0.10% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.10% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -66.0 dBm | 20.0 dBm | 0.03% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 13 | 0.21% | 36.8 B | 0.5 B | 69.1 us | 0.7 us | 5010 MHz | -66.0 dBm | 20.0 dBm | 0.16% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#cb1a20" /></svg> | Management: Action [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, BCC] | 2 | 0.03% | 34.0 B | 0.0 B | 112.8 us | 0.0 us | 5005 MHz, 5015 MHz | -66.0 dBm | - | 0.04% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#de1c12" /></svg> | Management: Action [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, BCC] | 18 | 0.29% | 34.0 B | 0.0 B | 199.2 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -66.0 dBm | - | 0.64% | 0.36% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#82cc33" /></svg> | A-MPDU Delimiter / Aggregation Overhead | 7 | 0.11% | 0.0 B | 0.0 B | 20.0 us | 0.0 us | 5010 MHz | - | - | 0.03% | 0.01% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 19 | 0.300644000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 20 | 0.300692000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 23 | 0.300902000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 26 | 0.301435000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 7, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=2654436768 | Carries protocol-visible MAC payload in the representative exchange. |
| 27 | 0.301435000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 7, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=2654436768 | Carries protocol-visible MAC payload in the representative exchange. |
| 28 | 0.301435000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 7, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=1013905259 | Carries protocol-visible MAC payload in the representative exchange. |
| 29 | 0.301435000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 7, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=1013905259 | Carries protocol-visible MAC payload in the representative exchange. |
| 30 | 0.301483000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 31 | 0.301572000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 32 | 0.301620000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 33 | 0.301709000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 34 | 0.302577000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 35 | 0.302626000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 36 | 0.302895000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 7, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=2654436512 | Carries protocol-visible MAC payload in the representative exchange. |
| 37 | 0.302895000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 7, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=1013905003 | Carries protocol-visible MAC payload in the representative exchange. |
| 38 | 0.302943000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |

Frame numbers are local to capture `DlMuMimo24MbpsLeakage001-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `DlMuMimo48MbpsLeakage01`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **7180**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36d72d" /></svg> | Data: QoS Data [HE-MU, HE-MCS 4, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 4190 | 58.36% | 1066.0 B | 0.0 B | 206.9 us | 17.2 us | 5010 MHz | - | 20.0 dBm | 85.15% | 86.71% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 4 | 0.06% | 391.0 B | 389.7 B | 249.9 us | 213.2 us | 5010 MHz | - | 20.0 dBm | 0.10% | 0.10% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 7 | 0.10% | 46.0 B | 0.0 B | 35.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.02% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 1464 | 20.39% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 4.03% | 4.10% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 1464 | 20.39% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 5010 MHz | -66.0 dBm | - | 10.16% | 10.35% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 4 | 0.06% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -66.0 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.08% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -66.0 dBm | 20.0 dBm | 0.01% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 13 | 0.18% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -66.0 dBm | 20.0 dBm | 0.09% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#de1c12" /></svg> | Management: Action [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, BCC] | 21 | 0.29% | 34.0 B | 0.0 B | 199.2 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -66.0 dBm | - | 0.41% | 0.42% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#82cc33" /></svg> | A-MPDU Delimiter / Aggregation Overhead | 7 | 0.10% | 0.0 B | 0.0 B | 20.0 us | 0.0 us | 5010 MHz | - | - | 0.01% | 0.01% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 19 | 0.300644000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 20 | 0.300692000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 23 | 0.300924000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 27 | 0.301936000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=2654436843 | Carries protocol-visible MAC payload in the representative exchange. |
| 28 | 0.301936000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=2654436843 | Carries protocol-visible MAC payload in the representative exchange. |
| 29 | 0.301936000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=1013905184 | Carries protocol-visible MAC payload in the representative exchange. |
| 30 | 0.301936000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=1013905184 | Carries protocol-visible MAC payload in the representative exchange. |
| 31 | 0.301936000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=1013905184 | Carries protocol-visible MAC payload in the representative exchange. |
| 32 | 0.301936000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=3668339065 | Carries protocol-visible MAC payload in the representative exchange. |
| 33 | 0.301936000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=3668339065 | Carries protocol-visible MAC payload in the representative exchange. |
| 34 | 0.301936000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=3668339065 | Carries protocol-visible MAC payload in the representative exchange. |
| 35 | 0.301984000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 36 | 0.302073000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 37 | 0.302121000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 38 | 0.302210000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 39 | 0.302258000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |

Frame numbers are local to capture `DlMuMimo48MbpsLeakage01-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `DlMuMimo72MbpsLeakage01`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **7256**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36d72d" /></svg> | Data: QoS Data [HE-MU, HE-MCS 4, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 4320 | 59.54% | 1066.0 B | 0.0 B | 206.4 us | 17.0 us | 5010 MHz | - | 20.0 dBm | 85.72% | 89.15% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 4 | 0.06% | 391.0 B | 389.7 B | 249.9 us | 213.2 us | 5010 MHz | - | 20.0 dBm | 0.10% | 0.10% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 7 | 0.10% | 46.0 B | 0.0 B | 35.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.02% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 1437 | 19.80% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 3.87% | 4.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 1437 | 19.80% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 5010 MHz | -66.0 dBm | - | 9.76% | 10.15% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 4 | 0.06% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -66.0 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.08% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -66.0 dBm | 20.0 dBm | 0.01% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 13 | 0.18% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -66.0 dBm | 20.0 dBm | 0.09% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#de1c12" /></svg> | Management: Action [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, BCC] | 21 | 0.29% | 34.0 B | 0.0 B | 199.2 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -66.0 dBm | - | 0.40% | 0.42% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#82cc33" /></svg> | A-MPDU Delimiter / Aggregation Overhead | 7 | 0.10% | 0.0 B | 0.0 B | 20.0 us | 0.0 us | 5010 MHz | - | - | 0.01% | 0.01% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 19 | 0.300644000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 20 | 0.300692000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 23 | 0.300906000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 27 | 0.301909000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=2654436829 | Carries protocol-visible MAC payload in the representative exchange. |
| 28 | 0.301909000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=2654436829 | Carries protocol-visible MAC payload in the representative exchange. |
| 29 | 0.301909000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=2654436829 | Carries protocol-visible MAC payload in the representative exchange. |
| 30 | 0.301909000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=1013905174 | Carries protocol-visible MAC payload in the representative exchange. |
| 31 | 0.301909000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=1013905174 | Carries protocol-visible MAC payload in the representative exchange. |
| 32 | 0.301909000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=1013905174 | Carries protocol-visible MAC payload in the representative exchange. |
| 33 | 0.301909000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=3668339023 | Carries protocol-visible MAC payload in the representative exchange. |
| 34 | 0.301909000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=3668339023 | Carries protocol-visible MAC payload in the representative exchange. |
| 35 | 0.301909000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=3668339023 | Carries protocol-visible MAC payload in the representative exchange. |
| 36 | 0.301957000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 37 | 0.302046000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 38 | 0.302094000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 39 | 0.302183000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |

Frame numbers are local to capture `DlMuMimo72MbpsLeakage01-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `DlMuMimo96MbpsLeakage01`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **7257**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36d72d" /></svg> | Data: QoS Data [HE-MU, HE-MCS 4, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 4320 | 59.53% | 1066.0 B | 0.0 B | 206.4 us | 17.0 us | 5010 MHz | - | 20.0 dBm | 85.72% | 89.15% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 4 | 0.06% | 391.0 B | 389.7 B | 249.9 us | 213.2 us | 5010 MHz | - | 20.0 dBm | 0.10% | 0.10% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 7 | 0.10% | 46.0 B | 0.0 B | 35.3 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 0.02% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 1438 | 19.82% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 3.87% | 4.03% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 1437 | 19.80% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 5010 MHz | -66.0 dBm | - | 9.76% | 10.15% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 4 | 0.06% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -66.0 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.08% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -66.0 dBm | 20.0 dBm | 0.01% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 13 | 0.18% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -66.0 dBm | 20.0 dBm | 0.09% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#de1c12" /></svg> | Management: Action [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, BCC] | 21 | 0.29% | 34.0 B | 0.0 B | 199.2 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -66.0 dBm | - | 0.40% | 0.42% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#82cc33" /></svg> | A-MPDU Delimiter / Aggregation Overhead | 7 | 0.10% | 0.0 B | 0.0 B | 20.0 us | 0.0 us | 5010 MHz | - | - | 0.01% | 0.01% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 19 | 0.300644000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 20 | 0.300692000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 23 | 0.300906000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 27 | 0.301927000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=2654436815 | Carries protocol-visible MAC payload in the representative exchange. |
| 28 | 0.301927000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=2654436815 | Carries protocol-visible MAC payload in the representative exchange. |
| 29 | 0.301927000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=6, A-MPDU=2654436815 | Carries protocol-visible MAC payload in the representative exchange. |
| 30 | 0.301927000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=1013905156 | Carries protocol-visible MAC payload in the representative exchange. |
| 31 | 0.301927000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=1013905156 | Carries protocol-visible MAC payload in the representative exchange. |
| 32 | 0.301927000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=1013905156 | Carries protocol-visible MAC payload in the representative exchange. |
| 33 | 0.301927000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=3668339037 | Carries protocol-visible MAC payload in the representative exchange. |
| 34 | 0.301927000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=3668339037 | Carries protocol-visible MAC payload in the representative exchange. |
| 35 | 0.301927000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 4, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=6, A-MPDU=3668339037 | Carries protocol-visible MAC payload in the representative exchange. |
| 36 | 0.301975000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Control: Block Ack Request (BAR) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 37 | 0.302064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 38 | 0.302112000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 39 | 0.302201000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |

Frame numbers are local to capture `DlMuMimo96MbpsLeakage01-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `EqualSizedRUs48Mbps`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **5652**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36ce36" /></svg> | Data: QoS Data [HE-MU, HE-MCS 8, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 3752 | 66.38% | 1066.0 B | 0.0 B | 232.0 us | 15.6 us | 5010 MHz | - | 20.0 dBm | 90.21% | 87.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 4 | 0.07% | 391.0 B | 389.7 B | 249.9 us | 213.2 us | 5010 MHz | - | 20.0 dBm | 0.10% | 0.10% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 940 | 16.63% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 2.73% | 2.63% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 940 | 16.63% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 5010 MHz | -66.0 dBm | - | 6.89% | 6.64% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 4 | 0.07% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -66.0 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.11% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -66.0 dBm | 20.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.11% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -66.0 dBm | 20.0 dBm | 0.04% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.200148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 2 | 0.200196000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 4 | 0.200293000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 6 | 0.200407000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 7 | 0.200598000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 8 | 0.200647000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 10 | 0.200743000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 12 | 0.200866000 | ? → 0a:aa:00:00:00:02 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 13 | 0.201048000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 14 | 0.201097000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 16 | 0.201194000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 18 | 0.201317000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 19 | 0.300644000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 20 | 0.300692000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 21 | 0.301272000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 8, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=2654436055 | Carries protocol-visible MAC payload in the representative exchange. |
| 22 | 0.301272000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 8, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=2654436055 | Carries protocol-visible MAC payload in the representative exchange. |

Frame numbers are local to capture `EqualSizedRUs48Mbps-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `EqualSizedRUs72Mbps`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **5657**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36ce36" /></svg> | Data: QoS Data [HE-MU, HE-MCS 8, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 3757 | 66.41% | 1066.0 B | 0.0 B | 232.0 us | 15.6 us | 5010 MHz | - | 20.0 dBm | 90.23% | 87.15% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 4 | 0.07% | 391.0 B | 389.7 B | 249.9 us | 213.2 us | 5010 MHz | - | 20.0 dBm | 0.10% | 0.10% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 940 | 16.62% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 2.72% | 2.63% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 940 | 16.62% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 5010 MHz | -66.0 dBm | - | 6.88% | 6.64% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 4 | 0.07% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -66.0 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.11% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -66.0 dBm | 20.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.11% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -66.0 dBm | 20.0 dBm | 0.04% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.200148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 2 | 0.200196000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 4 | 0.200293000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 6 | 0.200407000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 7 | 0.200598000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 8 | 0.200647000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 10 | 0.200743000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 12 | 0.200866000 | ? → 0a:aa:00:00:00:02 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 13 | 0.201048000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 14 | 0.201097000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 16 | 0.201194000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 18 | 0.201317000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 19 | 0.300644000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 20 | 0.300692000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 21 | 0.301478000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 8, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=2654435897 | Carries protocol-visible MAC payload in the representative exchange. |
| 22 | 0.301478000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 8, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=2654435897 | Carries protocol-visible MAC payload in the representative exchange. |

Frame numbers are local to capture `EqualSizedRUs72Mbps-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `EqualSizedRUs96Mbps`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **5658**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36ce36" /></svg> | Data: QoS Data [HE-MU, HE-MCS 8, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 3758 | 66.42% | 1066.0 B | 0.0 B | 232.0 us | 15.6 us | 5010 MHz | - | 20.0 dBm | 90.23% | 87.17% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 4 | 0.07% | 391.0 B | 389.7 B | 249.9 us | 213.2 us | 5010 MHz | - | 20.0 dBm | 0.10% | 0.10% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 940 | 16.61% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 2.72% | 2.63% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 940 | 16.61% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 5010 MHz | -66.0 dBm | - | 6.88% | 6.64% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 4 | 0.07% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -66.0 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.11% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -66.0 dBm | 20.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.11% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -66.0 dBm | 20.0 dBm | 0.04% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.200148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 2 | 0.200196000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 4 | 0.200293000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 6 | 0.200407000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 7 | 0.200598000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 8 | 0.200647000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 10 | 0.200743000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 12 | 0.200866000 | ? → 0a:aa:00:00:00:02 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 13 | 0.201048000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 14 | 0.201097000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 16 | 0.201194000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 18 | 0.201317000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 19 | 0.300644000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 20 | 0.300692000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 21 | 0.301478000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 8, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=2654435897 | Carries protocol-visible MAC payload in the representative exchange. |
| 22 | 0.301478000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 8, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=6, A-MPDU=2654435897 | Carries protocol-visible MAC payload in the representative exchange. |

Frame numbers are local to capture `EqualSizedRUs96Mbps-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `EqualSizedRUs_fBW`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **4731**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#36ce36" /></svg> | Data: QoS Data [HE-MU, HE-MCS 8, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 2095 | 44.28% | 1066.0 B | 0.0 B | 245.4 us | 17.4 us | 5010 MHz | - | 20.0 dBm | 79.73% | 51.42% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 4 | 0.08% | 391.0 B | 389.7 B | 249.9 us | 213.2 us | 5010 MHz | - | 20.0 dBm | 0.15% | 0.10% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 1308 | 27.65% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 5.68% | 3.66% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 1308 | 27.65% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 5010 MHz | -66.0 dBm | - | 14.33% | 9.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 4 | 0.08% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -66.0 dBm | - | 0.02% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.13% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -66.0 dBm | 20.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.13% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -66.0 dBm | 20.0 dBm | 0.06% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.200148000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 2 | 0.200196000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 4 | 0.200293000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 6 | 0.200407000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 7 | 0.200598000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 8 | 0.200647000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 10 | 0.200743000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 12 | 0.200866000 | ? → 0a:aa:00:00:00:02 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 13 | 0.201048000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 14 | 0.201097000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 16 | 0.201194000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 18 | 0.201317000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 19 | 0.300644000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 20 | 0.300692000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 21 | 0.301030000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 8, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=2654436069 | Carries protocol-visible MAC payload in the representative exchange. |
| 22 | 0.301030000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 8, 106-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=6, A-MPDU=1013903406 | Carries protocol-visible MAC payload in the representative exchange. |

Frame numbers are local to capture `EqualSizedRUs_fBW-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Analysis of Packet Distribution
Packet totals alone do not establish MU-MIMO. IEEE Std 802.11-2024 Clause 27.3.2.5 identifies each HE-MU user and its spatial streams; the direct evidence is multiple users in one PPDU with compatible, non-overlapping stream allocations. Use the RU/NSS allocation telemetry and five-run comparison documented above; the radiotap suffix establishes the PPDU format but not all users' stream allocations.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->

## [agent] Frame exchange analysis

The following command selects a representative three-user exchange from the
48 Mbit/s treatment:

```sh
tshark -n \
  -r 'examples/ieee80211ax/dl_mu_mimo/results/20260727T121100Z/DlMuMimo48MbpsLeakage01/DlMuMimo48MbpsLeakage01-#0Lan80211AxDlOfdma.ap.wlan[0].pcap' \
  -Y 'frame.number >= 27 && frame.number <= 40' \
  -T fields -E header=y -E separator='|' \
  -e frame.number -e frame.time_epoch -e wlan.ta -e wlan.ra \
  -e wlan.fc.type_subtype -e wlan.fc.retry -e wlan.seq -e wlan.qos.tid \
  -e radiotap.he.data_1.ppdu_format -e radiotap.he.data_3.data_mcs \
  -e radiotap.he.data_5.data_bw_ru_allocation -e radiotap.he.data_6.nsts
```

| Frame | Simulation time | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 27-28 | 0.301936 s | AP → `0a:aa:...:01` | QoS Data / HE-MU | MCS 4, 242-tone RU, NSS 1, A-MPDU, seq 2-3 | user payload for host 0 |
| 29-31 | 0.301936 s | AP → `0a:aa:...:02` | QoS Data / HE-MU | same PPDU time and PHY, seq 1-3 | simultaneous user payload for host 1 |
| 32-34 | 0.301936 s | AP → `0a:aa:...:03` | QoS Data / HE-MU | same PPDU time and PHY, seq 1-3 | simultaneous user payload for host 2 |
| 35-36 | 0.301984-0.302073 s | AP ↔ `...:01` | BAR then BA | retry 0 | polls and acknowledges host 0 |
| 37-38 | 0.302121-0.302210 s | AP ↔ `...:02` | BAR then BA | retry 0 | polls and acknowledges host 1 |
| 39-40 | 0.302258-0.302346 s | AP ↔ `...:03` | BAR then BA | retry 0 | polls and acknowledges host 2 |

The same capture timestamp and same 242-tone RU are direct packet evidence
that the three user A-MPDUs belong to one HE MU transmission observation. Their
non-overlapping stream starts are established separately by co-timestamped AP
result vectors; radiotap does not export that field. Frame numbers are TShark
capture indices, not OMNeT++ event numbers.

## [agent] Cross-layer findings and verdict

Evidence basis: the capability and feature-gate assignments are configuration
input; the allocation vectors and decoded frames are direct observations; the
per-run goodput and confidence intervals are derived measurements; and the
claim that ceiling-limited offered load explains the lack of a goodput benefit
is an inference consistent with those measurements.

| Claim | Verdict | Configuration evidence | Model telemetry | Packet evidence | Outcome evidence |
|---|---|---|---|---|---|
| DL MU-MIMO executes in all treatments | `PASS` | gate, beamformer/beamformee, antennas, sounding, scheduler | multiple same-time STA IDs with disjoint stream ranges in all 25 MU runs | HE-MU users share 242-tone RU | load-dependent delivery is recorded |
| OFDMA controls remain frequency-separated | `PASS` | identical prerequisites, MU gate disabled | no treatment stream check asserted | HE-MU MPDUs use 106-tone RUs at all four loads | load-dependent delivery is recorded |
| Lower CSI leakage changes PHY selection | `PASS` | leakage 0.001 versus 0.01; load otherwise matched | allocation remains structurally valid | dominant MCS rises from 4 to 6; estimated airtime falls | both deliver approximately 24 Mbit/s |
| Sweep exposes different saturation plateaus | `PASS` | matched packet size, intervals, channel, and seeds | all MU allocations remain valid | OFDMA and MU frame counts independently flatten | MU 49.3091 versus OFDMA about 42.9-43.1 Mbit/s under 72/96 Mbit/s load |
| MU-MIMO improves aggregate saturated-load goodput | `PASS` | matched OFDMA/MU pairs at 48/72/96 Mbit/s | spatial versus frequency allocation directly observed | full-band MU and 106-tone OFDMA geometry | paired mean gain is approximately 5.0-6.4 Mbit/s |
| Sequential BAR policy executes | `PASS` | `dlMuAckMethod="sequentialBar"` | acknowledgment vectors retained | frames 35-40 poll all three users in order | not an application-delivery proof |

The bounded verdict is that the example now directly demonstrates INET's
modeled DL MU-MIMO mechanism across the requested load sweep, shows the MCS
response to lower modeled CSI leakage, and demonstrates a higher modeled
aggregate-goodput plateau than matched OFDMA in this topology. At 24 Mbit/s
there is no throughput distinction because both methods are offered-load
limited. Scalar/vector results and run-0 packet captures were co-recorded in
session `20260727T121100Z`; packet statistics remain a single-seed
observation, while structural and delivery claims use five paired seeds.

## [agent] Limitations and inconclusive claims

- The packet-level scalar model does not validate real antenna steering,
  waveform channel matrices, synchronization, channel estimation, or hardware
  beamforming gain.
- PCAP does not expose `heStreamStartIndex`; the disjoint-stream conclusion
  requires AP result vectors.
- The current parallel user vectors use timestamp and fixed emission order,
  not an explicit PPDU correlation ID.
- The scalar analyzer now selects the manifest's half-open
  `[0.55 s, 0.88 s)` window with a strict upper bound. Regenerating the
  retained session confirmed that no receive or allocation sample occurs
  exactly at 0.88 s, so its values and verdicts are unchanged.
- Five seeds support repeatability in this deterministic stationary topology,
  not broad statistical claims about real deployments.
- Packet captures were analyzed only for run 0; MCS and frame-count
  comparisons are direct single-seed observations rather than confidence
  intervals.
- Aggregate goodput can conceal per-station imbalance; capacity conclusions
  should be read with the per-station/fairness analysis, not aggregate values
  alone.
- The four load points bracket saturation but do not precisely locate each
  knee. OFDMA transitions between 24 and 48 Mbit/s; MU-MIMO transitions
  between 48 and 72 Mbit/s.
- `DlMuMimo80MHz` is `NOT RUN`. Run it first with run 0 and verify sounding,
  80 MHz RU geometry, and disjoint stream vectors before publishing claims.

## [agent] Further experiments

- Refine the OFDMA knee between 24 and 48 Mbit/s and the MU-MIMO knee between
  48 and 72 Mbit/s using smaller increments, recording delay and queue length
  as well as delivered goodput.
- Repeat the matched sweep with more active stations to test scheduler
  grouping and fairness beyond the current three-user symmetric topology.
- Sweep CSI leakage around 0.001 and 0.01 while holding load fixed, and report
  MCS distribution, airtime, goodput, and delay rather than MCS alone.
- Run `DlMuMimo80MHz` and verify that wide-band capability is negotiated and
  that same-RU stream ranges remain disjoint.
- Disable one station's beamformee feedback capability as a negative case;
  predict that it is excluded from MU-MIMO groups while eligible peers remain.

## [agent] Implementation plan

No production `src/inet` implementation was needed. The existing scheduler,
sounding, rate-selection, and telemetry paths handle the expanded sweep. The
example configuration, experiment manifest, suite inventory, plot labeling,
generated evidence, and authored walkthrough were updated together.

| Item | Implementation outcome |
|---|---|
| New matched-load leakage case | `DlMuMimo24MbpsLeakage001`: aggregate 24 Mbit/s, `defaultCsiLeakage=0.001` |
| MU load sweep | `DlMuMimo48MbpsLeakage01`, `DlMuMimo72MbpsLeakage01`, and `DlMuMimo96MbpsLeakage01`, all with CSI leakage 0.01 |
| Matched OFDMA controls | `EqualSizedRUs48Mbps`, `EqualSizedRUs72Mbps`, and `EqualSizedRUs96Mbps`, with identical traffic intervals |
| Analysis registration | nine conditions in the AX suite and scalar manifest; five MU structural evidence contracts |
| Validation | the 72 Mbit/s diagnostic and all 45 publication simulations exited 0; all five MU evidence contracts pass; all 33 AX analysis tests pass |
| Existing analysis correction | half-open `[0.55,0.88)` result cropping remains covered by its boundary-sample unit test |
| Optional observability | consider an explicit PPDU correlation ID and `heMuMimoUsed` result to replace timestamp/order correlation |
| Architecture and sealing | no `src/inet` change was required; any telemetry change there requires a separate requirements, sealing, implementation, and regression review |

## [agent] Artifact provenance

| Artifact family | Session/path | Configurations/runs | Tool/filter/window | Integrity notes |
|---|---|---|---|---|
| Scalar/vector | [`results/20260727T121100Z`](results/20260727T121100Z) | nine configs, runs 0-4/seeds 0-4 | native OMNeT++ result API; `[0.55,0.88)` | hashes in [`evidence-ledger.json`](../../ieee80211/analysis/generated/sessions/20260727T121100Z/evidence-ledger.json) |
| Paired/fairness table | same scalar/vector session | nine configs, three sinks per run | `packetReceived:vector(packetBytes)`; pair by run/seed; Jain formula in text | agent-derived from raw vectors, not a generated metrics field |
| Scalar plot | [`mu-mimo-spatial-stream-matrix.png`](results/20260727T121100Z/mu-mimo-spatial-stream-matrix.png) | nine configs; 96 Mbit/s MU run 0 plotted | shared `mimo` renderer | [`PNG provenance`](results/20260727T121100Z/mu-mimo-spatial-stream-matrix.png.json) |
| PCAP | [`results/20260727T121100Z`](results/20260727T121100Z) | nine configs, run 0/seed 0 | AP capture, TShark/Capinfos 4.6.4 | manifest SHA-256 recorded in [`packet_metrics.json`](../../ieee80211/analysis/generated/ax/packet_metrics.json) |
| PCAP plot | [`packet_statistics.png`](results/20260727T121100Z/packet_statistics.png) | nine configs, run 0 | shared typed HE profile | [`PNG provenance`](results/20260727T121100Z/packet_statistics.png.json) |
