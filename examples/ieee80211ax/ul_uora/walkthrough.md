# Walkthrough: 802.11ax Uplink OFDMA Random Access

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `NOT RUN`
- PCAP: `NOT RUN`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `20260727T130440Z`.

This walkthrough isolates uplink orthogonal frequency-division multiple access
random access (UORA). It compares one and five random-access resource units
(RA-RUs) under the same eight-station heavy load. The retained evidence is a
co-recorded, 0.5-second diagnostic pair using run 0 and seed 0. It demonstrates
the mechanism, but it is not a multi-seed performance estimate.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- explain why an access point (AP) advertises RA-RUs in a Basic Trigger;
- identify associated-station RA-RUs from `AID12=0` User Info fields;
- follow the Trigger → HE trigger-based (HE-TB) response → Block Ack exchange;
- relate INET's UORA attempt and success counters to the packet evidence; and
- reproduce the representative check and diagnose its first failure.

UORA lets associated stations contend inside an AP-coordinated uplink
transmission. A station maintains an OFDMA contention window (OCW) and OFDMA
backoff counter (OBO). For a Trigger containing `N` eligible RA-RUs, a station
with queued data subtracts `N` while its OBO is larger than `N`; otherwise it
sets OBO to zero, chooses one eligible RA-RU, and may transmit an HE-TB PPDU.
Multiple eligible stations can choose concurrently, so the AP must resolve the
received transmissions. When the AP successfully receives frames that require
an immediate response, it returns the acknowledgment selected by the exchange;
an absent solicited response makes the UORA attempt unsuccessful. Success
resets the contention window; failure expands it within the advertised bounds.

## [agent] Scenario description

The [INI configuration](omnetpp.ini) uses the
[uplink OFDMA network](../ul_ofdma/Lan80211AxUlOfdma.ned), which extends the
[common single-BSS network](../common/HeSingleBssNetwork.ned).

```text
host[0..7] -- 802.11ax uplink --> AP === 100-Gbit/s Ethernet === server
                    Basic Trigger <--
```

The stations and AP are stationary in a 50 m × 50 m area. Eight-station
configurations draw each station's x/y position uniformly from 15–35 m. The
radio uses 5 GHz, a 20 MHz channel, 10 mW transmit power, and the scalar
radio/medium model; there is no mobility or external interferer. A short
1000-byte application phase starts at 0.2 s to establish Block Ack state.
The measured heavy application starts at 0.3 s and sends a 100-byte UDP packet
per station every 1 ms toward the wired server.

The configured run limit is 2 s. The retained diagnostic overrides it to
0.5 s so the comparison focuses on initial heavy-load UORA behavior. Both rows
use the same topology, load, run, seed, scheduler, and recording envelope; only
the configured RA-RU count changes.

## [agent] Standards and INET model boundary

IEEE Std 802.11-2024 Table 9-47 defines Trigger Type 0 as a Basic Trigger
(`80211ax-2024:chunk:01660`). In a Trigger User Info field, `AID12=0`
allocates RA-RUs to associated stations; the RU Allocation field identifies
the first RU and Number Of RA-RU encodes the count minus one (9.3.1.22,
Table 9-52 and Figure 9-95; chunks `01668` and `01671`).

Clause 26.5.4 defines UORA. Associated contenders maintain OCW/OBO state,
count eligible RA-RUs, select an eligible RU when OBO reaches the opportunity,
and construct an HE-TB response according to 26.5.2.3
(`80211ax-2024:chunk:09810`–`09814`). The exchange is Basic Trigger, SIFS,
simultaneous HE-TB response PPDUs, SIFS, then an allowed immediate
acknowledgment form (10.3.2.13.3, chunks `05102`–`05103`). The standard does
not require every such exchange to end in Multi-STA Block Ack.

INET implements this mechanism with `HeHcf`, `HeUlCoordinator`, and
`HeUlSchedulerBacklogBased`. Important abstractions are:

- `HeUlCoordinator` keeps OCW/OBO arrays per access category, while the cited
  standard describes station contention state without mandating that model
  split;
- the scheduler's RA-RU-count heuristic is an INET policy, not an IEEE rule;
- multiple RA-RUs are serialized as separate one-RU `AID12=0` User Info
  entries rather than one compressed contiguous-set entry; and
- the station implementation treats a correlated Multi-STA Block Ack or its
  timeout as the terminal UORA result.

