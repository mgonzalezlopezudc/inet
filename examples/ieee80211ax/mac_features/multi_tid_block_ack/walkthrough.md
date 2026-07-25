# Walkthrough - HE Multi-TID Block Ack

This example provides three traffic arrangements for inspecting Block Ack
exchanges when more than one traffic identifier (TID) or station is involved.

## Configurations

| Configuration | Input file | Traffic visible in the retained scalar file |
|---|---|---|
| `MultiTidBlockAck` | `downlink.ini` | The server sends two application flows to each of hosts 0 and 1. |
| `UlSuMultiTidBlockAck` | `uplink.ini` | Host 0 sends two application flows. |
| `UlMuMultiTidBlockAck` | `uplink.ini` | Hosts 0 and 1 each send one application flow; host 2 has no application send result. |

These descriptions are limited to the application counters in the retained
`.sca` files. TID values and Block Ack contents must be checked in decoded
packet fields or dedicated model telemetry.

## Run the configurations

```sh
bin/inet -u Cmdenv -c MultiTidBlockAck \
  examples/ieee80211ax/mac_features/multi_tid_block_ack/downlink.ini

bin/inet -u Cmdenv -c UlSuMultiTidBlockAck \
  examples/ieee80211ax/mac_features/multi_tid_block_ack/uplink.ini

bin/inet -u Cmdenv -c UlMuMultiTidBlockAck \
  examples/ieee80211ax/mac_features/multi_tid_block_ack/uplink.ini
```

To create packet captures in a separate result directory:

```sh
bin/inet -u Cmdenv -c UlMuMultiTidBlockAck \
  examples/ieee80211ax/mac_features/multi_tid_block_ack/uplink.ini \
  --result-dir=examples/ieee80211ax/mac_features/multi_tid_block_ack/results/manual-run \
  --**.numPcapRecorders=1 \
  --**.checksumMode=\"computed\" \
  --**.fcsMode=\"computed\"
```

## Scalar/vector run evidence

The run-0 scalar/vector artifacts for all three configurations are under the
retained `results/scalar-vector/20260725T120411Z` session:

```sh
opp_scavetool query -l \
  -f 'name =~ "packetSent:count" or name =~ "packetReceived:count"' \
  examples/ieee80211ax/mac_features/multi_tid_block_ack/results/scalar-vector/20260725T120411Z/UlMuMultiTidBlockAck/UlMuMultiTidBlockAck-\#0.sca

opp_scavetool query -l \
  -f 'type =~ vector and module =~ "*.app[*]" and (name =~ "packetSent:vector(packetBytes)" or name =~ "packetReceived:vector(packetBytes)")' \
  examples/ieee80211ax/mac_features/multi_tid_block_ack/results/scalar-vector/20260725T120411Z/UlMuMultiTidBlockAck/UlMuMultiTidBlockAck-\#0.vec
```

The run-0 scalar and application-vector queries report:

| Configuration and application result | Scalar count | Vector count and packet-byte range |
|---|---:|---|
| `MultiTidBlockAck`: `server.app[0..3] packetSent` | 141, 71, 141, 71 | 141 × 1000 B, 71 × 200 B, 141 × 1000 B, 71 × 200 B |
| `MultiTidBlockAck`: `host[0].app[0..1] packetReceived` | 134, 68 | 134 × 1000 B, 68 × 200 B |
| `MultiTidBlockAck`: `host[1].app[0..1] packetReceived` | 133, 68 | 133 × 1000 B, 68 × 200 B |
| `UlSuMultiTidBlockAck`: `host[0].app[0..1] packetSent` | 341, 171 | 341 × 1000 B, 171 × 200 B |
| `UlSuMultiTidBlockAck`: `server.app[0..1] packetReceived` | 340, 170 | 340 × 1000 B, 170 × 200 B |
| `UlMuMultiTidBlockAck`: `host[0..1].app[0] packetSent` | 341, 341 | 341 × 1000 B for each host |
| `UlMuMultiTidBlockAck`: `server.app[0..1] packetReceived` | 340, 340 | 340 × 1000 B for each application |

Exact run-0 artifacts:

- [`MultiTidBlockAck-#0.sca`](results/scalar-vector/20260725T120411Z/MultiTidBlockAck/MultiTidBlockAck-%230.sca) and [`MultiTidBlockAck-#0.vec`](results/scalar-vector/20260725T120411Z/MultiTidBlockAck/MultiTidBlockAck-%230.vec)
- [`UlSuMultiTidBlockAck-#0.sca`](results/scalar-vector/20260725T120411Z/UlSuMultiTidBlockAck/UlSuMultiTidBlockAck-%230.sca) and [`UlSuMultiTidBlockAck-#0.vec`](results/scalar-vector/20260725T120411Z/UlSuMultiTidBlockAck/UlSuMultiTidBlockAck-%230.vec)
- [`UlMuMultiTidBlockAck-#0.sca`](results/scalar-vector/20260725T120411Z/UlMuMultiTidBlockAck/UlMuMultiTidBlockAck-%230.sca) and [`UlMuMultiTidBlockAck-#0.vec`](results/scalar-vector/20260725T120411Z/UlMuMultiTidBlockAck/UlMuMultiTidBlockAck-%230.vec)

The scalar counts establish only the recorded application sends and receives.
The application vectors additionally establish the recorded sample counts and
packet-byte values shown in the table.
They do not identify a Block Ack variant, decoded per-TID entries, aggregation,
retransmissions, or why a packet was not received.

## Packet-statistics evidence

The `20260724T175025Z` packet-statistics session contains `.sca`, `.vec`,
AP/STA PCAP files as applicable, and Cmdenv output for each configuration.
The packet tables below use its AP captures. Its scalar files can be queried
independently with:

```sh
opp_scavetool query -l \
  -f 'name =~ "packetSent:count" or name =~ "packetReceived:count"' \
  examples/ieee80211ax/mac_features/multi_tid_block_ack/results/packet-statistics/20260724T175025Z/UlMuMultiTidBlockAck/UlMuMultiTidBlockAck-\#0.sca
```

Inspect an AP capture directly:

```sh
tshark -n \
  -r examples/ieee80211ax/mac_features/multi_tid_block_ack/results/packet-statistics/20260724T175025Z/UlMuMultiTidBlockAck/UlMuMultiTidBlockAck-\#0Lan80211AxUlOfdma.ap.wlan[0].pcap \
  -c 20
```

The following tables summarize frame labels in the named AP PCAPs. A row is a
recorded observation at that capture point. The tables do not de-duplicate
transmissions and do not expose Block Ack Control variants or per-AID/TID
entries.

### `MultiTidBlockAck`

Source:
[`MultiTidBlockAck AP PCAP`](results/packet-statistics/20260724T175025Z/MultiTidBlockAck/MultiTidBlockAck-%230Lan80211AxDlOfdma.ap.wlan[0].pcap)

| Decoded frame label | Observations |
|---|---:|
| QoS Data | 401 |
| Block Ack Request | 401 |
| Block Ack | 401 |
| Ack | 12 |
| Action | 8 |
| Record displayed by TShark as `PV1 QoS Data - with one SID` | 1 |
| **Total PCAP records** | **1224** |

### `UlSuMultiTidBlockAck`

Source:
[`UlSuMultiTidBlockAck AP PCAP`](results/packet-statistics/20260724T175025Z/UlSuMultiTidBlockAck/UlSuMultiTidBlockAck-%230Lan80211AxUlOfdma.ap.wlan[0].pcap)

| Decoded frame label | Observations |
|---|---:|
| QoS Data | 510 |
| Block Ack Request | 33 |
| Block Ack | 33 |
| Ack | 343 |
| Action | 2 |
| **Total PCAP records** | **921** |

### `UlMuMultiTidBlockAck`

Source:
[`UlMuMultiTidBlockAck AP PCAP`](results/packet-statistics/20260724T175025Z/UlMuMultiTidBlockAck/UlMuMultiTidBlockAck-%230Lan80211AxUlOfdma.ap.wlan[0].pcap)

| Decoded frame label | Observations |
|---|---:|
| QoS Data | 1176 |
| QoS Null | 45 |
| Trigger | 16 |
| Block Ack | 15 |
| Ack | 16 |
| **Total PCAP records** | **1268** |

## What remains inconclusive

The retained scalar files and the frame-label tables show application
counters and the presence of BAR/BA or Trigger/BA exchanges. They do not prove
that one response acknowledged multiple TIDs, identify the BA Control variant,
show decoded per-AID/TID entries, or quantify an airtime benefit. Those
conclusions require queryable decoded fields or dedicated vectors that are not
presented by these retained summaries.
