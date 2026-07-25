# HE Channel-Width Comparison

This example compares the 20, 40, 80, and 160 MHz configurations defined in
[omnetpp.ini](omnetpp.ini) on the
[HeChannelWidthsNetwork](HeChannelWidthsNetwork.ned) topology. The recorded
results measure application delivery and delay; the packet captures establish
the transmitted bandwidth labels and frame distributions.

## Run the example

Run one configuration from the repository root:

```sh
bin/inet -u Cmdenv -c Width20MHz \
  examples/ieee80211ax/he_channel_widths/omnetpp.ini -r 0
```

Generate runs `0–4` for every width and rebuild the dashboard:

```sh
python3 examples/ieee80211ax/analysis/run_campaign.py width -j$(nproc)
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211ax/analysis/first_tranche.py width
```

The result campaign is under
[`results/scalar-vector/20260725T120411Z`](results/scalar-vector/20260725T120411Z).
The dashboard provenance,
[`channel-width-dashboard.png.json`](../analysis/figures/width/channel-width-dashboard.png.json),
lists all `.sca` and `.vec` inputs with SHA-256 digests, bandwidth metadata,
runs `0–4`, and the `0.3–0.43 s` measurement window.

## Scalar/vector evidence

The dashboard computes aggregate goodput from
`packetReceived:vector(packetBytes)` and p95 latency from
`endToEndDelay:vector`, both at `**.app[*]`. Its bars use per-run values with
95% Student-t confidence intervals; its ECDF uses run 0.

```sh
opp_scavetool query -l \
  -f 'type =~ vector AND module =~ "**.app[*]" AND (name =~ "packetReceived:vector(packetBytes)" OR name =~ "endToEndDelay:vector")' \
  examples/ieee80211ax/he_channel_widths/results/scalar-vector/20260725T120411Z/*/*.vec
```

![Channel-width goodput and delay dashboard](../analysis/figures/width/channel-width-dashboard.png)

| Configuration | Aggregate goodput | p95 end-to-end delay |
|---|---:|---:|
| `Width20MHz` | 26.892 ± 0.000 Mbps | 97.253 ± 0.057 ms |
| `Width40MHz` | 50.708 ± 0.000 Mbps | 63.809 ± 0.077 ms |
| `Width80MHz` | 81.428 ± 0.335 Mbps | 40.060 ± 0.075 ms |
| `Width160MHz` | 118.646 ± 0.000 Mbps | 9.406 ± 0.075 ms |

The `packetReceived:vector(packetBytes)` values increase monotonically across
these four configurations, while the p95 of `endToEndDelay:vector` decreases
monotonically. These measurements characterize this configured topology,
traffic load, PHY rate, and sensitivity set; they do not measure coverage.

## Packet evidence

The retained run-0 packet session is
[`results/packet-statistics/20260724T175025Z`](results/packet-statistics/20260724T175025Z).
The exact AP/global transmission-observation totals are:

| Configuration | Transmission observations |
|---|---:|
| `Width20MHz` | 781 |
| `Width40MHz` | 734 |
| `Width80MHz` | 791 |
| `Width160MHz` | 865 |

These totals are capture-point observations rather than de-duplicated
application packets, and their non-monotonic order is not a capacity metric.

The packet-statistics QoS Data rows provide a direct bandwidth/duration check:

| Configuration | Decoded QoS Data format | Count | Mean duration |
|---|---|---:|---:|
| `Width20MHz` | HE-SU, MCS 1, 20 MHz | 5 | 619.1 µs |
| `Width40MHz` | HE-SU, MCS 1, 40 MHz | 5 | 327.6 µs |
| `Width80MHz` | HE-SU, MCS 1, 80 MHz | 5 | 175.2 µs |
| `Width160MHz` | HE-SU, MCS 1, 160 MHz | 5 | 105.6 µs |

Each row has mean size `1066.0 B`, GI `3.2 µs`, and LDPC in the retained
packet-statistics table. Thus the radiotap-decoded QoS Data rows establish the
configured bandwidth labels and their measured durations; application
capacity remains the `packetReceived:vector(packetBytes)` measurement above.

Inspect an AP capture:

```sh
tshark -n -r \
  'examples/ieee80211ax/he_channel_widths/results/packet-statistics/20260724T175025Z/Width20MHz/Width20MHz-#0HeChannelWidthsNetwork.ap.wlan[0].pcap' \
  -c 20
```

Regenerate the packet artifacts with:

```sh
MPLCONFIGDIR=/tmp/matplotlib \
  python3 examples/ieee80211ax/analysis/analyze_pcap_types.py \
  --generate --subdir he_channel_widths
```

See `packet_statistics.png` for the complete Trigger, Block Ack, Action,
QoS Data, frequency, power, and estimated-airtime breakdown.
