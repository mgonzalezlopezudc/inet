# Walkthrough - Dense IoT with 802.11ax OFDMA and TWT

This example compares an IEEE 802.11ax (Wi-Fi 6) dense-IoT treatment with a
matched IEEE 802.11ac (Wi-Fi 5) baseline. The first execution below is
deliberately limited to 64 stations and one repetition per configuration. It
exercises all three workloads and both technologies without starting the full
120-run campaign.

## Background: Dense IoT in 802.11ax

Dense IoT networks combine many low-rate sources into a demanding MAC-layer
workload. Although an individual sensor sends little data, synchronized channel
access, management exchanges, and idle listening become expensive when
hundreds of stations share one BSS.

The 802.11ax configurations use three mechanisms intended for this setting:

1. **Uplink and downlink OFDMA** divide the 20 MHz channel into resource units
   so the AP can coordinate transmissions involving several stations.
2. **Trigger-based uplink access** lets the AP schedule stations with reported
   backlog and provide random-access resource units for discovery and
   contention.
3. **Individual Target Wake Time (TWT)** gives each station a negotiated wake
   schedule. A station can sleep outside its service periods instead of idle
   listening continuously.

The 802.11ac baseline uses single-user EDCA, AARF rate control, and no TWT. It
retains the same topology, offered traffic, radio channel, propagation model,
and energy-consumption parameters, making it the control condition rather than
a separate scenario.

## Network Topology

The network in [DenseIotNetwork.ned](DenseIotNetwork.ned) contains one
infrastructure BSS:

- **`ap`** is fixed at the center of the deployment area.
- **`sta[0..numStations-1]`** are stationary wireless IoT devices placed
  uniformly in a 360 m by 360 m square around the AP.
- **`server`** is connected to the AP by a 100 Gbit/s Ethernet link and acts as
  the wired traffic endpoint.
- **`radioMedium`** models the shared 5 GHz, 20 MHz wireless channel.

```text
       sta[0]     sta[1]                  sta[63]
          \          |                    /
           \         |   5 GHz BSS       /
            +--------+------ ap --------+
                               |
                               | 100 Gbit/s Ethernet
                               |
                            server
```

Station placement, association timing, and application phases use separate
random-number streams. An AX/AC pair with the same repetition therefore uses
the same station coordinates and offered-load phases even though its MAC
operation consumes random numbers differently.

## Workloads and Configurations

The settings are defined in [omnetpp.ini](omnetpp.ini). Every concrete
configuration runs for 120 s and uses the first 20 s as warm-up time.

| Workload | 802.11ax treatment | 802.11ac baseline | Offered traffic per station |
|---|---|---|---|
| Uplink | `AxUl` | `AcUl` | One 100-byte UDP payload per second from each STA |
| Downlink | `AxDl` | `AcDl` | One 100-byte UDP payload per second to each STA |
| Mixed | `AxMixed` | `AcMixed` | Uplink once per second and downlink once per 10 s |

All application start times are independently phase-shifted. Association and
TWT setup attempts are also spread across the first 15 s so that the experiment
does not begin with one synchronized management-frame burst.

### 802.11ax treatment

The `Ax*` configurations use HE Minstrel with MCS 0 through 11 and one spatial
stream. The AP enables backlog-based downlink and uplink OFDMA schedulers with
up to eight scheduled stations per transmission. Uplink scheduling includes
random-access resource units and applies a 10 ms minimum interval between
transmitted Trigger frames.

Every station requests one implicit, individual, unannounced TWT agreement. The
configured wake interval is 100 ms and the nominal wake duration is 5 ms; the
duration carried by the TWT element is quantized to 5.12 ms. Randomized setup
times distribute the service-period phases across the BSS.

### 802.11ac baseline

The `Ac*` configurations use one-stream AARF rate control and ordinary
single-user HCF/EDCA access. TWT is absent, so stations remain available to the
channel rather than following negotiated sleep schedules.

## First Execution: 64 Stations and One Run

From the INET project root, run:

```sh
python3 examples/ieee80211ax/dense_iot/run_campaign.py \
  --station-count 64 \
  --runs-per-station-count 1 \
  -j12
```

This Cmdenv execution selects run `15` for each of `AxUl`, `AcUl`, `AxDl`,
`AcDl`, `AxMixed`, and `AcMixed`, producing six simulations in total. The
`-j12` option runs all six conditions in parallel. The runner prints the exact
`bin/inet` command for each condition and writes its result files under
`examples/ieee80211ax/dense_iot/results/`.

The resulting filenames have this form:

```text
AxUl-numStations=64-#0.sca
AxUl-numStations=64-#0.vec
AcUl-numStations=64-#0.sca
...
```

To preview the six commands without running them, append `--dry-run`.

