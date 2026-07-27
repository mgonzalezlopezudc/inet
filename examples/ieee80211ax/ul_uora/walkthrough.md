# Walkthrough: 802.11ax Uplink OFDMA Random Access

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260727T191738Z`
- PCAP: `20260727T142600Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `20260727T130440Z`, `20260727T142600Z`, `20260727T191738Z`.

This walkthrough isolates uplink orthogonal frequency-division multiple access
random access (UORA). It compares one and five random-access resource units
(RA-RUs) under the same eight-station heavy load and includes a separate
three-station scheduled-only negative control. Session
`20260727T130440Z` is a co-recorded 0.5-second diagnostic pair for the
representative mechanism. Publication session `20260727T142600Z` retains
representative run-0 AP captures for four UORA configurations. Scalar/vector
publication session `20260727T191738Z` adds the `ScheduledOnly` negative
control and contains all five configurations, five independent runs each, and
the full 2-second duration for the goodput, delay, and mechanism comparison.

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
The measured application starts at 0.3 s. The scheduled-only and mixed
configurations send one 1000-byte UDP packet per station every 5 ms from three
stations. The heavy comparison sends one 100-byte packet per station every
1 ms from eight stations toward the wired server.

The publication session uses the configured 2 s limit and analyzes
`[0.3,2.0)`. The separate diagnostic overrides the limit to 0.5 s so its
annotated comparison focuses on initial heavy-load UORA behavior. Within each
session, the heavy control and treatment use the same topology, load, run/seed
policy, scheduler, and recording envelope; only the configured RA-RU count
changes.

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
| Stations make modeled UORA attempts | `PASS` | per-station `heUlRandomAccessAttempt:count` | five runs/config plus diagnostic run 0 | publication heavy means 9.4 versus 19.2 attempts |
| Five-RA-RU treatment records more modeled successes in this campaign | `PASS` | `heUlRandomAccessSuccess:count` | paired seeds 0–4 | heavy means 0.8 versus 7.2 successes |
| Trigger/HE-TB/Block-Ack structure occurs | `PASS` | AP PCAP timeline | run 0, seed 0 | protocol-visible sequence |
| Scheduled-only uplink produces no modeled UORA attempts | `PASS` | `heUlRandomAccessAttempt:count` and `heUlRandomAccessSuccess:count` | five runs/seeds | both counters are zero in every run |
| Scheduled-only goodput and delay are measured | `PASS` | server `packetReceived:vector(packetBytes)` and `endToEndDelay:vector` | five runs/seeds | 4.781 Mbit/s goodput and 19.005 ms p95 delay; estimates are specific to the three-station workload |
| Five RA-RUs improve end-to-end performance generally | `INCONCLUSIVE` | five-run goodput and p95-delay estimates | paired heavy runs/seeds 0–4 | heavy confidence intervals overlap and coverage is one topology |
| Co-timed HE-TB frames identify collisions | `INCONCLUSIVE` | canonical per-response RU index unavailable | both captures | do not infer collision from timing |

The publication scalar/vector and packet artifacts are from separate sessions,
so they support adjacent outcome and mechanism claims but not event-level
causality. The older diagnostic remains the source of the annotated
UORA-specific Trigger exchange. Frame totals still do not identify the
station-side UORA decision; the model counters are authoritative for attempts
and successes.

## [agent] Configuration matrix

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| `ScheduledOnly` | Negative control | RA-RUs fixed at 0; conservative MU-EDCA values also differ from `MixedUora` | 3 stations, 1000 B/5 ms | runs/seeds 0–4 | zero UORA attempts; scheduled delivery remains observable |
| `MixedUora` | Reference | adaptive 1–3 RA-RUs | 3 stations, 1000 B/5 ms | runs/seeds 0–4 | scheduled and random access coexist |
| `UoraLightContention` | Load control | one RA-RU | 8 stations, 100 B/4 ms | runs/seeds 0–4 | UORA under lighter load |
| `UoraHeavyContention` | Retained control | one RA-RU | 8 stations, 100 B/1 ms, 20 MHz | runs/seeds 0–4 plus diagnostic run 0 | one `AID12=0` entry and modeled attempts |
| `UoraMoreRandomAccessRus` | Retained treatment | five RA-RUs | matched heavy inputs | runs/seeds 0–4 plus diagnostic run 0 | five `AID12=0` entries and more access capacity |

