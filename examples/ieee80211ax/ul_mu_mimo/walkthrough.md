# Walkthrough: HE MU-MIMO

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260725T120411Z`
- PCAP: `20260725T230510Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `20260725T120411Z`, `20260725T230510Z`.

This example demonstrates IEEE 802.11ax multi-user multiple-input
multiple-output (MU-MIMO): several stations use the same frequency resource
with disjoint spatial streams. Five retained downlink runs directly expose
full-bandwidth stream allocations and a matched OFDMA outcome comparison;
retained packet evidence also exposes sounding and a representative uplink
Trigger assignment. Claims are bounded to INET's packet-level model.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- distinguish spatial MU-MIMO separation from OFDMA frequency separation;
- identify sounding, user/RU assignment, and disjoint stream indices;
- reproduce downlink mechanism/outcome queries and uplink Trigger decoding;
- recognize why packet totals or antenna capability flags alone do not prove
  spatial multiplexing; and
- use the first divergent artifact as a focused diagnostic.

In downlink (DL) MU-MIMO the AP beamformer sounds eligible beamformees, obtains
channel-state information (CSI), and assigns users non-overlapping spatial
stream ranges. In uplink (UL), an AP Trigger aligns station transmissions and
assigns a common full-bandwidth RU with different starting stream indices.

## [agent] Scenario description

There is no local `omnetpp.ini`.
[downlink.ini](downlink.ini) includes
[`../../dl_ofdma_sched/omnetpp.ini`](../../dl_ofdma_sched/omnetpp.ini), and
[uplink.ini](uplink.ini) includes
[`../../ul_ofdma/omnetpp.ini`](../../ul_ofdma/omnetpp.ini).

```text
DL: server -- Ethernet -- 4-antenna AP ))) three 4-antenna STAs
UL: three STAs ))) 4-antenna AP -- Ethernet -- server
```

The focused DL comparison is stationary, 5 GHz/20 MHz, 1 s with a 0.25 s
warm-up. Both `DlMuMimo` and `EqualSizedRUs_fBW` send three 1,000-byte flows
every 1 ms and use `dlMuAckMethod="sequentialBar"`. The UL scenario is 2 s,
stationary, 20 MHz, and the wrapper raises offered load to one 1,000-byte
packet per 0.5 ms while allowing a 1.5 ms HE TB PPDU.

## [agent] Standards and INET model boundary

IEEE Std 802.11-2024 Clause 27.3.1.1 describes HE MU transmission; Clause
27.3.2.5 defines resource/user indication for full-bandwidth DL MU-MIMO; Clause
27.3.3.2.4 constrains UL MU-MIMO spatial-stream counts. Corpus chunks used are
`80211ax-2024:chunk:10040`, `:10062`, and `:10074`.

INET uses configured CSI freshness/leakage and ideal separation for disjoint
scalar stream ranges; it does not compute the measured gain from a waveform
channel matrix. AP result vectors are authoritative model telemetry for the
allocation. Native capture exposes Trigger fields, but not every HE-SIG-B
stream-allocation fact.

## [agent] Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| Split entry points resolve to intended DL/UL configurations | `PASS` | include chains and result metadata | DL 0-4; UL 0 | Configuration provenance |
| DL AP assigns several users one 242-tone RU with disjoint streams | `PASS` | AP `heRuToneSize`, `heStaId`, `heSpatialStreams`, `heStreamStartIndex` vectors | DL 0/0 | Direct model telemetry |
| DL matched-window goodput exceeds OFDMA control | `PASS` | application receive vectors | paired runs 0-4/seeds 0-4 | INET scenario outcome only |
| DL sounding feedback polling occurs | `PASS` | decoded BFRP Triggers | DL 0/0 | Packet-visible polling, not CSI validity |
| UL Trigger assigns AIDs 1-3 common RU 61 and streams 0-2 | `PASS` | decoded Basic Trigger fields | UL 0/0 | Representative exchange |
| UL goodput benefit versus EDCA | `NOT RUN` | no retained matched result set | none | Mechanism only |
| Shared AX analyzer regenerates the split scenario | `PASS` | session `20260725T230510Z` manifest resolves `downlink.ini` and `uplink.ini` by configuration | run 0/seed 0 | DL and UL treatments completed |

