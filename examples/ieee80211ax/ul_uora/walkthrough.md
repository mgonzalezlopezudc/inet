# Walkthrough: 802.11ax Uplink OFDMA Random Access

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260730T202045Z`
- PCAP: `20260730T202045Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `20260727T202540Z`.

This walkthrough explains uplink orthogonal frequency-division multiple access
random access (UORA) and treats the example as an executable feature check. It
uses one logical publication session: scalar/vector outcomes come from five
independent runs per configuration, while packet-capture mechanism evidence
comes from run/seed 0. The three eight-station run-0 captures were regenerated
separately after an interrupted campaign, so those packet and result artifacts
support adjacent claims but not event-level causality.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- explain why an access point (AP) advertises random-access resource units
  (RA-RUs) in a Basic Trigger;
- identify the Trigger → high-efficiency trigger-based (HE-TB) response →
  acknowledgment exchange;
- distinguish scheduled uplink traffic from modeled UORA attempts and
  successes;
- relate RA-RU count and offered load to goodput, delay, success probability,
  and fairness; and
- reproduce the experiment and follow a focused diagnostic path when an
  invariant fails.

UORA lets associated stations contend inside an AP-coordinated uplink
transmission. Each station maintains an OFDMA contention window (OCW) and an
OFDMA backoff (OBO) counter. A Basic Trigger advertises eligible RA-RUs. A
station with queued traffic counts those opportunities, decrements OBO, and,
when eligible, selects one advertised RA-RU for an HE-TB transmission.
Different stations can choose the same RU, so an attempt need not succeed.
The response or response timeout determines the modeled result and the next
contention state.

## [agent] Scenario description

The [INI file](omnetpp.ini) selects the
[uplink OFDMA network](../ul_ofdma/Lan80211AxUlOfdma.ned), which extends the
[common single-BSS network](../../ieee80211/common/SingleBssNetwork.ned):

```text
host[0..7] -- 802.11ax uplink --> AP === 100-Gbit/s Ethernet === server
                    Basic Trigger <--
```

The AP and stations are stationary in a 50 m × 50 m area. The eight-station
cases draw each position uniformly within the central 20 m square. The radio
uses the scalar radio model at 5 GHz on a 20 MHz channel, with 10 mW transmit
power, -85 dBm receiver sensitivity, and a 4 dB SNIR threshold. There is no
mobility or external interferer.

Every station emits one 1000-byte setup packet beginning at 0.2 s to establish
Block Ack state. The measured application begins at 0.3 s and sends UDP toward
the wired server. The simulation ends at 1 s, and new application-outcome
analysis uses the half-open measurement window `[0.3,0.95)`.

## [agent] Standards and INET model boundary

IEEE Std 802.11-2024 defines Trigger frames and their User Info fields in
9.3.1.22 (corpus chunks `80211ax-2024:chunk:01659`-`01671`). Trigger Type 0
is a Basic Trigger. For associated-station random access, an `AID12=0` User
Info field advertises RA-RUs; RU Allocation locates the first RU and Number Of
RA-RU encodes the count minus one.

Clause 26.5.4 (`80211ax-2024:chunk:09810`-`09814`) specifies UORA
contention. If OBO is no greater than the number of eligible RA-RUs, the
station sets OBO to zero and randomly selects one eligible RU; otherwise it
subtracts that number. Success resets OCW to OCWmin, while failure updates it
to `min(OCWmax, 2*OCW+1)`. Clause 26.5.2.3.3
(`80211ax-2024:chunk:09802`) defines how the HE-TB TXVECTOR is derived from
the Trigger. The immediate-response rules in 10.3.2.13.3
(`80211ax-2024:chunk:05102`) permit Ack, Compressed Block Ack, or Multi-STA
Block Ack after SIFS; a Multi-STA Block Ack is therefore not the only
standards-permitted conclusion.

INET configures the mechanism with `HeHcf` and
`HeUlSchedulerBacklogBased`. The scheduler's choice of how many RA-RUs to
offer is an INET policy rather than an IEEE requirement. INET also exposes
`heUlRandomAccessAttempt` and `heUlRandomAccessSuccess` counters; these are
model telemetry, not fields carried over the air. The AP capture directly
observes Trigger and HE-TB frames, but a packet total alone does not reveal a
station's OBO decision or prove why a response failed.

The example uses simplified, always-associated management, so it does not
exchange the management frames that would advertise OFDMA random-access
capability and UORA Parameter Set values. Instead, `HeUlCoordinator` directly
configures OCWmin=7 and OCWmax=31 and maintains separate OCW/OBO state per
access category. The Trigger serializer emits standard Common/User Info
fields, while trigger identity, response correlation, OBO/OCW state, and
attempt/success classification remain model-only metadata.

Implementation anchors include
[`HeUlCoordinator.cc`](../../../src/inet/linklayer/ieee80211/mac/coordinationfunction/HeUlCoordinator.cc),
[`HeHcfUl.cc`](../../../src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfUl.cc),
[`HeUlSchedulerBacklogBased.cc`](../../../src/inet/linklayer/ieee80211/mac/scheduler/HeUlSchedulerBacklogBased.cc),
and
[`HeUlMuTxOpFs.cc`](../../../src/inet/linklayer/ieee80211/mac/framesequence/HeUlMuTxOpFs.cc).
The standards references describe required protocol behavior; the checked-out
source and retained artifacts describe INET's implementation and this run.