`UoraHeavyContention` extends `UoraLightContention`, which extends
`MixedUora`. Child assignments fix `minRandomAccessRus=maxRandomAccessRus=1`.
`UoraMoreRandomAccessRus` extends the heavy configuration and replaces only
those values with 5. Both inherit `maxMuStations=2`. The treatment therefore
holds the offered load and scheduler family fixed. Random station placement is
seed-dependent but paired by seed across the five publication runs.

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
session directory. For a new run, use
`--result-dir="$PWD/examples/ieee80211ax/ul_uora/results/<new-session>/UoraHeavyContention"`
to keep `.sca`, `.vec`, and PCAP files together.

The following combined publication campaign was executed from the repository
root and exited 0 after all 20 simulations completed:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py run ul_uora \
  --suite ax --evidence both --runs 5 \
  --session-id 20260727T142600Z
python3 examples/ieee80211/analysis/wifi_analysis.py report ul_uora \
  --suite ax --session-id 20260727T142600Z
python3 examples/ieee80211/analysis/wifi_analysis.py publish ul_uora \
  --suite ax --session-id 20260727T142600Z --update
```

The campaign covers configurations `MixedUora`, `UoraLightContention`,
`UoraHeavyContention`, and `UoraMoreRandomAccessRus`, runs/seeds `[0,5)`.
Run 0 of each configuration also records the AP PCAP in the same result
session.

After adding `ScheduledOnly` to the scalar/vector manifest, this complete
five-configuration campaign was executed from the repository root. It exited
0 after all 25 simulations completed:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py run ul_uora \
  --suite ax --evidence scalar-vector --runs 5 \
  --session-id 20260727T191738Z
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211/analysis/wifi_analysis.py report ul_uora \
  --suite ax --session-id 20260727T191738Z
python3 examples/ieee80211ax/analysis/render_walkthrough_results.py \
  uora --update
```

This scalar/vector session covers all five configurations and runs/seeds
`[0,5)`. It does not contain packet captures; the PCAP sections therefore
retain session `20260727T142600Z`.

## [agent] Scalar and vector analysis

The diagnostic inputs are the two `.sca` and `.vec` pairs listed in Artifact
provenance. The narrow diagnostic scalar query is:

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

No plot is used for the two-row diagnostic table because it is single-run
mechanism evidence. The generated publication dashboard below answers the
separate five-run questions: how scheduled-only and UORA configurations
compare in aggregate goodput and delivered-packet p95 delay, and whether the
modeled UORA counters distinguish the negative control. Error bars are
Student-t 95% confidence intervals over one aggregate observation per run.

For the publication outcomes, the native result API selects
`Lan80211AxUlOfdma.server.app[0]`. Goodput sums
`packetReceived:vector(packetBytes)` values received in `[0.3,2.0)` and
divides the delivered bits by 1.7 s. Delay pools
`endToEndDelay:vector` samples within each run and takes that run's 95th
percentile before calculating the cross-run mean and interval. The
`packetBytes` recorder name defines byte values because its unit attribute is
empty; the delay vector records seconds. Every run has a nonempty delay
sample: 1014–1017 for `ScheduledOnly`, 1018–1020 for `MixedUora`, 3270–3357
for the light case, 6336–7508 for heavy one-RA-RU, and 6779–7574 for heavy
five-RA-RU.