To execute only one condition instead, select its configuration and its
absolute run number directly. Because 64 was appended to preserve the existing
run-number mapping, run `15` is the 64-station, repetition-0 case:

```sh
python3 examples/ieee80211ax/dense_iot/run_campaign.py --config AxUl --run 15 -j12
```

## Verifying the First Execution

For the completed six-condition analysis, confirm that these scalar result
files exist:

```sh
find examples/ieee80211ax/dense_iot/results -maxdepth 1 \
  -name '*-numStations=64-#0.sca' -print | sort
```

The list should contain `AxUl`, `AcUl`, `AxDl`, `AcDl`, `AxMixed`, and
`AcMixed`. Inspect the application delivery counters and payload-byte sums:

```sh
opp_scavetool query -l \
  -f 'name =~ "packetReceived:count" or name =~ "packetReceived:sum(packetBytes)"' \
  examples/ieee80211ax/dense_iot/results/*-numStations=64-#0.sca
```

Query the recorded end-to-end delay vectors separately:

```sh
opp_scavetool query -l \
  -f 'type =~ vector AND name =~ "endToEndDelay:vector"' \
  examples/ieee80211ax/dense_iot/results/*-numStations=64-#0.vec
```

For the AX treatment, verify that stations negotiated TWT agreements and that
the uplink workloads transmitted Basic Trigger frames:

```sh
opp_scavetool query -l \
  -f 'name =~ "twtAgreementCount" or name =~ "twtAwakeTime" or name =~ "twtSleepTime" or name =~ "heUlBasicTriggerSent:count"' \
  examples/ieee80211ax/dense_iot/results/Ax*-numStations=64-#0.sca
```

The completed `AxDl` and `AxMixed` AP-radio vectors provide evidence of
downlink HE multi-user resource allocation:

```sh
opp_scavetool query -l \
  -f 'module =~ "**.ap.wlan[0].radio" AND (name =~ "heStaId:vector" or name =~ "heRuToneSize:vector")' \
  examples/ieee80211ax/dense_iot/results/Ax{Dl,Mixed}-numStations=64-#0.vec
```

Finally, inspect the final station-energy values used by the comparison:

```sh
opp_scavetool query -l \
  -f 'module =~ "**.sta[*].energyStorage" AND name =~ "residualEnergyCapacity:last"' \
  examples/ieee80211ax/dense_iot/results/*-numStations=64-#0.sca
```

Each station begins with 1000 J. Its consumption is therefore `1000 J` minus
the final residual-energy scalar. Delivery must be checked alongside energy:
lower consumption caused by undelivered traffic or missing associations is not
an efficiency improvement.

## Result Analysis: 64 Stations, Repetition 0

The tables below use the completed `numStations=64`, run-15 result pairs:

| Configuration | `.sca` / `.vec` run identifier |
|---|---|
| `AxUl` | `AxUl-15-20260720-13:38:40-135` |
| `AcUl` | `AcUl-15-20260720-13:38:40-136` |
| `AxDl` | `AxDl-15-20260720-13:53:41-32` |
| `AcDl` | `AcDl-15-20260720-13:38:40-133` |
| `AxMixed` | `AxMixed-15-20260720-13:38:40-137` |
| `AcMixed` | `AcMixed-15-20260720-13:38:40-132` |

The scalar filters are `packetReceived:count`,
`packetReceived:sum(packetBytes)`, and `residualEnergyCapacity:last`. The
vector filter is `endToEndDelay:vector`. Delay samples are cropped to
`[20,120)` and pooled per configuration; goodput is received payload bits
divided by that 100 s interval. Mean station energy subtracts the final
`residualEnergyCapacity:last` from 1000 J, averages over the 64 STAs, and
covers the full 0–120 s run.

Over the 100 s measurement interval, the offered packet counts are:

| Workload | Direction | Offered packets |
|---|---:|---:|
| Uplink | UL | 6,400 |
| Downlink | DL | 6,400 |
| Mixed | UL | 6,400 |
| Mixed | DL | 640 |

### Application and energy summary

This table uses the aggregate row for each workload. For Mixed, aggregate
delay pools both directions within the run.

| Configuration | Delivered / offered | Delivery ratio | Goodput | Mean delay | p95 delay | Mean station energy |
|---|---:|---:|---:|---:|---:|---:|
| `AxUl` | 6,072 / 6,400 | 94.8750% | 48.576 kbps | 2.175482 s | 4.093415 s | 0.153198 J |
| `AcUl` | 6,391 / 6,400 | 99.8594% | 51.128 kbps | 0.082412 s | 0.000342 s | 0.246854 J |
| `AxDl` | 2,601 / 6,400 | 40.6406% | 20.808 kbps | 2.086516 s | 13.190793 s | 0.135516 J |
| `AcDl` | 6,341 / 6,400 | 99.0781% | 50.728 kbps | 0.000066 s | 0.000061 s | 0.245340 J |
| `AxMixed` | 6,754 / 7,040 | 95.9375% | 54.032 kbps | 2.090061 s | 5.017219 s | 0.154087 J |
| `AcMixed` | 7,036 / 7,040 | 99.9432% | 56.288 kbps | 0.022988 s | 0.000326 s | 0.247388 J |

