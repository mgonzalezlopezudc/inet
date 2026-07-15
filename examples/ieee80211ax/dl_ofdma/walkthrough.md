# Walkthrough: 802.11ax Downlink OFDMA

This example shows how downlink OFDMA behaves when several stations have data
ready at the same time. Earlier 802.11 generations give one station the whole
channel in each transmission opportunity; 802.11ax can divide that opportunity
into resource units (RUs) and serve several stations in one HE MU PPDU. The
comparison also shows that RU scheduling is a policy choice and that OFDMA is
not automatically faster than SU OFDM at every channel width and offered load.

## What the experiment demonstrates

Under the low-load, rate-matched three-station workload:

- at 20 MHz, `fBW` and `fHoL` DL OFDMA deliver `2.399 Mbps` and
  `2.395 Mbps`, essentially the complete `2.4 Mbps` offered load, while SU
  OFDM delivers `1.615 Mbps`;
- their p95 delays are `1.68 ms`, `2.31 ms`, and `52.26 ms` respectively, so
  both OFDMA policies avoid the queue buildup seen in SU;
- at 80 MHz, both OFDMA policies deliver `2.400 Mbps` with `0.58 ms` p95
  delay, while SU delivers `2.374 Mbps` with `12.80 ms` p95 delay; and
- fresh AP captures contain 3192, 2533, and 1605 decoded frames for 20 MHz
  `fBW`, `fHoL`, and SU respectively. TShark shows the common warm-up data at
  `0.200180 s`, normal data at `0.300180 s`, and the expected Action/ACK
  exchanges. HE RU metadata is retained in the `.vec` results because the
  native MAC capture does not expose every HE MU field.

The aggregate values are five-seed means; the capture observations are run 0.
They establish the behavior of this example without turning one seed into a
general performance claim.

## Standards baseline

The normative reference used here is IEEE Std 802.11-2024:

- Clause 26.5.1.1 defines HE DL MU operation as allowing an AP to transmit
  simultaneously to non-AP STAs using DL OFDMA, DL MU-MIMO, or both. It also
  requires the PSDUs on the allocated RUs to be padded to end together.
- Clause 26.5.1.2 requires the HE MU TXVECTOR to identify the STA associated
  with each RU. The capture therefore checks distinct AIDs and RU allocations.
- Clause 26.6.2.2 defines padding of the per-user A-MPDUs in an HE MU PPDU.
- Clause 9.3.1.22.4 defines an MU-BAR Trigger User Info field as BAR Control
  plus BAR Information, including the requested Block Ack bitmap window.
- Clause 26.5.2.3.3 requires a response to a Trigger frame to use HE-TB format
  and the RU, MCS, GI, and related parameters indicated by the Trigger.

The standard defines the frame formats and protocol behavior. It does not
define INET's `fBW` or `fHoL` heuristic, nor require OFDMA to outperform SU for
every workload.

## Controlled comparison matrix

The controlled configurations use the same topology, three stations, `100 mW`
transmit power, best-effort EDCA traffic, packet size, simulation duration,
`0.2–0.25 s` warm-up, and `0.3 s` normal-operation start. Each source offers a
100-byte packet every `1 ms`, for `2.4 Mbps` aggregate application load. The
analysis campaign measures `0.3–0.88 s`.

The SU rate is fixed by `**.wlan[*].bitrate`. The DL MU scheduler normally
selects a per-RU MCS independently from estimated SNR, so the AP's
`heMcsSnrThresholds` is capped at MCS 1. This makes the comparison rate-matched:
MCS 1 corresponds to `14.625 Mbps` at 20 MHz and `61.25 Mbps` at 80 MHz with
the configured guard interval. These are PHY data-field rates, not application
goodput limits.

| Configuration | Width | MAC/scheduler |
|---|---:|---|
| `EqualSizedRUs_fBW` | 20 MHz | DL OFDMA, maximize occupied bandwidth |
| `EqualSizedRUs_fHoL` | 20 MHz | DL OFDMA, prioritize HoL service |
| `SuEdcaBaseline` | 20 MHz | ordinary HCF/SU OFDM |
| `EqualSizedRUs80MHz_fBW` | 80 MHz | matched `fBW` comparison |
| `EqualSizedRUs80MHz_fHoL` | 80 MHz | matched `fHoL` comparison |
| `SuEdcaBaseline80MHz` | 80 MHz | matched SU OFDM comparison |

`WideBandwidth80MHz` remains a separate eight-station scaling scenario. It is
not used for the controlled width comparison because it intentionally changes
the station count and workload.

The short `10 ms` ADDBA retry interval is important: all independently
contending stations complete Block Ack setup before the measurement interval.
Port 80 maps the comparison traffic to `AC_BE`, avoiding a finite voice TXOP as
an accidental confounder.