`ScheduledOnly` directly records zero attempts and zero successes in every
run. Under the shared three-station workload, `MixedUora` records slightly
higher mean goodput (4.797 versus 4.781 Mbit/s) and lower mean p95 delay
(6.713 versus 19.005 ms). This is not a single-parameter causal comparison:
`ScheduledOnly` also assigns conservative MU-EDCA values. The eight-station
light and heavy cases change offered load as well as access behavior and must
not be compared to `ScheduledOnly` as if the RA-RU count were their only
difference. Within the paired heavy comparison, the five-RA-RU estimates have
higher mean goodput and lower mean p95 delay, but both 95% confidence
intervals overlap the one-RA-RU estimates, so no strict end-to-end ordering is
claimed.

The UORA attempt and success values are terminal scalar counters over the full
`[0,2.0]` simulation, including the setup phase; they are not restricted to
the `[0.3,2.0)` application-outcome window. This distinction does not weaken
the scheduled-only zero-attempt invariant.

<!-- BEGIN GENERATED: ieee80211-scalar-vector-uora -->
### [script] Generated scalar/vector plot and table

![uora scalar/vector analysis](results/20260727T191738Z/uora-dashboard.png)

Figure provenance: [`results/20260727T191738Z/uora-dashboard.png.json`](results/20260727T191738Z/uora-dashboard.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.server.app[*] / packetReceived:vector(packetBytes)<br>vector / **.server.app[*] / endToEndDelay:vector / unit=s<br>scalar / heUlRandomAccessAttempt:count<br>scalar / heUlRandomAccessSuccess:count
- Window / per-run aggregation / exclusions: [0.3, 2.0) s; delay=pool delivered-packet delays within each run over [0.3, 2.0), then take the 95th percentile; one value per run; goodput=sum delivered application bytes over [0.3, 2.0) across sink vectors, convert to bit/s; one value per run; mechanism=terminal full-simulation [0, 2.0] scalar counters summed across stations; one value per run; uncertainty=95% Student-t CI; zero_success_runs={'MixedUora': 0, 'ScheduledOnly': 5, 'UoraHeavyContention': 1, 'UoraLightContention': 0, 'UoraMoreRandomAccessRus': 0}
- Independent runs: run-level summaries: n=4, 5; direct observations: no independent-run estimate

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| Heavy, 1 RA-RU / attempts | 9.4 | 5.08931 |
| Heavy, 1 RA-RU / delay p95 ms | 484.385 | 139.49 |
| Heavy, 1 RA-RU / goodput mbps | 3.30202 | 0.271084 |
| Heavy, 1 RA-RU / success fairness | 0.125 | 0 |
| Heavy, 1 RA-RU / success probability | 0.119658 | 0.156692 |
| Heavy, 1 RA-RU / successful transmissions | 0.8 | 0.555289 |
| Heavy, 1 RA-RU / zero success run count | 1 | — |
| Heavy, 5 RA-RUs / attempts | 19.2 | 7.77405 |
| Heavy, 5 RA-RUs / delay p95 ms | 417.761 | 37.2561 |
| Heavy, 5 RA-RUs / goodput mbps | 3.38824 | 0.174175 |
| Heavy, 5 RA-RUs / success fairness | 0.543333 | 0.184415 |
| Heavy, 5 RA-RUs / success probability | 0.370399 | 0.122753 |
| Heavy, 5 RA-RUs / successful transmissions | 7.2 | 3.96556 |
| Heavy, 5 RA-RUs / zero success run count | 0 | — |
| Light, 1 RA-RU / attempts | 213.4 | 55.5539 |
| Light, 1 RA-RU / delay p95 ms | 60.2312 | 15.4339 |
| Light, 1 RA-RU / goodput mbps | 1.56188 | 0.0217023 |
| Light, 1 RA-RU / success fairness | 0.475238 | 0.181105 |
| Light, 1 RA-RU / success probability | 0.0775811 | 0.0291306 |
| Light, 1 RA-RU / successful transmissions | 16.2 | 5.07414 |
| Light, 1 RA-RU / zero success run count | 0 | — |
| Mixed, adaptive 1–3 RA-RUs / attempts | 68.6 | 11.7336 |
| Mixed, adaptive 1–3 RA-RUs / delay p95 ms | 6.71317 | 1.15087 |
| Mixed, adaptive 1–3 RA-RUs / goodput mbps | 4.79718 | 0.00522625 |
| Mixed, adaptive 1–3 RA-RUs / success fairness | 0.661106 | 0.00273725 |
| Mixed, adaptive 1–3 RA-RUs / success probability | 1 | 0 |
| Mixed, adaptive 1–3 RA-RUs / successful transmissions | 68.6 | 11.7336 |
| Mixed, adaptive 1–3 RA-RUs / zero success run count | 0 | — |
| Scheduled only, 0 RA-RUs / attempts | 0 | 0 |
| Scheduled only, 0 RA-RUs / delay p95 ms | 19.0045 | 1.54925 |
| Scheduled only, 0 RA-RUs / goodput mbps | 4.78118 | 0.00715634 |
| Scheduled only, 0 RA-RUs / successful transmissions | 0 | 0 |
| Scheduled only, 0 RA-RUs / zero success run count | 5 | — |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.
<!-- END GENERATED: ieee80211-scalar-vector-uora -->

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

No plot is used for the diagnostic pair because the decisive fact is the
one-versus-five `AID12=0` field count. The generated publication plot below
does compare overall packet composition and estimated airtime, but it remains
descriptive: it mixes scheduled, BSRP, and UORA traffic and does not attribute
those totals to UORA decisions.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260727T142600Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260727T142600Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260727T142600Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260727T142600Z.json` (SHA-256 `11e592e4103f83fb282198192eddc4a0e826db94452cd95ecca59fb4686a79a4`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `MixedUora` | `none (all decoded frames)` | 5085 | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] (1367), Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (1281), Control: Ack (1021) | 70.86% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UoraHeavyContention` | `none (all decoded frames)` | 14633 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (13351), Control: Block Ack (BA) (336), Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] (317) | 69.81% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UoraLightContention` | `none (all decoded frames)` | 11466 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (8111), Control: Block Ack (BA) (903), Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] (771) | 60.50% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UoraMoreRandomAccessRus` | `none (all decoded frames)` | 14027 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (12709), Control: Block Ack (BA) (341), Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] (323) | 67.13% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | MixedUora produced protocol-visible wireless observations | 5085 AP/global transmission observations |
| **PASS** | UoraHeavyContention produced protocol-visible wireless observations | 14633 AP/global transmission observations |
| **PASS** | UoraLightContention produced protocol-visible wireless observations | 11466 AP/global transmission observations |
| **PASS** | UoraMoreRandomAccessRus produced protocol-visible wireless observations | 14027 AP/global transmission observations |

