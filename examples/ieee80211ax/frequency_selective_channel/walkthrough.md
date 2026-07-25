# Walkthrough: HE operation on frequency-selective channels

This example uses INET's dimensional radio model to retain power spectral
density across frequency and evaluate HE resource units (RUs) within their
assigned bandwidth. Retained evidence covers only run 0 of the flat 80 MHz
OFDMA reference and static TGax Model B OFDMA case; the wider catalogue of
notch, interferer, puncturing, SU-control, and sweep configurations is
documented as `NOT RUN`.

## Learning objectives and feature primer

After completing this walkthrough, the reader can:

- explain why RU-local signal and noise matter on a frequency-selective channel;
- identify the dimensional radio/medium and HE RU telemetry;
- recognize Trigger followed by HE trigger-based (HE-TB) Block Ack exchanges;
  and
- distinguish demonstrated allocation from an untested impairment advantage.

Orthogonal frequency-division multiple access (OFDMA) assigns groups of
subcarriers, called RUs, to users. When noise or channel gain varies with
frequency, two RUs in the same wide channel can experience different
signal-to-interference-plus-noise ratio (SINR). A dimensional model can retain
that frequency dependence; a scalar wideband power cannot.

## Scenario description

[FrequencySelectiveChannelNetwork.ned](FrequencySelectiveChannelNetwork.ned)
extends the common single-BSS topology with four stationary, equidistant
stations around one AP and an optional legacy interferer. A wired server sends
four warm-up bursts, then four 100 B UDP flows every 0.25 ms from `0.3 s`
(12.8 Mbit/s aggregate). The HE scheduler assigns up to four equal-sized RUs
at no more than MCS 1. [omnetpp.ini](omnetpp.ini) contains the full catalogue.

```text
             host[1]
                |
host[0] --- AP --- host[2]       optional interferer
                |
             host[3]
                |
              server
```

Equal 40 m AP distances prevent geometry from being the intended source of
per-station differences. Energy detection is deliberately `-40 dBm` so the
controlled cases emphasize receiver error decisions rather than CCA deferral.

## Standards and INET model boundary

IEEE Std 802.11-2024 Clause 27.3.2.5 defines resource indication and user
identification in HE MU PPDUs; Clause 27.3.2.6 describes HE-TB resource
allocation following a Trigger frame (corpus chunks
`80211ax-2024:chunk:10062` and `10064`). The standard defines the exchange,
not INET's synthetic spectral profiles or the results below.

INET instantiates `Ieee80211DimensionalRadioMedium` and
`Ieee80211DimensionalRadio`; HE reception filters signal/noise to the assigned
RU. Synthetic notch gains, TGax realization, NIST/RBIR error models, scripted
puncturing, and `-40 dBm` energy detection are modeling choices. A configured
profile is input evidence until runtime telemetry demonstrates it.

## Evidence status

| Claim or check | Status | Authoritative evidence | Runs/seeds | Scope or gap |
|---|---|---|---|---|
| Dimensional HE RU allocations occurred | `PASS` | `heRuToneSize/Offset` vectors | run/seed `0` | flat and TGax Model B OFDMA |
| Trigger/HE-TB Block Ack exchange occurred | `PASS` | AP PCAP | run/seed `0` | both retained configs |
| Both retained cases delivered 14,388 packets | `PASS` | sink count scalars | run/seed `0` | direct single-run count |
| Frequency-selective impairment changes per-RU reception | `INCONCLUSIVE` | no retained impaired/control pair | none | decisive SNIR/error comparison missing |
| Notch, real interferer, puncturing, sweep, SU, SIMO/RBIR claims | `NOT RUN` | no retained result set | none | configuration input only |

## Configuration matrix

| Configuration | Role | Feature gate/delta | Workload/channel | Runs/seeds | Expected invariant |
|---|---|---|---|---|---|
| `FlatChannelOFDMA` | Retained control | dimensional flat 80 MHz; equal RUs | matched four-flow load | run/seed `0` | HE RU telemetry and delivery |
| `TgaxModelBOFDMA` | Retained treatment | static reciprocal SISO TGax Model B/NIST error model | matched | run/seed `0` | HE RU telemetry and delivery |
| `FlatChannelSU` / `TgaxModelBSU` | Counterfactual | HCF/EDCA whole-channel SU | matched | `NOT RUN` | no Trigger-scheduled OFDMA |
| `*Notch*`, `FortyMHz*Interferer*` | Stress | frequency-local noise or 20 MHz interferer | declared | `NOT RUN` | affected RUs change SNIR/error |
| `Puncturing*` / `Transient*` | Stress | mask or timed interferer | declared | `NOT RUN` | allocations avoid punctured spectrum |
| SIMO/MRC/L-MMSE/RBIR variants | Model study | receiver/error-model change | declared | `NOT RUN` | model-specific reception outcome |

