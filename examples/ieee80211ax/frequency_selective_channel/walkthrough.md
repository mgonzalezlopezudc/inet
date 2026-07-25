# Walkthrough - 802.11ax on frequency-selective channels

This example exercises IEEE 802.11ax resource allocation with a dimensional
radio model. The model retains power spectral density as a function of
frequency, allowing reception calculations to be narrowed to the assigned RU.

The artifact-backed analysis below uses two result families:

- `results/scalar-vector/20260725T120411Z/` contains run-0 `.sca` and `.vec`
  files for `FlatChannelOFDMA` and `TgaxModelBOFDMA`.
- `results/packet-statistics/20260724T175025Z/` contains run-0 `.sca`, `.vec`,
  and PCAP files for the same configurations; only its AP PCAP files are used
  in the packet table below.

Each configuration has one scalar/vector run in the named scalar-vector
session. These artifacts do not provide multi-run evidence and do not
establish throughput advantages for the notch, interferer, puncturing, or
sweep configurations.

## Topology and traffic

`FrequencySelectiveChannelNetwork` contains a wired server, one AP, and four
equidistant stations. Configurations may add a legacy interferer 21.2 m from
the AP. The symmetric station geometry removes distance as the intended source
of station-to-station differences.

Four staggered bursts establish Block Ack state before measured traffic starts
at 0.3 s. Each measured flow sends a 100-byte UDP packet every 0.25 ms, for
12.8 Mbit/s aggregate offered load. The AP is capped at HE MCS 1.

The main 40 MHz interference configurations use these bands:

| Signal | Frequency range | Role |
|---|---:|---|
| HE BSS | 5.16-5.20 GHz | 40 MHz channel centered at 5.18 GHz |
| Lower-half interferer | 5.16-5.18 GHz | 20 MHz channel centered at 5.17 GHz |
| Upper-half interferer | 5.18-5.20 GHz | 20 MHz channel centered at 5.19 GHz |

The interferer sends 1,000-byte frames with exponentially distributed 0.5 ms
mean intervals. Silent-control configurations instantiate the same interferer
but start its application after the simulation limit.

## Why the dimensional model matters

[omnetpp.ini](omnetpp.ini) selects `Ieee80211DimensionalRadioMedium` and
`Ieee80211DimensionalRadio`. For HE MU reception, the medium resolves a
station's RU from the HE PHY header and filters signal and noise power to the
RU bandwidth.

This design permits an impairment that overlaps one part of a wide channel to
affect overlapping RUs without automatically being assigned to every RU. The
receiver energy-detection threshold is set to -40 dBm to focus the scenarios
on PHY decoding and per-RU SNIR rather than CCA deferral. This threshold is a
scenario choice, not a regulatory CCA setting.

## Scenario catalogue

### Flat and partially overlapping channels

| Configuration pair | Width or impairment |
|---|---|
| `FlatChannelOFDMA` / `FlatChannelSU` | flat 80 MHz |
| `FortyMHzFlatOFDMA` / `FortyMHzFlatSU` | flat 40 MHz |
| `TwentyMHzFlatOFDMA` / `TwentyMHzFlatSU` | flat 20 MHz |
| `FortyMHzSilentInterfererOFDMA` / `FortyMHzSilentInterfererSU` | 40 MHz with silent interferer |
| `FortyMHzLowerHalfOFDMA` / `FortyMHzLowerHalfSU` | lower 20 MHz impaired |
| `FortyMHzUpperHalfOFDMA` / `FortyMHzUpperHalfSU` | upper 20 MHz impaired |

### Synthetic frequency-domain profiles

`Lower20MHzNotch*`, `MiddleLower20MHzNotch*`,
`MiddleUpper20MHzNotch*`, and `Upper20MHzNotch*` move a high-noise 20 MHz
slice across an 80 MHz channel. `NotchDepthSweepOFDMA` and
`NotchDepthSweepSU` vary the configured total noise power. These profiles
model a frequency-dependent noise/SNIR condition, not a multipath impulse
response.

### TGax Model B variants

`TgaxModelBOFDMA` and `TgaxModelBSU` select a reciprocal SISO TGax indoor
Model B realization sampled on the HE subcarrier grid, with
`TgaxIndoorPathLoss` supplying median distance loss.

