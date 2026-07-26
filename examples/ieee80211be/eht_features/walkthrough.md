# Walkthrough: 802.11be EHT width and modulation features

This example compares a 160 MHz IEEE 802.11ax (HE) link with a configured
320 MHz IEEE 802.11be Extremely High Throughput (EHT) link. Its learning goal
is to connect channel width, EHT Modulation and Coding Scheme (MCS) selection,
and protocol-visible PHY evidence. Its validation goal is stricter: a run
passes only when the selected configuration completes, authoritative capture
fields identify the requested PHY, and the receiver outcome is recorded.

## Learning objectives and feature primer

After completing this walkthrough, the reader can:

- explain why 320 MHz operation and 4096-QAM can raise the PHY rate;
- distinguish an advertised EHT capability from the mode used by a frame;
- locate the configuration, result, and radiotap observations needed to prove
  the comparison; and
- reproduce the current failure and identify the first useful diagnostic.

An access point (AP) may advertise EHT capabilities such as 320 MHz operation,
Low-Density Parity-Check (LDPC) coding, and 4096-QAM. Those inputs do not prove
that a transmitted Physical Layer Protocol Data Unit (PPDU) used them. The
decisive evidence is a successfully decoded EHT PPDU with known bandwidth,
MCS, number of spatial streams (NSS), and coding fields. Application goodput is
an outcome, not mechanism evidence.

## Scenario description

The [NED network](Lan80211BeEhtFeatures.ned) contains a wired 10 Gbit/s server,
one AP, one stationary wireless host, and an IEEE 802.11 scalar radio medium.
The AP bridges 1000-byte UDP datagrams from the server to `host[0]`, beginning
at 0.1 s. The configured 0.002 ms interval is a 4 Gbit/s offered load intended
to saturate either wireless treatment.

```text
server -- 10 Gbit/s Ethernet -- AP )))) HE or EHT )))) host[0]
```

The nodes are one metre apart with no configured mobility or external
interferer. See [omnetpp.ini](omnetpp.ini) for traffic, radio, aggregation, and
rate-selection inputs.

## Standards and INET model boundary

IEEE Std 802.11be-2024 Clause 36 defines the EHT PHY. Table 36-3 interprets
`CH_BANDWIDTH`, Clause 35.14.3 covers MCS/NSS/bandwidth selection, and the
Clause 36 MCS tables identify EHT-MCS 13 as 4096-QAM with coding rate 5/6
(corpus evidence `80211be-2024:chunk:01482`,
`80211be-2024:chunk:01409`, and `80211be-2024:chunk:01874`).

INET configuration advertises EHT capabilities through MIB parameters and
requests 2450 Mbit/s, 320 MHz, and one spatial stream for EHT data frames.
This example is a packet-level model, not a conformance test. Capability flags
and requested rate parameters are configuration input; only a decoded
radiotap EHT observation can establish the transmitted PHY facts.

## Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| HE control completes | `FAIL` | `results/20260725T202934Z/BaselineAx/cmdenv.stderr` | run 0, seed set 0 | Aborted at 0.107913906544 s: maximum transmission duration exceeded |
| HE frames were emitted before failure | `PASS` | Two retained BaselineAx PCAPng files, eight observations each | run 0, seed set 0 | Partial failed run only |
| EHT 320 MHz/MCS 13 is transmitted | `NOT RUN` | No EHT capture or result session | none | The shared campaign stopped after the HE control failed |
| End-to-end HE/EHT outcome comparison | `NOT RUN` | No completed `.sca`/`.vec` pair | none | No comparable completed runs |

The 20260725T202934Z capture session and failure logs were produced together.
No successful scalar/vector session is cited.

Evidence basis: the INI is **configuration input**; the exception and decoded
frames are **direct observations**; packet counts and rates would be
**derived measurements**; explanations beyond those observations are
**inference**.

