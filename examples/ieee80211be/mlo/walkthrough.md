# Walkthrough: 802.11be Multi-Link Operation

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `NOT RUN`
- PCAP: `NOT RUN`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This example places one Multi-Link Device (MLD) access point and one MLD
station over independent 5 GHz and 6 GHz links. It compares configured
Simultaneous Transmit and Receive (STR) capability with configured
Nonsimultaneous Transmit and Receive (NSTR) capability. The learning outcome
is to trace a packet from the upper MLD interface to a selected lower link.
The validation outcome is to prove both-link use and, separately, a
protocol-visible runtime difference between STR and NSTR.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- explain why a Multi-Link Operation (MLO) device presents one upper
  interface while coordinating several affiliated links;
- identify the MLD MAC, lower link MAC/PHY instances, and STR/NSTR inputs;
- distinguish “both links carried traffic” from “NSTR constraints executed”;
  and
- reproduce the retained result queries and design a decisive STR/NSTR check.

MLO lets affiliated stations exchange traffic over more than one link. In this
model, `Ieee80211MldMac` accepts packets from the common upper interface and
forwards them to `link[0]` or `link[1]`. STR and NSTR describe whether link
activity may overlap in transmit/receive directions; proving NSTR therefore
requires timing evidence, not merely capability flags or aggregate goodput.

## [agent] Scenario description

The [network](Lan80211BeMlo.ned) contains a wired server, an
[MLD AP](MldAp.ned), an [MLD host](MldHost.ned), and separate scalar radio
media. Each node's [MldInterface](MldInterface.ned) contains one upper
`Ieee80211MldMac` and two `Ieee80211Interface` lower links.

```text
                                  link[0]: 5 GHz / 160 MHz
server -- Ethernet -- AP MLD  =============================  host MLD
                                  link[1]: 6 GHz / 320 MHz
```

A 1000-byte UDP stream starts at 0.1 s with a 0.01 ms interval. Link 0 uses
the 5 GHz medium and link 1 uses the 6 GHz medium. Both nodes are stationary.
The lower pending queues are local to each link. See
[omnetpp.ini](omnetpp.ini).

## [agent] Standards and INET model boundary

IEEE Std 802.11be-2024 Clause 35.3 covers multi-link setup and operation;
Clause 35.3.16 specifies ML channel access, including STR in 35.3.16.3 and
NSTR in 35.3.16.4. Clause 35.3.16.5 covers PPDU end-time alignment on an NSTR
link pair (corpus evidence `80211be-2024:chunk:01287`, `:01288`, and
`:01292`).

INET's MLD compound and `ehtMlo`, `ehtStr`, and `ehtNstr` MIB inputs model a
subset of those concepts. A flag being parsed or advertised does not prove
that cross-link channel-access restrictions, self-interference, or PPDU
alignment are enforced. The checked-out source and correlated runtime evidence
govern claims about implemented behavior.

## [agent] Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| STR run uses both radio media | `PASS` | `Str-#0.sca`: medium transmission counts and per-link HCF scalars | run 0, seed set 0 | Single 1.0 s run |
| STR delivers application traffic | `PASS` | `host.app[0] packetReceived:count` | run 0, seed set 0 | 18,251 packets |
| NSTR has usable result evidence | `FAIL` | `Nstr-#0.sca` is absent and all 879 declared vectors have zero samples | run 0, seed set 0 | Retained metadata is limited to 0.15 s |
| NSTR changes overlapping link activity | `INCONCLUSIVE` | No co-recorded PCAP or explicit overlap invariant | run 0 | Capability input alone is insufficient |

The earlier claim that STR and NSTR have identical full-run metrics is not
retained as evidence: only STR has a 1.0 s scalar file.

Evidence basis: the INI is **configuration input**; scalar/vector records are
**direct observations**; payload rate and received/sent fraction are
**derived measurements**; unobserved STR/NSTR causality is **inference**.

