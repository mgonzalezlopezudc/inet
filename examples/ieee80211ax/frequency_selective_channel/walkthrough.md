# Walkthrough: 802.11ax on frequency-selective channels

This example shows why assigning users to frequency-domain resource units can
be useful when an otherwise wide channel is not equally usable at every
frequency. It uses the dimensional radio model, which retains power spectral
density as a function of frequency. A scalar radio can tell that interference
exists, but it cannot preserve which part of a channel is impaired.

The central lesson is deliberately qualified:

- HE OFDMA can keep useful work on clean RUs while another part of the channel
  has poor SNIR;
- HE preamble puncturing can avoid an impaired secondary 20 MHz subchannel;
- neither mechanism guarantees higher throughput in every load, seed, or
  scheduler state; and
- common signaling, acknowledgments, Block Ack setup, contention, and a poor
  allocation decision can still make the whole exchange fail.

The last two points matter. This is a controlled experiment, not a claim that
"OFDMA always wins."

## Topology and traffic

`FrequencySelectiveChannelNetwork` contains a wired server, one AP, and four
equidistant stations. A conditional legacy interferer is 21.2 m from the AP.
The symmetric station geometry removes distance as the intended explanation
for different reception quality.

Four short, staggered warm-up bursts establish Block Ack state before measured
traffic starts at `0.3 s`. Each measured flow sends a 100-byte UDP packet every
`0.25 ms`, for `12.8 Mbps` aggregate offered load. The AP is capped at HE MCS 1
so rate-selection changes do not hide the frequency-allocation effect.

The 40 MHz experiment is the clearest frequency-selective comparison:

| Signal | Frequency range | Role |
|---|---:|---|
| HE BSS | 5.16–5.20 GHz | 40 MHz channel centered at 5.18 GHz |
| Lower-half interferer | 5.16–5.18 GHz | legacy 20 MHz channel centered at 5.17 GHz |
| Upper-half interferer | 5.18–5.20 GHz | legacy 20 MHz channel centered at 5.19 GHz |

The interferer sends 1000-byte frames with exponentially distributed `0.5 ms`
mean intervals. It and all its submodules use RNG stream 1; the BSS remains on
RNG stream 0. The silent controls instantiate the identical interferer but
start its application at `10 s`, after the simulation limit. This preserves
the topology and RNG mapping between clean and impaired runs.

## Why the dimensional model is essential

The INI explicitly instantiates `Ieee80211DimensionalRadioMedium` and
`Ieee80211DimensionalRadio`. For HE MU reception, the radio medium resolves the
STA's RU from the HE PHY header, creates a reception centered on that RU, and
band-pass filters dimensional signal and noise power to the RU bandwidth.

Consequently, a legacy transmission overlapping only the lower 20 MHz half is
interference for lower-half RUs, not automatically for upper-half RUs. In a
scalar model, the total receive power has no frequency axis from which to make
that distinction.

The receiver energy-detection threshold is intentionally set to `-40 dBm`.
This keeps the experiment focused on PHY decoding and per-RU SNIR instead of
letting CCA defer every AP transmission as soon as the interferer is present.
Do not reuse that threshold for a regulatory CCA or spatial-reuse study.

## Scenario catalogue

### Controlled flat references

| Configuration | Width | Access method |
|---|---:|---|
| `FlatChannelOFDMA` / `FlatChannelSU` | 80 MHz | HE OFDMA / matched HE SU |
| `FortyMHzFlatOFDMA` / `FortyMHzFlatSU` | 40 MHz | HE OFDMA / matched HE SU |
| `TwentyMHzFlatOFDMA` / `TwentyMHzFlatSU` | 20 MHz | HE OFDMA / matched HE SU |

The 40 MHz comparison with an identical but silent interferer is stricter:

- `FortyMHzSilentInterfererOFDMA`
- `FortyMHzSilentInterfererSU`

### Partially overlapping legacy channels

These are the main didactic scenarios:

| Configuration pair | Impaired part of the 40 MHz HE channel |
|---|---|
| `FortyMHzLowerHalfOFDMA` / `FortyMHzLowerHalfSU` | lower 20 MHz |
| `FortyMHzUpperHalfOFDMA` / `FortyMHzUpperHalfSU` | upper 20 MHz |

Moving the interferer from the lower to the upper half is a useful symmetry
check. The aggregate behavior should not depend on calling one side "lower";
the RU tone offsets should move with the impaired band.

### Synthetic frequency-domain SNIR profiles

`DimensionalBackgroundNoise` provides a second way to construct a controlled
frequency-selective impairment. The four pairs
`Lower20MHzNotch*`, `MiddleLower20MHzNotch*`,
`MiddleUpper20MHzNotch*`, and `Upper20MHzNotch*` move a high-noise 20 MHz slice
across an 80 MHz channel. The profile uses absolute 5.16–5.24 GHz coordinates,
so it remains fixed when a receiver narrows its listening band to an RU.

