# Walkthrough: IEEE 802.11ax downlink MU-MIMO

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260727T100000Z`
- PCAP: `20260727T100100Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `20260727T100000Z`, `20260727T100100Z`.

This example contrasts downlink multi-user multiple-input multiple-output
(DL MU-MIMO) with a matched orthogonal frequency-division multiple access
(OFDMA) control. Five paired runs establish the modeled spatial-stream
allocation and application outcome; run 0 of a separate packet session shows
the protocol-visible HE MU data and sequential Block Ack exchange.

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
- apps 3-5 each send 1,000-byte packets every 1 ms from 0.3 s onward; these
  are the measured flows.

The outcome window is `[0.55 s, 0.88 s)`, after sounding and association
transients. The treatment and control use identical topology, traffic,
capabilities, channel, acknowledgment policy, run numbers, and seeds.

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
| Treatment prerequisites and feature gate are effective inputs | `PASS` | `omnetpp.ini` plus instantiated MIB/HCF types | configs, all runs | Configuration input; not alone proof of runtime use |
| A PPDU serves multiple users on disjoint spatial streams | `PASS` | AP `heStaId`, `heSpatialStreams`, and `heStreamStartIndex` vectors; evidence ledger | runs 0-4 / seeds 0-4 | 303-304 qualifying timestamps per run, zero overlaps |
| Treatment packet capture contains HE MU full-band data and sequential BAR/BA | `PASS` | AP PCAPng, frames 26-33, packet report | run 0 / seed 0 | Direct packet observation |
| OFDMA control uses frequency-separated RUs | `PASS` | AP PCAPng: HE MU MPDUs decoded on 106-tone RUs | run 0 / seed 0 | Packet-visible RU geometry |
| MU-MIMO increases goodput in this offered-load regime | `FAIL` | application receive vectors in `[0.55,0.88)` | paired runs 0-4 | 23.971 vs 24.000 Mbit/s; both reach offered-load ceiling |
| PCAP alone proves disjoint spatial streams | `INCONCLUSIVE` | radiotap lacks stream-start index | run 0 | Use result vectors |
| 80 MHz stress configuration behaves correctly | `NOT RUN` | no retained result/capture session | none | Requires a separate run and window |

## [agent] Configuration matrix

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| `EqualSizedRUs_fBW` | Control | Equal-sized-RU scheduler; `enableDlMuMimo=false` | three measured 8 Mbit/s flows, 20 MHz | 0-4 / 0-4 | users occupy different 106-tone RUs |
| `DlMuMimo` | Treatment | same scheduler/capabilities; `enableDlMuMimo=true` | matched traffic/channel | 0-4 / 0-4 | users share 242-tone RU with disjoint streams |
| `DlMuMimo80MHz` | Stress case | treatment at 80 MHz | matched three-station traffic; warm-up period 0.7 s | `NOT RUN` | shared wide-band RU and disjoint streams |

`[General]` supplies the common `HeHcf`,
`HeDlSchedulerEqualSizedRUs`, four AP and station antennas, AP beamformer and
four sounding dimensions, station beamformee/feedback capabilities, QoS,
aggregation, and Block Ack support. The concrete 20 MHz configurations change
only the DL MU-MIMO gate; both select `sequentialBar`. This removes scheduler,
traffic, and radio-capability confounders from the primary comparison.

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

Run from the INET repository root. The first corrected treatment diagnostic was
executed with release `bin/inet`, run 0/seed 0, and exit status 0:

```sh
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211/analysis/wifi_analysis.py run dl_mu_mimo \
  --suite ax --evidence scalar-vector --runs 1 --config DlMuMimo \
  --jobs 1 --session-id 20260727T095000Z
```

The publication scalar/vector campaign executed ten simulations (two
configurations times five runs) with exit status 0:

```sh
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211/analysis/wifi_analysis.py run dl_mu_mimo \
  --suite ax --evidence scalar-vector --runs 5 --jobs 10 \
  --session-id 20260727T100000Z

MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211/analysis/wifi_analysis.py report dl_mu_mimo \
  --suite ax --session-id 20260727T100000Z
```

The packet command completed both run-0 simulations with child exit status 0
and wrote PCAPng captures, but the wrapper exited 2 during post-processing
because the sandboxed TShark process could not traverse `/home/user`:

```sh
XDG_CONFIG_HOME=/tmp/codex-wireshark-config \
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211/analysis/wifi_analysis.py run dl_mu_mimo \
  --suite ax --evidence both --runs 1 --jobs 2 \
  --session-id 20260727T100100Z
```

Read-only copies of that session were decoded under
`/tmp/dl-mu-mimo-analysis.1FsZQZ/inet` with the shared analyzer; the analyzer
exited 0. The original captures and all hashes in the manifest refer to the
repository result session, not to the temporary copies.

## [agent] Scalar and vector analysis

Inputs are the ten `.sca`/`.vec` pairs under
[`results/20260727T100000Z`](results/20260727T100000Z). The result query used
by the shared analysis is:

```sh
opp_scavetool query -l \
  -f 'module =~ "**.ap.wlan[0].radio" and (name =~ "heRuToneSize:vector" or name =~ "heRuToneOffset:vector" or name =~ "heStaId:vector" or name =~ "heSpatialStreams:vector" or name =~ "heStreamStartIndex:vector")' \
  'examples/ieee80211ax/dl_mu_mimo/results/20260727T100000Z/DlMuMimo/DlMuMimo-#0.vec'
```

Goodput is calculated independently for each run by summing station
`packetReceived:vector(packetBytes)` samples in `[0.55 s, 0.88 s)`, converting
bytes to bits, and dividing by 0.33 s. Runs are paired by run number/seed.
The reported uncertainty is a 95% Student-t confidence interval across five
independent runs, never across vector samples.

| Configuration / invariant | Source result, module, unit | Window and per-run aggregation | Independent runs | Estimate |
|---|---|---|---:|---:|
| `DlMuMimo` goodput | `packetReceived:vector(packetBytes)`, `**.app[*]`; bytes inferred from `packetBytes`, native unit absent | target `[0.55,0.88)`; sum bytes × 8 / 0.33 s | 5, none excluded | 23.9709 ± 0.0495 Mbit/s |
| `EqualSizedRUs_fBW` goodput | same | same | 5, none excluded | 24.0000 ± 0 Mbit/s |
| MU-MIMO disjoint streams | AP radio HE allocation vectors | group equal timestamps; test half-open stream intervals | 5, none excluded | 303, 303, 303, 304, 303 PPDUs; zero overlaps |

Run 0 contains 1,805 AP allocation rows over the full simulation:
`heRuToneSize=242` and `heRuToneOffset=0` in every row, station IDs 1-3,
one stream per user, and stream-start indices 0-2. The evidence ledger's
303-304 PPDU counts apply only to the measurement window.

The control reaches the 24 Mbit/s offered-load ceiling in every run. The
treatment is effectively ceiling-limited: three runs reach 24 Mbit/s and two
are 3 kB per window short, for a 23.9709 Mbit/s mean. This workload therefore
cannot demonstrate a MU-MIMO goodput gain. That is a `FAIL` for the gain claim,
not a failure of the mechanism; the feature test passes because the structural
allocation invariant is directly observed in every treatment run.

<!-- BEGIN GENERATED: ieee80211-scalar-vector-mimo -->
### [script] Generated scalar/vector plot and table

![mimo scalar/vector analysis](results/20260727T100000Z/mu-mimo-spatial-stream-matrix.png)