## [agent] Configuration matrix

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| `DlMuMimo` via `downlink.ini` | DL treatment | 4-antenna AP, sounding, DL MU-MIMO | matched 3×8 Mbit/s offered load, 20 MHz | 0-4 / 0-4 | common RU, disjoint stream ranges |
| `EqualSizedRUs_fBW` via `downlink.ini` | DL OFDMA control | frequency-separated RUs | matched DL load/ack policy | 0-4 / 0-4 | no full-bandwidth shared stream allocation |
| `SuEdcaBaseline` via `downlink.ini` | DL SU control | HCF/SU path | inherited load | `NOT RUN` | single-user exchanges |
| `DlMuMimo80MHz` via `downlink.ini` | wide-band stress | 8 STAs/AP antennas, warm-up 0.7 s | 80 MHz | `NOT RUN` | disjoint streams after sounding |
| `UlMuMimo` via `uplink.ini` | UL treatment | full-bandwidth UL MU-MIMO | saturated three-STA UL, 20 MHz | 0/0 packet session | Trigger assigns common RU/disjoint streams |
| `EdcaBaseline` via `uplink.ini` | UL control | UL MU disabled | matched 0.5 ms load | `NOT RUN` | no Trigger-based spatial assignment |

The wrapper's later assignments win over included defaults for payload,
acknowledgment policy, send interval, PPDU limit, and wide-band warm-up.

## [agent] Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| CSI sounding precedes DL service | AP PCAP BFRP and CSI telemetry | no polls/stale table | sounding/CSI manager | inspect `csiTable` and BFRP timeline |
| DL users share RU with non-overlapping streams | AP HE vectors | RU differs or stream ranges overlap | DL scheduler/plan | inspect scheduler summary and allocation vectors |
| UL Trigger assigns common RU/disjoint indices | Trigger User Info | differing RU or overlapping indices | UL scheduler/Trigger serialization | query exact User Info fields and HCF decision log |
| MU outcome comparison is matched | metadata, inputs, receive vectors | workload/window/seed mismatch | experiment setup | inspect `.sca` run attrs before metrics |

## [agent] Reproduction

Run from the INET root. These illustrative commands were `NOT RUN` during
this documentation revision:

```sh
bin/inet -u Cmdenv -f examples/ieee80211ax/ul_mu_mimo/omnetpp.ini \
  -c DlMuMimo -r 0 --seed-set=0 \
  --result-dir="$PWD/examples/ieee80211ax/ul_mu_mimo/results/validation/dl-mu"

bin/inet -u Cmdenv -f examples/ieee80211ax/ul_mu_mimo/omnetpp.ini \
  -c EqualSizedRUs_fBW -r 0 --seed-set=0 \
  --result-dir="$PWD/examples/ieee80211ax/ul_mu_mimo/results/validation/dl-ofdma"
```

Repeat runs 1-4 into distinct directories for the retained comparison design.
The shared analyzer resolved the split downlink/uplink INI files by
configuration. This command was executed with exit status 0 and created
session `20260725T230510Z`:

```sh
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211/analysis/analyze_pcap.py \
  --suite examples/ieee80211/analysis/suites/ax.json \
  --generate --subdir multi_user/mu_mimo --run 0 --allow-failed-evidence
```

## [agent] Scalar and vector analysis

Inputs are the ten `.vec` files in
`results/20260725T120411Z/{DlMuMimo,EqualSizedRUs_fBW}/`.

