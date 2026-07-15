# Walkthrough: 802.11ax Downlink OFDMA

This example compares ordinary single-user (SU) OFDM with 802.11ax downlink
OFDMA. In SU operation the AP gives the complete channel to one receiver per
transmission. With OFDMA, the AP divides an HE MU PPDU into resource units
(RUs) and serves several receivers at the same time.

The experiments deliberately cover three different questions:

1. a low-load 20 MHz comparison, where OFDMA can reduce the cost of serving
   several sparse downlink queues;
2. a higher-load 80 MHz comparison, where scheduler efficiency and capacity
   become visible; and
3. an asymmetric overload scenario, where aggregate throughput alone hides
   important per-flow behavior.

All reported performance values use seed sets 0 through 4. Throughput is the
mean application payload throughput in the `0.3–0.88 s` measurement interval.
The p95 delay is the nearest-rank percentile of application delay samples
pooled across the five seeds and restricted to the same interval.

## Rate and buffer controls

The SU PHY rate is set by `**.wlan[*].bitrate`. The DL MU scheduler normally
selects an MCS separately for each RU from estimated SNR. To prevent OFDMA
from gaining an unintended rate advantage, `heMcsSnrThresholds` permits only
MCS 0 and MCS 1. MCS 1 corresponds to `14.625 Mbps` over 20 MHz and
`61.25 Mbps` over 80 MHz with the configured guard interval.

Those values are PHY data-field rates, not application goodput limits. MAC and
PHY headers, interframe spaces, contention, acknowledgments, Block Ack setup,
padding, retransmissions, and small-packet inefficiency all consume airtime.
Conversely, an aggregate OFDMA throughput can exceed `14.625 Mbps` only if its
RUs use independent PHY rates whose total exceeds the full-channel reference
rate. The MCS cap removes that earlier confounder in this example.

Queue capacity also needs careful treatment. The HE HCF creates one 100-packet
queue per STA and access category, so three downlink best-effort destinations
have a total budget of 300 packets. Ordinary HCF uses one shared best-effort
pending queue. The SU configurations therefore set only the AP's `AC_BE`
pending queue to 300 packets.

This is a fair comparison of total packet-buffer budget, but it does **not**
make the queueing systems equivalent. OFDMA still has three isolated FIFO
queues, while SU has one shared FIFO. Per-STA queues isolate drops and let the
scheduler choose among destinations; a shared queue can exhibit cross-flow
head-of-line effects. Queue structure is part of the mechanism being compared.

Increasing the SU queue from 100 to 300 packets does not increase its service
rate. It retains more backlog and trades some early drops for longer queueing
delay. That is why the matched-buffer SU result has a higher p95 delay than the
earlier 100-packet baseline.

## Comparison matrix

The topology, station positions, transmit power, best-effort classification,
packet size, warm-up, normal-traffic start, duration, and MCS cap are common to
the controlled configurations. The offered load intentionally differs between
the 20 and 80 MHz experiments:

| Configuration | Width | Offered load | MAC/scheduler |
|---|---:|---:|---|
| `EqualSizedRUs_fBW` | 20 MHz | 2.4 Mbps | DL OFDMA, maximize occupied bandwidth |
| `EqualSizedRUs_fHoL` | 20 MHz | 2.4 Mbps | DL OFDMA, serve candidates by HoL policy |
| `SuEdcaBaseline` | 20 MHz | 2.4 Mbps | SU OFDM, shared 300-packet `AC_BE` queue |
| `EqualSizedRUs80MHz_fBW` | 80 MHz | 24 Mbps | high-load DL OFDMA, `fBW` |
| `EqualSizedRUs80MHz_fHoL` | 80 MHz | 24 Mbps | high-load DL OFDMA, `fHoL` |
| `SuEdcaBaseline80MHz` | 80 MHz | 24 Mbps | high-load SU OFDM, shared 300-packet queue |

At 20 MHz, each source sends a 100-byte packet every `1 ms`. The 80 MHz base
configuration overrides that interval to `0.1 ms`, increasing aggregate load
tenfold. Thus the 80 MHz rows are a high-load capacity comparison, not a
one-variable channel-width comparison with the 20 MHz rows.