Figure provenance: [`results/20260727T100000Z/mu-mimo-spatial-stream-matrix.png.json`](results/20260727T100000Z/mu-mimo-spatial-stream-matrix.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.ap.wlan[0].radio / heStaId:vector<br>vector / **.ap.wlan[0].radio / heSpatialStreams:vector<br>vector / **.ap.wlan[0].radio / heStreamStartIndex:vector<br>vector / **.app[*] / packetReceived:vector(packetBytes) / unit=B
- Window / per-run aggregation / exclusions: [0.55, 0.88) s; goodput=per run with 95% Student-t CI; telemetry=all PPDUs validated; representative run 0 plotted
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| 20 MHz MU-MIMO / goodput mbps | 23.9709 | 0.0494609 |
| 20 MHz OFDMA / goodput mbps | 24 | 0 |

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
| `DlMuMimo` | AP `wlan[0]`; all decoded frames | 5,801 | 2,065 HE-MU QoS MPDUs on 242-tone RU; 1,804 BAR; 1,803 BA | stream-start indices come from vectors |
| `EqualSizedRUs_fBW` | AP `wlan[0]`; all decoded frames | 4,731 | 2,095 HE-MU QoS MPDUs on 106-tone RUs; 1,308 BAR and BA | HE-MU format alone does not distinguish OFDMA |

The generated count-versus-airtime plot materially distinguishes the
full-band spatial treatment from the narrower-RU control. Airtime is estimated
from available radiotap fields; HE MU/TB user-dependent signaling not exported
by radiotap remains approximate.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260727T100100Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260727T100100Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260727T100100Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260727T100100Z.json` (SHA-256 `2f6acbc177e338ba56497339f31912634f853f60ea46f391ca02ae4f300004c1`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `DlMuMimo` | `none (all decoded frames)` | 5801 | Data: QoS Data [HE-MU, HE-MCS 4, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] (1863), Control: Block Ack Request (BAR) (1804), Control: Block Ack (BA) (1803) | 66.27% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `EqualSizedRUs_fBW` | `none (all decoded frames)` | 4731 | Data: QoS Data [HE-MU, HE-MCS 8, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (2095), Control: Block Ack Request (BAR) (1308), Control: Block Ack (BA) (1308) | 64.49% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | DlMuMimo produced protocol-visible wireless observations | 5801 AP/global transmission observations |
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

The following command selects the representative treatment exchange:

```sh
tshark -n \
  -r 'examples/ieee80211ax/dl_mu_mimo/results/20260727T100100Z/DlMuMimo/DlMuMimo-#0Lan80211AxDlOfdma.ap.wlan[0].pcap' \
  -Y 'frame.number >= 26 && frame.number <= 33' \
  -T fields -E header=y -E separator='|' \
  -e frame.number -e frame.time_epoch -e wlan.ta -e wlan.ra \
  -e wlan.fc.type_subtype -e wlan.fc.retry -e wlan.seq -e wlan.qos.tid \
  -e radiotap.he.data_1.ppdu_format -e radiotap.he.data_3.data_mcs \
  -e radiotap.he.data_5.data_bw_ru_allocation -e radiotap.he.data_6.nsts
```

| Frame | Simulation time | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 26-27 | 0.301499 s | AP → `0a:aa:...:02` | QoS Data / HE-MU | MCS 5, 242-tone RU, NSS 1, A-MPDU, seq 1-2 | user payload for host 1 |
| 28-29 | 0.301499 s | AP → `0a:aa:...:03` | QoS Data / HE-MU | same PPDU time, MCS 5, 242-tone RU, NSS 1, seq 1-2 | simultaneous user payload for host 2 |
| 30 | 0.301547 s | AP → `...:02` | BAR / legacy control | retry 0 | solicits the first user's Block Ack |
| 31 | 0.301636 s | `...:02` → AP | BA / legacy control | receiver is AP | acknowledges the first user's aggregate |
| 32 | 0.301684 s | AP → `...:03` | BAR / legacy control | retry 0 | solicits the second user's Block Ack |
| 33 | 0.301773 s | `...:03` → AP | BA / legacy control | receiver is AP | acknowledges the second user's aggregate |

The same capture timestamp and same 242-tone RU are direct packet evidence
that the two user A-MPDUs belong to one HE MU transmission observation. Their
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
| DL MU-MIMO executes | `PASS` | gate, beamformer/beamformee, antennas, sounding, scheduler | multiple same-time STA IDs with disjoint stream ranges in all runs | HE-MU users share 242-tone RU | delivery reaches offered-load ceiling |
| OFDMA control remains frequency-separated | `PASS` | identical prerequisites, MU gate disabled | no treatment stream check asserted | HE-MU MPDUs use 106-tone RUs | 24.000 Mbit/s |
| Sequential BAR policy executes | `PASS` | `dlMuAckMethod="sequentialBar"` | acknowledgment vectors retained | frames 30-33 are BAR/BA then BAR/BA | not an application-delivery proof |
| MU-MIMO improves goodput | `FAIL` | matched load/channel/seeds | allocation succeeds | exchange succeeds | treatment is 0.029 Mbit/s lower on average |

The bounded verdict is that the example now directly demonstrates INET's
modeled DL MU-MIMO mechanism. It does not demonstrate a throughput advantage:
the offered load caps both configurations. Scalar/vector session
`20260727T100000Z` and packet session `20260727T100100Z` are separate, so
their adjacent findings do not prove event-by-event causality or exact
cross-session count equality.

## [agent] Limitations and inconclusive claims

- The packet-level scalar model does not validate real antenna steering,
  waveform channel matrices, synchronization, channel estimation, or hardware
  beamforming gain.
- PCAP does not expose `heStreamStartIndex`; the disjoint-stream conclusion
  requires AP result vectors.
- The current parallel user vectors use timestamp and fixed emission order,
  not an explicit PPDU correlation ID.
- The current scalar analyzer selects samples with `time <= 0.88 s` although
  the manifest specifies `[0.55 s, 0.88 s)`. No retained receive or allocation
  sample occurs exactly at 0.88 s, so this session's values and verdicts are
  unchanged; the smallest machinery fix is to use a strict upper bound and
  regenerate the presentation bundle.
- Five seeds support repeatability in this deterministic stationary topology,
  not broad statistical claims about real deployments.
- `DlMuMimo80MHz` is `NOT RUN`. Run it first with run 0 and verify sounding,
  80 MHz RU geometry, and disjoint stream vectors before publishing claims.
- A higher offered load or larger station set is needed to test capacity
  benefit; this workload is ceiling-limited at 24 Mbit/s.

## [agent] Further experiments

- Halve the measured send interval while holding every other parameter fixed;
  predict diverging queueing delay/goodput, then inspect application vectors
  and the same structural stream gate.
- Run `DlMuMimo80MHz` and verify that wide-band capability is negotiated and
  that same-RU stream ranges remain disjoint.
- Disable one station's beamformee feedback capability as a negative case;
  predict that it is excluded from MU-MIMO groups while eligible peers remain.
- Add controlled pairwise CSI leakage and predict a smaller compatible group;
  inspect allocation vectors rather than inferring the decision from goodput.

## [agent] Implementation plan

No production `src/inet` implementation is proposed. The configuration repair
was sufficient to exercise the existing scheduler, sounding, and telemetry
paths.

| Item | Evidence-backed plan |
|---|---|
| Demonstrated analysis gap | manifest specifies a half-open window, while `analysis_core.py` currently includes the end timestamp; this session has no boundary sample |
| Smallest analysis change | replace the inclusive upper comparison with a strict comparison and add a boundary-sample unit test |
| Validation | rerun AX analysis tests, regenerate this session's scalar bundle, and confirm unchanged values for this retained data |
| Optional observability | consider an explicit PPDU correlation ID and `heMuMimoUsed` result to replace timestamp/order correlation |
| Architecture and sealing | no `src/inet` change is authorized; any telemetry change there requires a separate requirements, sealing, implementation, and regression review |
| Next handoff | analysis-maintainer follow-up for the window predicate; Wi-Fi implementation owner only if explicit telemetry is pursued |

## [agent] Artifact provenance

| Artifact family | Session/path | Configurations/runs | Tool/filter/window | Integrity notes |
|---|---|---|---|---|
| Scalar/vector | [`results/20260727T100000Z`](results/20260727T100000Z) | both configs, runs 0-4/seeds 0-4 | native OMNeT++ result API; `[0.55,0.88)` | hashes in [`evidence-ledger.json`](../../ieee80211/analysis/generated/sessions/20260727T100000Z/evidence-ledger.json) |
| Scalar plot | [`mu-mimo-spatial-stream-matrix.png`](results/20260727T100000Z/mu-mimo-spatial-stream-matrix.png) | both configs; treatment run 0 plotted | shared `mimo` renderer | [`PNG provenance`](results/20260727T100000Z/mu-mimo-spatial-stream-matrix.png.json) |
| PCAP | [`results/20260727T100100Z`](results/20260727T100100Z) | both configs, run 0/seed 0 | AP capture, TShark/Capinfos 4.6.4 | manifest SHA-256 recorded in [`packet_metrics.json`](../../ieee80211/analysis/generated/ax/packet_metrics.json) |
| PCAP plot | [`packet_statistics.png`](results/20260727T100100Z/packet_statistics.png) | both configs, run 0 | shared typed HE profile | [`PNG provenance`](results/20260727T100100Z/packet_statistics.png.json) |