```sh
opp_scavetool query -l \
  -f 'module =~ "*.ap.wlan[0].radio" and (name =~ "heRuToneSize:vector" or name =~ "heStaId:vector" or name =~ "heSpatialStreams:vector" or name =~ "heStreamStartIndex:vector")' \
  examples/ieee80211ax/ul_mu_mimo/results/20260725T120411Z/DlMuMimo/DlMuMimo-\#0.vec
```

Goodput is the sum of three station `packetReceived:vector(packetBytes)`
samples in `[0.55 s,0.88 s)`, multiplied by 8 and divided by 0.33 s. Each
sample is 1,000 B. Runs are paired by run/seed and aggregated per run first.

| Run | DL MU packets | DL MU goodput | OFDMA packets | OFDMA goodput |
|---:|---:|---:|---:|---:|
| 0 | 1,044 | 25.309 Mbit/s | 440 | 10.667 Mbit/s |
| 1 | 1,041 | 25.236 Mbit/s | 440 | 10.667 Mbit/s |
| 2 | 1,044 | 25.309 Mbit/s | 440 | 10.667 Mbit/s |
| 3 | 1,043 | 25.285 Mbit/s | 440 | 10.667 Mbit/s |
| 4 | 1,044 | 25.309 Mbit/s | 440 | 10.667 Mbit/s |
| Mean | — | 25.290 Mbit/s | — | 10.667 Mbit/s |

The mean ratio is 2.37. The DL-MU 95% t interval is approximately
25.290 ± 0.039 Mbit/s; the five control values are identical. This is not a
population-level real-world capacity claim.

In run 0, each of the four AP allocation vectors has 727 records:
`heRuToneSize=242` throughout; station IDs are 1-3; each user has one stream;
starting indices are 0-2. These directly expose the modeled spatial allocation.
The performance comparison is accepted only together with the structural
gate: at least one PPDU must serve multiple users, and every user's half-open
spatial-stream interval must be disjoint. Goodput alone is not evidence that
MU-MIMO occurred.

<!-- BEGIN GENERATED: ieee80211-scalar-vector-mimo -->
### [script] Generated scalar/vector plot and table

![mimo scalar/vector analysis](../../analysis/figures/multi_user/mu_mimo/mu-mimo-spatial-stream-matrix.png)