The source anchors are
[`HeUlCoordinator.cc`](../../../src/inet/linklayer/ieee80211/mac/coordinationfunction/HeUlCoordinator.cc),
[`HeHcfUl.cc`](../../../src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfUl.cc),
[`HeUlSchedulerBacklogBased.cc`](../../../src/inet/linklayer/ieee80211/mac/scheduler/HeUlSchedulerBacklogBased.cc),
and
[`HeUlMuTxOpFs.cc`](../../../src/inet/linklayer/ieee80211/mac/framesequence/HeUlMuTxOpFs.cc).
Configuration requests the feature; the recorded counters and frames below
establish what occurred in the retained run.

## [agent] Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| Retained runs apply the intended UORA delta | `PASS` | INI, run metadata, and decoded Trigger fields | both configs | requested 1/5 values appear as observed 1/5-entry allocations |
| Basic Triggers advertise one versus five RA-RUs | `PASS` | AP PCAP `AID12` lists | run 0, seed 0 | representative Trigger fields |
| Stations make modeled UORA attempts | `PASS` | per-station `heUlRandomAccessAttempt:count` | run 0, seed 0 | 12 versus 21 attempts |
| Five-RA-RU treatment records UORA successes | `PASS` | `heUlRandomAccessSuccess:count` | run 0, seed 0 | 0 versus 8 successes |
| Trigger/HE-TB/Block-Ack structure occurs | `PASS` | AP PCAP timeline | run 0, seed 0 | protocol-visible sequence |
| Five RA-RUs improve population-level performance | `NOT RUN` | five independent runs required | one seed only | no uncertainty estimate |
| Co-timed HE-TB frames identify collisions | `INCONCLUSIVE` | canonical per-response RU index unavailable | both captures | do not infer collision from timing |

Scalar/vector and packet evidence are co-recorded in session
`20260727T130440Z`, so their configuration and trajectory are aligned. Frame
totals still do not identify the station-side UORA decision; the model counters
are authoritative for attempts and successes.

## [agent] Configuration matrix

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| `ScheduledOnly` | Negative control, not retained | RA-RUs fixed at 0 | 3 stations, 1000 B/5 ms | `NOT RUN` | no UORA attempts |
| `MixedUora` | Reference, not retained | adaptive 1–3 RA-RUs | 3 stations, 1000 B/5 ms | `NOT RUN` | scheduled and random access coexist |
| `UoraLightContention` | Load control, not retained | one RA-RU | 8 stations, 100 B/4 ms | `NOT RUN` | UORA under lighter load |
| `UoraHeavyContention` | Retained control | one RA-RU | 8 stations, 100 B/1 ms, 20 MHz | run 0/seed 0 | one `AID12=0` entry and modeled attempts |
| `UoraMoreRandomAccessRus` | Retained treatment | five RA-RUs | matched heavy inputs | run 0/seed 0 | five `AID12=0` entries and more access capacity |

`UoraHeavyContention` extends `UoraLightContention`, which extends
`MixedUora`. Child assignments fix `minRandomAccessRus=maxRandomAccessRus=1`.
`UoraMoreRandomAccessRus` extends the heavy configuration and replaces only
those values with 5. Both inherit `maxMuStations=2`. The treatment therefore
holds the offered load and scheduler family fixed. Random station placement is
seed-dependent but paired because both runs use seed 0.

## [agent] Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| Trigger carries the configured RA allocation | AP PCAP `AID12` and RU Allocation | zero-entry count is not 1/5 | scheduler or Trigger serialization | inspect `HeUlSchedulerBacklogBased` plan and Trigger fields |
| A station chooses at most one advertised RA-RU per opportunity | UORA counter plus targeted coordinator log | duplicate decision for one station/Trigger | `HeUlCoordinator` OCW/OBO state | log OBO, eligible-entry count, and selected index |
| A UORA attempt is classified exactly once | per-STA attempt/success scalars | success exceeds attempt or counters absent | coordinator signal/recorder | narrow scalar query, then `HeHcfUl` terminal response |
| Trigger is followed by HE-TB and immediate AP response | AP timeline | missing or badly ordered exchange | HCF frame sequence or PHY reception | correlate AP/STA capture, then event log |
| Heavy comparison remains paired | run metadata/config entries | seed, load, window, or scheduler differs | campaign configuration | inspect run attributes and effective INI |

## [agent] Reproduction

Run from the INET repository root. The following control command was executed
in release mode and exited 0 after reaching the 0.5 s simulation-time limit:

```sh
bin/inet -u Cmdenv -f examples/ieee80211ax/ul_uora/omnetpp.ini \
  -c UoraHeavyContention -r 0 --seed-set=0 --sim-time-limit=0.5s \
  --result-dir=examples/ieee80211ax/ul_uora/results/20260727T130440Z/UoraHeavyContention \
  '--**.scalar-recording=false' '--**.vector-recording=false' \
  '--**.heUlRandomAccessAttempt*.scalar-recording=true' \
  '--**.heUlRandomAccessSuccess*.scalar-recording=true' \
  '--**.packetReceived*.vector-recording=true' \
  '--**.endToEndDelay*.vector-recording=true' \
  '--**.ap.wlan[*].recordPcap=true' \
  '--**.wlan[*].pcapRecorder[*].moduleNamePatterns="mac"' \
  '--**.wlan[*].pcapRecorder[*].verbose=false' \
  '--**.wlan[*].pcapRecorder[*].fileFormat="pcapng"' \
  '--**.checksumMode="computed"' '--**.fcsMode="computed"' \
  --cmdenv-express-mode=true
```

The treatment used the same command with
`-c UoraMoreRandomAccessRus` and the matching result-directory suffix; it also
exited 0. Because the relative `--result-dir` is resolved against the INI
directory, `.sca`/`.vec` files appear under the nested path recorded in
Artifact provenance. The PCAP recorder path appears under the non-nested
session directory. Use `--result-dir="$PWD/examples/..."` in a new session to
avoid that path quirk.

The shared suite's publication campaign is intentionally `NOT RUN`; it requires
five runs:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py run ul_uora \
  --suite ax --evidence both --runs 5 --session-id <new-UTC-session>
