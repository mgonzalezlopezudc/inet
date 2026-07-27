# Walkthrough: IEEE 802.11ax scheduled uplink OFDMA

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260727T211515Z`
- PCAP: `20260727T211515Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `20260727T211515Z`.

This walkthrough compares two access-point (AP) scheduling policies with a
single-user Enhanced Distributed Channel Access (EDCA) control. The retained
five-run result set and run-0 packet captures are co-recorded in session
`20260727T211515Z`. They prove that the feature gate creates Trigger-led
multi-user exchanges, but they also expose two important findings: the
scheduled HE trigger-based (HE-TB) allocations are too small to test queued
1,000-byte payload service, and their on-wire Ack Policy conflicts with the
immediate Multi-STA Block Ack procedure. The first is an evidence boundary;
the second is a demonstrated standards/model mismatch.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- explain how an AP coordinates simultaneous uplink transmissions with a
  Trigger frame and per-station resource-unit (RU) assignments;
- distinguish HE-TB scheduled responses from ordinary HE single-user (HE-SU)
  uplink frames in a radiotap capture;
- compare backlog-based and equal-sized-RU scheduler inputs without treating
  delivered bytes as mechanism evidence;
- reproduce the result and packet analyses; and
- verify the allocation-capacity precondition before claiming that queued
  payload should appear in a scheduled uplink response.

Orthogonal Frequency-Division Multiple Access (OFDMA) partitions a channel into
RUs. For scheduled uplink OFDMA, the AP first wins channel access, broadcasts a
Basic Trigger, and assigns an association identifier (AID), RU, modulation and
coding scheme, stream allocation, and response duration to each selected
station. Addressed stations respond using HE-TB physical protocol data units
(PPDUs), aligned after a Short Interframe Space (SIFS). The AP then
acknowledges the multi-user response.

The important distinction is structural: a Trigger followed by HE-TB frames
shows that the coordinated response path ran, but only an HE-TB QoS Data
observation (or aligned scheduler/queue telemetry) shows that queued payload
used that path. Aggregate goodput alone cannot establish this.

## [agent] Scenario description

The [network](Lan80211AxUlOfdma.ned) extends the common
[`HeSingleBssNetwork`](../common/HeSingleBssNetwork.ned) with three stationary
wireless stations. Each station sends uplink UDP traffic through the AP and its
wired Ethernet connection to `server.app[0]`:

```text
host[0] --\
host[1] ---- 5 GHz / 20 MHz --> AP === 100 Gbit/s Ethernet === server
host[2] --/                    ^  |
                               |  +-- Multi-STA Block Ack
                               +----- Basic Trigger
```

The [configuration](omnetpp.ini) places the AP at the center of a 50 m × 50 m
area and the stations 5 m away, with no mobility or external interferer. All
variants use IEEE 802.11ax mode, 10 mW transmit power, fixed 14.625 Mbit/s
nominal bitrate, Block Ack support, A-MPDU aggregation, and the same receiver
thresholds. A one-packet-per-station warm-up at 0.2 s is intended to establish
Block Ack agreements. The measured application starts at 0.3 s: every station
sends 1,000 bytes every 5 ms, or 1.6 Mbit/s per station and 4.8 Mbit/s in
aggregate, until the 2 s limit.

UDP port 5000 maps to user priority 6 (`UP_VO`) through
`ExampleQosClassifier`. The high MU-EDCA contention values and zero configured
random-access RUs are constant across configurations. This example therefore
studies AP-scheduled access versus ordinary EDCA in one close-range,
single-basic-service-set topology; it is not a coverage, interference, or
Uplink OFDMA Random Access (UORA) experiment.

## [agent] Standards and INET model boundary

IEEE Std 802.11-2024 Clause 26.5.2.2.1 defines Trigger-based solicitation of
one or more HE-TB PPDUs. Clause 26.5.2.3.1 requires an eligible addressed
station's response to begin one SIFS after the Trigger-containing PPDU, and
Clause 26.5.2.3.3 derives HE-TB transmission parameters from Trigger Common
Info and User Info. Trigger format and per-user AID12/RU/MCS/FEC/stream fields
are defined in Clause 9.3.1.22 and Tables 9-47 and 9-53. Clause 26.5.2.4 allows
a QoS Null response when no pending frame fits the Basic Trigger allocation.
For immediate acknowledgement of more than one station, Clauses 10.3.2.13.3
and 26.4.4.5 describe the Multi-STA Block Ack response.

