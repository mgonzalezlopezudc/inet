# Walkthrough: HE MU-MIMO

This example demonstrates IEEE 802.11ax multi-user multiple-input
multiple-output (MU-MIMO): several stations use the same frequency resource
with disjoint spatial streams. Five retained downlink runs directly expose
full-bandwidth stream allocations and a matched OFDMA outcome comparison;
retained packet evidence also exposes sounding and a representative uplink
Trigger assignment. Claims are bounded to INET's packet-level model.

## Learning objectives and feature primer

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

## Scenario description

There is no local `omnetpp.ini`.
[downlink.ini](downlink.ini) includes
[`../../dl_ofdma/omnetpp.ini`](../../dl_ofdma/omnetpp.ini), and
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

## Standards and INET model boundary

IEEE Std 802.11-2024 Clause 27.3.1.1 describes HE MU transmission; Clause
27.3.2.5 defines resource/user indication for full-bandwidth DL MU-MIMO; Clause
27.3.3.2.4 constrains UL MU-MIMO spatial-stream counts. Corpus chunks used are
`80211ax-2024:chunk:10040`, `:10062`, and `:10074`.

INET uses configured CSI freshness/leakage and ideal separation for disjoint
scalar stream ranges; it does not compute the measured gain from a waveform
channel matrix. AP result vectors are authoritative model telemetry for the
allocation. Native capture exposes Trigger fields, but not every HE-SIG-B
stream-allocation fact.

## Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| Split entry points resolve to intended DL/UL configurations | `PASS` | include chains and result metadata | DL 0-4; UL 0 | Configuration provenance |
| DL AP assigns several users one 242-tone RU with disjoint streams | `PASS` | AP `heRuToneSize`, `heStaId`, `heSpatialStreams`, `heStreamStartIndex` vectors | DL 0/0 | Direct model telemetry |
| DL matched-window goodput exceeds OFDMA control | `PASS` | application receive vectors | paired runs 0-4/seeds 0-4 | INET scenario outcome only |
| DL sounding feedback polling occurs | `PASS` | decoded BFRP Triggers | DL 0/0 | Packet-visible polling, not CSI validity |
| UL Trigger assigns AIDs 1-3 common RU 61 and streams 0-2 | `PASS` | decoded Basic Trigger fields | UL 0/0 | Representative exchange |
| UL goodput benefit versus EDCA | `NOT RUN` | no retained matched result set | none | Mechanism only |
| Shared AX analyzer can regenerate scenario | `FAIL` | `ax.json` names nonexistent `mu_mimo/omnetpp.ini` | none | Tooling descriptor gap |

## Configuration matrix

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

## Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| CSI sounding precedes DL service | AP PCAP BFRP and CSI telemetry | no polls/stale table | sounding/CSI manager | inspect `csiTable` and BFRP timeline |
| DL users share RU with non-overlapping streams | AP HE vectors | RU differs or stream ranges overlap | DL scheduler/plan | inspect scheduler summary and allocation vectors |
| UL Trigger assigns common RU/disjoint indices | Trigger User Info | differing RU or overlapping indices | UL scheduler/Trigger serialization | query exact User Info fields and HCF decision log |
| MU outcome comparison is matched | metadata, inputs, receive vectors | workload/window/seed mismatch | experiment setup | inspect `.sca` run attrs before metrics |

## Reproduction

Run from the INET root. These illustrative commands were `NOT RUN` during
this documentation revision:

```sh
bin/inet -u Cmdenv -f examples/ieee80211ax/multi_user/mu_mimo/downlink.ini \
  -c DlMuMimo -r 0 --seed-set=0 \
  --result-dir=examples/ieee80211ax/multi_user/mu_mimo/results/validation/dl-mu

bin/inet -u Cmdenv -f examples/ieee80211ax/multi_user/mu_mimo/downlink.ini \
  -c EqualSizedRUs_fBW -r 0 --seed-set=0 \
  --result-dir=examples/ieee80211ax/multi_user/mu_mimo/results/validation/dl-ofdma
```