## [agent] Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| Scheduled-only produces no modeled UORA attempts or successes | `PASS` | per-station terminal scalar counters | runs/seeds 0-4 | negative control |
| UORA configurations make modeled attempts and record successes | `PASS` | `heUlRandomAccessAttempt:count` and `heUlRandomAccessSuccess:count` | runs/seeds 0-4 | model telemetry |
| Heavy Basic Triggers advertise one versus five RA-RUs | `PASS` | run-0 AP PCAP `AID12`, RU Allocation, and Number Of RA-RU | run/seed 0 | direct packet evidence for the advertised opportunities |
| Protocol-visible Trigger/HE-TB/acknowledgment exchanges occur | `PASS` | run-0 AP PCAPng timelines | run/seed 0 | representative packet evidence |
| One versus five RA-RUs changes UORA capacity under matched heavy load | `PASS` | effective configuration plus attempt/success summaries | paired runs/seeds 0-4 | bounded to this topology and policy |
| Every modeled random-access attempt used an RA-RU advertised by its Trigger | `INCONCLUSIVE` | session evidence ledger | runs/seeds 0-4 | no stable attempt-to-Trigger join |
| Five RA-RUs improve goodput and delay in general | `INCONCLUSIVE` | server goodput and p95 delay estimates | paired runs/seeds 0-4 | uncertainty and one workload prevent a general claim |
| A specific failed HE-TB response was caused by an RA-RU collision | `INCONCLUSIVE` | AP capture lacks a decisive station-decision/outcome join | run/seed 0 | requires correlated coordinator telemetry |

## [agent] Configuration matrix

| Configuration | Role | Feature gate or causal delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| `ScheduledOnly` | Negative control | fixed 0 RA-RUs; conservative MU-EDCA settings | 3 stations, 1000 B/5 ms each, 20 MHz | 0-4 | zero UORA attempts and successes |
| `MixedUora` | Mixed reference | adaptive 1-3 RA-RUs; at most 2 scheduled stations | same three-station 4.8-Mbit/s aggregate input | 0-4 | scheduled and random access coexist |
| `UoraLightContention` | Load control | fixed 1 RA-RU | 8 stations, 100 B/4 ms each | 0-4 | UORA remains observable under 1.6-Mbit/s aggregate input |
| `UoraHeavyContention` | Matched heavy control | fixed 1 RA-RU | 8 stations, 100 B/1 ms each | 0-4 | attempts contend for one RA-RU |
| `UoraMoreRandomAccessRus` | Matched heavy treatment | fixed 5 RA-RUs | otherwise identical heavy workload | 0-4 | more random-access opportunities are advertised |

`UoraHeavyContention` inherits `UoraLightContention`, which inherits
`MixedUora`. `UoraMoreRandomAccessRus` inherits the heavy case and replaces
only `minRandomAccessRus=maxRandomAccessRus=1` with 5. This is the clean
one-versus-five comparison. `ScheduledOnly` versus `MixedUora` is useful as a
mechanism contrast, but it is not a one-parameter RA-RU comparison because the
scheduled-only configuration also changes MU-EDCA settings.

## [agent] Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| Scheduled-only never enters modeled UORA | station `ulCoordinator` counters | any nonzero attempt/success | feature gate or recording path | inspect effective RA-RU bounds, then coordinator logs |
| Every UORA success is also an attempt | per-run summed attempt/success scalars | success exceeds attempt | signal accounting | query per-station scalars and inspect terminal response handling |
| AP emits Basic Triggers and stations answer with HE-TB | AP MAC PCAP timeline | Trigger without protocol-visible responses | HCF frame sequence, PHY, or contention state | compare AP and station captures, then targeted event log |
| Heavy comparison differs only in fixed RA-RU count | run metadata and effective INI | load, seed, scheduler, or window mismatch | campaign/configuration | inspect run attributes and winning assignments |
| Five-RA-RU case offers more modeled random-access capacity | attempt/success summaries | no attempts or all runs have zero success | scheduler plan or coordinator state | inspect advertised User Info and OCW/OBO decisions |

## [agent] Reproduction

The checked-in configuration now uses a total simulation time of 1 s, and the
current scalar/vector measurement window is `[0.3,0.95)` s. Numeric results
below remain provenance-bound to the earlier retained 2 s result session.

Run from the INET repository root. This direct single-run command shows the
campaign's essential Cmdenv invocation for the heavy control; it is
illustrative and was **NOT RUN separately** during this authoring session:

```sh
bin/inet -u Cmdenv -f examples/ieee80211ax/ul_uora/omnetpp.ini \
  -c UoraHeavyContention -r 0 --seed-set=0 \
  --result-dir="$PWD/examples/ieee80211ax/ul_uora/results/new-session/UoraHeavyContention" \
  --cmdenv-express-mode=true
```

The following shared campaign and report commands were executed for session
`20260727T202540Z`. The campaign runs each configuration for seeds/runs
`[0,5)`, records the selected `.sca`/`.vec` results for every run, and records
AP PCAPng on run 0:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py run ul_uora \
  --suite ax --evidence both --runs 5 \
  --session-id 20260727T202540Z
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211/analysis/wifi_analysis.py report ul_uora \
  --suite ax --session-id 20260727T202540Z
python3 examples/ieee80211/analysis/wifi_analysis.py publish ul_uora \
  --suite ax --session-id 20260727T202540Z --update
```

The initial campaign invocation was interrupted by the execution channel
before three run-0 files closed cleanly; it did not yield a usable success
status. The affected run-0 result pairs and AP captures were rerun to the
retained campaign's configured 2 s limit, validated, and placed as siblings in
their normal configuration directories. The final `report` and `publish` commands each
exited 0. Recovery copies were moved outside the repository to
`/tmp/ul-uora-20260727T202540Z-recovery/`.

The successful scalar/vector recovery command exited 0:

```sh
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211ax/analysis/run_campaign.py uora \
  --manifest examples/ieee80211/analysis/generated/sessions/20260727T202540Z/scalar-vector-manifest.json \
  --runs 1 --session-id 20260727T202540Z \
  --config UoraHeavyContention --config UoraMoreRandomAccessRus -j 2
```

The heavy run-0 capture was recovered with the following Cmdenv command; it
exited 0 at `t=2s`. The five-RA-RU recovery used the same command with
`UoraMoreRandomAccessRus` in the configuration and result paths and also
exited 0:

```sh
bin/inet -u Cmdenv -f examples/ieee80211ax/ul_uora/omnetpp.ini \
  -c UoraHeavyContention -r 0 --repeat=1 --seed-set=0 \
  --result-dir=examples/ieee80211ax/ul_uora/results/20260727T202540Z/pcap-refresh/UoraHeavyContention \
  '--**.vector-recording=false' '--**.scalar-recording=false' \
  '--**.ap.wlan[*].recordPcap=true' \
  '--**.wlan[*].pcapRecorder[*].moduleNamePatterns="mac"' \
  '--**.wlan[*].pcapRecorder[*].verbose=false' \
  '--**.wlan[*].pcapRecorder[*].fileFormat="pcapng"' \
  '--**.checksumMode="computed"' '--**.fcsMode="computed"' \
  --cmdenv-express-mode=true