INET represents this exchange with `HeHcf`, `HeUlCoordinator`, replaceable
`HeUlScheduler*` policies, and `HeUlMuTxOpFs`. The scheduler produces an
allocation; the station builds an HE-TB transmission after SIFS; and the AP's
frame-sequence model collects the responses and constructs a Multi-STA Block
Ack. INET also uses a model-only Trigger ID tag to correlate a response with
the solicitation. That tag is not an on-air IEEE field and cannot be observed
in PCAP.

The scalar radio approximates per-RU uplink reception by frequency/power
placement and does not model timing advance. `PcapRecorder` flattens A-MPDU
contents into one capture record per MPDU, omitting delimiters and padding.
Radiotap HE fields are authoritative only when their known bits are present.

One observed behavior is not standards-conformant. The three HE-TB QoS Null
responses after frame 270 carry Ack Policy `3` (`Block Ack`), while IEEE
802.11-2024 Clause 26.4.4.5 and Table 9-13 require an Ack Policy that solicits
the immediate response (Normal Ack or Implicit BAR) for this Basic-trigger
sequence. INET nevertheless unconditionally schedules the terminal Multi-STA
Block Ack. This walkthrough records that as a model-control versus serialized
wire-semantics mismatch, not as a TShark heuristic.

## [agent] Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| UL-MU feature gate changes the exchange | `PASS` | 166 Basic + 2 BSRP Triggers and 492 HE-TB responses in each scheduled run-0 capture; zero in EDCA | run 0 / seed 0 | Direct AP-capture observation |
| Basic Trigger assigns all three stations distinct RUs | `PASS` | frame 270: AID12 `1,2,3`, RU allocation `0,1,2`; frames 271–273 use distinct 26-tone RU frequencies | scheduled run 0 | One representative exchange |
| Queued 1,000-byte payload uses HE-TB | `INCONCLUSIVE` | Basic Trigger UL Length 730, MCS 0, 26-tone RUs; zero HE-TB QoS Data; 492 HE-TB QoS Null; 1,029 HE-SU QoS Data | scheduled run 0 | The recorded 1 ms allocation cannot fit the 1,000-byte MPDU; QoS Null is permitted |
| Backlog and equal-sized policies produce distinguishable behavior | `INCONCLUSIVE` | byte-identical run-0 captures; identical five-run goodput; both schedule zero reported bytes into the same three 26-tone RUs | runs/seeds 0–4 | The symmetric zero-byte candidates do not exercise a policy delta |
| Immediate-response Ack Policy matches IEEE semantics | `FAIL` | frames 271–273 decode `wlan.qos.ack=3`, followed by Multi-STA BA frame 274 | scheduled run 0 | Direct field observation plus normative rule |
| Offered payload reaches the server | `PASS` | `server.app[0] packetReceived:vector(packetBytes)` | runs/seeds 0–4 | Outcome only; not proof of OFDMA payload |
| Scheduled access improves goodput | `INCONCLUSIVE` | 4.781 ± 0.007 Mbit/s scheduled vs 4.800 Mbit/s EDCA | five paired seeds | No acceptance threshold; intervals do not support improvement |

## [agent] Configuration matrix

All three configurations inherit `[General]`; none uses a `[Config ...]
extends` chain.

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| `BacklogBased` | Treatment | `enableUlMuOfdma=true`; `HeUlSchedulerBacklogBased` sizes/ranks requests from reported backlog | 3 × 1,000 B/5 ms; AX, 20 MHz | 0–4 / 0–4 | Basic Trigger assigns users/RUs and queued data appears in HE-TB |
| `EqualSizedRUs` | Treatment/policy control | same gate; `HeUlSchedulerEqualSizedRUs` allocates equal RUs and uses backlog for duration | matched | 0–4 / 0–4 | Same exchange structure, with policy-dependent allocations when demand exposes a difference |
| `EdcaBaseline` | Negative control | `enableUlMuOfdma=false`; coordinator inactive | matched | 0–4 / 0–4 | no Basic/BSRP Trigger or HE-TB scheduled response |

The AP's NED default scheduler typename remains backlog-based in
`EdcaBaseline`, but it is inactive because the UL coordinator's `enabled`
parameter follows `enableUlMuOfdma`. Both treatments set
`minRandomAccessRus=maxRandomAccessRus=0`, so every Trigger user entry should
be scheduled rather than random access.

