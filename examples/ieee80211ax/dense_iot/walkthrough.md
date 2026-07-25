# Walkthrough - Dense IoT with 802.11ax OFDMA and TWT

This example compares an IEEE 802.11ax dense-IoT treatment with a matched
IEEE 802.11ac baseline. The AX configurations enable OFDMA and individual
Target Wake Time (TWT); the AC configurations use single-user EDCA and do not
enable TWT.

The retained scalar/vector session is
`results/scalar-vector/20260725T120411Z/`. Its complete result set consists of
five `AxUl` and five `AcUl` repetitions at each of 8 and 16 stations. The
session does not yet contain complete `.sca`/`.vec` pairs for `AxDl`, `AcDl`,
`AxMixed`, or `AcMixed`, so this walkthrough reports numeric results only for
the completed uplink configurations. There is no `results/packet-statistics/`
directory in this example, so no Dense IoT packet-capture claim is made.

## Topology

The network in [DenseIotNetwork.ned](DenseIotNetwork.ned) contains one
infrastructure BSS:

- `ap` is fixed at the deployment center.
- `sta[0..numStations-1]` are stationary devices placed uniformly in a
  360 m by 360 m square around the AP.
- `server` is connected to the AP through a 100 Gbit/s Ethernet link.
- `radioMedium` models the shared 5 GHz, 20 MHz wireless channel.

```text
       sta[0]     sta[1]           sta[numStations-1]
          \          |                    /
           \         |   5 GHz BSS       /
            +--------+------ ap --------+
                               |
                            server
```

Station placement, association timing, and application phases use separate
random-number streams. An AX/AC pair with the same repetition therefore uses
the same station coordinates and offered-load phases, although MAC operation
can consume random numbers differently.

## Workloads and configurations

The settings are in [omnetpp.ini](omnetpp.ini). Concrete configurations run
for 120 s and use the first 20 s as warm-up time.

| Workload | AX configuration | AC configuration | Offered traffic per station |
|---|---|---|---|
| Uplink | `AxUl` | `AcUl` | one 100-byte UDP payload per second |
| Downlink | `AxDl` | `AcDl` | one 100-byte UDP payload every 100 ms |
| Mixed | `AxMixed` | `AcMixed` | both uplink and downlink loads |

The `Ax*` configurations use HE Minstrel with one spatial stream. The AP
enables backlog-based downlink and uplink OFDMA scheduling with up to eight
scheduled stations per transmission. Uplink scheduling includes random-access
RUs and a 10 ms minimum Trigger interval.

Each AX station requests one implicit, individual, unannounced TWT agreement.
The configured wake interval is 100 ms and the nominal wake duration is 5 ms.
The AC configurations use one-stream AARF and ordinary single-user HCF/EDCA.

## Reproduce the campaign

From the INET project root:

```sh
python3 examples/ieee80211ax/dense_iot/run_campaign.py \
  --station-counts 8,16 \
  --runs-per-station-count 5 \
  -j12
```

Results are written under
`examples/ieee80211ax/dense_iot/results/scalar-vector/YYYYMMDDTHHMMSSZ/<configuration>/`.
To run a single 16-station AX uplink repetition:

```sh
python3 examples/ieee80211ax/dense_iot/run_campaign.py \
  --config AxUl --run 5
```

Query the uplink sink counts and delays directly:

```sh
opp_scavetool query -l \
  -f 'module =~ "*.server.app[0]" and (name =~ "packetReceived:count" or name =~ "endToEndDelay:vector")' \
  examples/ieee80211ax/dense_iot/results/scalar-vector/20260725T120411Z/AxUl/*.{sca,vec} \
  examples/ieee80211ax/dense_iot/results/scalar-vector/20260725T120411Z/AcUl/*.{sca,vec}
```

Query energy, TWT, and uplink scheduling scalars:

```sh
opp_scavetool query -l \
  -f 'name =~ "residualEnergyCapacity:last" or name =~ "twtAgreementCount" or name =~ "twtAwakeTime" or name =~ "twtSleepTime" or name =~ "heUlBasicTriggerSent:count"' \
  examples/ieee80211ax/dense_iot/results/scalar-vector/20260725T120411Z/AxUl/*.sca \
  examples/ieee80211ax/dense_iot/results/scalar-vector/20260725T120411Z/AcUl/*.sca
```

