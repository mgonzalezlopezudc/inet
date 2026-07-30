# Walkthrough: HE Buffer Status Reports

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `20260730T166000Z`
- PCAP: `20260730T166000Z`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `20260730T166000Z`.

This example tests AP-side BSR accounting across implicit reporting, default
fresh reporting, bursty traffic, and a short report-age limit. The result is a
bounded comparison of four configurations, five runs each, and is not a claim
about BSR performance in general.

## [agent] Learning objectives and feature primer

BSR is queue information sent by a non-AP station to help its AP decide how
much uplink capacity to grant. The AP schedules from the report state it has
received, which may differ from the station's current queue. The learning
question is whether the reported bytes and the bytes selected for a trigger
remain correlated when reports arrive implicitly and under fresh and
stale-report treatments.

The decisive evidence is the AP-side reported/planned-byte pair keyed by
`heUlTriggerDecisionId`. A report may exceed the bytes planned for one trigger
because the scheduler is allocating one transmission opportunity, not copying
the entire queue. BSR is scheduling state, not application goodput.

## [agent] Scenario description

[`HeBsrNetwork`](HeBsrNetwork.ned) contains one HE AP and three stationary
stations. The [configuration](omnetpp.ini) selects 5 GHz, 20 MHz HE operation,
QoS stations, `HeHcf`, and the AP-side `HeUlSchedulerBacklogBased` scheduler.
Each station offers 700-byte UDP packets every 2 ms during the active periods.
The warm-up traffic runs before the measurement window and is excluded from
the burst comparison.

`ImplicitBsr` uses the common saturated traffic but checks for AP-triggered
uplink exchanges every 0.5 s, allowing ordinary uplink QoS Data to provide
implicit reports first. `FreshBsr` makes no changes to the `General`
configuration. `BurstyTraffic` uses active periods 0.3–0.5 s and 0.65–0.95 s.
`StaleBsr` keeps the topology and offered traffic but sets `reportMaxAge =
10ms`. The analysis window is 0.3–0.95 s.

## [agent] Standards and INET model boundary

IEEE Std 802.11-2024 Clause 26.5.5 describes buffer status reporting. The
standard identifies BSRP as Trigger Type 4. The decoded Trigger table shows
the trigger types observed in the representative AP capture, but the fields
and the standard do not by themselves prove that the AP used a report
correctly.

INET exposes the mechanism through AP-side report/planning telemetry and an HE
uplink scheduler. The INI is requested behavior; the generated vectors are
derived measurements; and the AP PCAP directly observes the surrounding MAC
exchange. QoS Data counts alone do not prove BSR freshness or delivery of the
reported bytes.

## [agent] Evidence status

| Claim | Status | Script-generated evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| Reported backlog is joined to scheduler-consumed bytes by trigger decision. | `PASS` | Published scalar/vector executable check and joined decision table | Five runs, seeds 0–4, `FreshBsr`, `BurstyTraffic`, and `StaleBsr` | The join applies to configurations that produce scheduled decisions. |
| Implicit BSR produces AP-visible report telemetry. | `PASS` | Published scalar/vector plot and PCAP evidence | Five runs, seeds 0–4, `ImplicitBsr` | Its slow trigger-check interval produces no scheduled-byte trace in representative run 0. |
| The captured AP-MAC exchange contains the relevant HE trigger/data activity. | `PASS` | Published PCAP statistics, decoded trigger-type table, and frame exchange | Representative run 0 of all four configurations | BSR is an A-Control scheduling input, not a standalone frame subtype. |

## [agent] Configuration matrix

| Configuration | Role | Causal delta | Runs/seeds | Expected invariant |
|---|---|---|---|---|
| `ImplicitBsr` | Implicit-report treatment | `ulTriggerCheckInterval = 0.5s` | 0–4 | AP report telemetry is present; scheduled bytes may be absent when no trigger is generated. |
| `FreshBsr` | Fresh-report treatment | No assignment beyond `General` | 0–4 | Trigger decisions contain aligned reported and planned bytes. |
| `BurstyTraffic` | Bursty workload | Two ON/OFF traffic bursts | 0–4 | Trigger decisions contain aligned reported and planned bytes under bursty load. |
| `StaleBsr` | Freshness control | `reportMaxAge = 10ms` | 0–4 | Trigger decisions remain aligned while the AP applies the shorter freshness window. |

## [agent] Expected invariants and diagnostic map

| Invariant | Script-generated evidence | Failure symptom | First diagnostic |
|---|---|---|---|
| Each retained trigger decision has correlated report and plan values. | Joined decision evidence | Missing, misaligned, or non-finite decision fields | Check the `heUlTriggerDecision*` vectors at the AP coordinator. |
| The traffic bursts are present in the analysis window. | Reported-backlog plot and AP telemetry | No report activity during an ON period | Check application start/stop times and `heUlBufferStatusReportedBytes`. |

## [agent] Reproduction

Run from the INET repository root:

```sh
python3 examples/ieee80211/analysis/wifi_analysis.py inspect bsr
MPLCONFIGDIR=/tmp/matplotlib python3 examples/ieee80211/analysis/wifi_analysis.py run bsr \
  --evidence both --runs 5 --session-id 20260730T166000Z
MPLCONFIGDIR=/tmp/matplotlib python3 examples/ieee80211/analysis/wifi_analysis.py report bsr \
  --session-id 20260730T166000Z
MPLCONFIGDIR=/tmp/matplotlib python3 examples/ieee80211/analysis/wifi_analysis.py publish bsr \
  --session-id 20260730T166000Z --update
```

The retained session is `20260730T166000Z`: five runs for each
configuration, with trigger-decision vectors for the scheduled conditions and
an AP-MAC PCAP on representative run 0. Results are under
`examples/ieee80211ax/bsr/results/20260730T166000Z`.
The retained result set includes OMNeT++ `.sca` and `.vec` artifacts, for
example `results/20260730T166000Z/ImplicitBsr/ImplicitBsr-#0.sca` and
`results/20260730T166000Z/ImplicitBsr/ImplicitBsr-#0.vec`.

## [agent] Scalar and vector analysis

<!-- BEGIN GENERATED ANALYSIS: scalar-vector -->
<!-- END GENERATED ANALYSIS: scalar-vector -->

The generated comparison answers two bounded questions: what report backlog
the AP observes in each condition, and whether reported and planned bytes are
aligned for retained scheduled decisions. It does not measure application
goodput or establish a universal benefit for a 10 ms freshness window.

<!-- BEGIN GENERATED: ieee80211-scalar-vector-bsr -->
### [script] Generated scalar/vector plot and table

![bsr scalar/vector analysis](results/20260730T166000Z/bsr-reported-vs-scheduled.png)

Figure provenance: [`results/20260730T166000Z/bsr-reported-vs-scheduled.png.json`](results/20260730T166000Z/bsr-reported-vs-scheduled.png.json). Run-level metric source: [`../analysis/metrics.json`](../analysis/metrics.json).

Common table provenance:

- Source result filters / modules / units: vector / heUlBufferStatusReportedBytes:vector<br>vector / heUlBufferStatusScheduledBytes:vector
- Window / per-run aggregation / exclusions: [0.3, 0.95) s; timeline=representative run 0; event-driven step observations
- Independent runs: run-level summaries: n=5

| Configuration / observation | Mean or direct value | 95% CI half-width |
|---|---:|---:|
| Bursty traffic / reported backlog time weighted mean bytes | 1011.35 | 258.153 |
| Fresh BSR / reported backlog time weighted mean bytes | 1474.9 | 333.375 |
| Implicit BSR / reported backlog time weighted mean bytes | 1159.79 | 259.619 |
| Stale BSR / reported backlog time weighted mean bytes | 1518.6 | 98.1539 |

The table is a presentation view of the session-bound run-level summary; the common provenance applies to every row.

### [script] Executable evidence checks

| Status | Requirement | Evaluation |
|---|---|---|
| **PASS** | Reported backlog is joined to scheduler-consumed backlog | Reported and planned backlog bytes are aligned to a trigger decision ID for every retained decision. |

#### [script] Joined BSR scheduler-decision evidence: FreshBsr