```

## [agent] Scalar and vector analysis

Inputs are the two `.sca` and `.vec` pairs listed in Artifact provenance.
The narrow scalar query is:

```sh
opp_scavetool query -l \
  -f 'module =~ "Lan80211AxUlOfdma.host[*].wlan[0].mac.hcf.ulCoordinator" AND (name =~ "heUlRandomAccessAttempt*" OR name =~ "heUlRandomAccessSuccess*")' \
  examples/ieee80211ax/ul_uora/examples/ieee80211ax/ul_uora/results/20260727T130440Z/*/*.sca
```

| Configuration / metric | Source result and module / unit | Window and per-run aggregation | Independent runs | Single-run observation |
|---|---|---|---:|---:|
| Heavy, one RA-RU / attempts | station `ulCoordinator`, `heUlRandomAccessAttempt:count` / count | full `[0,0.5]`, sum over 8 stations | 1 | 12 |
| Heavy, one RA-RU / successes | station `ulCoordinator`, `heUlRandomAccessSuccess:count` / count | full `[0,0.5]`, sum over 8 stations | 1 | 0 |
| Heavy, five RA-RUs / attempts | same | full `[0,0.5]`, sum over 8 stations | 1 | 21 |
| Heavy, five RA-RUs / successes | same | full `[0,0.5]`, sum over 8 stations | 1 | 8 |
| Heavy, one RA-RU / delivered delay | `server.app[0]`, `endToEndDelay:vector` / s | receive time `[0.3,0.5)`, packet mean over 884 deliveries | 1 | 32.608 ms |
| Heavy, five RA-RUs / delivered delay | same | receive time `[0.3,0.5)`, packet mean over 917 deliveries | 1 | 24.726 ms |

The success fractions are derived measurements: 0/12 and 8/21 (38.1%).
Stations are components of one run, not eight repetitions. There is no
configured OMNeT++ warm-up period; the `[0.3,0.5)` filter removes the earlier
1000-byte setup packets and uses the receive timestamp of each delivered
100-byte packet. Delay is delivery-conditioned, so packets still queued at
0.5 s are excluded. No confidence interval or population ordering is claimed.

No plot: two short single-run diagnostic rows and four direct counter totals
are clearer in the provenance-bound table; a comparison plot would visually
overstate statistical support. The five-run suite should generate the
publication figure before population-level conclusions are added.

## [agent] PCAP statistics

Both captures observe transmitted frames at `ap.wlan[0]`. They are PCAPng with
radiotap plus IEEE 802.11 encapsulation, one `wlan0` interface, microsecond
precision, computed checksums/FCS, and TShark/capinfos 4.6.4. Counts are packet
observations at that capture point, not de-duplicated transmissions,
application deliveries, or UORA decisions.

| Configuration | Capture point and selection | Observation count | Decisive packet facts | Airtime and limits |
|---|---|---:|---|---|
| Heavy, one RA-RU | AP; all decoded frames | 2166 | 54 Basic Triggers; 101 BSRP Triggers; 316 HE-TB; 181 Block Ack; representative Basic Trigger has one `AID12=0` entry | no authoritative UORA-only airtime; HE-TB includes both Trigger types |
| Heavy, five RA-RUs | AP; all decoded frames | 2066 | 50 Basic Triggers; 101 BSRP; 323 HE-TB; 193 Block Ack; representative Basic Trigger has five `AID12=0` entries | same limits |

For a decoded count:

```sh
XDG_CONFIG_HOME=/tmp/uora-tshark-config \
  tshark -n -r /tmp/uora-heavy.pcap \
  -Y 'wlan.trigger.he.trigger_type == 0' \
  -T fields -e frame.number | wc -l
```

TShark could not open the workspace capture in place in the authoring
environment, so analysis used byte-identical temporary copies. The original
capture hashes are retained below. `radiotap.he.data_1.ppdu_format == 3`
selects HE-TB observations; subtype `0x0019` selects Block Ack observations.
Create and verify the aliases before running either TShark command:

```sh
cp 'examples/ieee80211ax/ul_uora/results/20260727T130440Z/UoraHeavyContention/UoraHeavyContention-#0Lan80211AxUlOfdma.ap.wlan[0].pcap' \
  /tmp/uora-heavy.pcap
cp 'examples/ieee80211ax/ul_uora/results/20260727T130440Z/UoraMoreRandomAccessRus/UoraMoreRandomAccessRus-#0Lan80211AxUlOfdma.ap.wlan[0].pcap' \
  /tmp/uora-more.pcap
sha256sum /tmp/uora-heavy.pcap /tmp/uora-more.pcap
```

The expected hashes are
`c1ba6885533d5e37420294d49ab7427551956035b9df0f88c3bc487a68152dbb`
and
`c4533c3e21331102981d9856d355b3ee69023a313558c29fb38e3c159a978efe`,
respectively.

No plot: the only authoritative cross-configuration PHY distinction is the
one-versus-five `AID12=0` field count, which the table and timeline expose
directly. Packet count/airtime composition would mix scheduled, BSRP, and UORA
traffic and could imply a UORA attribution that the capture cannot provide.

## [agent] Frame exchange analysis

The representative treatment exchange was extracted with:

```sh
XDG_CONFIG_HOME=/tmp/uora-tshark-config \
  tshark -n -r /tmp/uora-more.pcap \
  -Y 'frame.number >= 620 && frame.number <= 628' \
  -T fields -E header=y -E separator='|' -E occurrence=a \
  -e frame.number -e frame.time_epoch -e wlan.fc.type_subtype \
  -e wlan.ta -e wlan.ra -e radiotap.he.data_1.ppdu_format \
  -e wlan.trigger.he.user_info.aid12 \
  -e wlan.trigger.he.ru_allocation -e _ws.col.Info
```

| Frame | Simulation time | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 620 | 0.301611 s | AP → broadcast | Basic Trigger | AID12 `5,6,0,0,0,0,0`; RU Allocation `0..6` | directs two users and advertises five separate RA-RUs |
| 621–627 | 0.302627 s | seven STAs → AP | QoS Null / HE-TB | PPDU format 3; seven distinct transmitter addresses | simultaneous trigger-based responses |
| 628 | 0.302696 s | AP → broadcast | Block Ack | control subtype `0x19` | terminal AP response in INET's exchange |

The control has the analogous exchange at frames 620–625: Trigger AID12
`5,6,0`, four co-timed HE-TB observations, then Block Ack. Frame numbers are
local PCAP numbers, not OMNeT++ event numbers. The capture does not expose a
trustworthy canonical RU index for each HE-TB response, and its generic Block
Ack bitmap does not expose INET's per-AID UORA result. Therefore the timeline
proves exchange structure and advertised RA-RU count, not which frames were
UORA successes or collisions.

## [agent] Cross-layer findings and verdict

| Claim | Verdict | Configuration evidence | Model telemetry | Packet evidence | Outcome evidence |
|---|---|---|---|---|---|
| The heavy control exercises one-RA-RU UORA | `PASS` | RA-RU min/max 1 | 12 attempts, 0 successes | one `AID12=0`; Trigger/HE-TB/BA | 884 post-0.3 s deliveries |
| The treatment exercises five-RA-RU UORA | `PASS` | RA-RU min/max 5 | 21 attempts, 8 successes | five `AID12=0`; Trigger/HE-TB/BA | 917 post-0.3 s deliveries |
| More RA-RUs increased modeled random-access success in this seed | `PASS` | matched causal delta | 0 versus 8 successes | allocation delta directly decoded | lower delivery-conditioned mean delay |
| More RA-RUs generally improve performance | `NOT RUN` | only one seed | no uncertainty | one representative trace | no multi-run estimate |
| Co-timed HE-TB observations prove collisions | `INCONCLUSIVE` | contention is configured | counters do not expose collision cause | per-response RU identity unavailable | not applicable |

The cross-layer chain is direct through effective configuration, modeled UORA
counters, and packet-visible allocation/exchange. The association between
individual HE-TB frames and individual success-counter increments remains
unresolved because neither artifact exposes a common Trigger/attempt identity.

## [agent] Limitations and inconclusive claims

- Only run 0/seed 0 and the first 0.5 s are retained; the suite's five-run,
  2-second publication policy is `NOT RUN`.
- `ScheduledOnly`, `MixedUora`, and `UoraLightContention` are configuration
  context, not retained runtime controls.
- No result records expose per-Trigger advertised-RA-RU count or timestamped
  attempt/success decisions.
- AP captures mix Basic-Trigger and BSRP HE-TB traffic. Frame totals cannot be
  converted into UORA attempts or successes.
- A generic decoded Block Ack bitmap is not the model's per-AID success record.
- Delay is delivery-conditioned and the truncated run may leave queued packets.
- The smallest decisive extension is one five-run shared-suite session plus
  timestamped station decision telemetry keyed to Trigger and selected RU.

## [agent] Further experiments

- Run the shared five-seed campaign and check whether five RA-RUs retain a
  higher per-run success count without claiming an ordering when intervals
  overlap.
- Add the eight-station `ScheduledOnly` heavy workload as a matched negative
  control and verify zero UORA attempts.
- Sweep one through five RA-RUs at fixed load; record both UORA success and
  total delivered goodput to expose the scheduled/random-access trade-off.
- Sweep OCW bounds with one RA-RU and predict changes in attempts, successes,
  and station-level fairness.

## [agent] Implementation plan

| Item | Evidence-backed plan |
|---|---|
| Demonstrated gap | attempt/success scalars lack timestamps and a shared Trigger/RU identity |
| Intended behavior | make UORA selection and terminal outcome directly correlatable without changing protocol behavior |
| Smallest change surface | first extend the suite's UORA feature plugin and existing signal recording; change production signals only if no suitable identity exists |
| Observability | Trigger identity/time, station, access category, OBO/OCW, selected RA-RU, terminal outcome/reason |
| Validation | scheduled-only negative control; heavy 1-vs-5 RA-RU pair; run 0 mechanism check then five seeds |
| Compatibility and risks | added recording can change trajectory; compare configurations only within one co-recorded session |
| Architecture and sealing | apply `inet-architectural-requirements` and check seals before any future `src/inet` edit |
| Next handoff | simulation investigator maps existing signals before an implementation proposal |

This plan records a missing-observability follow-up; it is not authorization to
change production source.

## [agent] Artifact provenance

| Artifact family | Session/path | Configurations/runs | Tool/filter/window | Integrity notes |
|---|---|---|---|---|
| Scalar/vector | `examples/ieee80211ax/ul_uora/examples/ieee80211ax/ul_uora/results/20260727T130440Z` | heavy 1/5 RA-RU, run 0, seed 0 | `opp_scavetool`; UORA counts and sink vectors; `[0.3,0.5)` delay | `.sca` SHA256 `b5936f…b1142`, `5844b5…fe7eb`; `.vec` `10b6cd…14e1a`, `cb07c2…7d6fe` |
| PCAP | `results/20260727T130440Z` | same pair/run/seed | TShark/capinfos 4.6.4; AP MAC point | pcapng, radiotap, 1 µs; SHA256 `c1ba68…dbb`, `c4533c…8efe` |
| Excluded diagnostic | `results/20260727T125546Z` and `results/20260727T130246Z` | incomplete or capture-only | not used for any claim | absent final scalar/capture pairing |

The script-owned ledger remains `NOT RUN` because the shared publishers were
not used and this diagnostic session is below their five-run publication
threshold. The agent-owned ledger names the exact session used for all authored
claims.
