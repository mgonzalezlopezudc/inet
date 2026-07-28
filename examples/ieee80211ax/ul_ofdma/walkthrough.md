# Walkthrough: IEEE 802.11ax scheduled uplink OFDMA

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260727T225125Z`
- PCAP: `20260727T225125Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `20260727T211515Z`.

Post-implementation checks cited below are an independent, unretained local
verification set under `/tmp/ul-capacity-repaired-*`; they are not a managed
results session and are not part of either results-session ledger above.

This walkthrough compares two access-point (AP) scheduling policies with a
single-user Enhanced Distributed Channel Access (EDCA) control. The retained
five-run result set and run-0 packet captures are co-recorded in the
pre-implementation session `20260727T211515Z`. They prove that the feature gate
creates Trigger-led multi-user exchanges, but they also expose two findings
that motivated the implementation described below: the scheduled HE
trigger-based (HE-TB) allocations are too small to test queued 1,000-byte
payload service, and their on-wire Ack Policy conflicts with the immediate
Multi-STA Block Ack procedure. The implementation now corrects the Ack Policy,
removes received-Retry scheduling, adds owner-produced decision/response
events, and supplies deterministic capacity-fit and asymmetric-backlog
validation configurations. The retained publication session remains useful as
historical pre-fix evidence and has not been rewritten.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- explain how an AP coordinates simultaneous uplink transmissions with a
  Trigger frame and per-station resource-unit (RU) assignments;
- distinguish HE-TB scheduled responses from ordinary HE single-user (HE-SU)
  uplink frames in a radiotap capture;
- compare capacity-aware and equal-sized-RU scheduler inputs without treating
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
aggregate, until the effective 1 s limit.

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

`HeUlSchedulerBacklogBased` is now an INET capacity-aware policy, despite its
compatibility typename. It fixes a common HE-TB timing boundary, asks the PHY
calculator for each `(RU, MCS, NSS, FEC)` PSDU capacity, subtracts the shared
minimum QoS/A-MPDU framing estimate, and maximizes conservative planned service
bytes over the PHY-owned RU allocation tree. IEEE 802.11 defines the legal
Trigger fields, HE-TB encoding, and timing; it does not define this objective,
fairness order, random-access reservation, or tie-break policy. FEC is selected
per Trigger User Info, so a legal schedule may contain both BCC and LDPC users.

The scalar radio approximates per-RU uplink reception by frequency/power
placement and does not model timing advance. `PcapRecorder` flattens A-MPDU
contents into one capture record per MPDU, omitting delimiters and padding.
Radiotap HE fields are authoritative only when their known bits are present.

One behavior in the retained pre-implementation session is not
standards-conformant. The three HE-TB QoS Null responses after frame 270 carry
Ack Policy `3` (`Block Ack`), while IEEE 802.11-2024 Clause 26.4.4.5 and Table
9-13 require an Ack Policy that solicits the immediate response (Normal Ack or
Implicit BAR) for this Basic-trigger sequence. The old implementation
nevertheless unconditionally schedules the terminal Multi-STA Block Ack. This
walkthrough records that historical observation as a model-control versus
serialized wire-semantics mismatch, not as a TShark heuristic. The completed
implementation serializes wire bits `00` and preserves the immediate terminal
Multi-STA Block Ack.

## [agent] Evidence status

This table describes retained session `20260727T211515Z`, before the
implementation outcome reported later in this walkthrough.

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

The original three configurations inherit `[General]`; none uses a `[Config
...] extends` chain. The implementation adds `CapacityFit` and
`AsymmetricBacklog`, both extending `BacklogBased` as focused validation
controls.

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| `BacklogBased` | Treatment | `enableUlMuOfdma=true`; `HeUlSchedulerBacklogBased` scores PHY capacity against conservative BSR bounds | 3 × 1,000 B/5 ms; AX, 20 MHz | 0–4 / 0–4 | Basic Trigger assigns a deterministic fairness prefix and legal mixed/equal RUs |
| `EqualSizedRUs` | Treatment/policy control | same gate; `HeUlSchedulerEqualSizedRUs` allocates equal RUs and uses backlog for duration | matched | 0–4 / 0–4 | Same exchange structure, with policy-dependent allocations when demand exposes a difference |
| `EdcaBaseline` | Negative control | `enableUlMuOfdma=false`; coordinator inactive | matched | 0–4 / 0–4 | no Basic/BSRP Trigger or HE-TB scheduled response |
| `CapacityFit` | Positive mechanism control | capacity-aware scheduler; 10-byte packets; 1 ms source interval; CapacityFit-local legacy EDCA suppression | AX, 20 MHz; 1 s | run 0 / seed 1 | fitting queued packets produce HE-TB QoS Data with Ack Policy `00` and one terminal Multi-STA BA |
| `AsymmetricBacklog` | Scheduler discriminator | capacity-aware scheduler; 256-byte packets; per-station intervals 1/2/4 ms; AsymmetricBacklog-local legacy EDCA suppression | AX, 20 MHz; 1 s | run 0 / seed 1 | unequal conservative BSR bounds produce a deterministic mixed-RU plan when it scores best |