## [agent] Configuration matrix

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| `Str` | Control | `ehtStr=true`, `ehtNstr=false` | One saturated downlink; 5 GHz/160 MHz + 6 GHz/320 MHz | run 0/seed 0, 1.0 s | Both links carry traffic; permitted overlaps may occur |
| `Nstr` | Treatment | `ehtStr=false`, `ehtNstr=true` | Same topology and load | run 0/seed 0, retained vector to 0.15 s | Forbidden cross-link TX/RX overlaps are absent |

The only intended delta is the two capability flags. The unequal retained
durations and missing NSTR scalar prevent an outcome comparison.

## [agent] Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| MLD distributes traffic to both links | AP per-link `frameSequenceFinished` or packet vectors | One link remains idle | `Ieee80211MldMac` link selection | Inspect upper-to-lower dispatch and per-link queues |
| STR/NSTR gate changes link concurrency | Co-recorded per-link radio-state vectors or event log | Identical forbidden overlaps | MLD channel-access coordination | Compute interval overlap by direction and link |
| Receiver delivery is preserved | `host.app[0] packetReceived` | Missing/duplicate sequence numbers | MLD reordering/delivery | Correlate per-link frames with receive sequence vector |

## [agent] Reproduction

The exact historical command and exit statuses were not retained. Run from
the repository root (`NOT RUN` during this authoring pass):

```sh
bin/inet -u Cmdenv \
  -f examples/ieee80211be/mlo/omnetpp.ini \
  -c Str -r 0 --seed-set=0 \
  --result-dir="$PWD/examples/ieee80211be/mlo/results/reproduction/Str"
```

Repeat with `-c Nstr` into a separate directory. There is no shared suite
descriptor for MLO, so automatic PCAP/provenance generation is not yet
available.

## [agent] Scalar and vector analysis

Inputs: `results/Str-#0.{sca,vec}` and `results/Nstr-#0.vec`. The STR scalar
records a 1.0 s run; the NSTR vector header records a command-line
`sim-time-limit=0.15s`, has no matching `.sca`, and all 879 vector definitions
have zero samples.

```sh
opp_scavetool query -l \
  -f 'type =~ scalar AND (module =~ Lan80211BeMlo.radioMedium* OR module =~ Lan80211BeMlo.host.app[0] OR module =~ Lan80211BeMlo.ap.mldWlan.link[*].mac.hcf)' \
  examples/ieee80211be/mlo/results/Str-\#0.sca
```

| Metric or invariant | Source result and module | Window/aggregation | STR | NSTR | Interpretation |
|---|---|---|---:|---:|---|
| Host packets received | `host.app[0] packetReceived:count` | 0–1.0 s scalar | 18,251 | no samples | STR end-to-end outcome only |
| Link 0 frame sequences | `ap.mldWlan.link[0].mac.hcf frameSequenceFinished:count` | 0–1.0 s | 97 | unknown | Direct model telemetry for link use |
| Link 1 frame sequences | `ap.mldWlan.link[1].mac.hcf frameSequenceFinished:count` | 0–1.0 s | 190 | unknown | Direct model telemetry for link use |
| 5 GHz medium transmissions | `radioMedium5GHz transmission count` | 0–1.0 s | 303 | unknown | Includes all transmitted signals |
| 6 GHz medium transmissions | `radioMedium6GHz transmission count` | 0–1.0 s | 586 | unknown | Includes all transmitted signals |

The traffic starts at 0.1 s; no explicit warm-up policy is retained. The
90,001 server packets and 18,251 receiver packets in STR imply a 20.28%
end-of-run received/sent fraction and 146.008 Mbit/s of completed payload over
the 1.0 s run. This is not a loss estimate: queue overflow and unfinished
traffic dominate the overloaded run. The 18,251 packet-delay samples have a
15.404 ms mean (0.129–27.215 ms range), but packet samples are not independent
repetitions.

## [agent] PCAP statistics

No `.pcap` or `.pcapng` file is retained. A resolving run should capture both
lower interfaces at the AP and host with unique link-aware filenames and
microsecond-or-better timestamps.

```sh
tshark -n -r MLO_LINK_CAPTURE.pcapng -q \
  -z io,stat,0,'wlan'
```