Other opt-in configurations exercise ambient Doppler, selected-antenna
SIMO-MRC, covariance-aware SIMO L-MMSE, and RBIR:

- `TgaxModelBAmbientDopplerOFDMA`
- `TgaxModelBSimoMrcOFDMA`
- `TgaxModelBSimoLmmseInterferenceOFDMA`
- `TgaxModelBRbirOFDMA`

The RBIR configuration requires calibration extracted from a user-supplied
IEEE 802.11-14/0571r12 workbook:

```sh
../../../bin/inet_extract_tgax_rbir_calibration.py \
  /path/to/Microsoft_Excel_Worksheet1.xlsx /tmp/tgax-rbir.txt
```

### Puncturing and transient interference

The synthetic puncturing configurations are:

- `PuncturingCleanOFDMA`
- `PuncturingImpairedUnpunctured`
- `PuncturingImpairedPunctured`

The punctured case configures mask `0100`, represented as numeric value 2 in
`hePuncturedSubchannelMask`.

The real-interferer configurations include `NarrowbandInterference*` and
`TransientInterference*`. The scripted transient variant changes a
predetermined mask at configured times; it is not an autonomous channel
selection algorithm.

## Run the scenarios

From this example directory, run one configuration and seed:

```sh
../../../bin/inet --release -u Cmdenv -f omnetpp.ini \
  -c FortyMHzLowerHalfOFDMA -r 0 --seed-set=0 \
  --result-dir=results/forty-lower/ofdma/seed0
```

Run the static TGax Model B configuration:

```sh
../../../bin/inet --release -u Cmdenv -f omnetpp.ini \
  -c TgaxModelBOFDMA -r 0 --seed-set=0 \
  --result-dir=results/tgax/model-b/nist/seed0
```

Run the ambient-Doppler variant:

```sh
../../../bin/inet --release -u Cmdenv -f omnetpp.ini \
  -c TgaxModelBAmbientDopplerOFDMA -r 0 --seed-set=0 \
  --result-dir=results/tgax/model-b/ambient-doppler/seed0
```

Run the SIMO variants:

```sh
../../../bin/inet --release -u Cmdenv -f omnetpp.ini \
  -c TgaxModelBSimoMrcOFDMA -r 0 --seed-set=0 \
  --result-dir=results/tgax/model-b/simo-mrc/seed0

../../../bin/inet --release -u Cmdenv -f omnetpp.ini \
  -c TgaxModelBSimoLmmseInterferenceOFDMA -r 0 --seed-set=0 \
  --result-dir=results/tgax/model-b/simo-lmmse-interference/seed0
```

Run calibrated RBIR after extracting the calibration:

```sh
../../../bin/inet --release -u Cmdenv -f omnetpp.ini \
  -c TgaxModelBRbirOFDMA -r 0 --seed-set=0 \
  '--**.wlan[*].radio.receiver.errorModel.calibrationFile="/tmp/tgax-rbir.txt"' \
  --result-dir=results/tgax/model-b/rbir/seed0
```

## Query the scalar/vector results

The two scalar inputs are:

- [`FlatChannelOFDMA-#0.sca`](results/scalar-vector/20260725T120411Z/FlatChannelOFDMA/FlatChannelOFDMA-%230.sca)
- [`TgaxModelBOFDMA-#0.sca`](results/scalar-vector/20260725T120411Z/TgaxModelBOFDMA/TgaxModelBOFDMA-%230.sca)

Query their sink counts:

```sh
opp_scavetool query -l \
  -f 'name =~ "packetReceived:count" and module =~ "*.host[*].app[0]"' \
  results/scalar-vector/20260725T120411Z/FlatChannelOFDMA/FlatChannelOFDMA-#0.sca \
  results/scalar-vector/20260725T120411Z/TgaxModelBOFDMA/TgaxModelBOFDMA-#0.sca
```

The resulting scalar table is:

| Configuration | host[0] | host[1] | host[2] | host[3] | Aggregate |
|---|---:|---:|---:|---:|---:|
| `FlatChannelOFDMA` | 3,597 | 3,597 | 3,597 | 3,597 | 14,388 |
| `TgaxModelBOFDMA` | 3,597 | 3,597 | 3,597 | 3,597 | 14,388 |