The direction-specific Mixed rows explain the aggregate delivery ratios:

| Configuration | UL delivered / offered | DL delivered / offered | UL mean / p95 delay | DL mean / p95 delay |
|---|---:|---:|---:|---:|
| `AxMixed` | 6,116 / 6,400 | 638 / 640 | 2.295899 / 5.027717 s | 0.116860 / 0.129750 s |
| `AcMixed` | 6,396 / 6,400 | 640 / 640 | 0.025281 / 0.000326 s | 0.000064 / 0.000061 s |

### TWT and OFDMA evidence

The AX scalar results show one active individual TWT agreement for every STA.
The awake and sleep values below are per-station means over the 64 stations;
the configured 5 ms service duration is quantized in the TWT exchange, and
management/setup activity contributes to the measured totals.

| AX configuration | Active agreements | Mean awake time | Mean sleep time | Basic Triggers | BSRP Triggers | Scheduled users (sum) |
|---|---:|---:|---:|---:|---:|---:|
| `AxUl` | 64 / 64 | 13.148720 s | 106.851280 s | 8,504 | 686 | 40,436 |
| `AxDl` | 64 / 64 | 12.566553 s | 107.433447 s | 0 | 0 | 0 |
| `AxMixed` | 64 / 64 | 12.908501 s | 107.091499 s | 8,669 | 564 | 40,940 |

The `AxDl` AP-radio vectors contain 50 paired `heStaId:vector` and
`heRuToneSize:vector` samples across 24 allocation timestamps. Twenty-two
allocations contain two users and two contain three; all recorded RUs have 52
tones. The `AxMixed` vectors contain eight paired samples: six allocate 52-tone
RUs to STAs 30 and 61, while the other two allocate a 26-tone RU to STA 24 and
a 106-tone RU to STA 60. This is direct evidence of downlink HE multi-user
allocation. `AxUl` has no downlink application load, so it has no corresponding
downlink MU vector. The HE rate-control vectors span MCS 0 through 11 in
`AxUl` and `AxMixed`; `AxDl` has no nonempty selected-MCS vector. The AC
rate-change vectors contain 78 Mbit/s in the completed AC runs.

### Interpretation of this run

Use the received-packet counters to calculate delivery ratio, the payload-byte
sums to calculate goodput over 100 s, and the delay vectors to compare latency.
Select only the receiving applications: `server.app[0]` for uplink,
`sta[*].app[0]` for downlink, and, in the mixed workload, `server.app[0]` for
uplink plus `sta[*].app[1]` for downlink. Sender applications also emit
zero-valued receive scalars and must not be mistaken for failed delivery.
For AX, the TWT scalars establish agreement coverage and awake/sleep time; the
Trigger counters and AP-radio HE resource-unit vectors establish that the
configured OFDMA mechanisms were active.

In this seed, AX consumed less mean station energy than its matched AC
condition: about 38% less for UL (`0.153198` versus `0.246854 J`), 45% less
for DL (`0.135516` versus `0.245340 J`), and 38% less for Mixed (`0.154087`
versus `0.247388 J`). AX also delivered fewer packets and showed much higher
delay, most sharply in DL where it delivered only 40.64% of the offered load.
That is consistent with stations spending most of the run asleep, but it is
not evidence that AX is generally slower or less reliable: the AX and AC
values above are one repetition with randomized placement, association,
traffic phase, and MAC contention. One repetition is suitable for checking
that the scenario runs and produces the expected evidence, not for a
statistical performance claim. Do not report confidence intervals from this
table.

## Running and Analyzing the Full Campaign

After the first execution succeeds, run the complete comparison only if the
additional time and memory cost are acceptable:

```sh
python3 examples/ieee80211ax/dense_iot/run_campaign.py -j12
```

The full campaign contains six configurations, four station counts, and five
repetitions: 120 simulations. The `-j12` option runs up to 12 simultaneously;
make sure the machine has enough memory before using that concurrency with the
512-station cases.

Once all 120 runs are present, generate the CSV summaries and dashboard:

```sh
MPLCONFIGDIR=/tmp/matplotlib \
python3 examples/ieee80211ax/dense_iot/analyze.py
```

The analysis script intentionally validates the complete campaign before
computing cross-run means and 95% Student-t confidence intervals. It will reject
the six-condition, single-repetition first execution as incomplete; use the
direct checks and tables above for this initial run.