## [agent] Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| Feature gate controls Trigger exchange | AP PCAP: Basic/BSRP Trigger and HE-TB counts | scheduled capture has no Trigger, or EDCA has one | `HeHcf` / UL coordinator initialization | inspect effective `enableUlMuOfdma`, coordinator timer, and AP channel-access request |
| Trigger allocation is internally valid | Trigger AID12/RU fields and HE-TB RU/frequency | missing user, duplicate RU, wrong response PHY | scheduler → Trigger serialization → STA validation | export per-user Trigger fields and correlate with `HeUlMuTxOpFs` |
| Fitting queued payload is selected for the Basic Trigger | capacity-checked Trigger plus HE-TB QoS Data and coordinator reported/scheduled-byte vectors | QoS Null despite a proved fitting eligible MPDU | Trigger policy, buffer status, or triggered A-MPDU builder | first run a deterministic fitting-allocation control; then inspect queue/TID selection |
| Ack policy solicits the modeled immediate BA | `wlan.qos.ack` in HE-TB MPDUs | value `3` while AP sends immediate Multi-STA BA | HE-TB QoS header construction/serializer | inspect Ack Policy assignment and add field-level regression |
| Scheduler policies are observable | allocation telemetry and per-run outcome | identical zero-byte allocations | scheduler inputs or workload not discriminating | record the selected Trigger reason/retry-pending state; add asymmetric backlog case |

## [agent] Reproduction

Run from the INET repository root. This minimal direct command is a convenient
single-configuration check but was not the evidence-producing command; status:
`NOT RUN`.

```sh
bin/inet -u Cmdenv \
  -f examples/ieee80211ax/ul_ofdma/omnetpp.ini \
  -c BacklogBased -r 0 --seed-set=0 \
  --result-dir="$PWD/examples/ieee80211ax/ul_ofdma/results/manual/BacklogBased"
```

The publication campaign below was executed in release mode with exit status
0. It launched configurations one at a time per run through the shared
campaign machinery, covered run numbers `[0,5)` with seed sets 0–4, recorded
the selected vectors for every run, and additionally recorded AP `wlan[0]`
PCAPng in run 0:

```sh
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211/analysis/wifi_analysis.py run ul_ofdma \
  --suite ax --evidence both --runs 5 --jobs 12 \
  --session-id 20260727T211515Z
```

It wrote
`examples/ieee80211ax/ul_ofdma/results/20260727T211515Z/`.
The report command also exited 0:

```sh
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211/analysis/wifi_analysis.py report ul_ofdma \
  --suite ax --session-id 20260727T211515Z
```

Regenerate the marker-bounded presentation bundles after the authored
explanation exists:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py publish ul_ofdma \
  --suite ax --session-id 20260727T211515Z --update
```

## [agent] Scalar and vector analysis

The scalar/vector inputs are the 15 `.sca`/`.vec` pairs under
`results/20260727T211515Z/{BacklogBased,EqualSizedRUs,EdcaBaseline}/`.
The native result analysis matched exactly one vector per run:
`Lan80211AxUlOfdma.server.app[0] packetReceived:vector(packetBytes)`. Its
result-unit attribute is blank; byte semantics come from the
`vector(packetBytes)` recording mode and the analysis sidecar. For each run
the analysis sums delivered application bytes whose timestamps are in
`[0.3 s,2.0 s)`, multiplies by eight, and divides by 1.7 s. One aggregate value
per run enters the two-sided 95% Student-t confidence interval; packet samples
are not treated as repetitions. No run or vector was excluded.

```sh
opp_scavetool query -l \
  -f 'module =~ "*.server.app[0]" AND name =~ "packetReceived:vector(packetBytes)"' \
  examples/ieee80211ax/ul_ofdma/results/20260727T211515Z/BacklogBased/BacklogBased-\#0.vec