### [script] Configuration: `MixedUora`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **5085**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 1281 | 25.19% | 1070.0 B | 0.0 B | 621.3 us | 0.0 us | 5010 MHz | -63.3 dBm | - | 56.16% | 39.79% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d790b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 1367 | 26.88% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 38.45% | 27.25% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 708 | 13.92% | 49.9 B | 9.5 B | 36.6 us | 3.2 us | 5010 MHz | - | 10.0 dBm | 1.83% | 1.30% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 708 | 13.92% | 47.2 B | 3.6 B | 35.7 us | 1.2 us | 5010 MHz | - | 10.0 dBm | 1.79% | 1.27% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 1021 | 20.08% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 1.78% | 1.26% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.002064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=216 | Carries protocol-visible MAC payload in the representative exchange. |
| 3 | 0.002064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=224 | Carries protocol-visible MAC payload in the representative exchange. |
| 4 | 0.002129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 5 | 0.003048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 6 | 0.004064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=344 | Carries protocol-visible MAC payload in the representative exchange. |
| 7 | 0.004064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=352 | Carries protocol-visible MAC payload in the representative exchange. |
| 8 | 0.004129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 9 | 0.005048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 10 | 0.006064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=472 | Carries protocol-visible MAC payload in the representative exchange. |
| 11 | 0.006064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=480 | Carries protocol-visible MAC payload in the representative exchange. |
| 12 | 0.006129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 13 | 0.007048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 14 | 0.008064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=600 | Carries protocol-visible MAC payload in the representative exchange. |
| 15 | 0.008064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=608 | Carries protocol-visible MAC payload in the representative exchange. |
| 16 | 0.008129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |

Frame numbers are local to capture `MixedUora-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `UoraHeavyContention`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **14633**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 13351 | 91.24% | 166.1 B | 0.7 B | 91.8 us | 6.1 us | 5010 MHz | -60.3 dBm | - | 87.78% | 61.28% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 86 | 0.59% | 264.2 B | 275.5 B | 180.5 us | 150.7 us | 5010 MHz | -62.2 dBm | - | 1.11% | 0.78% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d790b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 317 | 2.17% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 9.05% | 6.32% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 156 | 1.07% | 63.5 B | 12.9 B | 41.2 us | 4.3 us | 5010 MHz | - | 10.0 dBm | 0.46% | 0.32% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 300 | 2.05% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -61.1 dBm | - | 0.60% | 0.42% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 336 | 2.30% | 38.5 B | 7.1 B | 32.8 us | 2.4 us | 5010 MHz | - | 10.0 dBm | 0.79% | 0.55% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 55 | 0.38% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.10% | 0.07% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 16 | 0.11% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -59.2 dBm | 10.0 dBm | 0.03% | 0.02% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 16 | 0.11% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -59.2 dBm | 10.0 dBm | 0.08% | 0.06% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.002064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=451 | Carries protocol-visible MAC payload in the representative exchange. |
| 3 | 0.002064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=457 | Carries protocol-visible MAC payload in the representative exchange. |
| 4 | 0.002129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 5 | 0.003048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 6 | 0.004064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=719 | Carries protocol-visible MAC payload in the representative exchange. |
| 7 | 0.004064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=725 | Carries protocol-visible MAC payload in the representative exchange. |
| 8 | 0.004129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 9 | 0.005048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 10 | 0.006064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=987 | Carries protocol-visible MAC payload in the representative exchange. |
| 11 | 0.006064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=993 | Carries protocol-visible MAC payload in the representative exchange. |
| 12 | 0.006129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 13 | 0.007048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 14 | 0.008064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1255 | Carries protocol-visible MAC payload in the representative exchange. |
| 15 | 0.008064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1261 | Carries protocol-visible MAC payload in the representative exchange. |
| 16 | 0.008129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |

Frame numbers are local to capture `UoraHeavyContention-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `UoraLightContention`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **11466**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 8111 | 70.74% | 167.2 B | 1.8 B | 95.0 us | 11.3 us | 5010 MHz | -59.2 dBm | - | 63.71% | 38.54% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 438 | 3.82% | 188.5 B | 127.7 B | 139.1 us | 69.8 us | 5010 MHz | -56.4 dBm | - | 5.04% | 3.05% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d790b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 771 | 6.72% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 25.40% | 15.37% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 562 | 4.90% | 50.9 B | 10.4 B | 37.0 us | 3.5 us | 5010 MHz | - | 10.0 dBm | 1.72% | 1.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 622 | 5.42% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -57.5 dBm | - | 1.44% | 0.87% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 903 | 7.88% | 41.0 B | 7.3 B | 33.7 us | 2.4 us | 5010 MHz | - | 10.0 dBm | 2.51% | 1.52% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 26 | 0.23% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.05% | 0.03% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 16 | 0.14% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -59.2 dBm | 10.0 dBm | 0.03% | 0.02% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 17 | 0.15% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -59.2 dBm | 10.0 dBm | 0.10% | 0.06% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.002064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=451 | Carries protocol-visible MAC payload in the representative exchange. |
| 3 | 0.002064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=457 | Carries protocol-visible MAC payload in the representative exchange. |
| 4 | 0.002129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 5 | 0.003048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 6 | 0.004064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=719 | Carries protocol-visible MAC payload in the representative exchange. |
| 7 | 0.004064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=725 | Carries protocol-visible MAC payload in the representative exchange. |
| 8 | 0.004129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 9 | 0.005048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 10 | 0.006064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=987 | Carries protocol-visible MAC payload in the representative exchange. |
| 11 | 0.006064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=993 | Carries protocol-visible MAC payload in the representative exchange. |
| 12 | 0.006129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 13 | 0.007048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 14 | 0.008064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1255 | Carries protocol-visible MAC payload in the representative exchange. |
| 15 | 0.008064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1261 | Carries protocol-visible MAC payload in the representative exchange. |
| 16 | 0.008129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |

Frame numbers are local to capture `UoraLightContention-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `UoraMoreRandomAccessRus`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **14027**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 12709 | 90.60% | 166.1 B | 0.7 B | 91.8 us | 6.1 us | 5010 MHz | -57.6 dBm | - | 86.91% | 58.35% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 101 | 0.72% | 250.2 B | 256.4 B | 172.9 us | 140.3 us | 5010 MHz | -58.3 dBm | - | 1.30% | 0.87% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d790b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 323 | 2.30% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz, 5008 MHz, 5010 MHz, 5012 MHz, 5014 MHz | -75.0 dBm | - | 9.59% | 6.44% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 151 | 1.08% | 72.0 B | 1.4 B | 44.0 us | 0.5 us | 5010 MHz | - | 10.0 dBm | 0.49% | 0.33% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 319 | 2.27% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -56.8 dBm | - | 0.67% | 0.45% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 341 | 2.43% | 38.5 B | 7.8 B | 32.8 us | 2.6 us | 5010 MHz | - | 10.0 dBm | 0.83% | 0.56% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 51 | 0.36% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.09% | 0.06% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 16 | 0.11% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -59.2 dBm | 10.0 dBm | 0.03% | 0.02% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 16 | 0.11% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -59.2 dBm | 10.0 dBm | 0.08% | 0.06% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.002064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=451 | Carries protocol-visible MAC payload in the representative exchange. |
| 3 | 0.002064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=457 | Carries protocol-visible MAC payload in the representative exchange. |
| 4 | 0.002129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 5 | 0.003048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 6 | 0.004064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=719 | Carries protocol-visible MAC payload in the representative exchange. |
| 7 | 0.004064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=725 | Carries protocol-visible MAC payload in the representative exchange. |
| 8 | 0.004129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 9 | 0.005048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 10 | 0.006064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=987 | Carries protocol-visible MAC payload in the representative exchange. |
| 11 | 0.006064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=993 | Carries protocol-visible MAC payload in the representative exchange. |
| 12 | 0.006129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 13 | 0.007048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 14 | 0.008064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1255 | Carries protocol-visible MAC payload in the representative exchange. |
| 15 | 0.008064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1261 | Carries protocol-visible MAC payload in the representative exchange. |
| 16 | 0.008129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |

Frame numbers are local to capture `UoraMoreRandomAccessRus-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Analysis of Packet Distribution
Across these configurations, **QoS Data** frames constitute the primary payload delivery mechanism, while **Block Ack (BA)** and **Block Ack Request (BAR)** control frames ensure reliable transport via the MAC-level acknowledgment protocol. Management frames, specifically **Beacons**, are transmitted periodically by the Access Point to maintain BSS time synchronization and broadcast network capabilities. The ratio of control/management overhead to actual data frames indicates the relative MAC efficiency of the chosen configurations.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->

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
| Scheduled-only uplink disables UORA | `PASS` | RA-RU min/max 0 | zero attempts and successes in all five runs | no same-session capture | 4.781 ± 0.007 Mbit/s goodput; 19.005 ± 1.549 ms p95 delay |
| The heavy control exercises one-RA-RU UORA | `PASS` | RA-RU min/max 1 | publication mean 9.4 attempts and 0.8 successes | run-0 capture plus diagnostic one-entry Trigger/HE-TB/BA | diagnostic: 884 post-0.3 s deliveries |
| The treatment exercises five-RA-RU UORA | `PASS` | RA-RU min/max 5 | publication mean 19.2 attempts and 7.2 successes | run-0 capture plus diagnostic five-entry Trigger/HE-TB/BA | diagnostic: 917 post-0.3 s deliveries |
| More RA-RUs increased modeled random-access success in this campaign | `PASS` | matched heavy delta and paired seeds | 0.8 ± 0.555 versus 7.2 ± 3.966 successes (95% CI half-width) | allocation delta directly decoded in representative diagnostic | publication bundle does not claim an end-to-end cause |
| More RA-RUs generally improve end-to-end performance | `INCONCLUSIVE` | five paired seeds in one topology | mechanism counters have uncertainty | packet composition is not delivery | 5-RA-RU mean goodput is higher and p95 delay lower, but both 95% intervals overlap |
| Co-timed HE-TB observations prove collisions | `INCONCLUSIVE` | contention is configured | counters do not expose collision cause | per-response RU identity unavailable | not applicable |