The AP's NED default scheduler typename remains backlog-based in
`EdcaBaseline`, but it is inactive because the UL coordinator's `enabled`
parameter follows `enableUlMuOfdma`. Both treatments set
`minRandomAccessRus=maxRandomAccessRus=0`, so every Trigger user entry should
be scheduled rather than random access.

The retained post-change seed-1 evidence keeps scheduler intent separate from
station and server outcomes:

| Configuration | RU layout (offset:size:AID) | Per-user MCS/FEC | Common duration | AP planned bytes | STA actual selected bytes | Status |
|---|---|---|---|---|---|---|
| `CapacityFit`, run 0, seed 1 | legal layouts with at most 3 scheduled users across 129 Basic Triggers | MCS 0; scheduler-selected FEC per negotiated user | Trigger-derived common boundary | recorded per user; the retained summary did not aggregate this field | 2,074 HE-TB QoS Data MPDUs of 10 payload bytes; Trigger/data/Multi-STA-BA sequences observed | `PASS` positive fitting boundary |
| `AsymmetricBacklog`, run 0, seed 1 | representative frame 300: AID 1 `106@0`, AID 2 `26@108`, AID 3 `106@136` (RU codes 53, 4, 54) | MCS 0; LDPC for the scheduled users in the retained trace | UL Length 1108 | 511 nonzero scheduled-user events, total 292,475 B; maximum 3 users per Trigger | server received 753 packets / 192,768 B; this aggregate is not a per-Trigger selected-byte identity | `PASS` mixed-layout discriminator |

## [agent] Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| Feature gate controls Trigger exchange | AP PCAP: Basic/BSRP Trigger and HE-TB counts | scheduled capture has no Trigger, or EDCA has one | `HeHcf` / UL coordinator initialization | inspect effective `enableUlMuOfdma`, coordinator timer, and AP channel-access request |
| Trigger allocation is internally valid | Trigger AID12/RU fields and HE-TB RU/frequency | missing user, duplicate RU, wrong response PHY | scheduler → Trigger serialization → STA validation | export per-user Trigger fields and correlate with `HeUlMuTxOpFs` |
| Fitting queued payload is selected for the Basic Trigger | capacity-checked Trigger plus HE-TB QoS Data and coordinator reported/scheduled-byte vectors | QoS Null despite a proved fitting eligible MPDU | Trigger policy, buffer status, or triggered A-MPDU builder | first run a deterministic fitting-allocation control; then inspect queue/TID selection |
| Ack policy solicits the modeled immediate BA | `wlan.qos.ack` in HE-TB MPDUs | value `3` while AP sends immediate Multi-STA BA | HE-TB QoS header construction/serializer | inspect Ack Policy assignment and add field-level regression |
| Scheduler policies are observable | allocation telemetry and per-run outcome | identical zero-byte allocations | scheduler inputs or workload not discriminating | record the selected Trigger reason/retry-pending state; add asymmetric backlog case |

## [agent] Reproduction

The checked-in configuration uses a total simulation time of 1 s. The current
default scalar/vector window is `[0.3,0.95)` s; the two condition-specific
`[0.3,0.8)` s windows remain unchanged. Numeric results below remain
provenance-bound to the earlier retained 2 s result session.

Run from the INET repository root. This minimal direct command is a convenient
single-configuration smoke check; the retained evidence commands follow it.

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

The post-implementation checks were executed in release mode, one
configuration and run at a time:

```sh
bin/inet -u Cmdenv \
  -f examples/ieee80211ax/ul_ofdma/omnetpp.ini \
  -c CapacityFit -r 0 --seed-set=1 \
  --result-dir=/tmp/ul-capacity-repaired-capacityfit-r0-s1 \
  --cmdenv-express-mode=true \
  '--*.ap.wlan[*].recordPcap=true' \
  '--*.ap.wlan[*].pcapRecorder[*].timePrecision=9' \
  '--*.ap.wlan[*].pcapRecorder[*].alwaysFlush=true' \
  '--*.ap.wlan[*].pcapRecorder[*].verbose=false' \
  '--**.checksumMode="computed"' '--**.fcsMode="computed"'
```

This checked-in configuration and retained run both use the effective 1 s
duration. It produced 129 Basic Triggers and 2,074 10-byte HE-TB QoS Data
MPDUs in complete Trigger/data/Multi-STA-BA exchanges.

The asymmetric, negative-control, smoke, and focused-test commands were:

```sh
bin/inet -u Cmdenv \
  -f examples/ieee80211ax/ul_ofdma/omnetpp.ini \
  -c AsymmetricBacklog -r 0 --seed-set=1 \
  --result-dir=/tmp/ul-capacity-repaired-asymmetric-r0-s1 \
  --cmdenv-express-mode=true

bin/inet -u Cmdenv \
  -f examples/ieee80211ax/ul_ofdma/omnetpp.ini \
  -c EdcaBaseline -r 0 --seed-set=1 \
  --result-dir=/tmp/ul-capacity-repaired-edca-r0-s1 \
  --cmdenv-express-mode=true

bin/inet -u Cmdenv \
  -f examples/ieee80211ax/ul_uora/omnetpp.ini \
  -c MixedUora -r 0 --seed-set=1 \
  --result-dir=/tmp/ul-capacity-repaired-mixeduora-r0-s1 \
  --cmdenv-express-mode=true

CCACHE_DISABLE=1 inet_run_unit_tests -m release \
  -f '(HeUlScheduler|Ieee80211HeBsrBsrpIntegration|Ieee80211HeErSuBsr|Ieee80211HePhyCalculatorGolden|Ieee80211HeRu|Ieee80211HeSchedulerValidation|Ieee80211HeTxVector_1|Ieee80211HeUlControlFrames|Ieee80211HeUlMuTransaction).*'

CCACHE_DISABLE=1 inet_run_module_tests -m release --no-build --no-concurrent \
  -f 'Ieee80211HeUlTriggerExchange_1.*'
```

The retained repaired evidence uses run 0 and seed set 1 for each focused
configuration. It is a deterministic regression set, not a multi-seed
statistical campaign.

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
each station records 164 allocation samples over the retained full 0–2 s run:

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

![ul_ofdma scalar/vector analysis](results/20260727T225125Z/ul-ofdma-delivery.png)

