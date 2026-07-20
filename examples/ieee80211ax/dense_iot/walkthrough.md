# Walkthrough - Dense IoT with 802.11ax OFDMA and TWT

This example compares an IEEE 802.11ax (Wi-Fi 6) dense-IoT treatment with a
matched IEEE 802.11ac (Wi-Fi 5) baseline. The first execution below is
deliberately limited to 128 stations and one repetition per configuration. It
exercises all three workloads and both technologies without starting the full
90-run campaign.

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
       sta[0]     sta[1]                 sta[127]
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

## First Execution: 128 Stations and One Run

From the INET project root, run:

```sh
python3 examples/ieee80211ax/dense_iot/run_campaign.py \
  --station-count 128 \
  --runs-per-station-count 1 \
  -j12
```

This Cmdenv execution selects run `0` for each of `AxUl`, `AcUl`, `AxDl`,
`AcDl`, `AxMixed`, and `AcMixed`, producing six simulations in total. The
`-j12` option runs all six conditions in parallel. The runner prints the exact
`bin/inet` command for each condition and writes its result files under
`examples/ieee80211ax/dense_iot/results/`.

The resulting filenames have this form:

```text
AxUl-numStations=128-#0.sca
AxUl-numStations=128-#0.vec
AcUl-numStations=128-#0.sca
...
```

To preview the six commands without running them, append `--dry-run`.

To execute only one condition instead, select its configuration and its
absolute run number directly. Run `0` is the 128-station, repetition-0 case:

```sh
python3 examples/ieee80211ax/dense_iot/run_campaign.py --config AxUl --run 0 -j12
```

## Verifying the First Execution

For the completed four-run analysis, confirm that these scalar result files
exist:

```sh
find examples/ieee80211ax/dense_iot/results -maxdepth 1 \
  -name '*-numStations=128-#0.sca' -print | sort
```

The list should contain `AxUl`, `AcUl`, `AxMixed`, and `AcMixed`. The
`AxDl`/`AcDl` downlink-only jobs do not have completed scalar files in this
result set. Inspect the application delivery counters and payload-byte sums for
the four completed configurations:

```sh
opp_scavetool query -l \
  -f 'name =~ "packetReceived:count" or name =~ "packetReceived:sum(packetBytes)"' \
  examples/ieee80211ax/dense_iot/results/*-numStations=128-#0.sca
```

Query the recorded end-to-end delay vectors separately:

```sh
opp_scavetool query -l \
  -f 'type =~ vector AND name =~ "endToEndDelay:vector"' \
  examples/ieee80211ax/dense_iot/results/*-numStations=128-#0.vec
```

For the AX treatment, verify that stations negotiated TWT agreements and that
the uplink workloads transmitted Basic Trigger frames:

```sh
opp_scavetool query -l \
  -f 'name =~ "twtAgreementCount" or name =~ "twtAwakeTime" or name =~ "twtSleepTime" or name =~ "heUlBasicTriggerSent:count"' \
  examples/ieee80211ax/dense_iot/results/Ax*-numStations=128-#0.sca
```

The completed `AxMixed` AP-radio vectors provide evidence of downlink HE
multi-user resource allocation:

```sh
opp_scavetool query -l \
  -f 'module =~ "**.ap.wlan[0].radio" AND (name =~ "heStaId:vector" or name =~ "heRuToneSize:vector")' \
  examples/ieee80211ax/dense_iot/results/AxMixed-numStations=128-#0.vec
```

Finally, inspect the final station-energy values used by the comparison:

```sh
opp_scavetool query -l \
  -f 'module =~ "**.sta[*].energyStorage" AND name =~ "residualEnergyCapacity:last"' \
  examples/ieee80211ax/dense_iot/results/*-numStations=128-#0.sca
```

Each station begins with 1000 J. Its consumption is therefore `1000 J` minus
the final residual-energy scalar. Delivery must be checked alongside energy:
lower consumption caused by undelivered traffic or missing associations is not
an efficiency improvement.

## Result Analysis: Four Completed Run-0 Conditions

The tables below use the four completed `numStations=128`, run-0 result pairs:

| Configuration | `.sca` / `.vec` run identifier |
|---|---|
| `AxUl` | `AxUl-0-20260719-19:34:58-127` |
| `AcUl` | `AcUl-0-20260719-19:34:58-121` |
| `AxMixed` | `AxMixed-0-20260719-19:34:58-126` |
| `AcMixed` | `AcMixed-0-20260719-19:34:58-125` |