Repeat runs 1-4 into distinct directories for the retained comparison design.
The packet-session logs end normally; original full commands/exit codes were
not retained.

The shared launcher is currently `NOT RUN` and expected to fail before
simulation because `ax.json` maps this split scenario to nonexistent
`multi_user/mu_mimo/omnetpp.ini`.

## Scalar and vector analysis

Inputs are the ten `.vec` files in
`results/scalar-vector/20260725T120411Z/{DlMuMimo,EqualSizedRUs_fBW}/`.

```sh
opp_scavetool query -l \
  -f 'module =~ "*.ap.wlan[0].radio" and (name =~ "heRuToneSize:vector" or name =~ "heStaId:vector" or name =~ "heSpatialStreams:vector" or name =~ "heStreamStartIndex:vector")' \
  examples/ieee80211ax/multi_user/mu_mimo/results/scalar-vector/20260725T120411Z/DlMuMimo/DlMuMimo-\#0.vec
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

## PCAP statistics

Retained captures are AP `wlan[0]` PCAPng/radiotap observations with
microsecond precision; decode used TShark 4.6.4.

| Configuration | Observation count | Relevant frame/PHY summary | Interpretation limit |
|---|---:|---|---|
| `DlMuMimo` | 1,770 | 7 Trigger, 724 BAR, 724 BA, 262 HE-MU aggregates | stream indices come from vectors |
| `UlMuMimo` | 3,777 | 49 Basic plus 2 other Triggers, 1,939 QoS Data/Null, 51 BA | no retained UL application comparison |

Rows count AP-interface observations, not de-duplicated application packets.

## Frame exchange analysis

```sh
tshark -n \
  -r 'examples/ieee80211ax/multi_user/mu_mimo/results/packet-statistics/20260724T175025Z/UlMuMimo/UlMuMimo-#0Lan80211AxUlOfdma.ap.wlan[0].pcap' \
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

## Cross-layer findings and verdict

| Claim | Verdict | Configuration evidence | Model telemetry | Packet evidence | Outcome evidence |
|---|---|---|---|---|---|
| DL full-bandwidth MU-MIMO executed | `PASS` | treatment enabled/capable | 242-tone RU, IDs 1-3, disjoint indices | sounding polls/HE-MU observations | paired five-run gain |
| UL full-bandwidth assignment executed | `PASS` | treatment enabled/capable | no retained allocation vectors cited | Trigger RU 61/indices 0-2 and simultaneous group | `NOT RUN` comparison |

The verdict is `PASS` for the scoped DL and UL mechanism invariants. Only DL
has retained matched multi-run outcome evidence. Mechanism and outcome sessions
are separate, so their event-level causality is inference.

## Limitations and inconclusive claims

- DL results and packet captures were recorded in separate sessions.
- No retained UL control/outcome campaign supports a throughput benefit.
- Native capture does not expose all DL HE-SIG-B stream fields.
- CSI and interference are packet-level abstractions, not waveform channel
  matrices.
- Wide-band and SU-control cases are `NOT RUN` in retained evidence.

## Further experiments

- Run paired `UlMuMimo`/`EdcaBaseline` seeds and check Trigger stream indices
  before comparing goodput/fairness.
- Run `DlMuMimo80MHz` after 0.7 s warm-up and verify all selected stream ranges.
- Sweep CSI validity while retaining sounding overhead, allocations, and
  per-run delivery.

## Implementation plan

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

## Artifact provenance

| Artifact family | Session/path | Configurations/runs | Tool/filter/window | Integrity notes |
|---|---|---|---|---|
| Scalar/vector | `results/scalar-vector/20260725T120411Z/` | DL treatment/control runs 0-4 | `opp_scavetool`; `[0.55,0.88)` outcome window | per-run aggregation; split INI in metadata |
| PCAP/log | `results/packet-statistics/20260724T175025Z/` | DL/UL treatment run 0 | TShark 4.6.4, AP `wlan[0]` | separate session |
| Standards | `80211ax-2024` corpus | IEEE Std 802.11-2024 | chunks named above | PDF not needed |