| Configuration | Observation count | Relevant frame/PHY summary | Interpretation limit |
|---|---:|---|---|
| `Str` | 0 retained | `NOT RUN` | Scalars prove link activity, not frame ordering |
| `Nstr` | 0 retained | `NOT RUN` | No NSTR concurrency claim |

## [agent] Frame exchange analysis

```sh
tshark -n -r MLO_LINK_CAPTURE.pcapng \
  -Y 'wlan' -T fields -E header=y -E separator='|' \
  -e frame.number -e frame.time_epoch -e wlan.sa -e wlan.da \
  -e wlan.fc.type_subtype
```

| Frame | Simulation time | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| `NOT RUN` | unknown | AP link 0 → host link 0 | EHT data | link identity and interval required | One side of cross-link overlap check |
| `NOT RUN` | unknown | AP/host link 1 | EHT data/control | link identity, direction, interval required | Other side of overlap check |

Separate capture frame numbers cannot establish concurrency. Compare
simulation timestamps and transmission intervals across links, preferably
against co-recorded radio-state vectors.

## [agent] Cross-layer findings and verdict

| Claim | Verdict | Configuration evidence | Model telemetry | Packet evidence | Outcome evidence |
|---|---|---|---|---|---|
| MLD uses both links in STR | `PASS` | Two lower links and STR configured | 97 and 190 frame sequences | None | 18,251 packets received |
| Runtime NSTR restriction is effective | `INCONCLUSIVE` | NSTR flag configured | No complete matched scalar or overlap metric | No capture | Incomparable duration |
| STR outperforms NSTR | `INCONCLUSIVE` | Matched intended inputs | Retained sessions unmatched | None | NSTR scalar absent |

The retained STR session directly proves both-link use. It does not prove STR
overlap semantics, and the current artifacts cannot assess NSTR.

## [agent] Limitations and inconclusive claims

- NSTR lacks a completed scalar file and matched duration.
- No explicit MLD link-selection, concurrency, or NSTR-block telemetry exists.
- No per-link PCAP session is retained.
- One saturated stream and one seed cannot establish general performance.
- The smallest next run records both configurations for the same duration and
  seed with per-link radio intervals, MLD decisions, captures, and app results.

## [agent] Further experiments

- Use a deterministic bidirectional workload that creates a potential
  cross-link TX/RX overlap; predict that STR permits and NSTR suppresses it.
- Compare round-robin selection against a single-link control while holding
  offered load below queue overflow.
- After the invariant passes, add matched seeds and report per-run delivery,
  delay, and link airtime.

## [agent] Implementation plan

| Item | Evidence-backed plan |
|---|---|
| Demonstrated gap | Runtime STR/NSTR behavior is not directly observable and NSTR results are incomplete |
| Intended behavior | Enforce the selected ML channel-access constraints or explicitly document a capability-only abstraction |
| Smallest change surface | First add suite/config coverage and MLD decision/overlap telemetry; production changes only after a failing overlap is proven |
| Observability | Link choice, blocked/released reason, per-link TX/RX intervals, and NSTR group |
| Validation | Matched STR/NSTR deterministic overlap scenario plus legacy single-link behavior |
| Compatibility and risks | Avoid changing ordinary per-link EDCA ordering or packet ownership |
| Architecture and sealing | Any future MLD C++/NED change requires architecture, WLAN-rule, and seal review |
| Next handoff | Navigator for MLD control path, then simulation detective and one implementation owner if needed |

## [agent] Artifact provenance

| Artifact family | Session/path | Configurations/runs | Tool/filter/window | Integrity notes |
|---|---|---|---|---|
| Scalar/vector | `results/Str-#0.{sca,vec}` | Str/run 0/seed 0 | `opp_scavetool`, 0–1.0 s | Scalar SHA-256 `c9f29c93...aeee9b` |
| Vector only | `results/Nstr-#0.vec` | Nstr/run 0/seed 0 | `opp_scavetool`, metadata to 0.15 s | No scalar; all 879 vectors empty; SHA-256 `cc9db2a1...9907e` |
| PCAP | No retained `.pcapng` | none | `NOT RUN` | Missing suite coverage |
