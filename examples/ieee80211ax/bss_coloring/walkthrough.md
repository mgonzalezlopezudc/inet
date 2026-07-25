# Walkthrough: HE BSS coloring and spatial reuse

This walkthrough compares disabled spatial reuse, three overlapping basic
service set packet-detect (OBSS/PD) thresholds, a same-color negative control,
and a dual-NAV case. Its strongest evidence is a five-run outcome campaign plus
feature-specific receiver-decision vectors; the retained MAC captures show the
exchange but do not expose the internal OBSS/PD decision.

## Learning objectives and feature primer

After completing this walkthrough, the reader can:

- explain how a BSS color distinguishes an intra-BSS physical layer protocol
  data unit (PPDU) from an overlapping-BSS PPDU;
- identify the OBSS/PD threshold as the gate for spatial reuse;
- distinguish receiver-decision telemetry from protocol-visible frames; and
- reproduce the representative run and first diagnostic queries.

An HE receiver can classify a received PPDU by its BSS color. For an
inter-BSS PPDU below the configured OBSS/PD level, spatial reuse may allow the
receiver to ignore that occupancy and its transmitter to contend. A higher
(less negative) threshold admits more reuse but can increase interference.
This is a receiver/carrier-sense decision, not something that frame counts
alone prove.

## Scenario description

[BssColoringNetwork.ned](BssColoringNetwork.ned) contains two APs, two
stations per AP, and a separate wired server per BSS. BSS 1 is stationary.
BSS 2 moves as a rigid group along the x axis; during the `0.3–0.95 s`
measurement window the AP separation grows, sweeping received OBSS power
through the configured thresholds. Both servers offer jittered downlink UDP
traffic after a Block Ack warm-up. All material assignments are in
[omnetpp.ini](omnetpp.ini).

```text
server1 -- AP1 ~~ sta1[0..1]     sta2[0..1] ~~ AP2 -- server2
                  stationary       moving BSS 2 →
```

The two stations per BSS allow HE multi-user scheduling, while the same-color
case is a negative control for inter-BSS classification.

## Standards and INET model boundary

IEEE Std 802.11-2024 Clause 26.10 describes HE spatial reuse; Clause 26.10.2
defines OBSS/PD-based operation, and Clause 26.2.3 defines spatial-reuse-group
PPDU identification. These references were verified in corpus chunks
`80211ax-2024:chunk:09886`, `09890`, and `09741`.

INET configures colors in the MIB and models the receiver decision through
`enableSpatialReuse` and `obssPdThreshold`. The moving topology, scalar radio
medium, traffic, and `sameTransmissionStartTimeCheck="ignore"` are experiment
choices. Observed receiver vectors establish modeled decisions; they are not a
standards-conformance certification.

## Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| Threshold changes modeled OBSS/PD decisions | `PASS` | receiver decision vectors and figure provenance | runs/seeds `0–4` | Direct model telemetry, `0.3–0.95 s` |
| More permissive thresholds increase concurrent AP airtime | `PASS` | `transmissionState:vector` | runs/seeds `0–4` | Per-run integration and 95% t CI |
| Same-color control reproduces disabled outcome | `PASS` | goodput, fairness, and airtime vectors | runs/seeds `0–4` | Exact reported campaign means |
| MAC exchange is present | `PASS` | run-0 AP PCAPs | run/seed `0` | Capture observations, not decisions |
| Two independent NAV transitions occur | `INCONCLUSIVE` | `TwoNav-#0.vec` | run/seed `0` | `nav:vector` exists; `intraBssNavChanged:vector` has no match |

## Configuration matrix

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| `BssColoringDisabled` | Control | spatial reuse off, ED `-82 dBm` | matched two-BSS downlink | `0–4` | no OBSS/PD ignores |
| `ObssPdConservative` | Treatment | enabled, `-81 dBm` | matched | `0–4` | least treatment reuse |
| `BssColoringEnabled` | Treatment | enabled, `-79 dBm` | matched | `0–4` | intermediate reuse |
| `ObssPdAggressive` | Stress | enabled, `-78 dBm` | matched | `0–4` | greatest reuse |
| `BssColoringCollision` | Negative | enabled, both BSSs color 1 | matched | `0–4` | behaves like disabled |
| `TwoNav` | Diagnostic | enabled plus `heTwoNav=true` | adds one uplink | packet run `0` | basic and intra-BSS NAV observable |

