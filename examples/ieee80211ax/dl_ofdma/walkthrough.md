# Walkthrough: 802.11ax Downlink OFDMA

This example compares IEEE 802.11ax downlink OFDMA with a matched single-user
OFDM baseline and exposes the trade-off between the `fBW` and `fHoL` equal-RU
scheduling policies. It is a standards-aware packet-level model, not a
bit-level interoperability or conformance test.

## Verification result

The corrected model now demonstrates the intended advantages under a saturated
three-station downlink workload:

- at 20 MHz, `fBW` DL OFDMA delivers `23.552 Mbps`, versus `7.520 Mbps` for
  matched SU OFDM: a `3.13x` throughput gain;
- at 20 MHz, `fHoL` delivers `20.224 Mbps` and exactly equal packet counts to
  all three stations, illustrating its concurrency/fairness objective;
- at 80 MHz, both OFDMA policies deliver essentially the full `40 Mbps`
  offered load with sub-millisecond mean delay; and
- the AP capture contains two simultaneous Block Ack responses per `fBW`
  MU-BAR and three per `fHoL` MU-BAR, while the SU capture contains no MU-BAR.

These are deterministic run-0 observations. They establish the behavior of
this example; use multiple independent seeds before making broader performance
claims.

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

All six core configurations use the same topology, three stations, `100 mW`
transmit power, best-effort EDCA traffic, packet size, start time, simulation
duration, warm-up period, and seed. Each source offers a 100-byte packet every
`0.06 ms`, for `40 Mbps` aggregate application load. Traffic starts at `0.5 s`;
statistics cover the `0.7-1.0 s` measurement interval.

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

With three backlogged stations on 20 MHz, the standard equal-RU layouts that
matter here contain two 106-tone RUs or four 52-tone RUs:

- `fBW` keeps the oldest station as the mandatory anchor, ranks the remaining
  candidates by eligible backlog, and selects the widest layout that fits. It
  therefore transmits to two stations on two 106-tone RUs and rotates service.
- `fHoL` ranks by head-of-line age and selects the smallest layout that can
  accommodate all candidates. It transmits to all three stations in three of
  the four 52-tone RU positions.

At 20 MHz, `fBW` occupies 212 tones and achieves higher aggregate throughput;
`fHoL` occupies 156 tones but serves every station in each MU exchange. At
80 MHz both have enough capacity for the offered load, so `fHoL`'s three-user
service gives slightly lower delay.

## Reproducing `.sca` and `.vec` results

Run one configuration and run number at a time from this directory. The
verified command was:

```sh
../../../bin/inet --release -u Cmdenv -f omnetpp.ini \
  -c EqualSizedRUs_fBW -r 0 --result-dir=results/comparison
```

Repeat it for the other five configurations in the table. Every verified run
exited successfully at the `1 s` simulation time limit and produced a `.sca`,
`.vec`, and `.vci` file under `results/comparison`.

Check run metadata rather than relying only on filenames:

```sh
opp_scavetool query -a \
  'results/comparison/EqualSizedRUs_fBW-#0.sca'
```

Query application totals and delay distributions with:

```sh
opp_scavetool query -s -l \
  -f 'module =~ **.host[*].app[0] AND (name =~ packetReceived* OR name =~ endToEndDelay*)' \
  results/comparison/*.sca

opp_scavetool query -v -l \
  -f 'module =~ **.host[*].app[0] AND (name =~ packetReceived* OR name =~ endToEndDelay*)' \
  results/comparison/*.vec
```

## Scalar results

Throughput uses application payload bytes received in the `0.3 s` measurement
interval. Mean delay is weighted over all received application packets.

| Configuration | Packets per host | App throughput | Mean E2E delay | Max E2E delay |
|---|---:|---:|---:|---:|
| `EqualSizedRUs_fBW` | 3072 / 2880 / 2880 | 23.552 Mbps | 10.411 ms | 12.245 ms |
| `EqualSizedRUs_fHoL` | 2512 / 2512 / 2512 | 20.096 Mbps | 12.857 ms | 14.287 ms |
| `SuEdcaBaseline` | 947 / 960 / 890 | 7.459 Mbps | 16.898 ms | 23.330 ms |
| `EqualSizedRUs80MHz_fBW` | 4993 / 4997 / 5011 | 40.003 Mbps | 0.763 ms | 1.485 ms |
| `EqualSizedRUs80MHz_fHoL` | 4996 / 4996 / 4996 | 39.968 Mbps | 0.707 ms | 1.146 ms |
| `SuEdcaBaseline80MHz` | 2847 / 2809 / 2725 | 22.349 Mbps | 5.058 ms | 7.027 ms |

The controlled comparisons show:

- 20 MHz `fBW` is `3.16x` SU throughput and lowers mean delay by about 38%;
- 20 MHz `fHoL` is `2.69x` SU throughput with perfectly even delivery;
- 20 MHz `fBW` is about 17% faster than `fHoL`, as expected from the wider RU
  layout;
- widening `fBW` from 20 to 80 MHz increases delivered throughput by about
  70% and removes the backlog; and