The cross-layer chain is direct through effective configuration, modeled UORA
counters, and packet-visible allocation/exchange. The association between
individual HE-TB frames and individual success-counter increments remains
unresolved because neither artifact exposes a common Trigger/attempt identity.

## [agent] Limitations and inconclusive claims

- Each publication condition has five independent seeds in one topology; this
  does not support a broad real-world performance claim.
- `ScheduledOnly` and `MixedUora` share the three-station 1000 B/5 ms
  workload, but the scheduled-only MU-EDCA assignments are an additional
  causal delta. The light/heavy cases use eight stations and different packet
  sizes and intervals, so cross-workload outcome differences are confounded.
- The scalar/vector and PCAP publication evidence comes from different
  sessions. It supports adjacent scoped claims, not event-level
  packet-to-result causality.
- No result records expose per-Trigger advertised-RA-RU count or timestamped
  attempt/success decisions.
- AP captures mix Basic-Trigger and BSRP HE-TB traffic. Frame totals cannot be
  converted into UORA attempts or successes.
- A generic decoded Block Ack bitmap is not the model's per-AID success record.
- Delay is delivery-conditioned; packets not delivered before 2 s do not
  contribute to the p95 calculation.
- The smallest decisive mechanism extension is timestamped station decision
  telemetry keyed to Trigger and selected RU.

## [agent] Further experiments

- Extend the paired campaign beyond five seeds and report the paired
  per-seed success-count differences, not only separate confidence intervals.
- Add an eight-station `ScheduledOnly` heavy workload as a matched negative
  control for the heavy pair; the retained scheduled-only control uses the
  three-station baseline workload.
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
| Scalar/vector publication | `results/20260727T191738Z` | five configs, runs/seeds 0–4 | native result API; server receive-byte and delay vectors; `[0.3,2.0)`; per-run aggregation and Student-t 95% CI | 25 `.sca`/`.vec` pairs; goodput/delay/mechanism dashboard plus JSON sidecar |
| PCAP publication | `results/20260727T142600Z` | four UORA configs, representative run/seed 0 | shared HE profile; TShark/capinfos 4.6.4; AP MAC point | four pcapng captures; immutable manifest SHA256 `11e592…79a4`; no `ScheduledOnly` capture |
| Scalar/vector | `examples/ieee80211ax/ul_uora/examples/ieee80211ax/ul_uora/results/20260727T130440Z` | heavy 1/5 RA-RU, run 0, seed 0 | `opp_scavetool`; UORA counts and sink vectors; `[0.3,0.5)` delay | `.sca` SHA256 `b5936f…b1142`, `5844b5…fe7eb`; `.vec` `10b6cd…14e1a`, `cb07c2…7d6fe` |
| PCAP | `results/20260727T130440Z` | same pair/run/seed | TShark/capinfos 4.6.4; AP MAC point | pcapng, radiotap, 1 µs; SHA256 `c1ba68…dbb`, `c4533c…8efe` |
| Excluded diagnostic | `results/20260727T125546Z` and `results/20260727T130246Z` | incomplete or capture-only | not used for any claim | absent final scalar/capture pairing |

The script-owned ledger names the scalar/vector and PCAP publication sessions
separately. The agent-owned ledger lists the diagnostic mechanism session and
both publication sessions used to reconcile the authored conclusions.