```

| Configuration | Metric/source/unit | Window and per-run aggregation | Independent runs | Estimate |
|---|---|---|---:|---:|
| `BacklogBased` | delivered goodput; `server.app[0] packetReceived:vector(packetBytes)`; derived Mbit/s | `[0.3,2.0)`; sum byte-valued samples × 8 / 1.7 s | 5 | 4.78118 ± 0.00716 |
| `EqualSizedRUs` | same | same | 5 | 4.78118 ± 0.00716 |
| `EdcaBaseline` | same | same | 5 | 4.80000 ± 0.00000 |

The two scheduled policies deliver the same near-offered-load goodput and are
slightly below EDCA. This is consistent with Trigger exchanges adding protocol
activity without carrying application payload in run 0, but that causal link
is not directly measured. The
goodput comparison has no manifest-defined acceptance threshold and does not
validate UL OFDMA by itself.

The AP radio's allocation vectors are empty because the AP is receiving rather
than transmitting the scheduled HE-TB users. The corresponding station-radio
vectors are nonempty and directly expose the modeled allocation. In run 0,
each station records 164 allocation samples over the full 0–2 s run:

| Module/user | RU offset and size | Scheduled PSDU | User PPDU duration | Interpretation |
|---|---:|---:|---:|---|
| `host[0].wlan[0].radio`, STA ID 1 | 0 / 26 tones | 38 B | 1 ms | first 26-tone HE-TB user |
| `host[1].wlan[0].radio`, STA ID 2 | 26 / 26 tones | 38 B | 1 ms | second 26-tone HE-TB user |
| `host[2].wlan[0].radio`, STA ID 3 | 54 / 26 tones | 38 B | 1 ms | third 26-tone HE-TB user |

The 38-byte scheduled PSDU is the QoS Null observation, not the 1,000-byte
application payload. Its 1 ms, MCS 0, 26-tone allocation cannot fit that
1,000-byte MPDU, so IEEE 802.11-2024 Clause 26.5.2.4 permits this response. At
`ap.wlan[0].mac.hcf.ulCoordinator`,
`heUlBufferStatusReportedBytes` has 1,332 run-0 samples (mean 888.333 B,
maximum 5,330 B), but all 498
`heUlBufferStatusScheduledBytes` samples are zero. The same zero-byte
candidate state makes the backlog scheduler choose its minimum 26-tone request
and the equal-sized scheduler choose the same three-user layout.

One source-level hypothesis is visible just after warm-up. At
0.201388 s the AP successfully receives and acknowledges a TID 6 QoS Data
retransmission whose Retry bit is set. `HeHcf::recipientProcessReceivedFrame()`
passes that current-frame Retry bit to
`HeUlCoordinator::updateBufferStatus()`, which stores it as future
`retryPending` state. `HeUlDefaultTriggerPolicy` then treats it as outstanding
work even though the MPDU has already been received. At
0.202232287954 s the scheduler records zero bytes for all three users, frame
16 sends a Basic Trigger at 0.202272 s, and frames 17–19 are QoS Null at
0.203288 s. The QoS Null fallback is correct for the insufficient allocation.
Because the retained session has no direct Trigger-reason or `retryPending`
signal and the focused debug log did not record the decision, treating the
received Retry bit as the cause remains an inference requiring a targeted
control or effective logging; it is not the payload verdict.

<!-- BEGIN GENERATED: ieee80211-scalar-vector-ul_ofdma -->
### [script] Generated scalar/vector plot and table

![ul_ofdma scalar/vector analysis](results/20260727T211515Z/ul-ofdma-delivery.png)

Figure provenance: [`results/20260727T211515Z/ul-ofdma-delivery.png.json`](results/20260727T211515Z/ul-ofdma-delivery.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)
- Window / per-run aggregation / exclusions: [0.3, 2.0) s; observation=one aggregate goodput value per run; uncertainty=95% Student-t CI
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| Backlog scheduler / goodput mbps | 4.78118 | 0.00715634 |
| EDCA baseline / goodput mbps | 4.8 | 0 |
| Equal-sized RUs / goodput mbps | 4.78118 | 0.00715634 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.
<!-- END GENERATED: ieee80211-scalar-vector-ul_ofdma -->

## [agent] PCAP statistics

The run-0 captures observe `ap.wlan[0]` MAC packet signals and are PCAPng with
radiotap, microsecond timestamps, computed checksums/FCS, and TShark 4.6.4.
Rows count AP-interface over-the-air frame/MPDU observations, not
de-duplicated transmissions or application deliveries. HE MU/TB airtime is
approximate where radiotap lacks user-dependent signaling.

```sh
tshark -n \
  -r examples/ieee80211ax/ul_ofdma/results/20260727T211515Z/BacklogBased/BacklogBased-\#0Lan80211AxUlOfdma.ap.wlan[0].pcap \
  -q -z io,stat,0,'wlan.trigger.he.trigger_type == 0','wlan.trigger.he.trigger_type == 4','wlan.fc.type_subtype == 0x28 && radiotap.he.data_1.ppdu_format == 3','wlan.fc.type_subtype == 0x2c && radiotap.he.data_1.ppdu_format == 3','wlan.fc.type_subtype == 0x28 && radiotap.he.data_1.ppdu_format == 0','wlan.fc.type_subtype == 0x19'