| Config | Run | Time (s) / Trigger | Users | Reported bytes | Planned bytes |
|---|---:|---:|---:|---:|---:|
| FreshBsr | 0 | 0.10400000000000001 / 2 | 3 | 0 | 0 |
| FreshBsr | 0 | 0.302 / 3 | 3 | 0 | 0 |
| FreshBsr | 0 | 0.305 / 4 | 3 | 0 | 0 |
| FreshBsr | 0 | 0.32800796704500007 / 5 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.33291385118800004 / 6 | 2 | 12256 | 12256 |
| FreshBsr | 0 | 0.33737061809400004 / 7 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.34694970306700007 / 8 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.3565883541170001 / 9 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.3598531546700001 / 10 | 1 | 6096 | 6096 |
| FreshBsr | 0 | 0.3634139552230001 / 11 | 1 | 6096 | 6096 |
| FreshBsr | 0 | 0.36664263853800005 / 12 | 1 | 6096 | 6096 |
| FreshBsr | 0 | 0.3838489729030001 / 13 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.3875577734550001 / 14 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.3990235667690001 / 15 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.4039745681500001 / 16 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.40978416925500005 / 17 | 1 | 42896 | 31336 |
| FreshBsr | 0 | 0.4129375695310001 / 18 | 1 | 12224 | 12224 |
| FreshBsr | 0 | 0.41500000000000004 / 19 | 1 | 12224 | 12224 |
| FreshBsr | 0 | 0.43158168607800007 / 20 | 1 | 49024 | 31336 |
| FreshBsr | 0 | 0.4390331699450001 / 21 | 1 | 18384 | 18384 |
| FreshBsr | 0 | 0.441 / 22 | 1 | 18384 | 18384 |
| FreshBsr | 0 | 0.44902648386700006 / 23 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.4534582844190001 / 24 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.4588124852470001 / 25 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.46147788552300006 / 26 | 1 | 12224 | 12224 |
| FreshBsr | 0 | 0.4656366860750001 / 27 | 1 | 6096 | 6096 |
| FreshBsr | 0 | 0.46978648662700007 / 28 | 1 | 12224 | 12224 |
| FreshBsr | 0 | 0.4730852871790001 / 29 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.4787362885600001 / 30 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.4922160904940001 / 31 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.5030414921510001 / 32 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.5150807765700002 / 33 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.519230577122 / 34 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.5238466607130001 / 35 | 1 | 49024 | 31336 |
| FreshBsr | 0 | 0.527998461265 / 36 | 1 | 18352 | 18352 |
| FreshBsr | 0 | 0.5329981445800002 / 37 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.5371259451320001 / 38 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.5453183467890002 / 39 | 1 | 55152 | 31336 |
| FreshBsr | 0 | 0.548 / 40 | 1 | 55152 | 31336 |
| FreshBsr | 0 | 0.5526160835910001 / 41 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.5567568841430001 / 42 | 1 | 6096 | 6096 |
| FreshBsr | 0 | 0.5600856846950001 / 43 | 2 | 24480 | 24480 |
| FreshBsr | 0 | 0.5632730849710001 / 44 | 1 | 12224 | 12224 |
| FreshBsr | 0 | 0.5660000000000001 / 45 | 1 | 12224 | 12224 |
| FreshBsr | 0 | 0.5696198005520001 / 46 | 1 | 12256 | 12256 |
| FreshBsr | 0 | 0.5773108841430001 / 47 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.5815006846950002 / 48 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.586 / 49 | 1 | 110304 | 31336 |
| FreshBsr | 0 | 0.5964180013800001 / 50 | 1 | 24512 | 24512 |
| FreshBsr | 0 | 0.6027302022080001 / 51 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.6083660027600002 / 52 | 2 | 12256 | 12256 |
| FreshBsr | 0 | 0.6100600027600002 / 53 | 2 | 18320 | 18320 |
| FreshBsr | 0 | 0.6142348033120001 / 54 | 2 | 12224 | 12224 |
| FreshBsr | 0 | 0.6180026038640001 / 55 | 2 | 18352 | 18352 |
| FreshBsr | 0 | 0.6216294044160001 / 56 | 2 | 18352 | 18352 |
| FreshBsr | 0 | 0.6277570055210001 / 57 | 2 | 177680 | 19704 |
| FreshBsr | 0 | 0.6323748060740001 / 58 | 2 | 183808 | 27216 |
| FreshBsr | 0 | 0.6340778060740001 / 59 | 1 | 24480 | 24480 |
| FreshBsr | 0 | 0.6408148074540001 / 60 | 1 | 12256 | 12256 |
| FreshBsr | 0 | 0.6445576080060001 / 61 | 1 | 6096 | 6096 |
| FreshBsr | 0 | 0.6498122091100001 / 62 | 2 | 18352 | 18352 |
| FreshBsr | 0 | 0.6539870096620001 / 63 | 2 | 18352 | 18352 |
| FreshBsr | 0 | 0.6599850932530001 / 64 | 3 | 30608 | 25832 |
| FreshBsr | 0 | 0.6631834935290001 / 65 | 3 | 30544 | 25800 |
| FreshBsr | 0 | 0.6665752940810001 / 66 | 2 | 24448 | 19704 |
| FreshBsr | 0 | 0.6704160946330001 / 67 | 2 | 18320 | 18320 |
| FreshBsr | 0 | 0.6732224949090001 / 68 | 2 | 24448 | 24448 |
| FreshBsr | 0 | 0.6765602954610002 / 69 | 3 | 18320 | 18320 |
| FreshBsr | 0 | 0.6802160960130001 / 70 | 3 | 36736 | 28968 |
| FreshBsr | 0 | 0.6842652968410001 / 71 | 2 | 24480 | 24480 |
| FreshBsr | 0 | 0.6883076971170001 / 72 | 3 | 30576 | 27584 |
| FreshBsr | 0 | 0.6900216971170001 / 73 | 3 | 36704 | 27616 |
| FreshBsr | 0 | 0.6948887807080001 / 74 | 1 | 18384 | 18384 |
| FreshBsr | 0 | 0.7028646648510001 / 75 | 1 | 18384 | 18384 |
| FreshBsr | 0 | 0.7096062659560002 / 76 | 1 | 18384 | 18384 |
| FreshBsr | 0 | 0.7171281501000002 / 77 | 2 | 12256 | 12256 |
| FreshBsr | 0 | 0.724 / 78 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.7295254838670001 / 79 | 3 | 67376 | 25832 |
| FreshBsr | 0 | 0.7337382844190001 / 80 | 1 | 42864 | 31336 |
| FreshBsr | 0 | 0.7379240849710001 / 81 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.7411484852470002 / 82 | 1 | 18384 | 18384 |
| FreshBsr | 0 | 0.7487923693900002 / 83 | 1 | 12256 | 12256 |
| FreshBsr | 0 | 0.751 / 84 | 1 | 12256 | 12256 |
| FreshBsr | 0 | 0.7590078841430001 / 85 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.7615121671820001 / 86 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.7660530006900002 / 87 | 2 | 79664 | 19736 |
| FreshBsr | 0 | 0.768 / 88 | 2 | 79664 | 19736 |
| FreshBsr | 0 | 0.7748096833150001 / 89 | 1 | 73536 | 31336 |
| FreshBsr | 0 | 0.7800877332600002 / 90 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.7834055338120002 / 91 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.786 / 92 | 1 | 6128 | 6128 |
| FreshBsr | 0 | 0.7908634002760001 / 93 | 1 | 6096 | 6096 |
| FreshBsr | 0 | 0.793 / 94 | 1 | 6096 | 6096 |
| FreshBsr | 0 | 0.7956924002760002 / 95 | 1 | 18352 | 18352 |
| FreshBsr | 0 | 0.7998379663540002 / 96 | 1 | 18352 | 18352 |
| FreshBsr | 0 | 0.8037191671820001 / 97 | 1 | 6096 | 6096 |
| FreshBsr | 0 | 0.8063935674580002 / 98 | 1 | 12224 | 12224 |
| FreshBsr | 0 | 0.8090079677340002 / 99 | 1 | 12224 | 12224 |
| FreshBsr | 0 | 0.8131667682860001 / 100 | 2 | 12224 | 12224 |
| FreshBsr | 0 | 0.8169095688380001 / 101 | 1 | 12224 | 12224 |

Showing 100 of 670 joined decisions for FreshBsr; the session-bound evidence ledger retains every observation.

#### [script] Joined BSR scheduler-decision evidence: StaleBsr