This table is the direct `packetReceived:count` result for run 0 of the two
named scalar files. It is a single-run observation, not a multi-run estimate,
and is not evidence about the unexecuted configuration pairs listed above.

The corresponding vector inputs are:

- [`FlatChannelOFDMA-#0.vec`](results/scalar-vector/20260725T120411Z/FlatChannelOFDMA/FlatChannelOFDMA-%230.vec)
- [`TgaxModelBOFDMA-#0.vec`](results/scalar-vector/20260725T120411Z/TgaxModelBOFDMA/TgaxModelBOFDMA-%230.vec)

Query RU telemetry:

```sh
opp_scavetool query -l \
  -f 'type =~ vector and (name =~ "heRuToneSize:vector" or name =~ "heRuToneOffset:vector" or name =~ "hePuncturedSubchannelMask:vector")' \
  results/scalar-vector/20260725T120411Z/FlatChannelOFDMA/FlatChannelOFDMA-#0.vec \
  results/scalar-vector/20260725T120411Z/TgaxModelBOFDMA/TgaxModelBOFDMA-#0.vec
```

For both vector files, the AP radio has 6,246 `heRuToneSize` records, with a
minimum of 242 and maximum of 484 tones. Its 6,246 `heRuToneOffset` records
range from 0 to 754. The AP also has 1,563 puncturing-mask records, all zero.
Each station radio has about 1,560 RU records. These exact run-0 vectors prove
that HE frequency-domain allocations were recorded; they do not prove an
advantage under a frequency-selective impairment.

## Packet evidence

The packet inputs are these AP captures:

- [`FlatChannelOFDMA AP PCAP`](results/packet-statistics/20260724T175025Z/FlatChannelOFDMA/FlatChannelOFDMA-%230FrequencySelectiveChannelNetwork.ap.wlan%5B0%5D.pcap)
- [`TgaxModelBOFDMA AP PCAP`](results/packet-statistics/20260724T175025Z/TgaxModelBOFDMA/TgaxModelBOFDMA-%230FrequencySelectiveChannelNetwork.ap.wlan%5B0%5D.pcap)

`capinfos` reports 9,401 packets in each named AP capture, spanning
0.080084 s through 1.199664 s. The following table is derived from those
captures:

| Decoded observation | `FlatChannelOFDMA` | `TgaxModelBOFDMA` |
|---|---:|---:|
| Trigger | 1,561 | 1,561 |
| HE-TB Block Ack, 242-tone RU | 6,240 | 6,240 |
| HE-TB Block Ack, 484-tone RU | 2 | 2 |
| QoS Data, HE-SU | 17 | 17 |
| All AP observations | 9,401 | 9,401 |

The PCAP table proves protocol-visible HE exchanges and identifies 242-tone
and 484-tone RU observations. A capture at one wireless interface does not by
itself establish per-RU SNIR, reception failure, or end-to-end loss.

## Evidence boundary

The named scalar-vector session and packet-statistics PCAPs support only these
conclusions:

- both run-0 configurations delivered 14,388 application packets according to
  their four `packetReceived:count` scalars;
- both run-0 vector files contain HE RU sizes and offsets; and
- both run-0 AP captures contain Trigger and HE-TB Block Ack observations.

The named artifacts contain one run per configuration and do not support a
multi-seed sweep comparison or a throughput claim for half-band interference,
notch depth, puncturing, or transient adaptation. Reproduce those
configurations and save their `.sca`/`.vec` files before making such claims.

## Standards and model boundary

IEEE Std 802.11-2024 Clause 26.5.1.1 permits HE DL MU operation through
OFDMA, MU-MIMO, or both. Clause 27.3.2.5 defines HE MU resource indication
and user identification. The channel-access rules associated with Table 10-17
permit specified 80 MHz HE MU puncturing patterns when the applicable
conditions are met.

INET's scheduler, synthetic noise profiles, scripted mask timing, dimensional
channel implementation, and -40 dBm threshold are model choices. The model
does not turn the packet-statistics evidence above into a general IEEE
guarantee about frequency-selective performance.
