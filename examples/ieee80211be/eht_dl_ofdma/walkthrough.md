# Walkthrough: 802.11be downlink OFDMA with MRUs

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `NOT RUN`
- PCAP: `NOT RUN`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This example contrasts equal-sized Multiple Resource Unit (MRU) scheduling on
a clean 320 MHz channel with the same scheduler under a configured puncturing
mask. The learning outcome is to understand how an EHT access point (AP) can
divide frequency resources among several downlink recipients. The validation
outcome is to observe a legal, decoded MRU allocation that avoids the
punctured subchannels while delivering data to all four stations.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- explain the roles of an AP, Resource Units (RUs), MRUs, and EHT MU PPDUs;
- identify the puncturing mask and scheduler inputs in the configuration;
- distinguish equal receiver counts from proof of an MRU allocation; and
- reproduce the scalar query and the missing packet-evidence diagnostic.

Orthogonal Frequency Division Multiple Access (OFDMA) lets an AP schedule
several stations in one multi-user Physical Layer Protocol Data Unit (PPDU) by
assigning disjoint frequency resources. IEEE 802.11be adds MRUs, which combine
permitted RU components. A valid feature test must observe the allocation
itself; throughput or delivery counts alone cannot show which tones were used.

## [agent] Scenario description

The [NED network](Lan80211BeDlOfdma.ned) contains a wired server, one AP, and
four stationary wireless hosts. Four 1000-byte UDP streams begin at 0.1 s and
offer one packet every 0.5 ms to their respective hosts.

```text
                       +--> host[0]
server -- Ethernet -- AP --> host[1]   320 MHz EHT downlink
                       +--> host[2]
                       +--> host[3]
```

The AP and hosts use a scalar radio medium with no external interferer.
`sameTransmissionStartTimeCheck="ignore"` permits OFDMA sub-transmissions to
start together. See [omnetpp.ini](omnetpp.ini).

## [agent] Standards and INET model boundary

IEEE Std 802.11be-2024 Clause 35.5.1.2 specifies RU allocation in an EHT MU
PPDU. Tables 36-33 through 36-35 define the EHT-SIG OFDMA common field, RU
Allocation subfields, and their RU/MRU associations; Figure 36-41 describes
EHT-SIG content channels in a 320 MHz PPDU (corpus evidence
`80211be-2024:chunk:01331`, `:01674`, `:01679`, `:01682`, and `:01704`).

INET configures `EhtDlSchedulerEqualSizedMRUs`, a four-station limit,
320 MHz operation, EHT DL OFDMA, and EHT preamble puncturing. The string
`0000 1100 0000 0000` is an INET scheduler/PHY input for the treatment. It is
not itself proof that the transmitted EHT-SIG contained a legal allocation.

## [agent] Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| Both configurations recorded receiver outcomes | `PASS` | Retained `.sca` files | run 0, seed set 0 | Historical 0.15 s diagnostic sessions |
| All four hosts receive data in both configs | `PASS` | `host[*].app[0] packetReceived:count` | run 0, seed set 0 | 28 packets per host in each config |
| Equal-sized MRUs were used | `INCONCLUSIVE` | No MRU allocation result or PCAP | run 0 | Scheduler name is only configuration input |
| Punctured allocation avoids masked subchannels | `INCONCLUSIVE` | No typed EHT capture or puncturing vector | run 0 | Decisive mechanism evidence is missing |

Evidence basis: the INI and retained run configuration are **configuration
input**; result records are **direct observations**; the payload rate is a
**derived measurement**; any scheduler explanation without allocation
telemetry is **inference**.

## [agent] Configuration matrix

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| `EqualSizedMRUs` | Control | EHT DL OFDMA and equal-sized MRU scheduler; no mask | Four matched UDP flows, 320 MHz | run 0/seed 0, 0.15 s retained | Four users receive; decoded allocation contains four valid user resources |
| `PuncturedMRUs` | Treatment | Adds `hePreamblePuncturing="0000 1100 0000 0000"` | Same topology/load/channel | run 0/seed 0, 0.15 s retained | Allocations avoid punctured 20 MHz subchannels |

Both retained result headers contain a command-line `sim-time-limit=0.15s`
that overrides the INI's 1.0 s value. The files are single-run diagnostic
evidence, not a performance campaign.

## [agent] Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| Scheduler selects no more than four recipients | AP scheduler result/log | Missing or oversized recipient set | EHT DL scheduler | Record recipient and MRU allocation telemetry |
| Treatment excludes punctured subchannels | Typed EHT PCAP plus mask vector | Allocated tones overlap mask or remain unknown | Puncturing/allocation encoding | Co-record AP PCAPng and puncturing/allocation vectors |
| Each scheduled user receives data | Host application scalars | Zero receiver count for a selected user | MU reception or address mapping | Correlate one allocation with receiver-side frames/results |

## [agent] Reproduction

The exact historical command that created the retained files was not
preserved, so its exit status is not asserted. Run this minimal command from
the repository root (`NOT RUN` during this authoring pass):

```sh
bin/inet -u Cmdenv \
  -f examples/ieee80211be/eht_dl_ofdma/omnetpp.ini \
  -c EqualSizedMRUs -r 0 --seed-set=0 --sim-time-limit=0.15s \
  --result-dir="$PWD/examples/ieee80211be/eht_dl_ofdma/results/reproduction"
```

There is currently no shared-suite entry for `eht_dl_ofdma`; consequently the
generation-neutral PCAP command cannot yet bind captures to this walkthrough.

## [agent] Scalar and vector analysis

Inputs are `results/EqualSizedMRUs-#0.{sca,vec}` and
`results/PuncturedMRUs-#0.{sca,vec}`. Both record run 0, seed set 0, and a
0.15 s limit. Query the application outcome with:

```sh
opp_scavetool query -l \
  -f 'type =~ scalar AND module =~ **.host[*].app[0] AND name =~ packetReceived:count' \
  examples/ieee80211be/eht_dl_ofdma/results/EqualSizedMRUs-\#0.sca \
  examples/ieee80211be/eht_dl_ofdma/results/PuncturedMRUs-\#0.sca
```

| Metric or invariant | Source result and module | Window/aggregation | Equal-sized | Punctured | Interpretation |
|---|---|---|---:|---:|---|
| Packets received per host | `host[*].app[0] packetReceived:count` | 0–0.15 s, each run | 28, 28, 28, 28 | 28, 28, 28, 28 | All flows deliver in the retained interval |
| Packets sent per server app | `server.app[*] packetSent:count` | 0–0.15 s, each run | 101 each | 101 each | Matched offered load |
| Completed payload rate | Four sink byte sums / 0.15 s | Per-run derived value | 5.973 Mbit/s | 5.973 Mbit/s | End-of-run outcome, not scheduler capacity |
| MRU/puncturing mechanism | No matching result | none | unknown | unknown | Outcome equality does not prove allocation behavior |

No explicit warm-up policy is retained. The application starts at 0.1 s, so
only the final 0.05 s can contain application traffic. Vector samples are not
repetitions. Each host delay vector contains 28 packets; the pooled
equal-count mean is about 18.087 ms, but hosts and packet samples are not
independent runs.

## [agent] PCAP statistics

No `.pcap` or `.pcapng` artifact is retained for either configuration. The
intended capture points are `ap.wlan[0]` and each `host[*].wlan[0]`, with
radiotap EHT fields decoded by the shared typed-PHY profile.

```sh
tshark -n \
  -r examples/ieee80211be/eht_dl_ofdma/results/SESSION_ID/CONFIG_NAME/*.pcap \
  -q -z io,stat,0,'wlan'
```

| Configuration | Observation count | Relevant frame/PHY summary | Interpretation limit |
|---|---:|---|---|
| `EqualSizedMRUs` | 0 retained | `NOT RUN` | No protocol or PHY claim |
| `PuncturedMRUs` | 0 retained | `NOT RUN` | No puncturing or MRU claim |

## [agent] Frame exchange analysis

Use the following query after capture support is added:

```sh
tshark -n -r EHT_DL_OFDMA_CAPTURE.pcapng \
  -Y 'wlan.fc.type == 2' \
  -T fields -E header=y -E separator='|' \
  -e frame.number -e frame.time_epoch -e wlan.sa -e wlan.da \
  -e radiotap.eht_usig.data_1_2 -e radiotap.eht.data_0
```

| Frame | Simulation time | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| `NOT RUN` | unknown | AP → multiple hosts | EHT MU | RU/MRU allocation and puncturing fields required | Would prove the scheduler's protocol-visible allocation |

TShark field availability varies by version. If EHT presence is known but
values are not exported, use the shared raw-radiotap fallback; do not fill
unknown fields from the INI.

## [agent] Cross-layer findings and verdict

| Claim | Verdict | Configuration evidence | Model telemetry | Packet evidence | Outcome evidence |
|---|---|---|---|---|---|
| Four downlink flows operate | `PASS` | Four apps and hosts configured | Application counts | None | 28 packets per host |
| Equal-sized MRUs are scheduled | `INCONCLUSIVE` | Scheduler type requested | No allocation telemetry | No capture | Delivery only |
| Puncturing changes legal MRU placement | `INCONCLUSIVE` | Mask requested | No mask/allocation vector | No capture | Counts are identical |

The retained results prove a matched, symmetric application outcome in a
short single run. They do not prove the feature mechanism.

## [agent] Limitations and inconclusive claims

- No shared suite descriptor, PCAP manifest, or typed EHT decode covers this
  scenario.
- No feature-specific scheduler allocation or puncturing telemetry is retained.
- The 0.05 s traffic window and one seed cannot support performance claims.
- The smallest resolving run co-records AP/host PCAPng, allocation telemetry,
  mask transitions, and application results for both configurations.

## [agent] Further experiments

- Add a no-puncturing/puncturing pair with one fixed seed and verify exact
  allocation legality before expanding seeds.
- Vary one mask bit pattern while holding offered load constant; predict a
  changed allocation but unchanged forbidden-tone overlap of zero.
- After mechanism evidence passes, run matched seeds and compare per-flow
  goodput and fairness.

## [agent] Implementation plan

| Item | Evidence-backed plan |
|---|---|
| Demonstrated gap | MRU and puncturing behavior is not directly observable |
| Intended behavior | Expose each scheduling decision's recipients, RU/MRU components, and active puncturing mask |
| Smallest change surface | Prefer a BE suite entry and feature plugin; add model telemetry only if the current result API exposes no allocation |
| Observability | AP allocation vector/log plus typed EHT capture fields |
| Validation | Clean/punctured pair, run 0 first, allocation legality and receiver correlation |
| Compatibility and risks | Keep common MAC/provenance machinery generation-neutral and EHT decoding typed |
| Architecture and sealing | Any future `src/inet` telemetry change requires architecture and seal checks |
| Next handoff | Suite/plugin owner, then results/packet reviewer |

## [agent] Artifact provenance

| Artifact family | Session/path | Configurations/runs | Tool/filter/window | Integrity notes |
|---|---|---|---|---|
| Scalar/vector | `results/{EqualSizedMRUs,PuncturedMRUs}-#0.{sca,vec}` | run 0/seed 0 | `opp_scavetool`, 0–0.15 s | Separate historical files; no explicit warm-up metadata |
| PCAP | No retained `.pcapng` | none | `NOT RUN` | Missing suite coverage |