## Configuration matrix

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| `BaselineAx` | Control | `opMode="ax"`, HE LDPC, 160 MHz | 4 Gbit/s UDP, 5 GHz | run 0/seed 0 attempted | Complete and expose authoritative HE width/MCS fields |
| `EhtFeatures` | Treatment | `opMode="be"`, EHT LDPC, EHT 4096-QAM, max MCS 13, 320 MHz, 1 NSS | Same UDP load, 6 GHz | `NOT RUN` | Complete and expose EHT, 320 MHz, MCS 13, 1 NSS |

The comparison intentionally changes generation, band, width, and selected
rate together. It therefore demonstrates a feature bundle, not the isolated
effect of either 320 MHz or 4096-QAM.

## Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| Both runs complete to 0.5 s | Cmdenv exit and result metadata | Runtime error before limit | PHY transmission construction or aggregation | Inspect the aggregate at the reported event with targeted PHY/MAC logs |
| EHT data uses known EHT/320 MHz/MCS 13/1 NSS fields | AP and host PCAPng typed EHT profile | Field absent, unknown, or different | Rate selection, EHT transmitter, or radiotap writer | Run shared typed-PHY analysis and inspect the first EHT data frame |
| Receiver records delivered bytes | `host[0].app[0] packetReceived:sum(packetBytes)` | Empty result or zero bytes | MAC reception, bridge, or application path | Correlate receiver capture with MAC and UDP result records |

## Reproduction

Run the shared five-run pipeline from the INET repository root:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py run eht_features \
  --suite examples/ieee80211/analysis/suites/be-eht-features.json \
  --evidence both --runs 5
```

Scalar/vector analysis uses all five runs. PCAP analysis uses run 0 from that
same result session.

For a minimal direct reproduction, use the following illustrative command
(`NOT RUN` during this authoring pass):

```sh
bin/inet -u Cmdenv \
  -f examples/ieee80211be/eht_features/omnetpp.ini \
  -c BaselineAx -r 0 --seed-set=0 \
  --result-dir=/tmp/inet-eht-features-baseline