```

| Configuration | Observation point/count | Selection | Decisive frame/PHY facts | Limit |
|---|---:|---|---|---|
| `BacklogBased` | AP `wlan[0]`; 2,877 | all decoded frames | 166 Basic + 2 BSRP Triggers; 492 HE-TB QoS Null; 0 HE-TB QoS Data; 1,029 HE-SU QoS Data; 168 BA | capture counts do not expose scheduler intent |
| `EqualSizedRUs` | AP `wlan[0]`; 2,877 | all decoded frames | byte-identical to `BacklogBased` with the same decisive counts | identical traffic may not force a policy delta |
| `EdcaBaseline` | AP `wlan[0]`; 2,483 | all decoded frames | 0 Trigger; 0 HE-TB; 1,460 HE-SU QoS Data; 0 BA (ordinary ACKs are used) | control proves exchange absence, not why |

The plot compares frame-observation count and estimated airtime composition.
It makes the control/treatment exchange difference visible, while the
identical scheduled bars correctly warn that the selected workload does not
separate the two policies.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260727T211515Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260727T211515Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260727T211515Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260727T211515Z.json` (SHA-256 `a8ebef158507eed350b92b1e50acaf07a417dd47e5a0b9aad8d7f0b7148322a2`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `BacklogBased` | `none (all decoded frames)` | 2877 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (1029), Control: Ack (1020), Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] (492) | 43.66% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `EdcaBaseline` | `none (all decoded frames)` | 2483 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (1460), Control: Ack (1023) | 46.62% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `EqualSizedRUs` | `none (all decoded frames)` | 2877 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (1029), Control: Ack (1020), Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] (492) | 43.66% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | BacklogBased produced protocol-visible wireless observations | 2877 AP/global transmission observations |
| **PASS** | EdcaBaseline produced protocol-visible wireless observations | 2483 AP/global transmission observations |
| **PASS** | EqualSizedRUs produced protocol-visible wireless observations | 2877 AP/global transmission observations |

### [script] Configuration: `BacklogBased`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2877**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1029 | 35.77% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -63.7 dBm | - | 73.22% | 31.97% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d790b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 492 | 17.10% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 22.46% | 9.81% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 168 | 5.84% | 46.3 B | 2.9 B | 35.4 us | 1.0 us | 5010 MHz | - | 10.0 dBm | 0.68% | 0.30% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 168 | 5.84% | 58.0 B | 0.0 B | 39.3 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.76% | 0.33% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1020 | 35.45% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 2.88% | 1.26% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.002064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=216 | Responds without MAC payload while preserving QoS control information. |
| 3 | 0.002064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=222 | Responds without MAC payload while preserving QoS control information. |
| 4 | 0.002064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=228 | Responds without MAC payload while preserving QoS control information. |
| 5 | 0.002133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 6 | 0.103048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 7 | 0.104064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=387 | Responds without MAC payload while preserving QoS control information. |
| 8 | 0.104064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=393 | Responds without MAC payload while preserving QoS control information. |
| 9 | 0.104064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=399 | Responds without MAC payload while preserving QoS control information. |
| 10 | 0.104133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 11 | 0.200644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 12 | 0.201388000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 13 | 0.201436000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 14 | 0.202123000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 15 | 0.202171000 | ? → 0a:aa:00:00:00:02 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 16 | 0.202272000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |

Frame numbers are local to capture `BacklogBased-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `EdcaBaseline`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2483**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1460 | 58.80% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -62.9 dBm | - | 97.29% | 45.35% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1023 | 41.20% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 2.71% | 1.26% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.200644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 2 | 0.201370000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 3 | 0.201418000 | ? → 0a:aa:00:00:00:02 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 4 | 0.202105000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 5 | 0.202153000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 6 | 0.202849000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 7 | 0.202897000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 8 | 0.300644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 9 | 0.301370000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 10 | 0.301418000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 11 | 0.302105000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 12 | 0.302831000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 13 | 0.302879000 | ? → 0a:aa:00:00:00:02 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 14 | 0.303620000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=1, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 15 | 0.303668000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 16 | 0.305644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |

Frame numbers are local to capture `EdcaBaseline-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `EqualSizedRUs`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **2877**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1029 | 35.77% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -63.7 dBm | - | 73.22% | 31.97% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d790b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 492 | 17.10% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 22.46% | 9.81% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 168 | 5.84% | 46.3 B | 2.9 B | 35.4 us | 1.0 us | 5010 MHz | - | 10.0 dBm | 0.68% | 0.30% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 168 | 5.84% | 58.0 B | 0.0 B | 39.3 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.76% | 0.33% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1020 | 35.45% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 2.88% | 1.26% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.002064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=216 | Responds without MAC payload while preserving QoS control information. |
| 3 | 0.002064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=222 | Responds without MAC payload while preserving QoS control information. |
| 4 | 0.002064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=228 | Responds without MAC payload while preserving QoS control information. |
| 5 | 0.002133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 6 | 0.103048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 7 | 0.104064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=387 | Responds without MAC payload while preserving QoS control information. |
| 8 | 0.104064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=393 | Responds without MAC payload while preserving QoS control information. |
| 9 | 0.104064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=399 | Responds without MAC payload while preserving QoS control information. |
| 10 | 0.104133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 11 | 0.200644000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 12 | 0.201388000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 13 | 0.201436000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 14 | 0.202123000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 15 | 0.202171000 | ? → 0a:aa:00:00:00:02 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 16 | 0.202272000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |

Frame numbers are local to capture `EqualSizedRUs-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Analysis of Packet Distribution
`EdcaBaseline` provides the non-triggered control. The two scheduled configurations contain repeated **Trigger** frames, solicited HE-TB observations, and AP **Block Ack** responses, which is the expected HE UL-MU exchange structure (IEEE Std 802.11-2024, Clause 26.5.2 and Annex G.5). Frame-subtype totals alone do not establish that queued payload was carried in the solicited responses or distinguish the two scheduler policies. Use decoded Trigger user allocations, HE-TB payload observations, and aligned scheduler/application telemetry for those decisions.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->

## [agent] Frame exchange analysis

This focused export selects the second measured Basic Trigger exchange:

```sh
tshark -n \
  -r examples/ieee80211ax/ul_ofdma/results/20260727T211515Z/BacklogBased/BacklogBased-\#0Lan80211AxUlOfdma.ap.wlan[0].pcap \
  -Y 'frame.number >= 270 && frame.number <= 274' \
  -T fields -E header=y -E separator='|' -E occurrence=a \
  -e frame.number -e frame.time_epoch -e wlan.fc.type_subtype \
  -e wlan.ta -e wlan.ra -e wlan.trigger.he.trigger_type \
  -e wlan.trigger.he.ul_length -e wlan.trigger.he.user_info.aid12 \
  -e wlan.trigger.he.ru_allocation -e wlan.trigger.he.ul_mcs \
  -e radiotap.he.data_1.ppdu_format \
  -e radiotap.he.data_5.data_bw_ru_allocation \
  -e wlan.qos.tid -e wlan.qos.ack -e wlan.fc.retry