All rows inherit the dimensional 80 MHz HE base unless an explicit base config
overrides width/channel. `TgaxModelBOFDMA` extends `Ofdma` and
`TgaxIndoorModelB80MHz`; the latter's channel/error-model assignments are the
effective delta. This matrix does not convert unexecuted rows into evidence.

## Expected invariants and diagnostic map

| Invariant | Evidence and observation point | Failure symptom | Likely subsystem | Next diagnostic |
|---|---|---|---|---|
| Scheduler emits RU sizes/offsets | AP radio vectors | empty/out-of-band RU | HE scheduler/PHY | query RU vectors and puncture mask |
| Trigger precedes HE-TB response | AP PCAP timestamps | missing response | HE MAC/Block Ack | inspect Trigger fields, receiver capture, MAC log |
| Local impairment affects overlapping RU only | per-RU SNIR/error vectors | all/no RUs affected | dimensional medium/receiver | co-record power, SNIR, RU offset, outcome |
| Application comparison is matched | sink scalars and run attrs | unequal load/seed/window | traffic/config | query run metadata first |

## Reproduction

Run from the repository root. This minimal command is illustrative and was
**NOT RUN** during this rewrite:

```sh
bin/inet --release -u Cmdenv \
  -f examples/ieee80211ax/frequency_selective_channel/omnetpp.ini \
  -c FlatChannelOFDMA -r 0 --seed-set=0 \
  --result-dir=examples/ieee80211ax/frequency_selective_channel/results/manual/FlatChannelOFDMA
```

Fresh suite PCAP generation is also `NOT RUN` here:

```sh
python3 examples/ieee80211/analysis/analyze_pcap.py \
  --suite examples/ieee80211/analysis/suites/ax.json \
  --generate --subdir frequency_selective_channel --run 0
```

## Scalar and vector analysis

Inputs:
`results/scalar-vector/20260725T120411Z/{FlatChannelOFDMA,TgaxModelBOFDMA}/*.{sca,vec}`.

```sh
opp_scavetool query -l \
  -f 'name =~ "packetReceived:count" AND module =~ "*.host[*].app[0]"' \
  examples/ieee80211ax/frequency_selective_channel/results/scalar-vector/20260725T120411Z/*/*.sca

opp_scavetool query -l \
  -f 'type =~ vector AND (name =~ "heRuToneSize:vector" OR name =~ "heRuToneOffset:vector" OR name =~ "hePuncturedSubchannelMask:vector")' \
  examples/ieee80211ax/frequency_selective_channel/results/scalar-vector/20260725T120411Z/*/*.vec
```

| Configuration | host[0] | host[1] | host[2] | host[3] | Aggregate |
|---|---:|---:|---:|---:|---:|
| `FlatChannelOFDMA` | 3597 | 3597 | 3597 | 3597 | 14,388 |
| `TgaxModelBOFDMA` | 3597 | 3597 | 3597 | 3597 | 14,388 |

In both run-0 vector files the AP has 6246 RU-size records ranging 242–484
tones and 6246 offset records ranging 0–754. It has 1563 puncturing-mask
records, all zero; each station has about 1560 RU records. These are
single-run direct observations. No confidence interval or warm-up-adjusted
rate is computed, and equality does not establish robustness.

## PCAP statistics

Session `results/packet-statistics/20260724T175025Z` contains run/seed 0 legacy
PCAPs at all WLAN MAC observation points. Each retained AP capture has 9401
observations from `0.080084 s` through `1.199664 s`; TShark 4.6.4 decodes it.

```sh
tshark -n -r 'examples/ieee80211ax/frequency_selective_channel/results/packet-statistics/20260724T175025Z/FlatChannelOFDMA/FlatChannelOFDMA-#0FrequencySelectiveChannelNetwork.ap.wlan[0].pcap' \
  -q -z io,stat,0,'wlan.fc.type==1 && (wlan.fc.subtype==2 || wlan.fc.subtype==9)'
```

| Decoded AP observation | `FlatChannelOFDMA` | `TgaxModelBOFDMA` |
|---|---:|---:|
| Trigger | 1561 | 1561 |
| HE-TB Block Ack, 242-tone RU | 6240 | 6240 |
| HE-TB Block Ack, 484-tone RU | 2 | 2 |
| QoS Data, HE-SU | 17 | 17 |
| All observations | 9401 | 9401 |

Rows count capture observations, including multiple HE-TB user observations;
they are not application delivery or scheduler-decision counts.

## Frame exchange analysis