| Config | Run | Time (s) / Trigger | Users | Reported bytes | Planned bytes |
|---|---:|---:|---:|---:|---:|
| StaleBsr | 0 | 0.014 / 2 | 3 | 0 | 0 |
| StaleBsr | 0 | 0.027 / 3 | 3 | 0 | 0 |
| StaleBsr | 0 | 0.04 / 4 | 3 | 0 | 0 |
| StaleBsr | 0 | 0.053 / 5 | 3 | 0 | 0 |
| StaleBsr | 0 | 0.066 / 6 | 3 | 0 | 0 |
| StaleBsr | 0 | 0.079 / 7 | 3 | 0 | 0 |
| StaleBsr | 0 | 0.092 / 8 | 3 | 0 | 0 |
| StaleBsr | 0 | 0.105 / 9 | 3 | 0 | 0 |
| StaleBsr | 0 | 0.11800000000000001 / 10 | 3 | 0 | 0 |
| StaleBsr | 0 | 0.131 / 11 | 3 | 0 | 0 |
| StaleBsr | 0 | 0.14400000000000002 / 12 | 3 | 0 | 0 |
| StaleBsr | 0 | 0.157 / 13 | 3 | 0 | 0 |
| StaleBsr | 0 | 0.17 / 14 | 3 | 0 | 0 |
| StaleBsr | 0 | 0.183 / 15 | 3 | 0 | 0 |
| StaleBsr | 0 | 0.196 / 16 | 3 | 0 | 0 |
| StaleBsr | 0 | 0.212 / 17 | 3 | 0 | 0 |
| StaleBsr | 0 | 0.225 / 18 | 3 | 0 | 0 |
| StaleBsr | 0 | 0.23800000000000002 / 19 | 3 | 0 | 0 |
| StaleBsr | 0 | 0.251 / 20 | 3 | 0 | 0 |
| StaleBsr | 0 | 0.264 / 21 | 3 | 0 | 0 |
| StaleBsr | 0 | 0.277 / 22 | 3 | 0 | 0 |
| StaleBsr | 0 | 0.29 / 23 | 3 | 0 | 0 |
| StaleBsr | 0 | 0.303 / 24 | 3 | 0 | 0 |
| StaleBsr | 0 | 0.32125396704500003 / 25 | 3 | 6128 | 0 |
| StaleBsr | 0 | 0.32518116787300005 / 26 | 3 | 42896 | 29000 |
| StaleBsr | 0 | 0.3302963687020001 / 27 | 3 | 42864 | 29000 |
| StaleBsr | 0 | 0.3344661692540001 / 28 | 1 | 6096 | 6096 |
| StaleBsr | 0 | 0.33778596980600006 / 29 | 1 | 12224 | 12224 |
| StaleBsr | 0 | 0.3411127703580001 / 30 | 1 | 6128 | 6128 |
| StaleBsr | 0 | 0.34372717063400005 / 31 | 2 | 6128 | 6128 |
| StaleBsr | 0 | 0.34717613698800004 / 32 | 1 | 0 | 0 |
| StaleBsr | 0 | 0.35383102113200005 / 33 | 2 | 55152 | 19736 |
| StaleBsr | 0 | 0.35746682168400007 / 34 | 2 | 67376 | 25864 |
| StaleBsr | 0 | 0.3611026222360001 / 35 | 2 | 67408 | 13608 |
| StaleBsr | 0 | 0.36734210610300005 / 36 | 3 | 12256 | 12256 |
| StaleBsr | 0 | 0.3743324727330001 / 37 | 3 | 49024 | 0 |
| StaleBsr | 0 | 0.3783152732850001 / 38 | 2 | 24512 | 19736 |
| StaleBsr | 0 | 0.3800182732850001 / 39 | 2 | 30576 | 25832 |
| StaleBsr | 0 | 0.3841930738370001 / 40 | 2 | 18320 | 18320 |
| StaleBsr | 0 | 0.38793387438900007 / 41 | 2 | 24448 | 24448 |
| StaleBsr | 0 | 0.39276307521700005 / 42 | 2 | 12192 | 12192 |
| StaleBsr | 0 | 0.39692627604600006 / 43 | 2 | 42864 | 27216 |
| StaleBsr | 0 | 0.4008054768740001 / 44 | 1 | 18352 | 18352 |
| StaleBsr | 0 | 0.4072779607410001 / 45 | 3 | 12256 | 0 |
| StaleBsr | 0 | 0.41262676129400006 / 46 | 3 | 42896 | 25864 |
| StaleBsr | 0 | 0.41630316157000014 / 47 | 2 | 12192 | 12192 |
| StaleBsr | 0 | 0.42212204571300005 / 48 | 2 | 24480 | 24480 |
| StaleBsr | 0 | 0.42533284626500006 / 49 | 2 | 24480 | 19736 |
| StaleBsr | 0 | 0.4270358462650001 / 50 | 2 | 18352 | 18352 |
| StaleBsr | 0 | 0.4345233301330001 / 51 | 2 | 6128 | 6128 |
| StaleBsr | 0 | 0.43967173179000013 / 52 | 2 | 49024 | 19736 |
| StaleBsr | 0 | 0.45024633289400007 / 53 | 1 | 0 | 0 |
| StaleBsr | 0 | 0.4559282992480001 / 54 | 3 | 6128 | 0 |
| StaleBsr | 0 | 0.4614473828390001 / 55 | 3 | 12256 | 12256 |
| StaleBsr | 0 | 0.46563318339100007 / 56 | 1 | 36768 | 31336 |
| StaleBsr | 0 | 0.4714928667060001 / 57 | 2 | 24480 | 19736 |
| StaleBsr | 0 | 0.4752650675340001 / 58 | 2 | 36736 | 25864 |
| StaleBsr | 0 | 0.47987915112500007 / 59 | 3 | 49024 | 25864 |
| StaleBsr | 0 | 0.48407395167700007 / 60 | 2 | 6128 | 6128 |
| StaleBsr | 0 | 0.4873867522290001 / 61 | 2 | 18384 | 13608 |
| StaleBsr | 0 | 0.4916658358200001 / 62 | 1 | 67408 | 31336 |
| StaleBsr | 0 | 0.4958479194110001 / 63 | 2 | 55120 | 19736 |
| StaleBsr | 0 | 0.5000227199630001 / 64 | 1 | 6128 | 6128 |
| StaleBsr | 0 | 0.5032175205150001 / 65 | 2 | 6128 | 6128 |
| StaleBsr | 0 | 0.5070967213430001 / 66 | 3 | 12256 | 12256 |
| StaleBsr | 0 | 0.511291521895 / 67 | 3 | 61248 | 25832 |
| StaleBsr | 0 | 0.513005521895 / 68 | 2 | 61216 | 25832 |
| StaleBsr | 0 | 0.5229638063150001 / 69 | 2 | 48992 | 19736 |
| StaleBsr | 0 | 0.5268250071430001 / 70 | 1 | 61248 | 31336 |
| StaleBsr | 0 | 0.530028807695 / 71 | 1 | 42864 | 31336 |
| StaleBsr | 0 | 0.538707691838 / 72 | 1 | 6128 | 6128 |
| StaleBsr | 0 | 0.5424354923900001 / 73 | 3 | 6128 | 0 |
| StaleBsr | 0 | 0.5486861757050001 / 74 | 3 | 36768 | 29000 |
| StaleBsr | 0 | 0.5534746595720001 / 75 | 2 | 36736 | 25864 |
| StaleBsr | 0 | 0.5579789089650001 / 76 | 1 | 24480 | 24480 |
| StaleBsr | 0 | 0.5621417095170002 / 77 | 2 | 24480 | 13608 |
| StaleBsr | 0 | 0.5648721097930001 / 78 | 2 | 67376 | 27216 |
| StaleBsr | 0 | 0.5670000000000001 / 79 | 2 | 67376 | 27216 |
| StaleBsr | 0 | 0.5699312008280001 / 80 | 2 | 67344 | 27216 |
| StaleBsr | 0 | 0.5726706011040001 / 81 | 2 | 55088 | 27216 |
| StaleBsr | 0 | 0.5750000000000001 / 82 | 2 | 55088 | 27216 |
| StaleBsr | 0 | 0.5778038005520001 / 83 | 2 | 67344 | 27216 |
| StaleBsr | 0 | 0.5819626011040001 / 84 | 1 | 18352 | 18352 |
| StaleBsr | 0 | 0.5911054516010001 / 85 | 1 | 6128 | 6128 |
| StaleBsr | 0 | 0.594461252153 / 86 | 3 | 36768 | 0 |
| StaleBsr | 0 | 0.5977716524290001 / 87 | 3 | 42896 | 29000 |
| StaleBsr | 0 | 0.6015594529810001 / 88 | 3 | 42896 | 29000 |
| StaleBsr | 0 | 0.6052152535330001 / 89 | 1 | 12256 | 12256 |
| StaleBsr | 0 | 0.6091560540850001 / 90 | 1 | 6128 | 6128 |
| StaleBsr | 0 | 0.6156452549140001 / 91 | 3 | 12256 | 12256 |
| StaleBsr | 0 | 0.6194370554660001 / 92 | 2 | 49024 | 19736 |
| StaleBsr | 0 | 0.6227518560180001 / 93 | 1 | 42864 | 31336 |
| StaleBsr | 0 | 0.6256286565700002 / 94 | 1 | 24480 | 24480 |
| StaleBsr | 0 | 0.6326588573980001 / 95 | 1 | 6128 | 6128 |
| StaleBsr | 0 | 0.6384940006900002 / 96 | 3 | 12256 | 12256 |
| StaleBsr | 0 | 0.6421678012420001 / 97 | 2 | 55120 | 19704 |
| StaleBsr | 0 | 0.6463516017940001 / 98 | 2 | 18352 | 18352 |
| StaleBsr | 0 | 0.6480546017940001 / 99 | 2 | 18320 | 18320 |
| StaleBsr | 0 | 0.6556120856620001 / 100 | 3 | 18352 | 18352 |
| StaleBsr | 0 | 0.6598068862140001 / 101 | 1 | 12224 | 12224 |

