# Walkthrough: Dense IoT with 802.11ax OFDMA and TWT

This example compares an IEEE 802.11ax dense-IoT treatment that requests
orthogonal frequency-division multiple access (OFDMA) and Target Wake Time
(TWT) with a matched IEEE 802.11ac single-user control. The retained evidence
supports a five-seed uplink comparison at 8 and 16 stations; the downlink and
mixed campaigns, and packet-level mechanism checks, remain incomplete.

## Learning objectives and feature primer

After completing this walkthrough, the reader can:

- explain how OFDMA divides a channel into resource units (RUs) and how TWT
  concentrates a station's activity into negotiated service periods;
- identify the AX/AC feature gates, TWT agreement telemetry, AP Basic Trigger
  counter, application outcomes, and station-energy results;
- explain why energy, delivery, and delay must be interpreted together; and
- reproduce the retained uplink queries and identify the first missing
  artifact for packet-level validation.

In the AX treatment, the access point (AP) may schedule several stations on
different RUs and each station requests one implicit, individual, unannounced
TWT agreement. A sleeping station should wake for its service period, exchange
queued traffic, and return to sleep. The AC control uses Enhanced Distributed
Channel Access (EDCA) without TWT. The validation outcome is deliberately
bounded: the treatment passes the retained energy comparison only as an
outcome observation, while the OFDMA frame exchange remains inconclusive.

## Scenario description

The [network](DenseIotNetwork.ned) and [configuration](omnetpp.ini) define one
infrastructure basic service set (BSS). A fixed AP at the center of a 500 m by
500 m area connects through 100 Gbit/s Ethernet to `server`; 8 or 16 stationary
stations are independently placed in the inner 360 m by 360 m square. All
wireless nodes use the same 5 GHz, 20 MHz scalar-radio channel, transmit power,
sensitivity, one antenna, and state-based radio energy model.

```text
       sta[0]       sta[1]       ...       sta[N-1]
          \            |                       /
           +-----------+---- 5 GHz BSS -------+
                              |
                             AP ===== 100 Gbit/s ===== server
```

Association starts are spread over the first 15 s. Applications start between
10 s and 11 s, the simulation lasts 120 s, and OMNeT++ removes the first 20 s
as warm-up. Uplink stations send 100-byte UDP payloads once per second.
Station placement, association, and application phases use separate random
number streams; paired AX/AC repetitions share those inputs, although their
MAC paths can consume randomness differently.

## Standards and INET model boundary

IEEE Std 802.11-2024 Clause 26.8.1 describes TWT as concentrating exchanges in
predefined service periods to reduce contention and awake time
(`80211ax-2024:chunk:09855`). Clause 26.5.4.1 describes AP allocation of
random-access RUs in Trigger frames and the station OFDMA random-access state
(`80211ax-2024:chunk:09810`). These are normative protocol concepts.

INET configures `HeHcf`, backlog-based DL/UL schedulers, an
`Ieee80211TwtManager`, and state-based power consumption. These are model
abstractions, not proof that every standard field or procedure occurred.
`twtAgreementCount` directly exposes model agreement state; the Basic Trigger
counter exposes AP coordinator activity. No retained Dense IoT PCAP is
available, so RU assignments, TWT setup frames, wake-service-period exchanges,
PHY fields, and acknowledgments are not directly observed.

## Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| AX stations establish the requested TWT agreement | `PASS` | `twtAgreementCount = 1` for every station in each `AxUl` scalar file | 8 and 16 stations, repetitions/seeds 0–4 | Direct model telemetry; not a decoded negotiation |
| AP emits Basic Triggers in every AX uplink run | `FAIL` | `heUlBasicTriggerSent:count` | Same ten runs | Nonzero in only 5 of 10 runs |
| AX uplink uses less modeled station energy than AC | `PASS` | residual-energy scalars and paired derived means | Same ten paired runs | 43.99% and 44.02% reductions; outcome only |
| Delivery and delay remain comparable | `INCONCLUSIVE` | server receive count and delay summary | Same ten paired runs | AX sample means are lower/higher respectively, but no acceptance gate or uncertainty was retained |
| OFDMA/TWT frame exchange occurs as intended | `INCONCLUSIVE` | No retained Dense IoT PCAP/log correlation | None | Decisive fields and representative exchange are missing |
| Downlink and mixed AX/AC outcomes | `NOT RUN` | No complete retained `.sca`/`.vec` pairs | None retained | Configurations exist; campaign evidence is incomplete |