`NotchDepthSweepOFDMA` and `NotchDepthSweepSU` sweep total power through
`-110`, `-90`, `-75`, and `-65 dBm`. The severe points may also prevent
wideband SU setup/control exchanges; that is an intended warning that data-RU
isolation does not isolate every part of the protocol.

These profiles model frequency-selective noise/SNIR, not a multipath impulse
response or coherence bandwidth.

### TGax multipath channel

The opt-in `TgaxModelBOFDMA` and `TgaxModelBSU` configurations replace the
flat channel with a persistent reciprocal SISO realization of TGax indoor
Model B. The realization is cached per unordered radio pair, uses the Model B
cluster delay/power profile at 80 MHz, and is sampled on the HE 78.125 kHz
subcarrier grid. `TgaxIndoorPathLoss` separately applies the Model B median
distance loss. These configurations retain the default NIST receiver error
model, so they are useful for channel integration and comparison but do not
constitute an RBIR-calibrated TGax system result.

`TgaxModelBAmbientDopplerOFDMA` enables stationary-user ambient evolution from
the TGac channel-model addendum. Each cached tap uses a deterministic
32-oscillator approximation of the TGn bell-shaped Doppler spectrum, truncated
at ±5 times its Doppler spread and sampled on a 1 ms piecewise-constant time
grid. The configured equivalent environmental speed is `0.089 km/h`; at 5 GHz
the source target is approximately `0.414 Hz` RMS Doppler and `800 ms` channel
coherence time. Packet boundaries do not redraw the channel. Doppler scales
from the configured `referenceFrequency`; time-varying transmissions may use
offset narrow RUs, but their complete passband must remain inside the system
band centered on that reference frequency.

`TgaxModelBSimoMrcOFDMA` opts into the static Model B matrix response. The AP
has one antenna and each station has two, so downlink data uses a 2x1 SIMO
channel and receive-side maximum-ratio combining. On the reciprocal control
and acknowledgment path, a station transmits from antenna 0 through the 1x2
reverse channel. This selected-antenna rule is an explicit one-hot precoder;
the current packet-level transmitter does not otherwise expose an antenna
mapping. Existing TGax configurations leave matrix emission and combining
disabled unless they extend a selected-antenna matrix control, and therefore
retain their original scalar SISO behavior.
The reported MRC signal gain assumes spatially white, equal-variance receive
noise. Its default `reject` interference mode remains deliberately
noise-limited: SNIR evaluation rejects an overlapping reception rather than
applying an incorrect scalar interference shortcut.
Energy detection and other scalar-only consumers use a normalized unit-gain
fallback for matrix snapshots; they do not project the realized matrix fading
into CCA power.

`TgaxModelBSimoLmmseInterferenceOFDMA` opts into covariance-aware L-MMSE
combining and adds an independent cochannel 80 MHz 802.11ax transmitter. For
each time-frequency cell, the dimensional analog model retains the desired and
interfering effective channel vectors and computes
`R = N0 I + sum(Pk gk gk^H)` followed by `Ps h^H R^-1 h`. Background noise is
modeled as spatially white with equal power on each receive antenna. The
piecewise-constant covariance grid defaults to 10 us by 78.125 kHz and is
bounded to two million cells per reception. Mixed scalar and matrix channel
metadata is rejected instead of being combined ambiguously.

The legacy ideal HE-TB spatial-separation shortcut is preserved for the
default, scalar, and matrix-MRC modes. When L-MMSE is explicitly enabled and
the desired reception has matrix metadata, same-trigger, same-RU users on
disjoint stream ranges are retained as covariance terms so the combiner can
evaluate their actual spatial separation.

`TgaxModelBRbirOFDMA` additionally selects `Ieee80211RbirErrorModel`. It must
be given the Appendix-3 Mean RBIR and PER curves from a user-supplied
IEEE 802.11-14/0571r12 workbook. The workbook and extracted data are not
redistributed with INET. Extract them with:

```sh
../../../bin/inet_extract_tgax_rbir_calibration.py \
  /path/to/Microsoft_Excel_Worksheet1.xlsx /tmp/tgax-rbir.txt
```