The scalar filters are `packetReceived:count`,
`packetReceived:sum(packetBytes)`, and `residualEnergyCapacity:last`. The
vector filter is `endToEndDelay:vector`. Delay samples are pooled per
configuration over the 20–120 s measurement interval; goodput is received
payload bits divided by that 100 s interval. Mean station energy is
`1000 J - residualEnergyCapacity:last`, averaged over the 128 STAs.

Over the 100 s measurement interval, the offered packet counts are:

| Workload | Direction | Offered packets |
|---|---:|---:|
| Uplink | UL | 12,800 |
| Downlink | DL | 12,800 |
| Mixed | UL | 12,800 |
| Mixed | DL | 1,280 |

### Application and energy summary

This table uses the aggregate row for each workload. For Mixed, aggregate
delay pools both directions within the run.

| Configuration | Delivered / offered | Delivery ratio | Goodput | Mean delay | p95 delay | Mean station energy |
|---|---:|---:|---:|---:|---:|---:|
| `AxUl` | 12,196 / 12,800 | 95.281% | 97.568 kbps | 2.398783 s | 5.079606 s | 0.160515 J |
| `AcUl` | 12,785 / 12,800 | 99.883% | 102.280 kbps | 0.165148 s | 1.000254 s | 0.251881 J |
| `AxMixed` | 13,326 / 14,080 | 94.645% | 106.608 kbps | 2.389380 s | 5.032261 s | 0.160242 J |
| `AcMixed` | 14,055 / 14,080 | 99.822% | 112.440 kbps | 0.134933 s | 0.000379 s | 0.252939 J |

The direction-specific Mixed rows explain the aggregate delivery ratios:

| Configuration | UL delivered / offered | DL delivered / offered | UL mean delay | DL mean delay |
|---|---:|---:|---:|---:|
| `AxMixed` | 12,184 / 12,800 | 1,142 / 1,280 | 2.607984 s | 0.057101 s |
| `AcMixed` | 12,775 / 12,800 | 1,280 / 1,280 | 0.148447 s | 0.000063 s |

### TWT and OFDMA evidence

The AX scalar results show one active individual TWT agreement for every STA.
The awake and sleep values below are per-station means over the 128 stations;
the configured 5 ms service duration is quantized in the TWT exchange, and
management/setup activity contributes to the measured totals.

| AX configuration | Active agreements | Mean awake time | Mean sleep time | Basic Triggers | BSRP Triggers | Scheduled users (sum) |
|---|---:|---:|---:|---:|---:|---:|
| `AxUl` | 128 / 128 | 13.866579 s | 106.133421 s | 8,626 | 172 | 42,578 |
| `AxMixed` | 128 / 128 | 14.106729 s | 105.893271 s | 8,801 | 208 | 43,711 |

The `AxMixed` AP-radio vectors contain 31 `heStaId:vector` observations and 31
matching `heRuToneSize:vector` observations. Every recorded RU is 52 tones in
this run. `AxUl` has no downlink application load, so it has no corresponding
downlink MU vector in the completed four-run set.

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

In this seed, AX consumed about 36% less mean station energy than its matched
AC condition (`0.160515` versus `0.251881 J` for UL; `0.160242` versus
`0.252939 J` for Mixed), while also delivering fewer packets and showing much
higher delay. That is consistent with stations spending most of the run asleep,
but it is not evidence that AX is generally slower or less reliable: the AX
and AC values above are one repetition with randomized placement, association,
traffic phase, and MAC contention. One repetition is suitable for checking that
the scenario runs and produces the expected evidence, not for a statistical
performance claim. Do not report confidence intervals from this table.

## Running and Analyzing the Full Campaign

After the first execution succeeds, run the complete comparison only if the
additional time and memory cost are acceptable:

```sh
python3 examples/ieee80211ax/dense_iot/run_campaign.py -j12
```

The full campaign contains six configurations, three station counts, and five
repetitions: 90 simulations. The `-j12` option runs up to 12 simultaneously;
make sure the machine has enough memory before using that concurrency with the
512-station cases.

Once all 90 runs are present, generate the CSV summaries and dashboard:

```sh
MPLCONFIGDIR=/tmp/matplotlib \
python3 examples/ieee80211ax/dense_iot/analyze.py
```

The analysis script intentionally validates the complete campaign before
computing cross-run means and 95% Student-t confidence intervals. It will reject
the four-result first execution as incomplete; use the direct checks and tables
above for this initial run.