## Why the policies differ

When all three queues are eligible on 20 MHz, the standard equal-RU layouts
that matter here contain two 106-tone RUs or four 52-tone RUs:

- `fBW` keeps the oldest station as the mandatory anchor, ranks the remaining
  candidates by eligible backlog, and selects the widest layout that fits. It
  therefore transmits to two stations on two 106-tone RUs and rotates service.
- `fHoL` ranks by head-of-line age and selects the smallest layout that can
  accommodate all candidates. It transmits to all three stations in three of
  the four 52-tone RU positions.

At 20 MHz, `fBW` occupies 212 tones and rotates service between two stations;
`fHoL` occupies 156 tones but serves every eligible station in each MU
exchange. Both carry essentially the full offered load. `fBW` has lower p95
delay here because its 106-tone RUs transmit each user's payload faster than
the 52-tone RUs used by `fHoL`. At 80 MHz both policies have ample capacity and
produce the same measured p95 delay.

## Reproducing `.sca` and `.vec` results

Run one configuration, run number, and seed set at a time from this directory.
The configurations define only run number 0, so the five-seed campaign varies
`--seed-set` rather than using run numbers 0 through 4. A verified command is:

```sh
../../../bin/inet --release -u Cmdenv -f omnetpp.ini \
  -c EqualSizedRUs_fBW -r 0 --seed-set=0 \
  --result-dir=results/comparison/EqualSizedRUs_fBW/seed0
```

Repeat it with seed sets 0 through 4 for the other five configurations in the
table. Every verified run exited successfully at the `1 s` simulation time
limit and produced a `.sca`, `.vec`, and `.vci` file in its result directory.

Check run metadata rather than relying only on filenames:

```sh
opp_scavetool query -a \
  'results/comparison/EqualSizedRUs_fBW/seed0/EqualSizedRUs_fBW-#0.sca'
```

Query application totals and delay distributions with:

```sh
opp_scavetool query -s -l \
  -f 'module =~ **.host[*].app[0] AND (name =~ packetReceived* OR name =~ endToEndDelay*)' \
  results/comparison/*/seed*/*.sca

opp_scavetool query -v -l \
  -f 'module =~ **.host[*].app[0] AND (name =~ packetReceived* OR name =~ endToEndDelay*)' \
  results/comparison/*/seed*/*.vec
```

## Performance results

Throughput uses application payload bytes received in the `0.3–0.88 s`
measurement interval and reports the mean of seed sets 0 through 4. The p95
values use the nearest-rank percentile of the corresponding application delay
samples pooled across those five seeds and restricted to the same interval.

| Configuration | Aggregate app throughput | p95 E2E delay |
|---|---:|---:|
| `EqualSizedRUs_fBW` | 2.399 Mbps | 1.68 ms |
| `EqualSizedRUs_fHoL` | 2.395 Mbps | 2.31 ms |
| `SuEdcaBaseline` | 1.615 Mbps | 52.26 ms |
| `EqualSizedRUs80MHz_fBW` | 2.400 Mbps | 0.58 ms |
| `EqualSizedRUs80MHz_fHoL` | 2.400 Mbps | 0.58 ms |
| `SuEdcaBaseline80MHz` | 2.374 Mbps | 12.80 ms |
| `BacklogBased` | 6.751 Mbps | 208.78 ms |
| `HoLMinDelay` | 5.472 Mbps | 221.27 ms |

The controlled comparisons show:

- at 20 MHz, both OFDMA policies carry essentially the full offered load,
  whereas SU carries about 67% of it;
- 20 MHz `fBW` has about 27% lower p95 delay than `fHoL`, while their aggregate
  goodputs are nearly identical;
- both 20 MHz OFDMA policies reduce p95 delay by more than 95% relative to SU;
- at 80 MHz, all three configurations carry nearly the full offered load, but
  OFDMA has `0.58 ms` p95 delay versus `12.80 ms` for SU;
- the asymmetric pair demonstrates the workload trade-off: BacklogBased
  delivers more aggregate goodput, while neither result should be read as a
  universal fairness or delay ordering.

The 20 MHz queue results explain the delay difference. Across all five seeds,
both OFDMA policies have zero AP destination-queue overflows; in run 0 their
per-destination queues contain at most two packets. The SU AP queue reaches its
100-packet limit in every seed, ends with 98 packets, and drops an average of
589.6 packets per run. The delay comparison therefore captures the intended
benefit of serving several sparse downlink flows together, not merely a small
difference between percentile estimators.

## Vector result

The packet-arrival vectors are monotonic and populated throughout the
`0.3–0.88 s` interval. The run-level aggregates confirm sustained delivery;
they are not scalars caused by the warm-up burst:

| Configuration | Vector evidence |
|---|---|
| 20 MHz `fBW` | All 15 station/seed arrival vectors nonempty; 2.399 Mbps mean |
| 20 MHz `fHoL` | All 15 station/seed arrival vectors nonempty; 2.395 Mbps mean |
| 20 MHz SU | All 15 station/seed arrival vectors nonempty; 1.615 Mbps mean |
| 80 MHz `fBW` | All 15 station/seed arrival vectors nonempty; 2.400 Mbps mean |
| 80 MHz `fHoL` | All 15 station/seed arrival vectors nonempty; 2.400 Mbps mean |
| 80 MHz SU | All 15 station/seed arrival vectors nonempty; 2.374 Mbps mean |

Values are aggregate application Mbps. The vector timestamps are used to apply
the same `0.3–0.88 s` interval to throughput and delay instead of relying on
whole-run scalar totals.

## PCAP and TShark verification

The following command records the AP MAC capture without permanently enabling
capture in `omnetpp.ini`:

```sh
../../../bin/inet --release -u Cmdenv -f omnetpp.ini \
  -c EqualSizedRUs_fBW -r 0 --seed-set=0 \
  --result-dir=results/pcap_comparison/fBW \
  '--*.ap.wlan[*].recordPcap=true' \
  '--*.ap.wlan[*].pcapRecorder[*].moduleNamePatterns="mac"' \
  '--*.ap.wlan[*].pcapRecorder[*].verbose=false' \
  '--**.checksumMode="computed"' '--**.fcsMode="computed"' \
  '--**.scalar-recording=false' '--**.vector-recording=false'
```

Repeat with `EqualSizedRUs_fHoL` and `SuEdcaBaseline`, changing the result
directory. The checksum/FCS overrides make packets serializable for capture;
the performance table comes from the non-capture campaign.

Validate a capture and inspect MU-BAR user allocations with:

```sh
capinfos \
  'results/pcap_comparison/fBW/EqualSizedRUs_fBW-#0Lan80211AxDlOfdma.ap.wlan[0].pcap'

tshark -n \
  -r 'results/pcap_comparison/fBW/EqualSizedRUs_fBW-#0Lan80211AxDlOfdma.ap.wlan[0].pcap' \
  -Y 'wlan.fc.type_subtype == 0x0028 || wlan.fc.type_subtype == 0x001d || wlan.fc.type_subtype == 0x000d' \
  -T fields -E header=y -E occurrence=a \
  -e frame.number -e frame.time_epoch -e wlan.fc.type_subtype \
  -e wlan.ta -e wlan.ra -e data.len
```

The AP captures are PCAPng files with native IEEE 802.11 encapsulation:

| Capture | PCAPng frames | TShark-visible evidence |
|---|---:|---|
| 20 MHz `fBW` | 3192 | QoS Data, ACK, and Action exchanges |
| 20 MHz `fHoL` | 2533 | QoS Data, ACK, and Action exchanges |
| 20 MHz SU | 1605 | SU data/control exchange |

The fresh captures start at simulation time `0.200180 s`; the first normal-data
observation is at `0.300180 s`. The native capture decoder does not expose the
HE RU user information used by the `.vec` analysis, so RU layout claims are
based on the result vectors rather than inferred from packet counts.

## How to read the result

- INET carries some HE PHY information as packet metadata, so the capture does
  not expose every HE-SIG field exactly as a real monitor capture would.
- Application delivery is established from sink `.sca`/`.vec` results. Do not
  compare configurations by TShark-decoded UDP count because aggregated MPDUs
  may be presented as opaque 802.11 payloads to the dissector.
- At 20 MHz, OFDMA wins because the AP can place several sparse destination
  queues in one HE MU PPDU. The SU baseline sends to one receiver per PPDU and,
  with only one 100-byte packet arriving per flow per millisecond, cannot
  amortize its per-transmission overhead through large aggregates as effectively.
- `fBW` and `fHoL` deliver nearly identical aggregate goodput because both have
  enough capacity for the offered load. `fBW` has lower p95 delay in this run:
  two 106-tone RUs send each selected payload faster than the 52-tone RUs in
  the three-user `fHoL` layout, despite `fBW` rotating one user between PPDUs.
- The fixed MCS cap is essential to this interpretation. The throughput and
  delay gains do not come from allowing OFDMA to select a higher MCS than SU.
- The results show the intended scheduler trade-off for this workload, not a
  universal claim that OFDMA always wins.

Within those limits, the example demonstrates simultaneous multi-station
delivery, lower queueing delay for matched-rate DL OFDMA with several sparse
downlink flows, the `fBW` versus `fHoL` trade-off, and bandwidth-dependent
behavior at 20 versus 80 MHz.