The current TGax channel implementation remains a NLOS baseline. Static
operation is the default; the opt-in ambient process covers stationary indoor
users only because the TGac source leaves mobile-user indoor Doppler speed and
spectral shape TBD. It does not yet implement LOS/K-factor evolution,
mobility-coupled non-stationarity, polarization, or time-varying spatial
channels. The profile catalog preserves the visually verified TGn Appendix C
cluster AoA/AoD/angular-spread metadata for Models B and D. Their opt-in matrix
path supports one or two half-wavelength-ULA antennas per endpoint, ordinary-
transpose reciprocity, an explicit selected transmit antenna, and single-stream
MRC or L-MMSE combining. It is not a general spatial-stream mapper: multi-stream
precoders and decoders, colored receiver noise, channel-estimation error,
timing/CFO error, and time-varying matrix channels are not modeled. Outdoor UMi
median LOS/NLOS path loss is available as
`TgaxUmiPathLoss`, but fixed outdoor UMi delay profiles are not exposed: the
primary ITU-R table contains an internally inconsistent printed delay row, so
the implementation does not silently guess a correction.

### 80 MHz puncturing and time variation

The synthetic puncturing trio uses the same backlog-aware scheduler:

- `PuncturingCleanOFDMA`
- `PuncturingImpairedUnpunctured`
- `PuncturingImpairedPunctured`

The punctured case applies mask `0100` and avoids the second 20 MHz subchannel.
When an HE MU transmission occurs, that mask is represented as numeric value
`2` in the `hePuncturedSubchannelMask` vector.

The real-interferer variants are:

- `NarrowbandInterferenceSU`
- `NarrowbandInterferenceOFDMA`
- `NarrowbandInterferencePunctured`
- `TransientInterferenceSU`
- `TransientInterferenceOFDMA`
- `TransientInterferenceStaticPuncturing`
- `TransientInterferenceScriptedDynamicPuncturing`

The transient interferer is active from `0.5 s` to `0.85 s`. "Scripted
dynamic" means INET changes a predetermined mask at those times; it does not
claim to implement autonomous channel sounding, classification, or an
802.11-mandated adaptation algorithm. Compare throughput vectors before,
during, and after the interval. Static puncturing pays a capacity cost outside
the impaired interval, while scripted puncturing can incur transition and
scheduler costs.

## Verified five-seed result

The following counts are full-run application packets received by all four
sinks, summed per seed. The `0.25 s` warm-up period excludes the staggered
setup bursts from recorded statistics. Runs use seed sets 0 through 4, the
release model library, run number 0, and the checked-in `1.2 s` limit.

| Configuration | Seed 0 | Seed 1 | Seed 2 | Seed 3 | Seed 4 | Mean |
|---|---:|---:|---:|---:|---:|---:|
| Silent control, OFDMA | 4297 | 2149 | 4056 | 3050 | 2112 | 3132.8 |
| Silent control, SU | 2162 | 13481 | 3802 | 4690 | 3773 | 5581.6 |
| Lower-half interference, OFDMA | 3916 | 2793 | 1649 | 1406 | 1473 | 2247.4 |
| Lower-half interference, SU | 1337 | 5424 | 941 | 1795 | 624 | 2024.2 |

The clean controls are an important counterexample: SU delivers more on
average when the entire 40 MHz channel is usable. Under half-band interference,
SU loses about 64% of its clean mean, while OFDMA loses about 28%. OFDMA's
impaired-channel mean is about 11% higher than SU's (`2247.4` versus `2024.2`).

The seed spread is large and the ordering is not unanimous. That variability
is itself didactic: random contention, interference overlap, Block Ack state,
and RU scheduling all matter. The defensible conclusion is that frequency
partitioning makes the OFDMA case substantially more resilient in this
experiment, not that OFDMA must win every run.

In lower-half OFDMA seed 0, the AP recorded 892 RU user entries. RU sizes were
106 or 242 tones and tone offsets ranged from 0 to 378, confirming real HE MU
frequency allocations rather than a configuration label. Use the per-host RU
vectors together with sink counts; aggregate throughput alone does not prove
the mechanism.

## Running the scenarios

From this directory, run one configuration and seed with:

```sh
../../../bin/inet --release -u Cmdenv -f omnetpp.ini \
  -c FortyMHzLowerHalfOFDMA -r 0 --seed-set=0 \
  --result-dir=results/forty-lower/ofdma/seed0
```

Run the static TGax Model B channel with the default NIST error model:

```sh
../../../bin/inet --release -u Cmdenv -f omnetpp.ini \
  -c TgaxModelBOFDMA -r 0 --seed-set=0 \
  --result-dir=results/tgax/model-b/nist/seed0
```

Run the stationary ambient-Doppler variant:

```sh
../../../bin/inet --release -u Cmdenv -f omnetpp.ini \
  -c TgaxModelBAmbientDopplerOFDMA -r 0 --seed-set=0 \
  --result-dir=results/tgax/model-b/ambient-doppler/seed0
```

This dynamic configuration intentionally retains the NIST receiver. The
current RBIR implementation rejects time-varying per-tone SNIR rather than
silently reducing it to one sample; combining `TgaxRbir` with ambient Doppler
is therefore unsupported.