Figure provenance: [`../../analysis/figures/multi_user/mu_mimo/mu-mimo-spatial-stream-matrix.png.json`](../../analysis/figures/multi_user/mu_mimo/mu-mimo-spatial-stream-matrix.png.json). Run-level metric source: [`../../analysis/metrics.json`](../../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.ap.wlan[0].radio / heStaId:vector<br>vector / **.ap.wlan[0].radio / heSpatialStreams:vector<br>vector / **.ap.wlan[0].radio / heStreamStartIndex:vector<br>vector / **.app[*] / packetReceived:vector(packetBytes) / unit=B
- Window / per-run aggregation / exclusions: [0.55, 0.88) s; goodput=per run with 95% Student-t CI; telemetry=all PPDUs validated; representative run 0 plotted
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| 20 MHz MU-MIMO / goodput mbps | 25.2897 | 0.0392468 |
| 20 MHz OFDMA / goodput mbps | 10.6667 | 0 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.
<!-- END GENERATED: ieee80211-scalar-vector-mimo -->

## [agent] PCAP statistics

Retained captures are AP `wlan[0]` PCAPng/radiotap observations with
microsecond precision; decode used TShark 4.6.4.

| Configuration | Observation count | Relevant frame/PHY summary | Interpretation limit |
|---|---:|---|---|
| `DlMuMimo` | 1,770 | 7 Trigger, 724 BAR, 724 BA, 262 HE-MU aggregates | stream indices come from vectors |
| `UlMuMimo` | 3,777 | 49 Basic plus 2 other Triggers, 1,939 QoS Data/Null, 51 BA | no retained UL application comparison |

Rows count AP-interface observations, not de-duplicated application packets.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](../../analysis/figures/multi_user/mu_mimo/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](../../analysis/figures/multi_user/mu_mimo/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260725T230510Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/pcapmanifests/20260725T230510Z.json` (SHA-256 `a8715becb4a1c69d7cf152a992b7a1abbfbdceedaa4ec2f25999e462603aa256`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `DlMuMimo` | `none (all decoded frames)` | 3597 | Data: QoS Data [HE-MU, HE-MCS 1, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] (2089), Control: Block Ack Request (BAR) (724), Control: Block Ack (BA) (724) | 132.19% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UlMuMimo` | `none (all decoded frames)` | 3777 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (1786), Control: Ack (1736), Data: QoS Null [HE-TB, HE, GI 3.2 us, A-MPDU] (147) | 58.47% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | DlMuMimo produced protocol-visible wireless observations | 3597 AP/global transmission observations |
| **PASS** | UlMuMimo produced protocol-visible wireless observations | 3777 AP/global transmission observations |
| **INCONCLUSIVE** | Multiple users with disjoint stream allocations in one PPDU | The packet-type table is exchange evidence only; use the recorded feature vectors/results |

### [script] Configuration: `DlMuMimo`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **3597**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#37de21" /></svg> | Data: QoS Data [HE-MU, HE-MCS 1, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 2089 | 58.08% | 1066.0 B | 0.0 B | 595.6 us | 17.1 us | 5010 MHz | - | 20.0 dBm | 94.12% | 124.42% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 4 | 0.11% | 391.0 B | 389.7 B | 249.9 us | 213.2 us | 5010 MHz | - | 20.0 dBm | 0.08% | 0.10% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 7 | 0.19% | 45.1 B | 2.1 B | 35.0 us | 0.7 us | 5010 MHz | - | 20.0 dBm | 0.02% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 724 | 20.13% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | - | 20.0 dBm | 1.53% | 2.03% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 724 | 20.13% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 5010 MHz | -65.3 dBm | - | 3.87% | 5.12% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 3 | 0.08% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.17% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.01% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 13 | 0.36% | 36.8 B | 0.5 B | 69.1 us | 0.7 us | 5010 MHz | -65.3 dBm | 20.0 dBm | 0.07% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#cb1a20" /></svg> | Management: Action [HE-TB, HE-MCS 0, 106-tone RU, GI 1.6 us, BCC] | 2 | 0.06% | 34.0 B | 0.0 B | 112.8 us | 0.0 us | 5005 MHz, 5015 MHz | -65.0 dBm | - | 0.02% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#de1c12" /></svg> | Management: Action [HE-TB, HE-MCS 0, 52-tone RU, GI 1.6 us, BCC] | 18 | 0.50% | 34.0 B | 0.0 B | 199.2 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -65.3 dBm | - | 0.27% | 0.36% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#82cc33" /></svg> | A-MPDU Delimiter / Aggregation Overhead | 7 | 0.19% | 0.0 B | 0.0 B | 20.0 us | 0.0 us | 5010 MHz | - | - | 0.01% | 0.01% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 18 | 0.201550000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 19 | 0.300644000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:01 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0 | Carries protocol-visible MAC payload in the representative exchange. |
| 22 | 0.300989000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 25 | 0.302466000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=2654435906 | Carries protocol-visible MAC payload in the representative exchange. |
| 26 | 0.302466000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=2654435906 | Carries protocol-visible MAC payload in the representative exchange. |
| 27 | 0.302466000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1013903497 | Carries protocol-visible MAC payload in the representative exchange. |
| 28 | 0.302466000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=1013903497 | Carries protocol-visible MAC payload in the representative exchange. |
| 29 | 0.302514000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Control: Block Ack Request (BAR) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 30 | 0.302602000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 31 | 0.302650000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 32 | 0.302739000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 33 | 0.304363000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=2654436726 | Carries protocol-visible MAC payload in the representative exchange. |
| 34 | 0.304363000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Data: QoS Data / HE-MU, HE-MCS 1, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=2654436726 | Carries protocol-visible MAC payload in the representative exchange. |
| 35 | 0.304363000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1013905341 | Carries protocol-visible MAC payload in the representative exchange. |
| 36 | 0.304363000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:02 | Data: QoS Data / HE-MU, HE-MCS 1, 242-tone RU, NSS 1, GI 3.2 us, LDPC | direction=from DS, retry=0, seq=4, frag=0, more-frag=0, TID=0, A-MPDU=1013905341 | Carries protocol-visible MAC payload in the representative exchange. |
| 37 | 0.304411000 | 10:00:00:00:00:00 → 0a:aa:00:00:00:03 | Control: Block Ack Request (BAR) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |

Frame numbers are local to capture `DlMuMimo-#0Lan80211AxDlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `UlMuMimo`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **3777**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1786 | 47.29% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -63.0 dBm | - | 94.89% | 55.48% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#135011" /></svg> | Data: QoS Null [HE-TB, HE, GI 3.2 us, A-MPDU] | 147 | 3.89% | 34.0 B | 0.0 B | 73.2 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 0.92% | 0.54% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d790b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 6 | 0.16% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.20% | 0.12% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 51 | 1.35% | 47.1 B | 5.2 B | 35.7 us | 1.7 us | 5010 MHz | - | 10.0 dBm | 0.16% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 51 | 1.35% | 58.0 B | 0.0 B | 39.3 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.17% | 0.10% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1736 | 45.96% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 3.66% | 2.14% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.002564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=218 | Carries protocol-visible MAC payload in the representative exchange. |
| 3 | 0.002564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=226 | Carries protocol-visible MAC payload in the representative exchange. |
| 4 | 0.002564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=234 | Carries protocol-visible MAC payload in the representative exchange. |
| 5 | 0.002633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 6 | 0.103048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 7 | 0.104564000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=419 | Carries protocol-visible MAC payload in the representative exchange. |
| 8 | 0.104564000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=427 | Carries protocol-visible MAC payload in the representative exchange. |
| 9 | 0.104564000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=435 | Carries protocol-visible MAC payload in the representative exchange. |
| 10 | 0.104633000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 11 | 0.200644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 12 | 0.201388000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 13 | 0.201436000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 14 | 0.202123000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 15 | 0.202171000 | ? → 0a:aa:00:00:00:02 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 16 | 0.202272000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |

Frame numbers are local to capture `UlMuMimo-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Analysis of Packet Distribution
Packet totals alone do not establish MU-MIMO. IEEE Std 802.11-2024 Clause 27.3.2.5 identifies each HE-MU user and its spatial streams; the direct evidence is multiple users in one PPDU with compatible, non-overlapping stream allocations. Use the RU/NSS allocation telemetry and five-run comparison documented above; the radiotap suffix establishes the PPDU format but not all users' stream allocations.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->

## [agent] Frame exchange analysis

```sh
tshark -n \
  -r 'examples/ieee80211ax/ul_mu_mimo/results/20260725T230510Z/UlMuMimo/UlMuMimo-#0Lan80211AxUlOfdma.ap.wlan[0].pcap' \
  -Y 'frame.number >= 16 && frame.number <= 20' \
  -T fields -E header=y -E separator='|' -E occurrence=a \
  -e frame.number -e frame.time_epoch -e wlan.sa -e wlan.da \
  -e wlan.trigger.he.trigger_type -e wlan.trigger.he.user_info.aid12 \
  -e wlan.trigger.he.ru_allocation \
  -e wlan.trigger.he.ru_starting_spatial_stream \
  -e wlan.trigger.he.ru_number_of_spatial_stream -e _ws.col.Info
```

| Frame | Simulation time | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 16 | 0.202272 s | AP → AIDs 1,2,3 | Basic Trigger | RU 61 for all; starts 0,1,2; encoded NSS 0 (= one stream) | assigns full-bandwidth UL MU-MIMO |
| 17 | 0.203788 s | STA 1 → AP | QoS Null/HE TB observation | same timestamp group | triggered response |
| 18 | 0.203788 s | STA 2 → AP | QoS Null/HE TB observation | same timestamp group | triggered response |
| 19 | 0.203788 s | STA 3 → AP | QoS Null/HE TB observation | same timestamp group | triggered response |
| 20 | 0.203857 s | AP → STAs | Block Ack | follows group | acknowledges exchange |

For DL sounding, `wlan.trigger.he.trigger_type==1` yields seven BFRP Triggers;
frame 22 at 0.300989 s polls AIDs 2 and 3. This proves polling, while CSI
validity and stream allocation remain separate telemetry claims.

## [agent] Cross-layer findings and verdict

| Claim | Verdict | Configuration evidence | Model telemetry | Packet evidence | Outcome evidence |
|---|---|---|---|---|---|
| DL full-bandwidth MU-MIMO executed | `PASS` | treatment enabled/capable | 242-tone RU, IDs 1-3, disjoint indices | sounding polls/HE-MU observations | paired five-run gain |
| UL full-bandwidth assignment executed | `PASS` | treatment enabled/capable | no retained allocation vectors cited | Trigger RU 61/indices 0-2 and simultaneous group | `NOT RUN` comparison |

The verdict is `PASS` for the scoped DL and UL mechanism invariants. Only DL
has retained matched multi-run outcome evidence. Mechanism and outcome sessions
are separate, so their event-level causality is inference.

## [agent] Limitations and inconclusive claims

- DL results and packet captures were recorded in separate sessions.
- No retained UL control/outcome campaign supports a throughput benefit.
- Native capture does not expose all DL HE-SIG-B stream fields.
- CSI and interference are packet-level abstractions, not waveform channel
  matrices.
- Wide-band and SU-control cases are `NOT RUN` in retained evidence.

## [agent] Further experiments

- Run paired `UlMuMimo`/`EdcaBaseline` seeds and check Trigger stream indices
  before comparing goodput/fairness.
- Run `DlMuMimo80MHz` after 0.7 s warm-up and verify all selected stream ranges.
- Sweep CSI validity while retaining sounding overhead, allocations, and
  per-run delivery.

## [agent] Implementation plan

| Item | Evidence-backed plan |
|---|---|
| Demonstrated gap | shared-suite descriptor cannot select split INIs; UL comparison is absent |
| Intended behavior | preserve direct typed PHY/Trigger evidence and paired per-run outcomes |
| Smallest change surface | AX suite descriptor scenario variants and MU-MIMO plugin; no production source change demonstrated |
| Observability | retain allocation vectors, Trigger User Info, CSI freshness, app outcomes in one session |
| Validation | DL/UL treatment-control seed 0 mechanism checks, then paired runs |
| Compatibility and risks | preserve OFDMA/SU controls and fail closed on absent HE-SIG fields |
| Architecture and sealing | apply architecture/seal rules before any `src/inet` proposal; none authorized here |
| Next handoff | analysis-suite owner and results analyst |

## [agent] Artifact provenance

| Artifact family | Session/path | Configurations/runs | Tool/filter/window | Integrity notes |
|---|---|---|---|---|
| Scalar/vector | `results/20260725T120411Z/` | DL treatment/control runs 0-4 | `opp_scavetool`; `[0.55,0.88)` outcome window | per-run aggregation; split INI in metadata |
| PCAP/log | `results/20260725T230510Z/` | DL/UL treatment run 0 | TShark 4.6.4, AP `wlan[0]` | manifest and hashes in generated block |
| Standards | `80211ax-2024` corpus | IEEE Std 802.11-2024 | chunks named above | PDF not needed |