```

Only the validated PCAP was installed beside the final `.sca`/`.vec` pair;
the `pcap-refresh` directory shown in this executed command was moved to the
recovery location after installation.

## [agent] Scalar and vector analysis

Inputs are the 25 `.sca`/`.vec` pairs under each configuration directory in
`results/20260727T202540Z/`. The native OMNeT++ result API
selects `Lan80211AxUlOfdma.server.app[0]` outcome vectors and the station
`ulCoordinator` mechanism counters. A narrow command-line discovery query is:

```sh
opp_scavetool query -l \
  -f 'module =~ "Lan80211AxUlOfdma.host[*].wlan[0].mac.hcf.ulCoordinator" AND (name =~ "heUlRandomAccessAttempt*" OR name =~ "heUlRandomAccessSuccess*")' \
  examples/ieee80211ax/ul_uora/results/20260727T202540Z/*/*.sca
```

Goodput sums `packetReceived:vector(packetBytes)` at the server within
`[0.3,2.0)` and converts the delivered bytes to bit/s over 1.7 s. Delay takes
the within-run 95th percentile of delivered-packet
`endToEndDelay:vector` samples in the same window. The analysis then computes
means and two-sided Student-t 95% confidence intervals across one aggregate
observation per run. UORA attempt/success values are terminal full-simulation
scalar counters summed across stations; they include setup time and are not
windowed. Stations and vector samples are never treated as repetitions.

The generated table and dashboard below are the session-bound presentation
bundle. They answer both the outcome question and the mechanism question; an
overlap in outcome confidence intervals is not converted into a strict
performance ordering.

The scheduled-only negative control records zero attempts and successes in
all five runs. Under matched heavy load, increasing the fixed RA-RU count from
one to five changes mean UORA success probability from 0.120 ± 0.157 to
0.370 ± 0.123 and mean successful transmissions from 0.8 ± 0.56 to
7.2 ± 3.97. Mean goodput changes from 3.302 ± 0.271 to
3.388 ± 0.174 Mbit/s, while mean per-run p95 delay changes from
484.4 ± 139.5 to 417.8 ± 37.3 ms. The outcome intervals overlap; the
walkthrough therefore treats the mechanism difference as observed but the
end-to-end ordering as inconclusive.

<!-- BEGIN GENERATED: ieee80211-scalar-vector-uora -->
### [script] Generated scalar/vector plot and table

![uora scalar/vector analysis](results/20260730T202045Z/uora-dashboard.png)

Figure provenance: [`results/20260730T202045Z/uora-dashboard.png.json`](results/20260730T202045Z/uora-dashboard.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / **.server.app[*] / packetReceived:vector(packetBytes)<br>vector / **.server.app[*] / endToEndDelay:vector / unit=s<br>scalar / heUlRandomAccessAttempt:count<br>scalar / heUlRandomAccessSuccess:count
- Window / per-run aggregation / exclusions: [0.3, 0.95) s; delay=pool delivered-packet delays within each run over the manifest measurement window, then take the 95th percentile; one value per run; goodput=sum delivered application bytes over the manifest measurement window across sink vectors, convert to bit/s; one value per run; mechanism=terminal full-simulation scalar counters summed across stations; one value per run; uncertainty=95% Student-t CI; zero_success_runs={'MixedUora': 0, 'ScheduledOnly': 5, 'UoraHeavyContention': 1, 'UoraLightContention': 1, 'UoraMoreRandomAccessRus': 0}
- Independent runs: run-level summaries: n=4, 5; direct observations: no independent-run estimate

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| Heavy, 1 RA-RU / attempts | 1698.4 | 201.763 |
| Heavy, 1 RA-RU / delay p95 ms | 382.999 | 75.5114 |
| Heavy, 1 RA-RU / goodput mbps | 4.24468 | 0.340867 |
| Heavy, 1 RA-RU / success fairness | 0.14375 | 0.0596709 |
| Heavy, 1 RA-RU / success probability | 0.00132265 | 0.00241829 |
| Heavy, 1 RA-RU / successful transmissions | 2.2 | 4.06159 |
| Heavy, 1 RA-RU / zero success run count | 1 | — |
| Heavy, 5 RA-RUs / attempts | 1750 | 747.397 |
| Heavy, 5 RA-RUs / delay p95 ms | 303.036 | 101.228 |
| Heavy, 5 RA-RUs / goodput mbps | 2.87975 | 0.732149 |
| Heavy, 5 RA-RUs / success fairness | 0.919229 | 0.214877 |
| Heavy, 5 RA-RUs / success probability | 0.341364 | 0.0332496 |
| Heavy, 5 RA-RUs / successful transmissions | 585 | 225.64 |
| Heavy, 5 RA-RUs / zero success run count | 0 | — |
| Light, 1 RA-RU / attempts | 1510 | 161.476 |
| Light, 1 RA-RU / delay p95 ms | 397.469 | 87.6146 |
| Light, 1 RA-RU / goodput mbps | 2.20554 | 0.172308 |
| Light, 1 RA-RU / success fairness | 0.218269 | 0.0608685 |
| Light, 1 RA-RU / success probability | 0.00179673 | 0.00182641 |
| Light, 1 RA-RU / successful transmissions | 2.8 | 2.83143 |
| Light, 1 RA-RU / zero success run count | 1 | — |
| Mixed, adaptive 1–3 RA-RUs / attempts | 1740.2 | 525.363 |
| Mixed, adaptive 1–3 RA-RUs / delay p95 ms | 404.389 | 70.2549 |
| Mixed, adaptive 1–3 RA-RUs / goodput mbps | 4.11963 | 0.929055 |
| Mixed, adaptive 1–3 RA-RUs / success fairness | 0.327296 | 0.319148 |
| Mixed, adaptive 1–3 RA-RUs / success probability | 0.010627 | 0.0247729 |
| Mixed, adaptive 1–3 RA-RUs / successful transmissions | 11.8 | 23.8257 |
| Mixed, adaptive 1–3 RA-RUs / zero success run count | 0 | — |
| Scheduled only, 0 RA-RUs / attempts | 0 | 0 |
| Scheduled only, 0 RA-RUs / delay p95 ms | 177.676 | 72.6611 |
| Scheduled only, 0 RA-RUs / goodput mbps | 3.50129 | 0.351889 |
| Scheduled only, 0 RA-RUs / successful transmissions | 0 | 0 |
| Scheduled only, 0 RA-RUs / zero success run count | 5 | — |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **INCONCLUSIVE** | Random-access attempts use advertised RA-RUs | The retained results do not correlate each random-access attempt with the Trigger frame that advertised its RA-RU. |
<!-- END GENERATED: ieee80211-scalar-vector-uora -->

## [agent] PCAP statistics

The five run-0 captures observe transmitted frames at `ap.wlan[0]`. They are
PCAPng files with radiotap plus IEEE 802.11 encapsulation, recorded at the AP
MAC observation point with computed checksums/FCS. Counts are capture
observations, not de-duplicated medium transmissions, successful receptions,
UORA decisions, or application deliveries.

For a reproducible Basic-Trigger selection:

```sh
tshark -n \
  -r 'examples/ieee80211ax/ul_uora/results/20260727T202540Z/UoraHeavyContention/UoraHeavyContention-#0Lan80211AxUlOfdma.ap.wlan[0].pcap' \
  -Y 'wlan.trigger.he.trigger_type == 0' \
  -T fields -e frame.number -e frame.time_epoch \
  -e wlan.trigger.he.trigger_type -e wlan.trigger.he.user_info.aid12
```

The generated compact table and count-versus-airtime plot below compare
overall packet composition. Airtime is estimated from decoded PHY metadata;
parallel multi-user observations are summed, so it is not a union of channel
busy time. Fields that radiotap/TShark does not mark authoritative remain
unknown.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260730T202045Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260730T202045Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260730T202045Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260730T202045Z.json` (SHA-256 `bc1bcac98e3d3e8bfbf026d8a3c455a963f60bae5433ea02f445419622701b5f`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `ScheduledOnly` | `none (all decoded frames)` | 3317 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (2538), Data: QoS Null [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] (205), Control: Block Ack (BA) (194) | 47.96% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `MixedUora` | `none (all decoded frames)` | 7254 | Data: QoS Data [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (3852), Data: QoS Data [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, BCC, A-MPDU] (1944), Control: Block Ack (BA) (474) | 279.34% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UoraLightContention` | `none (all decoded frames)` | 5064 | Data: QoS Data [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, BCC, A-MPDU] (1678), Data: QoS Data [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (1038), Data: QoS Data [HE-TB, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] (715) | 229.11% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UoraHeavyContention` | `none (all decoded frames)` | 6844 | Data: QoS Data [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (2976), Data: QoS Data [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, BCC, A-MPDU] (1515), Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (1205) | 228.58% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `UoraMoreRandomAccessRus` | `none (all decoded frames)` | 5311 | Data: QoS Data [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, BCC, A-MPDU] (2035), Data: QoS Data [HE-TB, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] (2028), Control: Block Ack (BA) (463) | 296.16% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | MixedUora produced protocol-visible wireless observations | 7254 AP/global transmission observations |
| **PASS** | ScheduledOnly produced protocol-visible wireless observations | 3317 AP/global transmission observations |
| **PASS** | UoraHeavyContention produced protocol-visible wireless observations | 6844 AP/global transmission observations |
| **PASS** | UoraLightContention produced protocol-visible wireless observations | 5064 AP/global transmission observations |
| **PASS** | UoraMoreRandomAccessRus produced protocol-visible wireless observations | 5311 AP/global transmission observations |

### [script] Configuration: `ScheduledOnly`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **3317**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 2538 | 76.51% | 266.3 B | 1.0 B | 147.3 us | 8.0 us | 5010 MHz | -58.1 dBm | - | 77.96% | 37.39% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 30 | 0.90% | 270.0 B | 0.0 B | 183.7 us | 0.0 us | 5010 MHz | -56.2 dBm | - | 1.15% | 0.55% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24a331" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 34 | 1.03% | 270.0 B | 0.0 B | 756.0 us | 0.0 us | 5017 MHz | -75.0 dBm | - | 5.36% | 2.57% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 16 | 0.48% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz, 5008 MHz, 5010 MHz, 5012 MHz, 5014 MHz, 5016 MHz | -75.0 dBm | - | 1.33% | 0.64% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#07400c" /></svg> | Data: QoS Null [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 205 | 6.18% | 34.0 B | 0.0 B | 217.3 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz, 5008 MHz, 5010 MHz, 5012 MHz, 5014 MHz | -75.0 dBm | - | 9.29% | 4.46% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d4512" /></svg> | Data: QoS Null [HE-TB, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 6 | 0.18% | 34.0 B | 0.0 B | 126.7 us | 0.0 us | 5013 MHz, 5017 MHz | -75.0 dBm | - | 0.16% | 0.08% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 129 | 3.89% | 75.0 B | 4.2 B | 45.0 us | 1.4 us | 5010 MHz | - | 10.0 dBm | 1.21% | 0.58% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 107 | 3.23% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -58.1 dBm | - | 0.62% | 0.30% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#8d5134" /></svg> | Control: Block Ack Request (BAR) [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 17 | 0.51% | 24.0 B | 0.0 B | 164.0 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz, 5008 MHz, 5010 MHz, 5012 MHz | -75.0 dBm | - | 0.58% | 0.28% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 194 | 5.85% | 87.9 B | 40.2 B | 49.3 us | 13.4 us | 5010 MHz | - | 10.0 dBm | 1.99% | 0.96% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 16 | 0.48% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -59.2 dBm | 10.0 dBm | 0.08% | 0.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 8 | 0.24% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.04% | 0.02% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 8 | 0.24% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -59.2 dBm | - | 0.12% | 0.06% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | Management: Action [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 9 | 0.27% | 37.0 B | 0.0 B | 56.2 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.11% | 0.05% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.003064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=451 | Responds without MAC payload while preserving QoS control information. |
| 3 | 0.003064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=457 | Responds without MAC payload while preserving QoS control information. |
| 4 | 0.003064000 | 0a:aa:00:00:00:04 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=463 | Responds without MAC payload while preserving QoS control information. |
| 5 | 0.003064000 | 0a:aa:00:00:00:08 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=469 | Responds without MAC payload while preserving QoS control information. |
| 6 | 0.003064000 | 0a:aa:00:00:00:07 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=475 | Responds without MAC payload while preserving QoS control information. |
| 7 | 0.003064000 | 0a:aa:00:00:00:06 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=481 | Responds without MAC payload while preserving QoS control information. |
| 8 | 0.003064000 | 0a:aa:00:00:00:05 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=487 | Responds without MAC payload while preserving QoS control information. |
| 9 | 0.003064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=493 | Responds without MAC payload while preserving QoS control information. |
| 10 | 0.003153000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 11 | 0.104048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 12 | 0.106064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1173 | Responds without MAC payload while preserving QoS control information. |
| 13 | 0.106064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1179 | Responds without MAC payload while preserving QoS control information. |
| 14 | 0.106064000 | 0a:aa:00:00:00:04 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1185 | Responds without MAC payload while preserving QoS control information. |
| 15 | 0.106064000 | 0a:aa:00:00:00:08 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1191 | Responds without MAC payload while preserving QoS control information. |
| 16 | 0.106064000 | 0a:aa:00:00:00:07 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=1197 | Responds without MAC payload while preserving QoS control information. |

Frame numbers are local to capture `ScheduledOnly-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `MixedUora`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **7254**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 280 | 3.86% | 167.7 B | 2.0 B | 97.4 us | 13.7 us | 5010 MHz | -56.0 dBm | - | 0.98% | 2.73% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 33 | 0.45% | 206.4 B | 48.1 B | 148.9 us | 26.3 us | 5010 MHz | -57.0 dBm | - | 0.18% | 0.49% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39c224" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 3852 | 53.10% | 167.7 B | 2.0 B | 216.4 us | 14.9 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 29.85% | 83.37% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28af31" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 1944 | 26.80% | 170.0 B | 0.0 B | 942.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5010 MHz | -75.0 dBm | - | 65.60% | 183.25% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#18a523" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 4 | 0.06% | 170.0 B | 0.0 B | 942.7 us | 0.0 us | 5004 MHz, 5006 MHz, 5008 MHz, 5010 MHz | -75.0 dBm | - | 0.13% | 0.38% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 137 | 1.89% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 1.96% | 5.46% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1a620f" /></svg> | Data: QoS Null [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 3 | 0.04% | 34.0 B | 0.0 B | 78.7 us | 0.0 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 0.01% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#07400c" /></svg> | Data: QoS Null [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 2 | 0.03% | 34.0 B | 0.0 B | 217.3 us | 0.0 us | 5004 MHz, 5008 MHz | -75.0 dBm | - | 0.02% | 0.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d4512" /></svg> | Data: QoS Null [HE-TB, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 1 | 0.01% | 34.0 B | 0.0 B | 126.7 us | 0.0 us | 5007 MHz | -75.0 dBm | - | 0.00% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 453 | 6.24% | 50.1 B | 9.7 B | 36.7 us | 3.2 us | 5010 MHz | - | 10.0 dBm | 0.60% | 1.66% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 31 | 0.43% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -56.8 dBm | - | 0.03% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 474 | 6.53% | 45.4 B | 3.0 B | 35.1 us | 1.0 us | 5010 MHz | - | 10.0 dBm | 0.60% | 1.67% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 16 | 0.22% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -59.2 dBm | 10.0 dBm | 0.01% | 0.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 8 | 0.11% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.01% | 0.02% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 8 | 0.11% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -59.2 dBm | - | 0.02% | 0.06% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | Management: Action [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 8 | 0.11% | 37.0 B | 0.0 B | 56.2 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.02% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.003064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=451 | Responds without MAC payload while preserving QoS control information. |
| 3 | 0.003064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=457 | Responds without MAC payload while preserving QoS control information. |
| 4 | 0.003129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 5 | 0.004048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 6 | 0.006064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=705 | Responds without MAC payload while preserving QoS control information. |
| 7 | 0.006064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=711 | Responds without MAC payload while preserving QoS control information. |
| 8 | 0.006129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 9 | 0.007048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 10 | 0.009064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=959 | Responds without MAC payload while preserving QoS control information. |
| 11 | 0.009064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=965 | Responds without MAC payload while preserving QoS control information. |
| 12 | 0.009129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 13 | 0.010048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 14 | 0.012064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1213 | Responds without MAC payload while preserving QoS control information. |
| 15 | 0.012064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1219 | Responds without MAC payload while preserving QoS control information. |
| 16 | 0.012129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |

Frame numbers are local to capture `MixedUora-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `UoraLightContention`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **5064**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 162 | 3.20% | 168.5 B | 1.9 B | 97.1 us | 12.7 us | 5010 MHz | -59.6 dBm | - | 0.69% | 1.57% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 186 | 3.67% | 176.5 B | 24.6 B | 132.5 us | 13.4 us | 5010 MHz | -61.6 dBm | - | 1.08% | 2.46% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39c224" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 1038 | 20.50% | 169.1 B | 1.7 B | 223.4 us | 17.5 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 10.12% | 23.18% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28af31" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 1678 | 33.14% | 170.0 B | 0.0 B | 942.7 us | 0.0 us | 5002 MHz, 5010 MHz | -75.0 dBm | - | 69.04% | 158.18% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#18a523" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 2 | 0.04% | 170.0 B | 0.0 B | 942.7 us | 0.0 us | 5004 MHz | -75.0 dBm | - | 0.08% | 0.19% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24a331" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 715 | 14.12% | 168.7 B | 1.9 B | 461.9 us | 20.0 us | 5007 MHz, 5013 MHz | -75.0 dBm | - | 14.42% | 33.03% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 137 | 2.71% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 2.38% | 5.46% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1a620f" /></svg> | Data: QoS Null [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 39 | 0.77% | 34.0 B | 0.0 B | 78.7 us | 0.0 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 0.13% | 0.31% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#07400c" /></svg> | Data: QoS Null [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 25 | 0.49% | 34.0 B | 0.0 B | 217.3 us | 0.0 us | 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.24% | 0.54% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d4512" /></svg> | Data: QoS Null [HE-TB, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 36 | 0.71% | 34.0 B | 0.0 B | 126.7 us | 0.0 us | 5007 MHz, 5013 MHz | -75.0 dBm | - | 0.20% | 0.46% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 459 | 9.06% | 50.0 B | 9.6 B | 36.7 us | 3.2 us | 5010 MHz | - | 10.0 dBm | 0.73% | 1.68% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 48 | 0.95% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -60.7 dBm | - | 0.06% | 0.13% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 499 | 9.85% | 44.9 B | 4.0 B | 35.0 us | 1.3 us | 5010 MHz | - | 10.0 dBm | 0.76% | 1.75% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 16 | 0.32% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -59.2 dBm | 10.0 dBm | 0.02% | 0.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 8 | 0.16% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.01% | 0.02% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 8 | 0.16% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -59.2 dBm | - | 0.02% | 0.06% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | Management: Action [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 8 | 0.16% | 37.0 B | 0.0 B | 56.2 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.02% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.003064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=451 | Responds without MAC payload while preserving QoS control information. |
| 3 | 0.003064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=457 | Responds without MAC payload while preserving QoS control information. |
| 4 | 0.003129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 5 | 0.004048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 6 | 0.006064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=705 | Responds without MAC payload while preserving QoS control information. |
| 7 | 0.006064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=711 | Responds without MAC payload while preserving QoS control information. |
| 8 | 0.006129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 9 | 0.007048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 10 | 0.009064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=959 | Responds without MAC payload while preserving QoS control information. |
| 11 | 0.009064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=965 | Responds without MAC payload while preserving QoS control information. |
| 12 | 0.009129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 13 | 0.010048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 14 | 0.012064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1213 | Responds without MAC payload while preserving QoS control information. |
| 15 | 0.012064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1219 | Responds without MAC payload while preserving QoS control information. |
| 16 | 0.012129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |

Frame numbers are local to capture `UoraLightContention-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `UoraHeavyContention`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **6844**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 1205 | 17.61% | 166.9 B | 1.7 B | 93.8 us | 9.7 us | 5010 MHz | -52.8 dBm | - | 4.95% | 11.31% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 42 | 0.61% | 198.6 B | 45.2 B | 144.6 us | 24.7 us | 5010 MHz | -54.0 dBm | - | 0.27% | 0.61% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#39c224" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 2976 | 43.48% | 167.5 B | 1.9 B | 216.3 us | 14.9 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 28.16% | 64.36% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28af31" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 1515 | 22.14% | 170.0 B | 0.0 B | 942.7 us | 0.0 us | 5002 MHz, 5010 MHz | -75.0 dBm | - | 62.48% | 142.81% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#18a523" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 3 | 0.04% | 170.0 B | 0.0 B | 942.7 us | 0.0 us | 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.12% | 0.28% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24a331" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 1 | 0.01% | 170.0 B | 0.0 B | 489.3 us | 0.0 us | 5007 MHz | -75.0 dBm | - | 0.02% | 0.05% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 137 | 2.00% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 2.39% | 5.46% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#1a620f" /></svg> | Data: QoS Null [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 1 | 0.01% | 34.0 B | 0.0 B | 78.7 us | 0.0 us | 5015 MHz | -75.0 dBm | - | 0.00% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#105114" /></svg> | Data: QoS Null [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 2 | 0.03% | 34.0 B | 0.0 B | 217.3 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 0.02% | 0.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#07400c" /></svg> | Data: QoS Null [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 10 | 0.15% | 34.0 B | 0.0 B | 217.3 us | 0.0 us | 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.10% | 0.22% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 409 | 5.98% | 50.5 B | 10.1 B | 36.8 us | 3.4 us | 5010 MHz | - | 10.0 dBm | 0.66% | 1.51% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 61 | 0.89% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -52.7 dBm | - | 0.07% | 0.17% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#c1773e" /></svg> | Control: Block Ack Request (BAR) [HE-TB, HE-MCS 1, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 2 | 0.03% | 24.0 B | 0.0 B | 66.1 us | 0.0 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#946938" /></svg> | Control: Block Ack Request (BAR) [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 3 | 0.04% | 24.0 B | 0.0 B | 164.0 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 0.02% | 0.05% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 437 | 6.39% | 45.1 B | 3.5 B | 35.0 us | 1.2 us | 5010 MHz | - | 10.0 dBm | 0.67% | 1.53% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 16 | 0.23% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -59.2 dBm | 10.0 dBm | 0.02% | 0.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 8 | 0.12% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.01% | 0.02% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 8 | 0.12% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -59.2 dBm | - | 0.02% | 0.06% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | Management: Action [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 8 | 0.12% | 37.0 B | 0.0 B | 56.2 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.02% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.003064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=451 | Responds without MAC payload while preserving QoS control information. |
| 3 | 0.003064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=457 | Responds without MAC payload while preserving QoS control information. |
| 4 | 0.003129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 5 | 0.004048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 6 | 0.006064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=705 | Responds without MAC payload while preserving QoS control information. |
| 7 | 0.006064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=711 | Responds without MAC payload while preserving QoS control information. |
| 8 | 0.006129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 9 | 0.007048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 10 | 0.009064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=959 | Responds without MAC payload while preserving QoS control information. |
| 11 | 0.009064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=965 | Responds without MAC payload while preserving QoS control information. |
| 12 | 0.009129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 13 | 0.010048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 14 | 0.012064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1213 | Responds without MAC payload while preserving QoS control information. |
| 15 | 0.012064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1219 | Responds without MAC payload while preserving QoS control information. |
| 16 | 0.012129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |

Frame numbers are local to capture `UoraHeavyContention-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Configuration: `UoraMoreRandomAccessRus`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **5311**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f5dc00" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 106 | 2.00% | 167.3 B | 1.9 B | 95.6 us | 12.0 us | 5010 MHz | -62.2 dBm | - | 0.34% | 1.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 30 | 0.56% | 210.0 B | 49.0 B | 150.9 us | 26.8 us | 5010 MHz | -58.1 dBm | - | 0.15% | 0.45% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#28af31" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 2035 | 38.32% | 170.0 B | 0.0 B | 942.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz, 5008 MHz, 5010 MHz | -75.0 dBm | - | 64.77% | 191.83% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#18a523" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 1 | 0.02% | 170.0 B | 0.0 B | 942.7 us | 0.0 us | 5012 MHz | -75.0 dBm | - | 0.03% | 0.09% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24a331" /></svg> | Data: QoS Data [HE-TB, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 2028 | 38.18% | 167.8 B | 2.0 B | 459.5 us | 21.4 us | 5013 MHz, 5017 MHz | -75.0 dBm | - | 31.47% | 93.19% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 137 | 2.58% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 1.84% | 5.46% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#07400c" /></svg> | Data: QoS Null [HE-TB, HE-MCS 1, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 1 | 0.02% | 34.0 B | 0.0 B | 217.3 us | 0.0 us | 5012 MHz | -75.0 dBm | - | 0.01% | 0.02% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0d4512" /></svg> | Data: QoS Null [HE-TB, HE-MCS 1, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 1 | 0.02% | 34.0 B | 0.0 B | 126.7 us | 0.0 us | 5013 MHz | -75.0 dBm | - | 0.00% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 458 | 8.62% | 70.4 B | 1.1 B | 43.5 us | 0.4 us | 5010 MHz | - | 10.0 dBm | 0.67% | 1.99% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 11 | 0.21% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -56.6 dBm | - | 0.01% | 0.03% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 463 | 8.72% | 62.9 B | 15.7 B | 41.0 us | 5.2 us | 5010 MHz | - | 10.0 dBm | 0.64% | 1.90% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 16 | 0.30% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -59.2 dBm | 10.0 dBm | 0.01% | 0.04% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 8 | 0.15% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.01% | 0.02% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 8 | 0.15% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -59.2 dBm | - | 0.02% | 0.06% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f3e816" /></svg> | Management: Action [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 8 | 0.15% | 37.0 B | 0.0 B | 56.2 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.02% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.003064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=451 | Responds without MAC payload while preserving QoS control information. |
| 3 | 0.003064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=457 | Responds without MAC payload while preserving QoS control information. |
| 4 | 0.003129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 5 | 0.004048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 6 | 0.006064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=705 | Responds without MAC payload while preserving QoS control information. |
| 7 | 0.006064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=711 | Responds without MAC payload while preserving QoS control information. |
| 8 | 0.006129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 9 | 0.007048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 10 | 0.009064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=959 | Responds without MAC payload while preserving QoS control information. |
| 11 | 0.009064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=965 | Responds without MAC payload while preserving QoS control information. |
| 12 | 0.009129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 13 | 0.010048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 14 | 0.012064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1213 | Responds without MAC payload while preserving QoS control information. |
| 15 | 0.012064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=3, frag=0, more-frag=0, TID=0, A-MPDU=1219 | Responds without MAC payload while preserving QoS control information. |
| 16 | 0.012129000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |

Frame numbers are local to capture `UoraMoreRandomAccessRus-#0Lan80211AxUlOfdma.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

### [script] Analysis of Packet Distribution
Across these configurations, **QoS Data** frames constitute the primary payload delivery mechanism, while **Block Ack (BA)** and **Block Ack Request (BAR)** control frames ensure reliable transport via the MAC-level acknowledgment protocol. Management frames, specifically **Beacons**, are transmitted periodically by the Access Point to maintain BSS time synchronization and broadcast network capabilities. The ratio of control/management overhead to actual data frames indicates the relative MAC efficiency of the chosen configurations.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->

## [agent] Frame exchange analysis

The generated per-configuration timelines provide frame number, simulation
timestamp, transmitter/receiver, PHY form, and decisive decoded fields. A
representative mechanism sequence is:

| Frame step | Simulation ordering | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | first | AP → associated stations | Basic Trigger | Trigger Type 0; RA-RU User Info | advertises uplink opportunities |
| 2 | after SIFS | selected station(s) → AP | QoS data/null in HE-TB | HE-TB format, RU, MCS/NSS when known | carries a solicited uplink response |
| 3 | after SIFS | AP → station(s) | Block Ack or other allowed response | recipient/acknowledgment fields | terminates the modeled exchange when correlated |

The following feature-specific comparison uses the AP captures and this
selection:

```sh
tshark -n -r CAPTURE \
  -Y 'wlan.trigger.he.trigger_type == 0 && wlan.trigger.he.user_info.aid12 == 0' \
  -T fields -E occurrence=a -E separator='|' \
  -e frame.number -e frame.time_epoch -e wlan.ta -e wlan.ra \
  -e wlan.trigger.he.trigger_type -e wlan.trigger.he.user_info.aid12 \
  -e wlan.trigger.he.ru_allocation -e wlan.trigger.he.ru_number_of_ra_ru
```

| Configuration / frame | Simulation time | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| Heavy, 1 RA-RU / 620 | 0.301531 s | AP → broadcast | Basic Trigger | `AID12=5,6,0`; RU Allocation `0,1,2`; Number Of RA-RU `0` | two dedicated allocations plus one one-RU associated-STA random-access allocation |
| Heavy, 1 RA-RU / 621-624 | 0.302547 s | four station TAs → AP | simultaneous HE-TB QoS responses | four distinct TAs; decoded HE-TB profile | protocol-visible responses to the Trigger |
| Heavy, 1 RA-RU / 625 | 0.302612 s | AP → broadcast | Block Ack | subtype `0x0019` | immediate response after the HE-TB transmissions |
| Heavy, 5 RA-RUs / 620 | 0.301611 s | AP → broadcast | Basic Trigger | `AID12=5,6,0,0,0,0,0`; RU Allocation `0..6`; each Number Of RA-RU `0` | two dedicated allocations plus five one-RU random-access allocations |
| Heavy, 5 RA-RUs / 621-627 | 0.302627 s | seven station TAs → AP | simultaneous HE-TB QoS responses | seven distinct TAs; decoded HE-TB profile | protocol-visible responses to the Trigger |
| Heavy, 5 RA-RUs / 628 | 0.302696 s | AP → broadcast | Block Ack | subtype `0x0019` | immediate response after the HE-TB transmissions |

The Number Of RA-RU field encodes count minus one, so raw `0` means one
RA-RU per `AID12=0` entry. The one-versus-five advertised allocation is a
direct packet observation. More transmitters respond than there are dedicated
AID entries, which is consistent with use of the advertised RA-RUs, but the
capture does not expose a stable station-decision identifier that joins each
response to the corresponding modeled attempt. The executable evidence
contract therefore remains `INCONCLUSIVE` for the stronger per-attempt claim.

Frame numbers in the generated tables are capture-local and are not OMNeT++
event numbers. The generic timeline proves the Trigger/HE-TB/response
structure. The modeled UORA counter is still the authoritative observation
for whether a station classified an attempt or success; co-timing alone does
not establish a collision.

## [agent] Cross-layer findings and verdict

| Claim | Verdict | Configuration evidence | Model telemetry | Packet evidence | Outcome evidence |
|---|---|---|---|---|---|
| Scheduled-only disables UORA | `PASS` | fixed 0 RA-RUs | zero attempts/successes | scheduled wireless exchange remains visible | server delivery is retained |
| UORA executes in enabled cases | `PASS` | 1-3, 1, or 5 RA-RUs | nonzero attempts and successes | Trigger and HE-TB observations | goodput/delay are measured |
| One versus five RA-RUs is a matched heavy comparison | `PASS` | only fixed RA-RU count changes | attempt/success/fairness summaries differ | run-0 packet composition differs | paired five-run estimates |
| Five RA-RUs universally improve performance | `INCONCLUSIVE` | one policy/topology/load | only five seeds | one representative capture per condition | confidence intervals and scope limit generalization |

Configuration, scalar/vector results, and PCAP evidence share the same logical
session, configuration, run number, and seed policy. Only the scheduled-only
and mixed run-0 artifacts came from the uninterrupted combined jobs. The
light and both heavy run-0 result/capture families were separately regenerated
with matched inputs, so their timestamps do not establish event-level
causality or exact frame-to-counter agreement. The aggregate dashboard also
summarizes five independent runs and must not be read as an event join.

Evidence basis: effective assignments are **configuration input**; decoded
Trigger/HE-TB fields and recorded counters are **direct observations**;
goodput, p95 delay, confidence intervals, and fairness are **derived
measurements**; explanations that connect scheduling capacity to outcomes are
**inferences** unless a same-run identifier joins the events.

## [agent] Limitations and inconclusive claims

- The AP capture does not expose each station's internal OBO value or the
  precise reason for a failed modeled UORA attempt.
- The three eight-station run-0 result/capture families were separately
  regenerated after interruption; they cannot support event-level joins.
- Overall HE-TB counts include scheduled and buffer-status traffic; they are
  not a direct count of UORA attempts.
- The packet decoder cannot be used to fill unknown PHY fields from INI
  defaults.
- Five paired seeds in one stationary topology support a bounded simulation
  comparison, not a population-level or real-deployment claim.
- A decisive collision study needs correlated Trigger identity, advertised RU,
  station selection, AP reception outcome, and terminal UORA result.

## [agent] Further experiments

- Sweep fixed RA-RU count from 0 through the scheduler's valid maximum while
  holding the heavy workload fixed; predict increasing access capacity but
  decreasing scheduled-RU capacity.
- Sweep `muCwMin`/`muCwMax` at fixed load and RA-RU count; inspect attempts,
  successes, fairness, and per-run p95 delay.
- Add AP and station capture points for one seed and correlate a selected
  Trigger with coordinator logs to turn the collision claim into a directly
  testable invariant.
- Extend the paired seed set before making a stronger outcome-ordering claim.

## [agent] Implementation plan

The walkthrough exposes an observability gap rather than a demonstrated
protocol defect. No production-code change is proposed here. A future change
could add a stable Trigger/opportunity identifier to correlate advertised
RA-RUs, station OBO/selection, AP reception, and terminal UORA outcome.
Before planning or editing files under `src/inet`, that work must apply the
architectural requirements and sealing checks, define a focused
control/treatment regression, and obtain any required permission.

## [agent] Artifact provenance

| Artifact family | Session/path | Configurations/runs | Tool/filter/window | Integrity notes |
|---|---|---|---|---|
| Scalar/vector | `results/20260727T202540Z` | all five configs; runs/seeds 0-4 | OMNeT++ 6.4 native result API; server receive/delay vectors; coordinator counters; `[0.3,2.0)` outcomes | 25 validated `.sca`/`.vec` pairs; figure sidecar binds hashes and aggregation |
| PCAP | `results/20260727T202540Z` | all five configs; run/seed 0 | shared HE decode profile; AP MAC point; TShark/capinfos 4.6.4 | five validated captures; manifest SHA-256 `1c265f…bcfbd6`; separate recovery sessions disclosed above |
| Executable evidence ledger | [`../../ieee80211/analysis/generated/sessions/20260727T202540Z/evidence-ledger.json`](../../ieee80211/analysis/generated/sessions/20260727T202540Z/evidence-ledger.json) | group `uora` | attempt/Trigger evidence contract | `INCONCLUSIVE`: retained results do not join an attempt to its advertising Trigger |
| Analysis descriptors | [`../analysis/experiments.json`](../analysis/experiments.json), [`../../ieee80211/analysis/suites/ax.json`](../../ieee80211/analysis/suites/ax.json) | group `uora`, scenario `ul_uora` | five-run policy and AP capture pattern | declarative inputs to shared campaign/report/publish workflow |