## Configuration matrix

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| `AcUl` | Control | `opMode=ac`, `Hcf`, AARF, no TWT | 8/16 STAs, 100 B uplink each 1 s, 20 MHz | 0–4 | Single-user reference |
| `AxUl` | Treatment | `opMode=ax`, `HeHcf`, HE Minstrel, DL/UL schedulers, individual unannounced TWT | Matched to `AcUl` | 0–4 | One agreement per STA; AP trigger telemetry when UL MU is used |
| `AcDl` / `AxDl` | Planned control/treatment | Downlink AX disables empty UL polling; AC remains SU | 100 B per STA each 100 ms | `NOT RUN` in retained session | DL OFDMA outcome comparison |
| `AcMixed` / `AxMixed` | Planned stress pair | Combined UL and DL; AX OFDMA/TWT | Both workloads | `NOT RUN` in retained session | Energy/delivery/delay under mixed load |

The winning AX assignments come from `AxUl extends = Ax, UlTraffic`:
`HeHcf`, backlog-based UL scheduling, 2–4 random-access RUs, a 10 ms minimum
Trigger interval, and one 100 ms/5 ms implicit unannounced agreement per STA.
The AC control changes several coupled mechanisms—PHY generation, rate
control, queueing/access, OFDMA, and TWT—so it is a system treatment/control
comparison, not an isolated estimate of either OFDMA or TWT.

## Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| One TWT agreement per AX STA | station `twtAgreementCount` | zero/multiple agreements | TWT agent, management, manager | Targeted TWT setup logs and AP/STA PCAP |
| AP supplies UL opportunities | AP `heUlBasicTriggerSent:count` and Trigger frames | zero counter or no Basic Trigger | UL trigger policy/coordinator | Correlate coordinator logs with AP PCAP |
| Trigger contains intended RU allocations | Trigger AID12/RU fields | absent/undecoded allocation | scheduler/recorder/dissector | Typed-HE decode from retained AP PCAP |
| Energy is not saved by suppressing work | residual energy plus sink receive/delay | lower energy with lower delivery/higher delay | TWT timing, queues, application window | Co-record radio mode, power, delivery, and queue state |

## Reproduction

Run from the INET repository root. This minimal command was **not executed
during this walkthrough rewrite**; its status is `NOT RUN`:

```sh
bin/inet -u Cmdenv -f examples/ieee80211ax/dense_iot/omnetpp.ini \
  -c AxUl -r 0 --result-dir=/tmp/inet-dense-iot-axul-r0
```

The retained campaign was created by the example runner. A full regeneration
command is:

```sh
python3 examples/ieee80211ax/dense_iot/run_campaign.py \
  --station-counts 8,16 --runs-per-station-count 5 -j$(nproc)
```

These commands are recipes, not claims about a newly observed exit status.
The retained scalar/vector files identify repetitions and seed sets 0–4.

## Scalar and vector analysis

Inputs are the 20 `.sca` and 20 `.vec` files under
`results/scalar-vector/20260725T120411Z/{AxUl,AcUl}/`. Query only the receiving
application and named mechanism results:

```sh
opp_scavetool query -l \
  -f 'module =~ "*.server.app[0]" AND (name =~ "packetReceived:count" OR name =~ "endToEndDelay:vector")' \
  examples/ieee80211ax/dense_iot/results/scalar-vector/20260725T120411Z/AxUl/*.{sca,vec} \
  examples/ieee80211ax/dense_iot/results/scalar-vector/20260725T120411Z/AcUl/*.{sca,vec}

opp_scavetool query -l \
  -f 'name =~ "residualEnergyCapacity:last" OR name =~ "twtAgreementCount" OR name =~ "twtAwakeTime" OR name =~ "twtSleepTime" OR name =~ "heUlBasicTriggerSent:count"' \
  examples/ieee80211ax/dense_iot/results/scalar-vector/20260725T120411Z/AxUl/*.sca \
  examples/ieee80211ax/dense_iot/results/scalar-vector/20260725T120411Z/AcUl/*.sca
```