Figure provenance: [`results/20260727T225125Z/ul-ofdma-delivery.png.json`](results/20260727T225125Z/ul-ofdma-delivery.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.app[*] / packetReceived:vector(packetBytes)
- Window / per-run aggregation / exclusions: [0.3, 2.0) s; observation=one aggregate goodput value per run; uncertainty=95% Student-t CI
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| Backlog scheduler / goodput mbps | 2.38748 | 0.00449959 |
| EDCA baseline / goodput mbps | 2.39501 | 0.00224789 |
| Equal-sized RUs / goodput mbps | 2.38748 | 0.00449959 |

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
![802.11 Packet Type Statistics](results/20260727T225125Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260727T225125Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260727T225125Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260727T225125Z.json` (SHA-256 `4107f6ffc4285a265d3b9ee37891e31e79c7bc34fb819b680f4162bc75ce6edf`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `BacklogBased` | `none (all decoded frames)` | 7103 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (4901), Control: Block Ack (BA) (653), Control: Block Ack Request (BAR) (493) | 37.61% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `EdcaBaseline` | `none (all decoded frames)` | 12057 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (7968), Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (1934), Control: Block Ack Request (BAR) (1267) | 54.88% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `EqualSizedRUs` | `none (all decoded frames)` | 7103 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (4901), Control: Block Ack (BA) (653), Control: Block Ack Request (BAR) (493) | 37.61% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | BacklogBased produced protocol-visible wireless observations | 7103 AP/global transmission observations |
| **PASS** | EdcaBaseline produced protocol-visible wireless observations | 12057 AP/global transmission observations |
| **PASS** | EqualSizedRUs produced protocol-visible wireless observations | 7103 AP/global transmission observations |

### [script] Configuration: `BacklogBased`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **7103**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 4901 | 69.00% | 166.6 B | 1.4 B | 95.6 us | 12.5 us | 5010 MHz | -50.0 dBm | - | 62.25% | 23.41% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 376 | 5.29% | 170.0 B | 0.0 B | 129.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 6.45% | 2.43% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d790b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 484 | 6.81% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 25.65% | 9.65% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 181 | 2.55% | 46.9 B | 4.8 B | 35.6 us | 1.6 us | 5010 MHz | - | 10.0 dBm | 0.86% | 0.32% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 493 | 6.94% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 1.84% | 0.69% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 653 | 9.19% | 39.2 B | 11.6 B | 33.1 us | 3.9 us | 5010 MHz | - | 10.0 dBm | 2.87% | 1.08% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 3 | 0.04% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.01% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.08% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.08% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.06% | 0.02% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.002064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=220 | Responds without MAC payload while preserving QoS control information. |
| 3 | 0.002064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=224 | Responds without MAC payload while preserving QoS control information. |
| 4 | 0.002064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=228 | Responds without MAC payload while preserving QoS control information. |
| 5 | 0.002133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 6 | 0.103048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 7 | 0.104064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=385 | Responds without MAC payload while preserving QoS control information. |
| 8 | 0.104064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=389 | Responds without MAC payload while preserving QoS control information. |
| 9 | 0.104064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=393 | Responds without MAC payload while preserving QoS control information. |
| 10 | 0.104133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 11 | 0.200164000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 12 | 0.200437000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 13 | 0.200722000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 14 | 0.200770000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 16 | 0.200866000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 17 | 0.201064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |

Frame numbers are local to capture `BacklogBased-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `EdcaBaseline`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **12057**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 7968 | 66.09% | 168.5 B | 1.9 B | 98.6 us | 14.1 us | 5010 MHz | -50.0 dBm | - | 71.54% | 39.26% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1934 | 16.04% | 170.0 B | 0.0 B | 129.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 22.73% | 12.47% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 1267 | 10.51% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 3.23% | 1.77% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 873 | 7.24% | 32.0 B | 0.0 B | 30.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 2.44% | 1.34% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 3 | 0.02% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.01% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.05% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.01% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.05% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.04% | 0.02% |

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
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **7103**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 4901 | 69.00% | 166.6 B | 1.4 B | 95.6 us | 12.5 us | 5010 MHz | -50.0 dBm | - | 62.25% | 23.41% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 376 | 5.29% | 170.0 B | 0.0 B | 129.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 6.45% | 2.43% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d790b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 484 | 6.81% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 25.65% | 9.65% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 181 | 2.55% | 46.9 B | 4.8 B | 35.6 us | 1.6 us | 5010 MHz | - | 10.0 dBm | 0.86% | 0.32% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 493 | 6.94% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -50.0 dBm | - | 1.84% | 0.69% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 653 | 9.19% | 39.2 B | 11.6 B | 33.1 us | 3.9 us | 5010 MHz | - | 10.0 dBm | 2.87% | 1.08% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 3 | 0.04% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.01% | 0.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.08% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.08% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -50.0 dBm | 10.0 dBm | 0.06% | 0.02% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.002064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=220 | Responds without MAC payload while preserving QoS control information. |
| 3 | 0.002064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=224 | Responds without MAC payload while preserving QoS control information. |
| 4 | 0.002064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=228 | Responds without MAC payload while preserving QoS control information. |
| 5 | 0.002133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 6 | 0.103048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 7 | 0.104064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=385 | Responds without MAC payload while preserving QoS control information. |
| 8 | 0.104064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=389 | Responds without MAC payload while preserving QoS control information. |
| 9 | 0.104064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=393 | Responds without MAC payload while preserving QoS control information. |
| 10 | 0.104133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
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

The pre-implementation co-recorded run-0 PCAP and result metadata share the same configuration,
run, seed, directory, and simulation trajectory, so adjacent packet and
outcome observations are session-aligned. The evidence directly proves
feature-gate activity and distinct RU use. It does not test successful
scheduled payload service because the allocation is insufficient; QoS Null is
the standards-permitted boundary response. The bounded validation verdict for
the exchange structure is `PASS`, scheduled payload service is
`INCONCLUSIVE`, and the serialized Ack Policy is `FAIL`. That retained session
therefore has an overall `FAIL` due to the field-level mismatch, despite
successful simulation exits and near-offered-load delivery over HE-SU/EDCA.
The current implementation verdict is reported below.

Evidence basis: Trigger/HE-TB/Ack-Policy fields and recorded coordinator/radio
vectors are direct observations; goodput and confidence intervals are derived
measurements; the explanation that Trigger overhead accounts for the small
delivery difference remains an inference.

## [agent] Limitations and inconclusive claims

- Run-0 is representative packet evidence; contention-sensitive packet
  behavior was not compared across all five captures because only run 0 was
  captured by design.
- The historical publication result envelope lacks the per-AID
  `retryPending` state and Trigger-selection reason. The repaired model now
  emits those owner-produced fields, but the unretained focused artifacts do
  not retroactively add them to the historical session.
- The symmetric workload may legitimately lead both policies to the same
  allocations. The repaired asymmetric control proves that the capacity-aware
  policy selects mixed sizes; it does not by itself quantify a paired
  equal-sized-policy outcome.
- PCAP cannot expose INET's model-only Trigger ID, queue ownership, buffer
  status freshness, internal response eligibility, or Trigger-selection
  reason. The retained capacity fields explain why QoS Null is legal, not why
  the AP chose that zero-byte opportunity.
- The scalar radio, fixed short distances, one BSS, one bandwidth/MCS basis,
  the historical five-seed outcome set, and the repaired single-seed mechanism
  checks do not support real-network capacity, fairness, interference, or
  coverage claims.
- The timeline is not a SIFS-conformance measurement because the recorder's
  observation timestamp does not isolate PPDU end-to-response-start timing.
- The post-implementation `/tmp/ul-capacity-repaired-*` checks are unretained local
  regression artifacts, not a managed publication session. Their exact
  commands, paths, and scope are recorded here, but they may not survive
  workspace cleanup and are not listed in the generated results-session
  blocks.

## [agent] Further experiments

- Run the asymmetric workload with the equal-sized policy under the same seed
  and retain both decision-event streams before comparing goodput.
- Retain the same seed and toggle only `enableUlMuOfdma`. Compare delivery,
  delay, Trigger overhead, and radio energy; the present EDCA control records
  delivery and feature-gate behavior but is not an energy comparison.
- Repeat the repaired mechanism controls across a predefined seed set only if
  the claim is expanded from deterministic mechanism evidence to a
  contention-sensitive statistical outcome.

## [agent] Implementation outcome

The plan has been executed. The affected IEEE 802.11 MAC files were confirmed
unsealed before modification; the sealed recursive
`src/inet/common/packet/` subtree was not touched.

| Item | Completed outcome and evidence |
|---|---|
| Ack Policy | HE-TB QoS Data and QoS Null construction now uses `NORMAL_ACK`, whose serialized bits are `00`. A unit test checks data, null, and every member of a serialized two-MPDU A-MPDU, including exactly one EOF/Tag delimiter. |
| Immediate response ownership | Valid HE-TB responses stay owned by the active Trigger collection and terminal Multi-STA BA. Overheard, late, or out-of-window HE-TB packets are discarded before legacy HCF Ack processing, preventing a parallel `WlanAck` timer. |
| Retry semantics | A received frame's Retry bit is no longer cached as future scheduler work. The compatibility overload and layout remain available, while coordinator values stay false/zero. Originator-owned Block Ack failure/timeout requeue and Retry marking are unchanged. |
| Observability | `HeUlCoordinator` emits a typed Trigger-decision event with reason and per-user AID/backlog/reported/selected-byte/TID/AC/RU fields. `HeHcf` emits a typed station-response event with response reason, Trigger identity/type, AID/TID/AC/RU, selected/reported bytes, and actual Ack Policy. |
| Capacity-fit control | `CapacityFit` retains live backlog for the effective 1 s run with a 1 ms source and local legacy-EDCA contention settings. Repaired run 0 / seed 1 recorded 129 Basic Triggers, at most three scheduled users, and 2,074 length-10 HE-TB QoS Data MPDUs in Trigger/data/Multi-STA-BA exchanges. |
| Asymmetric control | Repaired run 0 / seed 1 recorded 282 Basic Triggers and 511 scheduled-user events, with as many as three users per Trigger. All 511 planned-byte values were nonzero and totaled 292,475 B. Representative frame 300 used `106+26+106` for AIDs 1, 2, and 3 at MCS 0 and UL Length 1108. The server received 753 packets / 192,768 B; that aggregate outcome is not substituted for per-user planned or selected bytes. |
| Negative and compatibility controls | Repaired `EdcaBaseline` run 0 / seed 1 recorded no Basic/BSRP Trigger, scheduled-user, or RA-RU event and delivered 2,090 packets / 209,000 B. Repaired `MixedUora` recorded 765 Basic Triggers, 1,468 scheduled-user assignments (maximum two), and 765 RA assignments (maximum one); representative frame 390 used one 26-tone RA RU plus AID 2 on a 106-tone RU, followed by a 100-byte QoS Data response and a QoS Null response. Focused release units and the deterministic release module test pass; no fingerprint baseline was changed. |
| Fingerprint diagnostic | The narrow `MixedUora` fingerprint remains an intentional mismatch: expected `8705-fe54/tplx`, calculated `3953-411d/tplx`. The changed workload and legacy-EDCA suppression create the first Basic Trigger at 0.203988 s, where the prior run had none. No fingerprint CSV or generated update artifact was retained. |

The AP schedules from cached BSR state, while each station selects from its
current EDCA queue at Trigger time and then performs a capacity check. This
explains why positive AP backlog alone did not prove a data-bearing response:
ordinary EDCA could drain the packet first, and a 64-byte packet still did not
fit after protocol overhead. The 10-byte capacity-fit control exercises the
positive boundary. The retained 1,000-byte workload remains a non-fitting
boundary; the asymmetric 256-byte run is a scheduler-plan discriminator, and
its server aggregate must not be treated as per-Trigger selected-byte proof.

The current bounded implementation verdict is `PASS` for Ack Policy,
Trigger-owned terminal acknowledgment, capacity-fit HE-TB payload service,
received-Retry scheduling semantics, typed observability, deterministic
focused execution, and the EDCA feature gate.

## [agent] Artifact provenance

| Artifact family | Session/path | Configurations/runs | Tool/filter/window | Integrity notes |
|---|---|---|---|---|
| Session manifest | `examples/ieee80211/analysis/generated/sessions/20260727T211515Z/session.json` | three configs; runs 0–4 | shared AX suite | publication session; run 0 is the PCAP representative |
| Scalar/vector | `results/20260727T211515Z/` and `../analysis/metrics.json` | three configs; runs/seeds 0–4 | native OMNeT++ result API; `[0.3,2.0)` | file hashes are recorded in `metrics.json` and the figure sidecar |
| Scalar/vector figure | `results/20260727T211515Z/ul-ofdma-delivery.png` | three five-run estimates | per-run delivered-byte goodput, 95% t CI | provenance: `ul-ofdma-delivery.png.json` |
| PCAP | `results/20260727T211515Z/*/*ap.wlan[0].pcap` | three configs; run/seed 0 | TShark/capinfos 4.6.4; typed legacy/HE profiles | capture paths and SHA-256 hashes are in the selected capture manifest |
| PCAP figure | `results/20260727T211515Z/packet_statistics.png` | three run-0 captures | observation count and estimated airtime composition | provenance: `packet_statistics.png.json` |
| Capture manifest | `examples/ieee80211/analysis/generated/ax/capture_manifests/20260727T211515Z.json` | three configs; run 0 | AP `wlan[0]`, PCAPng/radiotap | binds config, run, seed, capture metadata, hashes, and source revision |
| Repaired CapacityFit | `/tmp/ul-capacity-repaired-capacityfit-r0-s1/` | `CapacityFit`; run 0; seed set 1 | scalar/vector inspection and AP PCAP correlation | unretained local verification; 129 Basic Triggers and 2,074 length-10 HE-TB QoS Data MPDUs |
| Repaired asymmetric | `/tmp/ul-capacity-repaired-asymmetric-r0-s1/` | `AsymmetricBacklog`; run 0; seed set 1 | scheduler event vectors, server results, and AP PCAP | unretained local verification; representative frame 300 documented above |
| Repaired feature-gate control | `/tmp/ul-capacity-repaired-edca-r0-s1/` | `EdcaBaseline`; run 0; seed set 1 | scalar/vector inspection and AP PCAP | no HE UL Trigger/scheduled/RA events; 2,090 packets / 209,000 B |
| Repaired mixed UORA | `/tmp/ul-capacity-repaired-mixeduora-r0-s1/` | `MixedUora`; run 0; seed set 1 | scheduler event vectors and AP PCAP | 765 Basic Triggers; scheduled and RA assignments coexist |
| Focused tests | `/tmp/capacity-final-nine-units.log`, `/tmp/capacity-final-module.log` | nine unit tests and one deterministic module test | release unit and module runners | final self-test artifacts |