`WideBandwidth80MHz` remains a separate eight-station scaling scenario and is
not part of this three-station comparison.

## Equal-RU scheduler behavior

At 20 MHz, the relevant equal-RU layouts contain two 106-tone RUs or four
52-tone RUs:

- `fBW` selects a wide layout that fits the chosen users. It normally serves
  two stations on two 106-tone RUs and rotates service.
- `fHoL` selects a smaller-RU layout that accommodates all three eligible
  stations, using three 52-tone positions.

At low load both policies have enough capacity. The wider per-user RUs make
`fBW` somewhat faster at draining a selected queue. At high load, the layout
choice matters more: serving all candidates on smaller RUs can reduce total
goodput even though more users appear in each MU transmission.

## Controlled performance results

| Configuration | Aggregate app throughput | p95 E2E delay |
|---|---:|---:|
| `EqualSizedRUs_fBW` | 2.399 Mbps | 1.68 ms |
| `EqualSizedRUs_fHoL` | 2.395 Mbps | 2.31 ms |
| `SuEdcaBaseline` | 1.588 Mbps | 155.96 ms |
| `EqualSizedRUs80MHz_fBW` | 22.465 Mbps | 11.47 ms |
| `EqualSizedRUs80MHz_fHoL` | 19.607 Mbps | 14.03 ms |
| `SuEdcaBaseline80MHz` | 22.389 Mbps | 13.42 ms |

The 20 MHz workload is sparse enough for both OFDMA policies to deliver
almost all `2.4 Mbps`. The SU AP cannot serve the three streams as efficiently
and its shared queue saturates. Matching its aggregate buffer budget does not
fix the service-rate deficit: the queue reaches 300 packets in every seed,
ends with 298.2 packets on average, and drops 413.6 packets per run on average.
Both OFDMA configurations have zero queue-overflow drops and a maximum of two
packets in any per-STA queue.

At 80 MHz, `fBW` and SU have nearly equal aggregate throughput. `fBW` has about
15% lower p95 delay (`11.47 ms` versus `13.42 ms`), but this result does not
support a blanket claim that OFDMA always has higher throughput. `fHoL` serves
more destinations with smaller RUs and delivers less aggregate traffic under
this load. Its p95 delay is also slightly higher than SU's.

The 24 Mbps load is close enough to the effective capacity to saturate queues:
the mean total overflow counts are 1034.2 for `fBW`, 3504.0 for `fHoL`, and
961.4 for SU. Those are full-run queue counters, whereas the throughput and
delay values use only `0.3–0.88 s`. They show that the high-load experiment is
a scheduler/capacity stress test rather than an uncongested latency test.

The point of OFDMA is therefore not an unconditional aggregate-throughput
gain. It is the ability to schedule multiple destination queues in one access,
which can greatly reduce latency and queue buildup for many small or sparse
flows. Whether that benefit outweighs smaller-RU rates and MU overhead depends
on bandwidth, load, packet sizes, and scheduler policy.

## Asymmetric flows: per-flow results

`BacklogBased` and `HoLMinDelay` use the same deliberately overloaded traffic:

| Flow | Destination | Packet/interval | Offered load |
|---|---|---:|---:|
| Heavy | `host[0]` | 1000 B / 0.1 ms | 80 Mbps |
| Medium | `host[1]` | 400 B / 0.4 ms | 8 Mbps |
| Light | `host[2]` | 100 B / 1 ms | 0.8 Mbps |

The aggregate offered load is `88.8 Mbps`, far beyond this 20 MHz scenario's
application capacity. Consequently, absolute offered-load satisfaction is
more informative than aggregate throughput alone.

| Scheduler | Flow | Throughput | Delivered/offered | Per-flow p95 delay |
|---|---|---:|---:|---:|
| `BacklogBased` | Heavy | 3.986 Mbps | 5.0% | 209.31 ms |
| `BacklogBased` | Medium | 2.109 Mbps | 26.4% | 155.35 ms |
| `BacklogBased` | Light | 0.657 Mbps | 82.1% | 98.15 ms |
| `HoLMinDelay` | Heavy | 3.652 Mbps | 4.6% | 221.36 ms |
| `HoLMinDelay` | Medium | 1.455 Mbps | 18.2% | 221.24 ms |
| `HoLMinDelay` | Light | 0.364 Mbps | 45.5% | 220.96 ms |