Receive count is `DenseIotNetwork.server.app[0] packetReceived:count`; delay is
the summary mean of its `endToEndDelay:vector`. Mean station energy used is the
per-run station average of `1000 J - residualEnergyCapacity:last`. Each table
entry is one run; the final row is the arithmetic mean of five independent
seed-set repetitions. No confidence interval was retained for this analysis.

### Eight stations

| Repetition | AX received | AC received | AX mean delay | AC mean delay | AX mean station energy | AC mean station energy |
|---:|---:|---:|---:|---:|---:|---:|
| 0 | 700 | 800 | 0.028219 s | 0.000105 s | 0.135945 J | 0.242398 J |
| 1 | 800 | 800 | 0.020160 s | 0.000104 s | 0.134993 J | 0.242427 J |
| 2 | 800 | 800 | 0.046847 s | 0.000104 s | 0.136388 J | 0.242382 J |
| 3 | 800 | 800 | 0.040412 s | 0.000104 s | 0.136055 J | 0.242359 J |
| 4 | 800 | 800 | 0.047810 s | 0.000103 s | 0.135473 J | 0.242431 J |
| Five-run mean | 780.0 | 800.0 | 0.036690 s | 0.000104 s | 0.135771 J | 0.242400 J |

### Sixteen stations

| Repetition | AX received | AC received | AX mean delay | AC mean delay | AX mean station energy | AC mean station energy |
|---:|---:|---:|---:|---:|---:|---:|
| 0 | 1,570 | 1,600 | 1.345015 s | 0.000104 s | 0.135930 J | 0.242981 J |
| 1 | 1,565 | 1,592 | 1.325530 s | 0.431639 s | 0.136496 J | 0.243095 J |
| 2 | 1,613 | 1,607 | 1.037000 s | 0.028115 s | 0.135499 J | 0.243096 J |
| 3 | 1,600 | 1,600 | 0.048280 s | 0.000104 s | 0.135194 J | 0.243066 J |
| 4 | 1,598 | 1,600 | 0.120907 s | 0.000103 s | 0.137231 J | 0.243062 J |
| Five-run mean | 1,589.2 | 1,599.8 | 0.775346 s | 0.091813 s | 0.136070 J | 0.243060 J |

The derived AX energy reductions are 43.99% at 8 stations and 44.02% at 16.
They do not establish a better overall outcome: AX has lower mean receive
counts and substantially higher mean delay. Counts can exceed the nominal
measurement-window generation count because packets generated during warm-up
may arrive after recording begins; they are therefore not delivery ratios.
Every AX station records one agreement, while Basic Trigger counts are
`170,0,0,0,0` (8 STAs) and `10,10,10,0,440` (16 STAs).

## PCAP statistics

Capture point, `.pcap`/`.pcapng` format, precision, and decode version:
`NOT RUN`. No `results/packet-statistics/` session is retained for Dense IoT,
so there is no direct observation from TShark.

```sh
python3 examples/ieee80211/analysis/analyze_pcap.py \
  --suite examples/ieee80211/analysis/suites/ax.json \
  --generate --subdir dense_iot --run 0
```

This is the bounded regeneration recipe and was `NOT RUN` during this rewrite.

| Configuration | Observation count | Relevant frame/PHY summary | Interpretation limit |
|---|---:|---|---|
| `AxUl` | `NOT RUN` | No retained typed-HE decode | OFDMA and TWT exchanges unproven |
| `AcUl` | `NOT RUN` | No retained VHT decode | No packet-level counterfactual |

## Frame exchange analysis

No representative retained exchange can be cited.

| Frame | Simulation time | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| — | — | — | `NOT RUN` | Trigger RU/AID12, TWT setup, wake state, acknowledgment absent | Required to connect agreement state and coordinator counters to packets |