Showing 100 of 940 joined decisions for StaleBsr; the session-bound evidence ledger retains every observation.

#### [script] Joined BSR scheduler-decision evidence: BurstyTraffic

| Config | Run | Time (s) / Trigger | Users | Reported bytes | Planned bytes |
|---|---:|---:|---:|---:|---:|
| BurstyTraffic | 0 | 0.10400000000000001 / 2 | 3 | 0 | 0 |
| BurstyTraffic | 0 | 0.20700000000000002 / 3 | 3 | 0 | 0 |
| BurstyTraffic | 0 | 0.33537273533500006 / 4 | 2 | 12256 | 12256 |
| BurstyTraffic | 0 | 0.3412431019650001 / 5 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.343 / 6 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.34660180055200007 / 7 | 1 | 30640 | 30640 |
| BurstyTraffic | 0 | 0.3559666846950001 / 8 | 2 | 18384 | 18384 |
| BurstyTraffic | 0 | 0.35874508497100005 / 9 | 1 | 12224 | 12224 |
| BurstyTraffic | 0 | 0.36568936732000007 / 10 | 1 | 42896 | 31336 |
| BurstyTraffic | 0 | 0.3813836531240001 / 11 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.3852682542280001 / 12 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.38761793754300006 / 13 | 1 | 6096 | 6096 |
| BurstyTraffic | 0 | 0.3916876208580001 / 14 | 1 | 6096 | 6096 |
| BurstyTraffic | 0 | 0.40032998748900006 / 15 | 1 | 12224 | 12224 |
| BurstyTraffic | 0 | 0.40201698748900005 / 16 | 1 | 6096 | 6096 |
| BurstyTraffic | 0 | 0.40602447135600006 / 17 | 1 | 6096 | 6096 |
| BurstyTraffic | 0 | 0.4099848716320001 / 18 | 1 | 12224 | 12224 |
| BurstyTraffic | 0 | 0.4154936385380001 / 19 | 1 | 12224 | 12224 |
| BurstyTraffic | 0 | 0.4295936413010001 / 20 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.4363218421290001 / 21 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.4380088421290001 / 22 | 1 | 6096 | 6096 |
| BurstyTraffic | 0 | 0.44176064268100007 / 23 | 1 | 6096 | 6096 |
| BurstyTraffic | 0 | 0.44592844323300007 / 24 | 1 | 6096 | 6096 |
| BurstyTraffic | 0 | 0.4576906454410001 / 25 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.4612920457170001 / 26 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.4654508462690001 / 27 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.46938064682100006 / 28 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.4735304473730001 / 29 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.4830783315160001 / 30 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.485 / 31 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.48918780055200006 / 32 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.4955084016560001 / 33 | 1 | 12256 | 12256 |
| BurstyTraffic | 0 | 0.512 / 34 | 1 | 116432 | 31336 |
| BurstyTraffic | 0 | 0.514 / 35 | 1 | 116432 | 31336 |
| BurstyTraffic | 0 | 0.516 / 36 | 1 | 116432 | 31336 |
| BurstyTraffic | 0 | 0.518 / 37 | 1 | 116432 | 31336 |
| BurstyTraffic | 0 | 0.52 / 38 | 1 | 116432 | 31336 |
| BurstyTraffic | 0 | 0.522 / 39 | 1 | 116432 | 31336 |
| BurstyTraffic | 0 | 0.524 / 40 | 1 | 116432 | 31336 |
| BurstyTraffic | 0 | 0.646 / 41 | 3 | 0 | 0 |
| BurstyTraffic | 0 | 0.6741880520180001 / 42 | 3 | 18384 | 18384 |
| BurstyTraffic | 0 | 0.6801701356090001 / 43 | 3 | 30608 | 25832 |
| BurstyTraffic | 0 | 0.682 / 44 | 1 | 18352 | 18352 |
| BurstyTraffic | 0 | 0.6887458841430001 / 45 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.6963310849720001 / 46 | 2 | 24512 | 19736 |
| BurstyTraffic | 0 | 0.7000438518790001 / 47 | 2 | 18384 | 18384 |
| BurstyTraffic | 0 | 0.7036706524310001 / 48 | 2 | 12224 | 12224 |
| BurstyTraffic | 0 | 0.7070134529830001 / 49 | 2 | 12224 | 12224 |
| BurstyTraffic | 0 | 0.7118889368500002 / 50 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.7175805379550001 / 51 | 1 | 18384 | 18384 |
| BurstyTraffic | 0 | 0.7226666215460001 / 52 | 1 | 73536 | 31336 |
| BurstyTraffic | 0 | 0.7296152226500002 / 53 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.7351594234790001 / 54 | 2 | 30640 | 25864 |
| BurstyTraffic | 0 | 0.7393342240310001 / 55 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.7506951917670002 / 56 | 1 | 18384 | 18384 |
| BurstyTraffic | 0 | 0.7565497928720002 / 57 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.7630626770160002 / 58 | 2 | 24512 | 19736 |
| BurstyTraffic | 0 | 0.7664054775680001 / 59 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.769 / 60 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.7736340835910002 / 61 | 1 | 49024 | 31336 |
| BurstyTraffic | 0 | 0.7768144838670001 / 62 | 1 | 18352 | 18352 |
| BurstyTraffic | 0 | 0.7804612844190001 / 63 | 1 | 30640 | 30640 |
| BurstyTraffic | 0 | 0.7866187682860002 / 64 | 1 | 12256 | 12256 |
| BurstyTraffic | 0 | 0.7908949691150001 / 65 | 2 | 67408 | 25864 |
| BurstyTraffic | 0 | 0.7935763693910002 / 66 | 1 | 48992 | 31336 |
| BurstyTraffic | 0 | 0.7971961699430001 / 67 | 2 | 85760 | 27216 |
| BurstyTraffic | 0 | 0.8009459704950002 / 68 | 1 | 24480 | 24480 |
| BurstyTraffic | 0 | 0.8048227710470002 / 69 | 1 | 6096 | 6096 |
| BurstyTraffic | 0 | 0.8107728546380002 / 70 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.8215653398860001 / 71 | 1 | 18384 | 18384 |
| BurstyTraffic | 0 | 0.8253191404380001 / 72 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.8319407415420002 / 73 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.8355511418180002 / 74 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.8402872254090001 / 75 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.8438456256850001 / 76 | 1 | 6096 | 6096 |
| BurstyTraffic | 0 | 0.8471678265130002 / 77 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.8509150273410001 / 78 | 2 | 30640 | 19736 |
| BurstyTraffic | 0 | 0.8550988278930002 / 79 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.874 / 80 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.8781768005520002 / 81 | 1 | 6096 | 6096 |
| BurstyTraffic | 0 | 0.8819286011040002 / 82 | 1 | 12224 | 12224 |
| BurstyTraffic | 0 | 0.8859078019320001 / 83 | 1 | 6096 | 6096 |
| BurstyTraffic | 0 | 0.9005472871800002 / 84 | 1 | 122560 | 31336 |
| BurstyTraffic | 0 | 0.9034624880080002 / 85 | 1 | 98016 | 31336 |
| BurstyTraffic | 0 | 0.9072232885600002 / 86 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.9110610891120001 / 87 | 1 | 6128 | 6128 |
| BurstyTraffic | 0 | 0.9156681727030002 / 88 | 1 | 55152 | 31336 |
| BurstyTraffic | 0 | 0.9198359732550002 / 89 | 1 | 24480 | 24480 |
| BurstyTraffic | 0 | 0.9245236565700002 / 90 | 1 | 6096 | 6096 |
| BurstyTraffic | 0 | 0.9271890568460002 / 91 | 1 | 6096 | 6096 |
| BurstyTraffic | 0 | 0.93 / 92 | 1 | 6096 | 6096 |
| BurstyTraffic | 0 | 0.9336018005520001 / 93 | 2 | 79632 | 27216 |
| BurstyTraffic | 0 | 0.9377946011040001 / 94 | 2 | 18352 | 18352 |
| BurstyTraffic | 0 | 0.9419514016560002 / 95 | 2 | 18352 | 18352 |
| BurstyTraffic | 0 | 0.9452892022080002 / 96 | 1 | 12224 | 12224 |
| BurstyTraffic | 0 | 0.9475036024840001 / 97 | 1 | 12224 | 12224 |
| BurstyTraffic | 0 | 0.9501270027600003 / 98 | 1 | 6096 | 6096 |
| BurstyTraffic | 0 | 0.9530000000000001 / 99 | 1 | 6096 | 6096 |
| BurstyTraffic | 0 | 0.9566288005520002 / 100 | 1 | 6128 | 6128 |
| BurstyTraffic | 1 | 0.10400000000000001 / 2 | 3 | 0 | 0 |