```sh
tshark -n -r 'examples/ieee80211ax/frequency_selective_channel/results/packet-statistics/20260724T175025Z/FlatChannelOFDMA/FlatChannelOFDMA-#0FrequencySelectiveChannelNetwork.ap.wlan[0].pcap' \
  -Y 'frame.number >= 39 && frame.number <= 41' \
  -T fields -E header=y -E separator='|' \
  -e frame.number -e frame.time_epoch -e wlan.fc.type -e wlan.fc.subtype \
  -e radiotap.he.data_1.ppdu_format -e radiotap.he.data_3.data_mcs \
  -e radiotap.he.data_5.data_bw_ru_allocation \
  -e radiotap.he.data_2.ru_allocation_offset
```

| Frame | Simulation time | Transmitter → receiver | Type/PHY | Decisive fields | Role in exchange |
|---:|---:|---|---|---|---|
| 39 | 0.300360 s | AP → stations | Trigger | control subtype 2 | solicits HE-TB responses |
| 40 | 0.300453 s | station → AP | Block Ack / HE-TB | PPDU 3, MCS 0, RU code 8, offset 0 | first captured 242-tone response |
| 41 | 0.300453 s | station → AP | Block Ack / HE-TB | same timestamp/profile | another user observation |

The typed analyzer maps the known RU code to 242 tones. Empty transmitter
addresses on control rows are a capture/dissector limitation, and equal
timestamps can represent per-user observations of one multi-user exchange.

## Cross-layer findings and verdict

| Claim | Verdict | Configuration evidence | Model telemetry | Packet evidence | Outcome evidence |
|---|---|---|---|---|---|
| HE frequency-domain allocation runs | `PASS` | dimensional radio + OFDMA scheduler | RU sizes/offsets | Trigger and HE-TB BA | all sinks receive |
| Flat and TGax retained runs differ in delivery | `FAIL` | channel-model delta | allocation counts equal | packet summaries equal | both 14,388 |
| Selective impairment advantage | `INCONCLUSIVE` | scenarios declared | decisive impaired vectors absent | impaired captures absent | controls absent |

The bounded verdict is `PASS` for exercising and observing HE RU allocation,
but `INCONCLUSIVE` for the example's central frequency-selective impairment
comparison. The retained pair happens to have equal delivery; it is not a
failed standard invariant.

## Limitations and inconclusive claims

- Only one seed and two OFDMA configurations are retained.
- The decisive impaired/control and matched SU pairs were not run.
- AP PCAP cannot expose per-RU SNIR, receiver error cause, or application loss.
- Result and packet sessions are separate and cannot establish event causality.
- The synthetic notch profiles are frequency-domain noise, not multipath
  impulse responses.

The smallest resolving experiment is one matched
`FortyMHzSilentInterfererOFDMA`/`FortyMHzLowerHalfOFDMA` run with co-recorded
RU offset, filtered signal/noise, minimum SNIR, reception outcome, sink count,
and AP/receiver PCAP; then repeat across seeds only after the mechanism appears.

## Further experiments

- Move the 20 MHz impairment from lower to upper half and predict which RU
  offsets show reduced SNIR.
- Compare matched OFDMA and SU controls at one impairment and report delivery
  per run before adding a notch-depth sweep.
- Run clean/impaired punctured controls and verify the runtime mask and RU
  placement before comparing delivery.

## Implementation plan

| Item | Evidence-backed plan |
|---|---|
| Demonstrated gap | retained artifacts do not expose per-RU filtered power/SNIR tied to reception outcome |
| Intended behavior | make frequency-local impairment causally testable |
| Smallest change surface | first enable/discover existing radio results; extend typed analysis only if the decisive telemetry is absent |
| Observability | RU size/offset, mask, filtered signal/noise, SNIR, error stage, sink delivery |
| Validation | silent/active interferer pair, one seed first, then independent seeds and SU control |
| Compatibility and risks | preserve dimensional/scalar radio behavior and RNG trajectory where possible |
| Architecture and sealing | apply architecture/sealing review before any `src/inet` edit |
| Next handoff | dimensional PHY/results specialist after a co-recorded reproduction |

## Artifact provenance

| Artifact family | Session/path | Configurations/runs | Tool/filter/window | Integrity notes |
|---|---|---|---|---|
| Scalar/vector | `results/scalar-vector/20260725T120411Z` | flat and TGax OFDMA, run/seed `0` | exact queries above | single-run only |
| PCAP/results | `results/packet-statistics/20260724T175025Z` | same configs, run/seed `0` | TShark 4.6.4; MAC | separate session |
| Config catalogue | `omnetpp.ini` | all declared cases | input inspection | not runtime evidence |