- at 80 MHz, `fHoL` and `fBW` both meet the offered load, with `fHoL` reducing
  mean delay by about 7.3%.

## Vector result

The packet-arrival vectors were binned into consecutive 50 ms intervals. The
result confirms sustained delivery rather than a scalar caused by one burst:

| Configuration | 0.70-.75 | .75-.80 | .80-.85 | .85-.90 | .90-.95 | .95-1.00 |
|---|---:|---:|---:|---:|---:|---:|
| 20 MHz `fBW` | 23.552 | 23.552 | 23.552 | 23.040 | 23.552 | 24.064 |
| 20 MHz `fHoL` | 19.968 | 19.968 | 19.968 | 19.968 | 20.736 | 19.968 |
| 20 MHz SU | 7.632 | 7.104 | 7.616 | 7.648 | 7.120 | 7.632 |
| 80 MHz `fBW` | 40.112 | 40.144 | 39.760 | 40.320 | 39.648 | 40.032 |
| 80 MHz `fHoL` | 40.128 | 39.888 | 39.888 | 39.984 | 39.984 | 39.936 |
| 80 MHz SU | 22.336 | 22.352 | 22.336 | 22.352 | 22.384 | 22.336 |

Values are aggregate application Mbps. The delay vectors independently match
the scalar histogram counts, means, minima, and maxima shown above.

## PCAP and TShark verification

The following command records the AP MAC capture without permanently enabling
capture in `omnetpp.ini`:

```sh
../../../bin/inet --release -u Cmdenv -f omnetpp.ini \
  -c EqualSizedRUs_fBW -r 0 --result-dir=results/pcap_comparison/fBW \
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
  -Y 'wlan.trigger.he.trigger_type == 2' \
  -T fields -E header=y -E occurrence=a \
  -e frame.time_relative \
  -e wlan.trigger.he.user_info.aid12 \
  -e wlan.trigger.he.ru_allocation
```

The AP captures are PCAPng files with native IEEE 802.11 encapsulation:

| Capture | Packets | MU-BAR triggers | Block Ack frames | Observed response grouping |
|---|---:|---:|---:|---|
| 20 MHz `fBW` | 1842 | 456 | 912 | 2 simultaneous BAs per trigger |
| 20 MHz `fHoL` | 1323 | 261 | 783 | 3 simultaneous BAs per trigger |
| 20 MHz SU | 450 | 0 | 144 | ordinary SU BAR/BA only |

Representative `fBW` Trigger rows contain rotating AID pairs and TShark RU
values `53,54`, which INET's serializer maps to the two 106-tone positions.
Representative `fHoL` rows contain AIDs `1,2,3` and values `37,38,39`, the first
three positions in the four-52-tone layout. This is direct protocol-visible
evidence that the policies select different RU layouts.

The two `fBW` HE-TB Block Acks have identical timestamps after each Trigger;
the three `fHoL` responses do likewise. The Block Ack starting sequence numbers
also advance from 1 to values above 800, proving that the bitmap window advances
beyond its initial 64 MPDUs instead of stalling.

## Defects found and fixed

The poor earlier result was not just a parameter problem. Two code defects and
several scenario confounders hid the OFDMA benefit:

1. `HeDlSchedulerEqualSizedRUs` estimated each allocation's packing horizon
   from only the HoL MPDU. The packing planner therefore stopped at roughly one
   MPDU per user even when an eligible backlog and A-MPDU capacity existed. It
   now estimates from the complete eligible per-user backlog.
2. A triggered Block Ack response used the recipient agreement's original
   starting sequence number instead of the starting sequence requested in the
   MU-BAR User Info. After the first 64-MPDU bitmap window, outstanding MPDUs
   were not released and DL MU service stalled. The response now uses the
   trigger-requested window.
3. The old workload used synchronized voice traffic, low power, a retry timeout
   longer than the useful setup interval, and an 80 MHz case that also changed
   host count and load. The controlled matrix removes those confounders.

Focused regression coverage verifies backlog-vs-HoL ranking, backlog-sized
packing duration, MU-BAR bitmap-window selection, BA-window exhaustion, and
HE MU TXOP trimming:

```sh
CCACHE_DISABLE=1 inet_run_unit_tests -m release \
  -f '(HeDlScheduler|Ieee80211HeDlMuTxOpFs|Ieee80211HeMuBlockAckGating).*\.test'
```

All four selected tests pass.

## Limitations

- INET carries some HE PHY information as packet metadata, so the capture does
  not expose every HE-SIG field exactly as a real monitor capture would.
- Application delivery is established from sink `.sca`/`.vec` results. Do not
  compare configurations by TShark-decoded UDP count because aggregated MPDUs
  may be presented as opaque 802.11 payloads to the dissector.
- The results show the intended scheduler trade-off for this workload, not a
  universal claim that one policy or OFDMA always wins.

Within those limits, the example now clearly demonstrates simultaneous
multi-station delivery, the throughput advantage of DL OFDMA over matched SU
OFDM, the `fBW` versus `fHoL` trade-off, and the capacity effect of 20 versus
80 MHz.