Showing 100 of 628 joined decisions for BurstyTraffic; the session-bound evidence ledger retains every observation.
<!-- END GENERATED: ieee80211-scalar-vector-bsr -->

## [agent] PCAP statistics

<!-- BEGIN GENERATED ANALYSIS: pcap -->
<!-- END GENERATED ANALYSIS: pcap -->

The generated packet view uses the AP-MAC capture from representative run 0.
The trigger-type table distinguishes the decoded Trigger records, while the
timeline shows their HE-TB responses. Together they establish the surrounding
exchange; the AP decision telemetry remains the authoritative evidence for
reported and planned BSR bytes.

<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
### [script] Generated PCAP plots and tables
![802.11 Packet Type Statistics](results/20260730T166000Z/packet_statistics.png)

Figure provenance: [`packet_statistics.png.json`](results/20260730T166000Z/packet_statistics.png.json).

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260730T166000Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. The selected manifest is `examples/ieee80211/analysis/generated/ax/capture_manifests/20260730T166000Z.json` (SHA-256 `fa16a32559db307e2f3bf29cc08fbbd4451f70d3dbd5b8b83537b9f2b411249e`). HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %** (and **Estimated airtime / sim time** in the summary table): The sum of estimated frame airtimes divided by the simulation time limit. Parallel multi-user transmissions (e.g., OFDMA resource units or MU-MIMO spatial streams) and concurrent observations across multiple capture points are summed per transmission/RU, so this cumulative metric can exceed 100%; it is not the union of busy channel time.

#### [script] Compact cross-configuration summary

Observation point: Access Point (AP) wireless interfaces.

| Configuration | Selection/filter | Observations | Dominant decoded frame/PHY evidence | Estimated airtime / sim time | Limits |
|---|---|---:|---|---:|---|
| `FreshBsr` | `none (all decoded frames)` | 1620 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (434), Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (256), Control: Ack (230) | 61.52% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `StaleBsr` | `none (all decoded frames)` | 1905 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (402), Data: QoS Data [HE-TB, HE-MCS 2, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] (246), Control: Block Ack (BA) (239) | 72.20% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `ImplicitBsr` | `none (all decoded frames)` | 1472 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (597), Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (336), Control: Ack (335) | 53.21% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |
| `BurstyTraffic` | `none (all decoded frames)` | 1311 | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] (380), Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] (233), Control: Ack (208) | 47.95% | Not delivery or de-duplicated transmissions; unknown PHY fields stay unknown |

### [script] Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | FreshBsr produced protocol-visible wireless observations | 1620 AP/global transmission observations |
| **PASS** | StaleBsr produced protocol-visible wireless observations | 1905 AP/global transmission observations |
| **PASS** | ImplicitBsr produced protocol-visible wireless observations | 1472 AP/global transmission observations |
| **PASS** | BurstyTraffic produced protocol-visible wireless observations | 1311 AP/global transmission observations |

### [script] Configuration: `FreshBsr`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1620**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 256 | 15.80% | 652.0 B | 284.9 B | 366.5 us | 153.1 us | 5010 MHz | -72.0 dBm | - | 15.25% | 9.38% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 434 | 26.79% | 1187.9 B | 460.7 B | 685.8 us | 252.0 us | 5010 MHz | -72.0 dBm | - | 48.38% | 29.76% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31d62e" /></svg> | Data: QoS Data [HE-TB, HE-MCS 2, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 104 | 6.42% | 768.2 B | 2.0 B | 662.0 us | 19.6 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 11.19% | 6.89% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24ae34" /></svg> | Data: QoS Data [HE-TB, HE-MCS 2, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 78 | 4.81% | 767.1 B | 1.8 B | 289.9 us | 16.9 us | 5010 MHz | -75.0 dBm | - | 3.68% | 2.26% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#37b52c" /></svg> | Data: QoS Data [HE-TB, HE-MCS 2, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 76 | 4.69% | 770.0 B | 0.0 B | 1404.9 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -75.0 dBm | - | 17.36% | 10.68% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 9 | 0.56% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 0.58% | 0.36% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#13560b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 2, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 3 | 0.19% | 34.0 B | 0.0 B | 156.9 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 0.08% | 0.05% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 144 | 8.89% | 37.2 B | 6.9 B | 32.4 us | 2.3 us | 5010 MHz | - | 10.0 dBm | 0.76% | 0.47% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 75 | 4.63% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -72.0 dBm | - | 0.34% | 0.21% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 199 | 12.28% | 67.1 B | 49.6 B | 42.4 us | 16.5 us | 5010 MHz | - | 10.0 dBm | 1.37% | 0.84% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 230 | 14.20% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.92% | 0.57% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.37% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.37% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.07% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.003064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=226 | Responds without MAC payload while preserving QoS control information. |
| 3 | 0.003064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=230 | Responds without MAC payload while preserving QoS control information. |
| 4 | 0.003064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=234 | Responds without MAC payload while preserving QoS control information. |
| 5 | 0.003133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 6 | 0.104048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 7 | 0.106064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=418 | Responds without MAC payload while preserving QoS control information. |
| 8 | 0.106064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=422 | Responds without MAC payload while preserving QoS control information. |
| 9 | 0.106064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=426 | Responds without MAC payload while preserving QoS control information. |
| 10 | 0.106133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 11 | 0.200484000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 12 | 0.201077000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 13 | 0.201682000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 14 | 0.201730000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 16 | 0.201826000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 17 | 0.202345000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |

Frame numbers are local to capture `FreshBsr-#0HeBsrNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded HE Trigger user allocations

| Frame | Simulation time (s) | Trigger type | Ordered user allocations |
|---:|---:|---:|---|
| 1 | 0.001048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 6 | 0.104048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 32 | 0.302048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 35 | 0.305048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 86 | 0.328043000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 100 | 0.332949000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=38, MCS=2, target RSSI=35 |
| 112 | 0.337406000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 133 | 0.346985000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 156 | 0.356624000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 161 | 0.359889000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 168 | 0.363449000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 173 | 0.366678000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 213 | 0.383884000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 221 | 0.387593000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 242 | 0.399059000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 256 | 0.404010000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 269 | 0.409820000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 277 | 0.412973000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35 |
| 280 | 0.415036000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35 |
| 316 | 0.431617000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 334 | 0.439069000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 337 | 0.441036000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 352 | 0.449062000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 364 | 0.453494000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 379 | 0.458848000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 383 | 0.461513000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35 |
| 392 | 0.465672000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 400 | 0.469822000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35 |
| 409 | 0.473121000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 423 | 0.478772000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 455 | 0.492252000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 476 | 0.503077000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 497 | 0.515116000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 505 | 0.519266000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 513 | 0.523882000 | 0 | #0: AID=1, RU=61, MCS=2, target RSSI=35 |
| 525 | 0.528034000 | 0 | #0: AID=1, RU=61, MCS=2, target RSSI=35 |
| 540 | 0.533034000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 552 | 0.537161000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 578 | 0.545354000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 582 | 0.548036000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 590 | 0.552652000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 598 | 0.556792000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 606 | 0.560121000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 612 | 0.563309000 | 0 | #0: AID=3, RU=53, MCS=2, target RSSI=35 |
| 615 | 0.566036000 | 0 | #0: AID=3, RU=53, MCS=2, target RSSI=35 |
| 623 | 0.569655000 | 0 | #0: AID=3, RU=53, MCS=2, target RSSI=35 |
| 637 | 0.577346000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 647 | 0.581536000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 656 | 0.586036000 | 0 | #0: AID=1, RU=61, MCS=2, target RSSI=35 |
| 673 | 0.596454000 | 0 | #0: AID=1, RU=61, MCS=2, target RSSI=35 |
| 689 | 0.602766000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 702 | 0.608402000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=38, MCS=2, target RSSI=35 |
| 706 | 0.610096000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=54, MCS=2, target RSSI=35 |
| 716 | 0.614270000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=38, MCS=2, target RSSI=35 |
| 724 | 0.618038000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=54, MCS=2, target RSSI=35 |
| 733 | 0.621665000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=54, MCS=2, target RSSI=35 |
| 750 | 0.627793000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 761 | 0.632410000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 766 | 0.634113000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 784 | 0.640850000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35 |
| 793 | 0.644593000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 806 | 0.649848000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=54, MCS=2, target RSSI=35 |
| 816 | 0.654023000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=54, MCS=2, target RSSI=35 |
| 830 | 0.660025000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35; #2: AID=2, RU=54, MCS=2, target RSSI=35 |
| 837 | 0.663223000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=38, MCS=2, target RSSI=35; #2: AID=3, RU=54, MCS=2, target RSSI=35 |
| 846 | 0.666611000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 857 | 0.670452000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=54, MCS=2, target RSSI=35 |
| 864 | 0.673258000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 876 | 0.676600000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=38, MCS=2, target RSSI=35; #2: AID=3, RU=39, MCS=2, target RSSI=35 |
| 884 | 0.680256000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=1, RU=4, MCS=2, target RSSI=35; #2: AID=3, RU=54, MCS=2, target RSSI=35 |
| 899 | 0.684301000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 910 | 0.688347000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=2, RU=4, MCS=2, target RSSI=35; #2: AID=3, RU=54, MCS=2, target RSSI=35 |
| 917 | 0.690061000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=4, MCS=2, target RSSI=35; #2: AID=2, RU=54, MCS=2, target RSSI=35 |
| 933 | 0.694924000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 957 | 0.702900000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 973 | 0.709642000 | 0 | #0: AID=1, RU=61, MCS=2, target RSSI=35 |
| 991 | 0.717164000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35 |
| 1007 | 0.724036000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1018 | 0.729565000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35; #2: AID=2, RU=54, MCS=2, target RSSI=35 |
| 1029 | 0.733774000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 1041 | 0.737960000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 1047 | 0.741184000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 1067 | 0.748828000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35 |
| 1070 | 0.751036000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35 |
| 1086 | 0.759043000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 1089 | 0.761548000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 1102 | 0.766089000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 1106 | 0.768036000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 1125 | 0.774845000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 1140 | 0.780123000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1147 | 0.783441000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1150 | 0.786036000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1154 | 0.790899000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1158 | 0.793036000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1162 | 0.795728000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 1165 | 0.799873000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 1179 | 0.803755000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1183 | 0.806429000 | 0 | #0: AID=3, RU=53, MCS=2, target RSSI=35 |
| 1188 | 0.809043000 | 0 | #0: AID=3, RU=53, MCS=2, target RSSI=35 |
| 1197 | 0.813202000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35 |

Showing the first 100 of 144 decoded Trigger frames; the script-owned packet metrics JSON preserves every row.

### [script] Configuration: `StaleBsr`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1905**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 234 | 12.28% | 606.4 B | 310.2 B | 341.9 us | 166.3 us | 5010 MHz | -72.0 dBm | - | 11.08% | 8.00% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 402 | 21.10% | 1188.1 B | 486.0 B | 685.9 us | 265.9 us | 5010 MHz | -72.0 dBm | - | 38.19% | 27.57% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31d62e" /></svg> | Data: QoS Data [HE-TB, HE-MCS 2, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 246 | 12.91% | 768.0 B | 2.0 B | 661.0 us | 19.7 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 22.52% | 16.26% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24ae34" /></svg> | Data: QoS Data [HE-TB, HE-MCS 2, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 59 | 3.10% | 766.8 B | 1.6 B | 287.0 us | 15.1 us | 5010 MHz | -75.0 dBm | - | 2.34% | 1.69% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#37b52c" /></svg> | Data: QoS Data [HE-TB, HE-MCS 2, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 80 | 4.20% | 770.0 B | 0.0 B | 1404.9 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -75.0 dBm | - | 15.57% | 11.24% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 102 | 5.35% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 5.63% | 4.07% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#166d09" /></svg> | Data: QoS Null [HE-TB, HE-MCS 2, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 44 | 2.31% | 34.0 B | 0.0 B | 156.9 us | 0.0 us | 5002 MHz | -75.0 dBm | - | 0.96% | 0.69% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#13560b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 2, 26-tone RU, GI 3.2 us, LDPC, A-MPDU] | 14 | 0.73% | 34.0 B | 0.0 B | 156.9 us | 0.0 us | 5010 MHz | -75.0 dBm | - | 0.30% | 0.22% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 191 | 10.03% | 45.8 B | 13.4 B | 35.3 us | 4.5 us | 5010 MHz | - | 10.0 dBm | 0.93% | 0.67% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 57 | 2.99% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -72.0 dBm | - | 0.22% | 0.16% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#946833" /></svg> | Control: Block Ack Request (BAR) [HE-TB, HE-MCS 2, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 1 | 0.05% | 24.0 B | 0.0 B | 56.1 us | 0.0 us | 5005 MHz | -75.0 dBm | - | 0.01% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 239 | 12.55% | 66.7 B | 42.6 B | 42.2 us | 14.2 us | 5010 MHz | - | 10.0 dBm | 1.40% | 1.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 224 | 11.76% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.77% | 0.55% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.31% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.02% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.31% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.06% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.003064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=226 | Responds without MAC payload while preserving QoS control information. |
| 3 | 0.003064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=230 | Responds without MAC payload while preserving QoS control information. |
| 4 | 0.003064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=234 | Responds without MAC payload while preserving QoS control information. |
| 5 | 0.003133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 6 | 0.014048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 7 | 0.016064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=418 | Responds without MAC payload while preserving QoS control information. |
| 8 | 0.016064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=422 | Responds without MAC payload while preserving QoS control information. |
| 9 | 0.016064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=426 | Responds without MAC payload while preserving QoS control information. |
| 10 | 0.016133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 11 | 0.027048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 12 | 0.029064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=610 | Responds without MAC payload while preserving QoS control information. |
| 13 | 0.029064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=614 | Responds without MAC payload while preserving QoS control information. |
| 14 | 0.029064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=618 | Responds without MAC payload while preserving QoS control information. |
| 15 | 0.029133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 16 | 0.040048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |

Frame numbers are local to capture `StaleBsr-#0HeBsrNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded HE Trigger user allocations