## Completed uplink results

The two tables in this section use the 20 `.sca` files and their 20 matching
`.vec` files under
`results/scalar-vector/20260725T120411Z/{AxUl,AcUl}/`. Received packets are
the `DenseIotNetwork.server.app[0] packetReceived:count` scalars. Mean delay is
the summary mean of that module's `endToEndDelay:vector`. Mean station energy
used is the station average of
`1000 J - residualEnergyCapacity:last`.

### Eight stations

| Repetition | AX received | AC received | AX mean delay | AC mean delay | AX mean station energy | AC mean station energy |
|---:|---:|---:|---:|---:|---:|---:|
| 0 | 700 | 800 | 0.028219 s | 0.000105 s | 0.135945 J | 0.242398 J |
| 1 | 800 | 800 | 0.020160 s | 0.000104 s | 0.134993 J | 0.242427 J |
| 2 | 800 | 800 | 0.046847 s | 0.000104 s | 0.136388 J | 0.242382 J |
| 3 | 800 | 800 | 0.040412 s | 0.000104 s | 0.136055 J | 0.242359 J |
| 4 | 800 | 800 | 0.047810 s | 0.000103 s | 0.135473 J | 0.242431 J |
| Five-run mean | 780.0 | 800.0 | 0.036690 s | 0.000104 s | 0.135771 J | 0.242400 J |

The eight-station table yields a 43.99% reduction in mean station energy for
`AxUl` relative to `AcUl`. The received-count mean is lower for `AxUl`, so the
energy comparison must be read together with the delivery and delay columns.

### Sixteen stations

| Repetition | AX received | AC received | AX mean delay | AC mean delay | AX mean station energy | AC mean station energy |
|---:|---:|---:|---:|---:|---:|---:|
| 0 | 1,570 | 1,600 | 1.345015 s | 0.000104 s | 0.135930 J | 0.242981 J |
| 1 | 1,565 | 1,592 | 1.325530 s | 0.431639 s | 0.136496 J | 0.243095 J |
| 2 | 1,613 | 1,607 | 1.037000 s | 0.028115 s | 0.135499 J | 0.243096 J |
| 3 | 1,600 | 1,600 | 0.048280 s | 0.000104 s | 0.135194 J | 0.243066 J |
| 4 | 1,598 | 1,600 | 0.120907 s | 0.000103 s | 0.137231 J | 0.243062 J |
| Five-run mean | 1,589.2 | 1,599.8 | 0.775346 s | 0.091813 s | 0.136070 J | 0.243060 J |

The sixteen-station table yields a 44.02% reduction in mean station energy
for `AxUl` relative to `AcUl`. Some receive counts exceed the nominal 1,600
packets generated during the 100 s measurement interval because packets
generated during warm-up can be delivered after recording begins. These
counts must therefore not be divided by 1,600 and presented as delivery
ratios.

## Mechanism checks

Every station in each of the ten `AxUl` `.sca` files under
`results/scalar-vector/20260725T120411Z/AxUl/` has
`twtAgreementCount = 1`. Those scalars directly establish that the configured
TWT agreements were present in every completed AX uplink run.

The same ten files contain the following
`DenseIotNetwork.ap.wlan[0].mac.hcf.ulCoordinator
heUlBasicTriggerSent:count` values:

| Stations | Repetitions 0-4 |
|---:|---|
| 8 | 170, 0, 0, 0, 0 |
| 16 | 10, 10, 10, 0, 440 |

This table is direct scheduling telemetry. It shows that Basic Triggers were
sent in five of the ten completed `AxUl` runs; it does not support a claim
that trigger-based uplink scheduling occurred in every repetition.

## Interpretation limits

The completed session supports a five-repetition uplink comparison at two
station counts. It does not support numeric downlink or Mixed conclusions.
The energy reduction is a result for these paired runs, not a general
AX-versus-AC efficiency guarantee. Delivery, delay, and energy must be
considered together.

When inspecting application results, use the receiving application only:
`server.app[0]` for uplink. Sender applications also emit zero-valued receive
scalars and must not be interpreted as failed delivery.