```

| Frame | Simulation time | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 270 | 0.303001 s | AP → broadcast | Basic Trigger / legacy control | trigger type 0; UL Length 730; AID12 `1,2,3`; RU allocation `0,1,2`; MCS 0 | schedules all three associated stations on distinct RUs |
| 271 | 0.304017 s | host[0] → AP | QoS Null / HE-TB | 26-tone RU at 5002 MHz; TID 0; Ack Policy 3 | solicited response, but no queued payload |
| 272 | 0.304017 s | host[2] → AP | QoS Null / HE-TB | 26-tone RU at 5006 MHz; TID 0; Ack Policy 3 | simultaneous response on a different RU |
| 273 | 0.304017 s | host[1] → AP | QoS Null / HE-TB | 26-tone RU at 5004 MHz; TID 0; Ack Policy 3 | simultaneous response on a different RU |
| 274 | 0.304086 s | AP → broadcast | Multi-STA Block Ack / legacy control | follows the three responses | terminates the modeled UL-MU exchange |

The three HE-TB observations share one timestamp and occupy distinct RU
frequencies, directly demonstrating the modeled simultaneous allocation.
Their 1.016 ms gap from the Trigger timestamp is much larger than a literal
SIFS because the AP-interface capture timestamps reflect INET's packet-level
recording/model timing boundary; the capture alone is not suitable for a
standards timing-conformance verdict. Frame numbers are local PCAP indices,
not OMNeT++ event numbers.

Frames 264, 266, 268, 275, and later application frames decode as HE-SU QoS
Data with TID 6, not as HE-TB. The contrast with the Trigger-assigned TID 0
QoS Null is a capacity boundary, not proof of wrong payload selection: the
retained HE-TB allocation cannot fit the 1,000-byte application MPDU.

## [agent] Cross-layer findings and verdict

| Claim | Verdict | Configuration evidence | Model telemetry | Packet evidence | Outcome evidence |
|---|---|---|---|---|---|
| The feature gate creates scheduled UL exchanges | `PASS` | coordinator enabled only in treatments | station allocation vectors record three 26-tone users; coordinator schedules zero bytes | Trigger → three simultaneous HE-TB → Multi-STA BA; absent in EDCA | all configs deliver traffic |
| Scheduled RUs carry queued application payload | `INCONCLUSIVE` | three stations offered TID 6 traffic | coordinator records zero scheduled bytes; station vectors record 38-byte responses | zero HE-TB QoS Data; payload remains HE-SU | the retained allocation cannot fit a 1,000-byte MPDU |
| The two scheduler policies are distinguished | `INCONCLUSIVE` | different scheduler typenames | both receive zero-byte candidates and emit the same 26-tone allocation | scheduled PCAPs byte-identical | five-run estimates identical |
| Serialized Ack Policy matches immediate BA semantics | `FAIL` | Block Ack enabled | AP still completes exchange | HE-TB Ack Policy 3 followed by immediate Multi-STA BA | not an application-outcome question |

The co-recorded run-0 PCAP and result metadata share the same configuration,
run, seed, directory, and simulation trajectory, so adjacent packet and
outcome observations are session-aligned. The evidence directly proves
feature-gate activity and distinct RU use. It does not test successful
scheduled payload service because the allocation is insufficient; QoS Null is
the standards-permitted boundary response. The bounded validation verdict for
the exchange structure is `PASS`, scheduled payload service is
`INCONCLUSIVE`, and the serialized Ack Policy is `FAIL`. The example therefore
still has an overall `FAIL` due to that field-level mismatch, despite
successful simulation exits and near-offered-load delivery over HE-SU/EDCA.

Evidence basis: Trigger/HE-TB/Ack-Policy fields and recorded coordinator/radio
vectors are direct observations; goodput and confidence intervals are derived
measurements; the explanation that Trigger overhead accounts for the small
delivery difference remains an inference.

## [agent] Limitations and inconclusive claims

- Run-0 is representative packet evidence; contention-sensitive packet
  behavior was not compared across all five captures because only run 0 was
  captured by design.
- The result envelope exposes station-side RU/user/PSDU/duration and AP
  reported/scheduled bytes, but not the per-AID `retryPending` state or the
  reason the Trigger policy selected Basic. A focused owner-produced signal is
  the smallest addition needed to make that decision direct evidence.
- The symmetric, equal-size workload may legitimately lead both policies to
  the same allocations. An asymmetric per-station backlog control is needed
  before claiming that the policy implementations are equivalent.
- PCAP cannot expose INET's model-only Trigger ID, queue ownership, buffer
  status freshness, internal response eligibility, or Trigger-selection
  reason. The retained capacity fields explain why QoS Null is legal, not why
  the AP chose that zero-byte opportunity.
- The scalar radio, fixed short distances, one BSS, one bandwidth/MCS basis,
  and five seeds do not support real-network capacity, fairness, interference,
  or coverage claims.
- The timeline is not a SIFS-conformance measurement because the recorder's
  observation timestamp does not isolate PPDU end-to-response-start timing.

## [agent] Further experiments

- Give the stations asymmetric packet intervals while keeping the aggregate
  offered load fixed. Predict different AID/RU-size decisions from the two
  policies; inspect new allocation telemetry before comparing goodput.
- Add a deterministic capacity-fit case by reducing the MPDU size or increasing
  RU/MCS/duration, and retain the current insufficient-capacity case as a
  negative boundary. Assert HE-TB QoS Data only in the proved-fit case and QoS
  Null in the boundary case; inspect Trigger UL Length/RU/MCS and the first
  response after fresh buffer-status reporting.
- Retain the same seed and toggle only `enableUlMuOfdma`. Compare delivery,
  delay, Trigger overhead, and radio energy after the payload invariant passes.
- Add a field-level assertion that every Basic-trigger HE-TB MPDU participating
  in the immediate Multi-STA BA sequence uses a standards-permitted Ack Policy.

## [agent] Implementation plan

No production change is authorized by this walkthrough. The likely IEEE
802.11 MAC files named below are currently unsealed under
`src/inet/linklayer/ieee80211/`; the sealed recursive
`src/inet/common/packet/` subtree is not a proposed change surface. Any future
implementation still requires a fresh architecture/sealing check, one
production-code writer, independent regression evidence, and review.

| Item | Evidence-backed plan |
|---|---|
| Demonstrated gap | HE-TB Ack Policy is 3 although an immediate Multi-STA BA follows; payload service remains inconclusive because the retained 1 ms/26-tone/MCS 0 allocation cannot fit the 1,000-byte MPDU |
| Intended behavior | Follow IEEE 802.11-2024 Clauses 26.5.2.4 and 26.4.4.5: use QoS Null when no pending MPDU fits, include fitting pending MPDUs in a capacity-fit control, and serialize an Ack Policy compatible with the immediate acknowledgment procedure |
| Smallest likely change surface | First fix/test HE-TB Ack Policy construction in `HeHcfUl.cc` and reconcile it with `HeUlMuTxOpFs.cc`; separately instrument `HeUlCoordinator`/`HeUlDefaultTriggerPolicy` before deciding whether received-frame Retry handling in `HeHcfTxRx.cc` requires correction |
| Observability | Emit one owner-produced Trigger-decision event containing Trigger reason, AID, `retryPending`, reported bytes, selected bytes, TID, and RU; retain station response reason and Ack Policy as a separate owned event |
| Validation | Matched EDCA/backlog/equal-RU configs; an insufficient-capacity QoS-Null boundary; a deterministic capacity-fit QoS Data case; a payload A-MPDU/Block-Ack-window case; asymmetric backlog; five seeds after one-seed mechanism checks; assert Trigger capacity fields, response payload/null choice, Ack Policy/Multi-STA BA, server delivery, and unchanged legacy/SU behavior |
| Compatibility and risks | Preserve queue ownership, Block Ack state, deterministic user ordering, QoS/TID mapping, aggregation boundaries, and non-AX/EDCA paths |
| Architecture and sealing | Apply `R-RUN-REPRO`, `R-RESULT-BUILTIN`, `R-RESULT-EXPORT`, `AR-OBS-SIGNALS`, `AR-QUAL-TESTS`, `AR-QUAL-DETERMINISM`, `AR-WLAN-STD-TRACE`, `AR-WLAN-ARCH-OWNERSHIP`, `AR-WLAN-MAC-EXCHANGE`, `AR-WLAN-MAC-MULTIUSER`, `AR-WLAN-OBS-EVENTS`, and `AR-WLAN-QUAL-TESTS`; all named MAC files are unsealed as of this walkthrough |
| Next handoff | Runtime investigator establishes the exact queue/TID decision first; then one implementer owns the bounded fix, followed by independent Wi-Fi regression and standards/architecture review |

## [agent] Artifact provenance

| Artifact family | Session/path | Configurations/runs | Tool/filter/window | Integrity notes |
|---|---|---|---|---|
| Session manifest | `examples/ieee80211/analysis/generated/sessions/20260727T211515Z/session.json` | three configs; runs 0–4 | shared AX suite | publication session; run 0 is the PCAP representative |
| Scalar/vector | `results/20260727T211515Z/` and `../analysis/metrics.json` | three configs; runs/seeds 0–4 | native OMNeT++ result API; `[0.3,2.0)` | file hashes are recorded in `metrics.json` and the figure sidecar |
| Scalar/vector figure | `results/20260727T211515Z/ul-ofdma-delivery.png` | three five-run estimates | per-run delivered-byte goodput, 95% t CI | provenance: `ul-ofdma-delivery.png.json` |
| PCAP | `results/20260727T211515Z/*/*ap.wlan[0].pcap` | three configs; run/seed 0 | TShark/capinfos 4.6.4; typed legacy/HE profiles | capture paths and SHA-256 hashes are in the selected capture manifest |
| PCAP figure | `results/20260727T211515Z/packet_statistics.png` | three run-0 captures | observation count and estimated airtime composition | provenance: `packet_statistics.png.json` |
| Capture manifest | `examples/ieee80211/analysis/generated/ax/capture_manifests/20260727T211515Z.json` | three configs; run 0 | AP `wlan[0]`, PCAPng/radiotap | binds config, run, seed, capture metadata, hashes, and source revision |