| Frame | Simulation time (s) | Trigger type | Ordered user allocations |
|---:|---:|---:|---|
| 1 | 0.001048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 6 | 0.014048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 11 | 0.027048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 16 | 0.040048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 21 | 0.053048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 26 | 0.066048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 31 | 0.079048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 36 | 0.092048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 41 | 0.105048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 46 | 0.118048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 51 | 0.131048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 56 | 0.144048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 61 | 0.157048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 66 | 0.170048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 71 | 0.183048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 76 | 0.196048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 101 | 0.212048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 106 | 0.225048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 111 | 0.238048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 116 | 0.251048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 121 | 0.264048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 126 | 0.277048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 131 | 0.290048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 138 | 0.303048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 177 | 0.321301000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 187 | 0.325221000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=4, MCS=2, target RSSI=35; #2: AID=2, RU=54, MCS=2, target RSSI=35 |
| 197 | 0.330336000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=2, RU=4, MCS=2, target RSSI=35; #2: AID=3, RU=54, MCS=2, target RSSI=35 |
| 209 | 0.334502000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 216 | 0.337821000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35 |
| 225 | 0.341148000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 229 | 0.343763000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35; #1: AID=1, RU=38, MCS=2, target RSSI=35 |
| 236 | 0.347212000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35 |
| 251 | 0.353867000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 259 | 0.357502000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 269 | 0.361138000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 285 | 0.367382000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35; #1: AID=2, RU=38, MCS=2, target RSSI=35; #2: AID=3, RU=39, MCS=2, target RSSI=35 |
| 300 | 0.374380000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 310 | 0.378351000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 315 | 0.380054000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 326 | 0.384229000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 336 | 0.387969000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 351 | 0.392799000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35 |
| 360 | 0.396962000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 373 | 0.400841000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 391 | 0.407325000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 403 | 0.412666000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=38, MCS=2, target RSSI=35; #2: AID=3, RU=54, MCS=2, target RSSI=35 |
| 412 | 0.416339000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=38, MCS=2, target RSSI=35 |
| 427 | 0.422158000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 437 | 0.425368000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35; #1: AID=1, RU=54, MCS=2, target RSSI=35 |
| 441 | 0.427071000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35; #1: AID=1, RU=54, MCS=2, target RSSI=35 |
| 459 | 0.434559000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35 |
| 475 | 0.439707000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=54, MCS=2, target RSSI=35 |
| 496 | 0.450282000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35 |
| 509 | 0.455976000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 525 | 0.461487000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35; #1: AID=1, RU=38, MCS=2, target RSSI=35; #2: AID=2, RU=39, MCS=2, target RSSI=35 |
| 535 | 0.465669000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 553 | 0.471528000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 564 | 0.475301000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 573 | 0.479919000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=0, RU=4, MCS=2, target RSSI=35; #2: AID=3, RU=54, MCS=2, target RSSI=35 |
| 584 | 0.484109000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35 |
| 593 | 0.487422000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35; #1: AID=2, RU=54, MCS=2, target RSSI=35 |
| 604 | 0.491701000 | 0 | #0: AID=1, RU=61, MCS=2, target RSSI=35 |
| 616 | 0.495883000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35; #1: AID=1, RU=54, MCS=2, target RSSI=35 |
| 625 | 0.500058000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 632 | 0.503253000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35 |
| 643 | 0.507136000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35; #1: AID=1, RU=38, MCS=2, target RSSI=35; #2: AID=3, RU=39, MCS=2, target RSSI=35 |
| 653 | 0.511331000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35; #2: AID=2, RU=54, MCS=2, target RSSI=35 |
| 659 | 0.513041000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 685 | 0.522999000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=54, MCS=2, target RSSI=35 |
| 697 | 0.526861000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 707 | 0.530064000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 728 | 0.538743000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 735 | 0.542483000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 749 | 0.548726000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=2, RU=4, MCS=2, target RSSI=35; #2: AID=3, RU=54, MCS=2, target RSSI=35 |
| 763 | 0.553510000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 771 | 0.558014000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 784 | 0.562177000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 791 | 0.564908000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 794 | 0.567036000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 805 | 0.569967000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 813 | 0.572706000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 816 | 0.575036000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 825 | 0.577839000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 836 | 0.581998000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 859 | 0.591141000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 867 | 0.594509000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 873 | 0.597811000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=1, RU=4, MCS=2, target RSSI=35; #2: AID=3, RU=54, MCS=2, target RSSI=35 |
| 881 | 0.601599000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=1, RU=4, MCS=2, target RSSI=35; #2: AID=3, RU=54, MCS=2, target RSSI=35 |
| 891 | 0.605251000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35 |
| 904 | 0.609192000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 917 | 0.615685000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35; #1: AID=1, RU=38, MCS=2, target RSSI=35; #2: AID=3, RU=39, MCS=2, target RSSI=35 |
| 928 | 0.619473000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=54, MCS=2, target RSSI=35 |
| 938 | 0.622787000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 949 | 0.625664000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 966 | 0.632694000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 980 | 0.638534000 | 0 | #0: AID=0, RU=0, MCS=2, target RSSI=35; #1: AID=1, RU=38, MCS=2, target RSSI=35; #2: AID=3, RU=39, MCS=2, target RSSI=35 |
| 989 | 0.642203000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=54, MCS=2, target RSSI=35 |
| 999 | 0.646387000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 1004 | 0.648090000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 1024 | 0.655652000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=38, MCS=2, target RSSI=35; #2: AID=3, RU=39, MCS=2, target RSSI=35 |

Showing the first 100 of 191 decoded Trigger frames; the script-owned packet metrics JSON preserves every row.

### [script] Configuration: `ImplicitBsr`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1472**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 336 | 22.83% | 628.1 B | 293.2 B | 355.3 us | 154.7 us | 5010 MHz | -72.0 dBm | - | 22.43% | 11.94% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 597 | 40.56% | 1143.8 B | 486.0 B | 661.7 us | 265.8 us | 5010 MHz | -72.0 dBm | - | 74.24% | 39.50% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 102 | 6.93% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -72.0 dBm | - | 0.54% | 0.29% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 90 | 6.11% | 140.0 B | 36.0 B | 66.7 us | 12.0 us | 5010 MHz | - | 10.0 dBm | 1.13% | 0.60% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 335 | 22.76% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 1.55% | 0.83% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.41% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.03% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.41% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.08% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.200484000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 2 | 0.201059000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 3 | 0.201107000 | ? → 0a:aa:00:00:00:02 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 5 | 0.201203000 | ? → 0a:aa:00:00:00:02 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 7 | 0.201318000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 8 | 0.201872000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 9 | 0.202543000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 10 | 0.202591000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 12 | 0.202687000 | ? → 0a:aa:00:00:00:01 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 14 | 0.202829000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 15 | 0.203356000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=1, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 16 | 0.203404000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 18 | 0.203500000 | ? → 0a:aa:00:00:00:03 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 20 | 0.203615000 | ? → 10:00:00:00:00:00 | Control: Ack / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges the preceding unicast frame. |
| 21 | 0.300484000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |
| 22 | 0.302484000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |

Frame numbers are local to capture `ImplicitBsr-#0HeBsrNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded HE Trigger user allocations

No HE Trigger User Info fields were decoded.

### [script] Configuration: `BurstyTraffic`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **1311**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 233 | 17.77% | 642.1 B | 283.0 B | 360.5 us | 150.9 us | 5010 MHz | -72.0 dBm | - | 17.52% | 8.40% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 380 | 28.99% | 1132.2 B | 521.2 B | 655.3 us | 285.1 us | 5010 MHz | -72.0 dBm | - | 51.93% | 24.90% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#31d62e" /></svg> | Data: QoS Data [HE-TB, HE-MCS 2, 106-tone RU, GI 3.2 us, LDPC, A-MPDU] | 40 | 3.05% | 768.1 B | 2.0 B | 661.5 us | 19.6 us | 5005 MHz, 5015 MHz | -75.0 dBm | - | 5.52% | 2.65% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#24ae34" /></svg> | Data: QoS Data [HE-TB, HE-MCS 2, 242-tone RU, GI 3.2 us, LDPC, A-MPDU] | 63 | 4.81% | 767.0 B | 1.7 B | 288.9 us | 16.3 us | 5010 MHz | -75.0 dBm | - | 3.79% | 1.82% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#37b52c" /></svg> | Data: QoS Data [HE-TB, HE-MCS 2, 52-tone RU, GI 3.2 us, LDPC, A-MPDU] | 57 | 4.35% | 770.0 B | 0.0 B | 1404.9 us | 0.0 us | 5003 MHz, 5007 MHz, 5013 MHz | -75.0 dBm | - | 16.70% | 8.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0c420b" /></svg> | Data: QoS Null [HE-TB, HE-MCS 0, 26-tone RU, GI 3.2 us, BCC, A-MPDU] | 12 | 0.92% | 34.0 B | 0.0 B | 398.7 us | 0.0 us | 5002 MHz, 5004 MHz, 5006 MHz | -75.0 dBm | - | 1.00% | 0.48% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#f99406" /></svg> | Control: Trigger | 100 | 7.63% | 36.6 B | 7.9 B | 32.2 us | 2.6 us | 5010 MHz | - | 10.0 dBm | 0.67% | 0.32% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 59 | 4.50% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -72.0 dBm | - | 0.34% | 0.17% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 147 | 11.21% | 71.4 B | 53.0 B | 43.8 us | 17.7 us | 5010 MHz | - | 10.0 dBm | 1.34% | 0.64% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 208 | 15.87% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 1.07% | 0.51% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.46% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.03% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.46% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -72.0 dBm | 10.0 dBm | 0.09% | 0.04% |

#### [script] Representative frame-exchange timeline

| Frame | Simulation time (s) | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 1 | 0.001048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 2 | 0.003064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=226 | Responds without MAC payload while preserving QoS control information. |
| 3 | 0.003064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=230 | Responds without MAC payload while preserving QoS control information. |
| 4 | 0.003064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=0, A-MPDU=234 | Responds without MAC payload while preserving QoS control information. |
| 5 | 0.003133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 6 | 0.104048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 7 | 0.106064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=418 | Responds without MAC payload while preserving QoS control information. |
| 8 | 0.106064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=422 | Responds without MAC payload while preserving QoS control information. |
| 9 | 0.106064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=1, frag=0, more-frag=0, TID=0, A-MPDU=426 | Responds without MAC payload while preserving QoS control information. |
| 10 | 0.106133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 11 | 0.207048000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Trigger / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Coordinates the following HE multi-user response. |
| 12 | 0.209064000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=610 | Responds without MAC payload while preserving QoS control information. |
| 13 | 0.209064000 | 0a:aa:00:00:00:02 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=614 | Responds without MAC payload while preserving QoS control information. |
| 14 | 0.209064000 | 0a:aa:00:00:00:03 → 10:00:00:00:00:00 | Data: QoS Null / HE-TB, HE-MCS 0, 26-tone RU, NSS 1, GI 3.2 us, BCC | direction=to DS, retry=0, seq=2, frag=0, more-frag=0, TID=0, A-MPDU=618 | Responds without MAC payload while preserving QoS control information. |
| 15 | 0.209133000 | 10:00:00:00:00:00 → ff:ff:ff:ff:ff:ff | Control: Block Ack (BA) / Legacy/HT/VHT | direction=direct/IBSS, retry=0, seq=-, frag=-, more-frag=0, TID=- | Acknowledges a preceding aggregate or scheduled transmission. |
| 16 | 0.300484000 | 0a:aa:00:00:00:01 → 10:00:00:00:00:00 | Data: QoS Data / HE-SU, HE-MCS 1, 20 MHz, NSS 1, GI 3.2 us, LDPC | direction=to DS, retry=0, seq=0, frag=0, more-frag=0, TID=6 | Carries protocol-visible MAC payload in the representative exchange. |

Frame numbers are local to capture `BurstyTraffic-#0HeBsrNetwork.ap.wlan[0].pcap`, not OMNeT++ event numbers. For readability, the table collapses observations with the same timestamp and MAC identity across capture interfaces; aggregate PCAP statistics retain the original observation counts.