`ObssPdConservative`, `ObssPdAggressive`, `BssColoringCollision`, and
`TwoNav` extend `BssColoringEnabled`; their later, configuration-specific
assignments win. All APs and stations receive their local color explicitly.

## Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| Inter-BSS classification precedes ignore | receiver decision vectors | missing/wrong color or reason | HE receiver/MIB | query local/received color and reason vectors |
| Airtime order is disabled < conservative < enabled < aggressive | AP `transmissionState:vector` | non-strict order | receiver threshold or radio state | align decision and state timestamps |
| Same-color equals disabled | outcome vectors | collision case reuses medium | color classification | inspect effective colors at all six radios |
| Dual NAV changes separately | `nav` and `intraBssNavChanged` | second vector absent | HE MAC NAV | targeted MAC log or add recorder |

## Reproduction

Run from the repository root. This command is illustrative and was **NOT RUN**
during this rewrite; no historical exit status is inferred:

```sh
bin/inet -u Cmdenv -f examples/ieee80211ax/bss_coloring/omnetpp.ini \
  -c BssColoringEnabled -r 0 --seed-set=0 \
  --result-dir=examples/ieee80211ax/bss_coloring/results/manual/BssColoringEnabled
```

The retained campaign can be regenerated with:

```sh
python3 examples/ieee80211ax/analysis/run_campaign.py bss -j$(nproc)
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211ax/analysis/first_tranche.py bss
```

## Scalar and vector analysis

Inputs are the `.sca`/`.vec` pairs under each configuration directory in
`results/scalar-vector/20260725T120411Z/`. The provenance
[bss-coloring-comparison.png.json](../analysis/figures/bss/bss-coloring-comparison.png.json)
records hashes, run binding, filters, and the `0.3–0.95 s` window.

```sh
opp_scavetool query -l \
  -f 'type =~ vector AND (name =~ "packetReceived:vector(packetBytes)" OR name =~ "transmissionState:vector" OR name =~ "obssPd*:vector")' \
  examples/ieee80211ax/bss_coloring/results/scalar-vector/20260725T120411Z/*/*.vec
```

| Configuration | Aggregate goodput | Jain fairness | Concurrent AP airtime |
|---|---:|---:|---:|
| `BssColoringDisabled` | 7.909 ± 3.398 Mbps | 0.887 ± 0.218 | 0.904 ± 0.419% |
| `ObssPdConservative` | 8.475 ± 3.352 Mbps | 0.922 ± 0.164 | 7.590 ± 1.365% |
| `BssColoringEnabled` | 9.669 ± 1.478 Mbps | 0.974 ± 0.067 | 24.068 ± 2.021% |
| `ObssPdAggressive` | 9.782 ± 1.103 Mbps | 0.982 ± 0.030 | 31.290 ± 1.843% |
| `BssColoringCollision` | 7.909 ± 3.398 Mbps | 0.887 ± 0.218 | 0.904 ± 0.419% |

Values are per-run means ± two-sided 95% Student-t CIs over five independent
seeds. Goodput is summed application bytes per window; fairness is calculated
per run before the CI; concurrent airtime is integrated per run. Overlapping
goodput intervals do not establish a strict goodput order.

For the dual-NAV boundary:

```sh
opp_scavetool query -l \
  -f 'type =~ vector AND (name =~ "nav:vector" OR name =~ "intraBssNavChanged:vector")' \
  examples/ieee80211ax/bss_coloring/results/packet-statistics/20260724T175025Z/TwoNav/TwoNav-#0.vec
```

The six `nav:vector` streams contain 524, 1380, 1169, 1240, 1452, and 1287
samples; the intra-BSS result is absent.

## PCAP statistics

Capture session:
`results/packet-statistics/20260724T175025Z`; legacy PCAP, `mac` observation
at every `wlan[0]`, run/seed 0. TShark 4.6.4 decodes the retained files.