Run the static selected-antenna/SIMO-MRC variant:

```sh
../../../bin/inet --release -u Cmdenv -f omnetpp.ini \
  -c TgaxModelBSimoMrcOFDMA -r 0 --seed-set=0 \
  --result-dir=results/tgax/model-b/simo-mrc/seed0
```

Run the covariance-aware SIMO L-MMSE variant with a cochannel 802.11ax
interferer:

```sh
../../../bin/inet --release -u Cmdenv -f omnetpp.ini \
  -c TgaxModelBSimoLmmseInterferenceOFDMA -r 0 --seed-set=0 \
  --result-dir=results/tgax/model-b/simo-lmmse-interference/seed0
```

Run the calibrated RBIR variant after extracting the user-supplied workbook:

```sh
../../../bin/inet --release -u Cmdenv -f omnetpp.ini \
  -c TgaxModelBRbirOFDMA -r 0 --seed-set=0 \
  '--**.wlan[*].radio.receiver.errorModel.calibrationFile="/tmp/tgax-rbir.txt"' \
  --result-dir=results/tgax/model-b/rbir/seed0
```

The RBIR model accepts HE MCS 0–9 and evaluates the assigned RU's physical
data subcarriers. Its packet-length scaling uses the 32-byte and 1458-byte
reference PER curves defined by the TGax methodology; it rejects incomplete
or malformed calibration input at initialization.

Repeat seed sets 0 through 4 for the four rows in the table. Every verified
run exited successfully at the simulation-time limit and produced `.sca`,
`.vec`, and `.vci` files.

List application counts:

```sh
opp_scavetool query -s -l \
  -f 'module =~ **.host[*].app[0] AND name =~ packetReceived:count' \
  results/forty-lower/*/seed*/*.sca
```

Verify actual RU sizes, positions, and puncturing masks:

```sh
opp_scavetool query -v -l \
  -f 'name =~ heRuToneSize:vector OR name =~ heRuToneOffset:vector OR name =~ hePuncturedSubchannelMask:vector' \
  results/forty-lower/*/seed*/*.vec
```

For transient configurations, export only the application throughput and RU
mask vectors, then compare `0.3–0.5 s`, `0.5–0.85 s`, and `0.85–1.2 s` rather
than collapsing the entire run into one number.

## Standards baseline and model boundary

IEEE Std 802.11-2024 provides the protocol baseline:

- Clause 26.5.1.1 states that HE DL MU operation allows simultaneous AP
  transmission to one or more non-AP STAs using DL OFDMA, DL MU-MIMO, or both.
- Clause 27.3.2.5 defines HE MU resource indication and user identification;
  HE-SIG-B carries user allocation for one or more 20 MHz subchannels.
- The channel-access rules associated with Table 10-17 permit specified 80 MHz
  HE MU puncturing patterns when the unpunctured 20 MHz subchannels satisfy the
  required idle conditions.

The standard does not define INET's `fBW`, backlog-aware scheduler, synthetic
noise profile, scripted mask timing, or the chosen CCA threshold. The model
also assumes ideal non-overlapping RU isolation and does not model adjacent-RU
leakage, oscillator error, or a waveform-level multipath channel. Those are
model boundaries, not 802.11 guarantees.

## 802.11 Packet Type Statistics
This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were aggregated across all active wireless interfaces (`wlan[0]`) in the network.

Two airtime occupancy percentages are provided:
- **Air Time %**: The percentage of the total transmission airtime of all packets occupied by this frame type.
- **Air Time (Sim Time) %**: The percentage of the total simulation time occupied by the transmission of this frame type (defined as the sum of physical airtimes of this frame type w.r.t. the total simulation time limit).

### Configuration: `TgaxModelBOFDMA`
Total over-the-air packets captured (Global BSS/AP): **5232**

| Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Air Time % | Air Time (Sim Time) % |
|---|---:|---:|---:|---:|---:|---:|
| Management: Action | 2082 | 39.79% | 37.0 B | 0.0 B | 27.68% | 12.03% |
| Data: QoS Data | 1575 | 30.10% | 166.0 B | 0.0 B | 64.87% | 28.19% |
| Control: Ack | 1575 | 30.10% | 14.0 B | 0.0 B | 7.45% | 3.24% |

### Analysis of Packet Distribution
Across these configurations, **QoS Data** frames constitute the primary payload delivery mechanism, while **Block Ack (BA)** and **Block Ack Request (BAR)** control frames ensure reliable transport via the MAC-level acknowledgment protocol. Management frames, specifically **Beacons**, are transmitted periodically by the Access Point to maintain BSS time synchronization and broadcast network capabilities. The ratio of control/management overhead to actual data frames indicates the relative MAC efficiency of the chosen configurations.