#### [script] Decoded HE Trigger user allocations

| Frame | Simulation time (s) | Trigger type | Ordered user allocations |
|---:|---:|---:|---|
| 1 | 0.001048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 6 | 0.104048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 11 | 0.207048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 100 | 0.335408000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35 |
| 117 | 0.341279000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 120 | 0.343036000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 127 | 0.346637000 | 0 | #0: AID=1, RU=61, MCS=2, target RSSI=35 |
| 156 | 0.356002000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=54, MCS=2, target RSSI=35 |
| 164 | 0.358781000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35 |
| 179 | 0.365725000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 219 | 0.381419000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 233 | 0.385304000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 238 | 0.387653000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 243 | 0.391723000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 259 | 0.400365000 | 0 | #0: AID=3, RU=53, MCS=2, target RSSI=35 |
| 263 | 0.402052000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 274 | 0.406060000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 283 | 0.410020000 | 0 | #0: AID=3, RU=53, MCS=2, target RSSI=35 |
| 298 | 0.415529000 | 0 | #0: AID=3, RU=53, MCS=2, target RSSI=35 |
| 329 | 0.429629000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 342 | 0.436357000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 345 | 0.438044000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 353 | 0.441796000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 361 | 0.445964000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 389 | 0.457726000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 396 | 0.461328000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 404 | 0.465486000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 417 | 0.469416000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 425 | 0.473566000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 443 | 0.483114000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 446 | 0.485036000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 454 | 0.489223000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 467 | 0.495544000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35 |
| 494 | 0.512036000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 497 | 0.514036000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 500 | 0.516036000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 503 | 0.518036000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 506 | 0.520036000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 509 | 0.522036000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 512 | 0.524036000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 556 | 0.646048000 | 4 | #0: AID=1, RU=0, MCS=0, target RSSI=35; #1: AID=2, RU=1, MCS=0, target RSSI=35; #2: AID=3, RU=2, MCS=0, target RSSI=35; #3: AID=0, RU=3, MCS=0, target RSSI=35; #4: AID=0, RU=4, MCS=0, target RSSI=35; #5: AID=0, RU=5, MCS=0, target RSSI=35; #6: AID=0, RU=6, MCS=0, target RSSI=35; #7: AID=0, RU=7, MCS=0, target RSSI=35; #8: AID=0, RU=8, MCS=0, target RSSI=35 |
| 619 | 0.674228000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=2, RU=38, MCS=2, target RSSI=35; #2: AID=3, RU=39, MCS=2, target RSSI=35 |
| 635 | 0.680210000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35; #2: AID=1, RU=54, MCS=2, target RSSI=35 |
| 641 | 0.682036000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 661 | 0.688781000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 677 | 0.696367000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35; #1: AID=1, RU=54, MCS=2, target RSSI=35 |
| 685 | 0.700079000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35; #1: AID=1, RU=54, MCS=2, target RSSI=35 |
| 693 | 0.703706000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35 |
| 701 | 0.707049000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=38, MCS=2, target RSSI=35 |
| 712 | 0.711924000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 725 | 0.717616000 | 0 | #0: AID=1, RU=61, MCS=2, target RSSI=35 |
| 735 | 0.722702000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 754 | 0.729651000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 767 | 0.735195000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 779 | 0.739370000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 809 | 0.750731000 | 0 | #0: AID=1, RU=61, MCS=2, target RSSI=35 |
| 825 | 0.756585000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 837 | 0.763098000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35; #1: AID=1, RU=54, MCS=2, target RSSI=35 |
| 846 | 0.766441000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 849 | 0.769036000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 857 | 0.773670000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 865 | 0.776850000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 872 | 0.780497000 | 0 | #0: AID=1, RU=61, MCS=2, target RSSI=35 |
| 890 | 0.786654000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35 |
| 898 | 0.790930000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 905 | 0.793612000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 916 | 0.797232000 | 0 | #0: AID=2, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 927 | 0.800981000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 940 | 0.804858000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 952 | 0.810808000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 975 | 0.821601000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 985 | 0.825355000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 1004 | 0.831976000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35 |
| 1011 | 0.835587000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 1021 | 0.840323000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 1025 | 0.843881000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 1034 | 0.847203000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 1043 | 0.850951000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 1053 | 0.855134000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1090 | 0.874036000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1098 | 0.878212000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1106 | 0.881964000 | 0 | #0: AID=3, RU=53, MCS=2, target RSSI=35 |
| 1118 | 0.885943000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1149 | 0.900583000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 1161 | 0.903498000 | 0 | #0: AID=2, RU=61, MCS=2, target RSSI=35 |
| 1173 | 0.907259000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 1182 | 0.911097000 | 0 | #0: AID=2, RU=37, MCS=2, target RSSI=35 |
| 1190 | 0.915704000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 1202 | 0.919871000 | 0 | #0: AID=3, RU=61, MCS=2, target RSSI=35 |
| 1214 | 0.924559000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1218 | 0.927225000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1221 | 0.930036000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1228 | 0.933637000 | 0 | #0: AID=1, RU=53, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 1239 | 0.937830000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 1249 | 0.941987000 | 0 | #0: AID=1, RU=37, MCS=2, target RSSI=35; #1: AID=3, RU=54, MCS=2, target RSSI=35 |
| 1260 | 0.945325000 | 0 | #0: AID=3, RU=53, MCS=2, target RSSI=35 |
| 1265 | 0.947539000 | 0 | #0: AID=3, RU=53, MCS=2, target RSSI=35 |
| 1270 | 0.950163000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1273 | 0.953036000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |
| 1280 | 0.956664000 | 0 | #0: AID=3, RU=37, MCS=2, target RSSI=35 |

### [script] Analysis of Packet Distribution
The scheduled conditions contain the expected Trigger/response activity, but a BSR is an A-Control scheduling input rather than a frame subtype. IEEE Std 802.11-2024 Clause 26.5.5 requires the report contents and capability conditions; use the AP-reported and scheduled-backlog telemetry documented above. QoS Data counts are not evidence that a BSR was fresh or that the reported bytes were delivered.
<!-- END GENERATED: ieee80211ax-pcap-statistics -->

## [agent] Frame exchange analysis

<!-- BEGIN GENERATED ANALYSIS: frame-exchange -->
<!-- END GENERATED ANALYSIS: frame-exchange -->

The generated timeline begins with an AP Trigger, continues with HE-TB station
responses, and ends with acknowledgment activity. The decoded trigger-type
table shows both the periodic scheduling triggers and the triggers associated
with the observed uplink exchange. These PCAP rows show what was transmitted;
the trigger-decision vectors provide the corresponding BSR accounting. The
timeline is local to representative run 0, not an event-log reconstruction.

## [agent] Cross-layer findings and verdict

The four configurations hold the topology constant while varying how the AP
obtains or ages BSR state and how traffic is offered. Across seeds 0–4, every
retained scheduled trigger decision in `FreshBsr`, `BurstyTraffic`, and `StaleBsr` has an aligned ID, reported backlog,
and planned bytes. `ImplicitBsr` contributes report telemetry and packet
evidence, while its representative run has no scheduled-byte trace because
the 0.5 s trigger-check interval does not produce a scheduled exchange in that
run. This supports a scoped `PASS` for the accounting comparison and for
protocol-visible Trigger/HE-TB activity. The evidence does not show that the
reported bytes were all delivered or that the 10 ms policy is generally
better.

## [agent] Limitations and inconclusive claims

- The comparison is limited to this topology, workload, five seeds, and one
  10 ms freshness setting. Broader claims require additional freshness windows
  and matched workloads.
- BSR is not an application packet and is not necessarily a standalone frame
  subtype. Use AP decision telemetry together with the decoded trigger/data
  exchange.
- The PCAP capture is representative run-0 mechanism evidence; it does not
  replace the five-run scalar/vector evidence.