```

## Scalar and vector analysis

The failed campaign produced no completed `.sca` or `.vec` artifact. Its
intended measurement window was 0–0.5 s with traffic from 0.1 s; no explicit
warm-up period was configured. After the runtime failure is resolved, discover
the receiver outcome with:

```sh
opp_scavetool query -l \
  -f 'module =~ **.host[0].app[0] AND name =~ packetReceived:*' \
  examples/ieee80211be/eht_features/results/SESSION_ID/CONFIG_NAME/*.sca \
  examples/ieee80211be/eht_features/results/SESSION_ID/CONFIG_NAME/*.vec
```

Use the 0.1–0.5 s application interval, aggregate within each run, and compare
matched seeds. One run can prove a deterministic exchange but not statistical
robustness. No throughput value is reported here.

## PCAP statistics

Capture points: `ap.wlan[0]` and `host[0].wlan[0]`. The retained files are
PCAPng with radiotap, microsecond timestamps, one interface per file, and
eight observations each. TShark 4.6.4 and `capinfos` decoded them. The AP
capture SHA-256 is
`ed900a21a83d209347b938d9ee648c14a3be399428952df0485277e02a1792b9`;
the host capture SHA-256 is
`d7e4ef90e523875cb45771e7f7fb316bdbca3f4300d6d12664ed057d170c8d8d`.

```sh
capinfos \
  examples/ieee80211be/eht_features/results/20260725T202934Z/BaselineAx/*.pcap
```

| Configuration | Observation count | Relevant frame summary | Interpretation limit |
|---|---:|---|---|
| `BaselineAx` | 8 per capture point | QoS data, acknowledgments, Block Ack Request, Block Ack | Partial failed run; duplicate AP/host observations are not independent transmissions |
| `EhtFeatures` | 0 | `NOT RUN` | No EHT PHY fact is observable |

## Frame exchange analysis

```sh
tshark -n \
  -r examples/ieee80211be/eht_features/results/20260725T202934Z/BaselineAx/BaselineAx-\#0Lan80211BeEhtFeatures.ap.wlan[0].pcap \
  -Y 'wlan' -T fields -E header=y -E separator='|' \
  -e frame.number -e frame.time_epoch -e wlan.fc.type_subtype \
  -e wlan.sa -e wlan.da
```

| Frame | Simulation time | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.100380 s | AP → host | QoS Data / HE run | subtype `0x28` | First observed downlink data |
| 2 | 0.100424 s | host → AP | ACK | subtype `0x1d` | Acknowledges frame 1 |
| 3 | 0.103980 s | AP → host | aggregated QoS Data / HE run | subtype `0x28` | Larger downlink aggregate before failure |
| 4 | 0.104024 s | host → AP | ACK | subtype `0x1d` | Acknowledges frame 3 |
| 5 | 0.107712 s | AP → host | Block Ack Request | subtype `0x0d` | Requests aggregate reception state |
| 7 | 0.107835 s | host → AP | Block Ack | subtype `0x0d` | Returns reception state |

These frames directly establish a partial bidirectional HE exchange. They do
not establish completed delivery, EHT operation, 320 MHz operation, or MCS 13.

## Cross-layer findings and verdict

| Claim | Verdict | Configuration evidence | Model telemetry | Packet evidence | Outcome evidence |
|---|---|---|---|---|---|
| HE control is runnable under the declared load | `FAIL` | HE/160 MHz selected | Runtime duration exception | Partial exchange only | No completed results |
| EHT feature bundle took effect | `NOT RUN` | EHT flags and rate requested | None | None | None |
| EHT outperforms HE | `INCONCLUSIVE` | Confounded treatment/control | None | No EHT capture | No matched outcome |

The only direct protocol finding is that the HE control exchanged data and
acknowledgments before aborting. All EHT performance claims remain untested.

## Limitations and inconclusive claims

- The control failure prevents the intended comparison.
- The treatment changes band, generation, channel width, and MCS together.
- No retained EHT capture exposes authoritative typed-PHY fields.
- No completed scalar/vector session or multi-seed campaign exists.
- The smallest next step is to reproduce the duration exception with a
  bounded aggregate, then co-record AP/host PCAPng and results for both configs.

## Further experiments

- After the failure is fixed, hold EHT MCS/NSS constant and compare 160 versus
  320 MHz; predict a width-field change in the typed capture profile.
- Hold 320 MHz constant and compare EHT-MCS 11 and 13; predict an MCS and
  goodput change without a width change.
- Run at least five matched seeds after the single-run exchange passes.

## Implementation plan

| Item | Evidence-backed plan |
|---|---|
| Demonstrated gap | BaselineAx aborts with a maximum-transmission-duration exception |
| Intended behavior | Either construct aggregates within the PHY duration bound or reject/limit them before transmission |
| Smallest change surface | First inspect aggregation policy, rate selection, and PHY duration validation; no source edit is authorized here |
| Observability | Record aggregate size, selected mode, computed duration, and configured maximum at the failing decision |
| Validation | HE and EHT control/treatment, run 0 first, then matched seeds; require completion, typed PHY fields, and receiver bytes |
| Compatibility and risks | Preserve legacy HE aggregation and rate-selection behavior |
| Architecture and sealing | Any future `src/inet` change requires the architectural-requirements and sealing workflow |
| Next handoff | Simulation detective for event 55521, then one implementation owner if the mechanism is proven |

## Artifact provenance

| Artifact family | Session/path | Configurations/runs | Tool/filter/window | Integrity notes |
|---|---|---|---|---|
| Logs | `results/20260725T202934Z/BaselineAx/cmdenv.{stdout,stderr}` | BaselineAx/run 0/seed 0 | Cmdenv, 0–0.107913906544 s | Failed run, co-recorded |
| PCAP | `results/20260725T202934Z/BaselineAx/*.pcap` | BaselineAx/run 0/seed 0 | TShark 4.6.4, 0.100380–0.107879 s | Two observation points; partial run |
| Scalar/vector | No completed `.sca`/`.vec` | none | `NOT RUN` | Missing by failure |