The corresponding aggregate throughput and pooled p95 delay are:

| Scheduler | Aggregate throughput | Aggregate p95 delay |
|---|---:|---:|
| `BacklogBased` | 6.751 Mbps | 208.78 ms |
| `HoLMinDelay` | 5.472 Mbps | 221.27 ms |

Several details disappear in the aggregate table:

- `BacklogBased` delivers more traffic and lower p95 delay for every flow in
  this five-seed campaign, not only for the aggregate.
- The heavy flow contributes the most absolute throughput but receives only
  about 5% of its offered load because its source is intentionally extreme.
- Under `BacklogBased`, the light flow receives 82.1% of its offered traffic
  and has much lower p95 delay than the other flows. `HoLMinDelay` produces
  nearly the same high p95 delay for all three flows and serves only 45.5% of
  the light load.
- Aggregate p95 delay mixes flows with different packet sizes, rates, and
  sample counts. It must not be interpreted as the delay experienced by each
  flow.

These observations are properties of this workload and implementation; they
are not a universal ordering of the two scheduling algorithms.

## Reproducing the results

Run from `examples/ieee80211ax/dl_ofdma`. The configurations define run number
0, so vary `--seed-set` rather than using run numbers 0 through 4:

```sh
../../../bin/inet --release -u Cmdenv -f omnetpp.ini \
  -c EqualSizedRUs_fBW -r 0 --seed-set=0 \
  --result-dir=results/comparison/EqualSizedRUs_fBW/seed0
```

Repeat for seed sets 0 through 4 and for the configurations in the tables.
Each verified run exited successfully at the `1 s` simulation-time limit and
produced `.sca`, `.vec`, and `.vci` files.

Inspect run metadata before combining files:

```sh
opp_scavetool query -a \
  'results/comparison/EqualSizedRUs_fBW/seed0/EqualSizedRUs_fBW-#0.sca'
```

List sink totals and delay vectors with:

```sh
opp_scavetool query -s -l \
  -f 'module =~ **.host[*].app[0] AND (name =~ packetReceived* OR name =~ endToEndDelay*)' \
  results/comparison/*/seed*/*.sca

opp_scavetool query -v -l \
  -f 'module =~ **.host[*].app[0] AND (name =~ packetReceived* OR name =~ endToEndDelay*)' \
  results/comparison/*/seed*/*.vec
```

Per-flow analysis must retain the `host[0]`, `host[1]`, and `host[2]` module
names instead of summing them before computing percentiles. Apply the same
`0.3–0.88 s` timestamp filter to both received-byte and delay vectors.

## Protocol verification

The short `10 ms` ADDBA retry interval lets all independently contending
stations complete Block Ack setup before the measurement interval. Port 80
maps the comparison traffic to `AC_BE`, avoiding the finite voice TXOP as an
accidental confounder.

For a structural packet-exchange check, enable AP MAC capture from the command
line rather than permanently changing `omnetpp.ini`:

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

The native INET MAC capture exposes QoS Data, Action, ACK, and related frame
exchanges, but not every HE RU field carried internally as packet metadata.
Use sink vectors to establish delivery and HE result vectors for RU analysis;
do not infer application goodput from decoded UDP frame counts in an aggregated
802.11 capture.

## Standards baseline

The normative reference is IEEE Std 802.11-2024:

- Clause 26.5.1.1 defines HE DL MU operation and permits simultaneous delivery
  by DL OFDMA, DL MU-MIMO, or both. It also requires PSDUs on allocated RUs to
  be padded to finish together.
- Clause 26.5.1.2 requires the HE MU TXVECTOR to identify the STA associated
  with each RU.
- Clause 26.6.2.2 defines padding of per-user A-MPDUs in an HE MU PPDU.
- Clause 9.3.1.22.4 defines the MU-BAR Trigger User Info field.
- Clause 26.5.2.3.3 specifies the HE-TB response parameters indicated by a
  Trigger frame.

The standard defines frame formats and protocol behavior. It does not define
INET's `fBW`, `fHoL`, `BacklogBased`, or `HoLMinDelay` heuristics, and it does
not require OFDMA to outperform SU for every workload.