The smallest decisive run would co-record AP and at least one STA PCAP,
`twtAgreementCount`, Basic Trigger counters, radio mode/power, and application
delivery for one `AxUl` seed, then compare the same seed with `AcUl`.

## Cross-layer findings and verdict

| Claim | Verdict | Configuration evidence | Model telemetry | Packet evidence | Outcome evidence |
|---|---|---|---|---|---|
| TWT is established | `PASS` | requester/responder and manager enabled | one agreement per STA | `INCONCLUSIVE` | sleep/energy scalars are adjacent evidence |
| UL OFDMA is consistently exercised | `FAIL` | UL scheduler enabled | zero Basic Triggers in 5/10 runs | `INCONCLUSIVE` | no mechanism-to-outcome causality |
| AX reduces modeled station energy | `PASS` | matched energy model | residual capacity | `INCONCLUSIVE` | 43.99%/44.02% lower mean energy |
| AX preserves service quality | `INCONCLUSIVE` | matched offered uplink load | receive/delay results | `INCONCLUSIVE` | lower mean receive count and higher delay are sample observations; no acceptance gate was defined |

The cross-layer verdict is mixed. Configuration and TWT telemetry directly
show requested agreements; those are direct observations from model results.
The energy reductions are derived measurements from residual capacity, while
the proposed mechanism-to-outcome explanation is inference.
Missing packet evidence and intermittent trigger telemetry prevent a complete
OFDMA/TWT causal chain.

## Limitations and inconclusive claims

- There is no retained Dense IoT PCAP, so RU allocation, TWT negotiation,
  service-period timing, retries, and acknowledgments are `INCONCLUSIVE`.
- Only uplink at 8 and 16 stations is complete; downlink, mixed, and larger
  dense-BSS points are `NOT RUN` in the retained session.
- The AX/AC pair changes multiple mechanisms and cannot isolate TWT from OFDMA,
  rate control, or queueing.
- The smallest resolution is one co-recorded paired seed, followed by the
  remaining four seeds only if the mechanism is observed and stable.

## Further experiments

- Disable TWT while retaining AX/OFDMA; predict higher awake energy with
  similar Trigger telemetry.
- Disable UL OFDMA while retaining AX/TWT; predict zero Basic Trigger count and
  expose the energy/delay contribution of scheduled access.
- Repeat one 16-STA pair with AP/STA captures; require a decoded Trigger/RU
  exchange and aligned power/application telemetry before expanding seeds.

## Implementation plan

| Item | Evidence-backed plan |
|---|---|
| Demonstrated gap | Five AX uplink runs record zero Basic Triggers, and no packet session explains why |
| Intended behavior | Determine whether TWT timing legitimately suppresses UL trigger opportunities or whether observability/scheduling is deficient |
| Smallest change surface | First inspect `omnetpp.ini` trigger/TWT timing and existing coordinator/TWT signals; no production-code change is yet justified |
| Observability | Co-record Basic/BSRP Trigger decisions, TWT awake state, queue backlog, AP/STA PCAP, and delivery |
| Validation | `AxUl` versus `AcUl`, one paired seed first; invariant is a correlated awake service period and successful exchange |
| Compatibility and risks | Instrumentation may alter event trajectory; do not compare exact counts across sessions |
| Architecture and sealing | No `src/inet` change is proposed; apply architectural/sealing review before any future production edit |
| Next handoff | Wi-Fi simulation investigator after a co-recorded run establishes the first divergence |

## Artifact provenance

| Artifact family | Session/path | Configurations/runs | Tool/filter/window | Integrity notes |
|---|---|---|---|---|
| Scalar/vector | `results/scalar-vector/20260725T120411Z` | `AxUl`, `AcUl`; 8/16 STAs; seeds 0–4 | queries above; warm-up 20 s | 20 matched `.sca`/`.vec` pairs per technology/size set |
| PCAP | none retained | none | `NOT RUN` | packet claims remain inconclusive |
| Configuration | `omnetpp.ini`, `DenseIotNetwork.ned` | all declared configs | inheritance described above | configuration input, not runtime proof |