```sh
tshark -n -r 'examples/ieee80211ax/bss_coloring/results/packet-statistics/20260724T175025Z/BssColoringEnabled/BssColoringEnabled-#0BssColoringNetwork.ap1.wlan[0].pcap' \
  -q -z io,stat,0,'wlan'
```

| Configuration | AP/global observations | Interpretation limit |
|---|---:|---|
| disabled / same-color | 2586 each | capture observations |
| conservative / enabled / aggressive | 2553 / 2347 / 2378 | not de-duplicated packets |
| `TwoNav` | 1805 | does not expose NAV state |

## Frame exchange analysis

```sh
tshark -n -r 'examples/ieee80211ax/bss_coloring/results/packet-statistics/20260724T175025Z/BssColoringEnabled/BssColoringEnabled-#0BssColoringNetwork.ap1.wlan[0].pcap' \
  -Y 'frame.number <= 2' -T fields -E header=y -E separator='|' \
  -e frame.number -e frame.time_epoch -e wlan.sa -e wlan.da \
  -e wlan.fc.type_subtype -e radiotap.he.data_1.ppdu_format
```

| Frame | Simulation time | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.201236 s | AP1 → STA | QoS Data / HE-SU | subtype `0x28`, PPDU `0` | warm-up unicast |
| 2 | 0.201336 s | STA → AP1 | Ack | subtype `0x1d` | confirms MAC reception |

This timeline directly proves a data/ACK exchange. It does not prove that an
OBSS PPDU was ignored; that conclusion comes from separately recorded receiver
telemetry, so event-level PCAP-to-vector causality remains an inference.

## Cross-layer findings and verdict

| Claim | Verdict | Configuration evidence | Model telemetry | Packet evidence | Outcome evidence |
|---|---|---|---|---|---|
| OBSS/PD changes spatial reuse | `PASS` | enabled and threshold sweep | decision vectors; airtime order | HE exchanges present | campaign goodput/fairness |
| Same color disables inter-BSS reuse | `PASS` | both BSS colors 1 | airtime equals disabled | exchange present | exact disabled means |
| Dual NAV is demonstrated | `INCONCLUSIVE` | `heTwoNav=true` | intra-BSS vector absent | no NAV header state | not evaluated |

The bounded verdict is `PASS` for modeled OBSS/PD spatial reuse in this
moving two-BSS experiment and `INCONCLUSIVE` for dual NAV.

## Limitations and inconclusive claims

- Captures and scalar/vector outcomes are separate sessions; they cannot prove
  event-level causality or exact count agreement.
- One AP capture cannot establish reception at another node.
- Resolve dual NAV with one co-recorded run exposing both NAV vectors and a
  targeted HE-MAC log.
- The scalar radio model and one movement path do not establish real-world
  deployment performance.

## Further experiments

- Repeat the threshold sweep over additional movement speeds and verify that
  decision-transition timestamps move predictably.
- Run a wrong-color-at-one-STA negative case and inspect its classification.

## Implementation plan

| Item | Evidence-backed plan |
|---|---|
| Demonstrated gap | `intraBssNavChanged:vector` is absent in retained `TwoNav` results |
| Intended behavior | expose basic and intra-BSS NAV transitions independently |
| Smallest change surface | first verify recorder path/signal name; only then inspect HE MAC NAV observability |
| Observability | co-record both NAV signals, MAC log, and AP/STA PCAP |
| Validation | `TwoNav` run 0 plus disabled dual-NAV control; assert both streams and timestamp ordering |
| Compatibility and risks | avoid changing NAV behavior merely to add telemetry |
| Architecture and sealing | apply architecture/sealing review before any `src/inet` change |
| Next handoff | HE MAC maintainer after configuration/recorder verification |

## Artifact provenance

| Artifact family | Session/path | Configurations/runs | Tool/filter/window | Integrity notes |
|---|---|---|---|---|
| Scalar/vector | `results/scalar-vector/20260725T120411Z` | five configs, runs/seeds `0–4` | figure provenance, `0.3–0.95 s` | SHA-256 per input |
| PCAP/results | `results/packet-statistics/20260724T175025Z` | six configs, run/seed `0` | TShark 4.6.4; MAC captures | separate session |
| Figure | `../analysis/figures/bss/bss-coloring-comparison.png` | five configs | per-run aggregation | provenance sidecar |
