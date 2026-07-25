# Dynamic Fragmentation Walkthrough

This walkthrough reports only transmitted-frame size, acknowledgment airtime,
and delivery evidence for the configurations in [omnetpp.ini](omnetpp.ini).

## Five-run comparison

The scalar/vector session
[`20260725T120411Z`](results/scalar-vector/20260725T120411Z) contains runs
`0` through `4` for `DynamicFragmentation`, `StaticFragmentation`, and
`NoFragmentation`.

| Configuration | `packetSentToPeer:vector(packetBytes)` mean frame size | `acknowledgmentAirtime:vector` total | Delivery evidence |
|---|---:|---:|---|
| `DynamicFragmentation` | `293.002 ± 0.792 B` | `42.205 ± 0.537 ms` | `server.app[0] packetReceived:vector(packetBytes)` contains `1019, 1021, 1019, 1021, 1019` packets in runs `0–4`. |
| `StaticFragmentation` | `293.002 ± 0.792 B` | `42.205 ± 0.537 ms` | `server.app[0] packetReceived:vector(packetBytes)` contains `1019, 1021, 1019, 1021, 1019` packets in runs `0–4`. |
| `NoFragmentation` | `1070.000 ± 0.000 B` | `32.640 ± 0.000 ms` | `server.app[0] packetReceived:vector(packetBytes)` contains `1023` packets in each run. |

The dynamic and static size and ACK-airtime measurements are identical in this
session. The unfragmented configuration records larger MAC frames and less
total acknowledgment airtime. These vectors demonstrate the configured
fragment-size outcome and its acknowledgment cost; they do not demonstrate
opportunity-dependent changes in fragment size. The server delivery vector
also records four to five fewer packets per fragmented run than the
unfragmented runs.

## Reproduce and inspect

```sh
bin/inet -u Cmdenv -c DynamicFragmentation -r 0 examples/ieee80211ax/mac_features/dynamic_fragmentation/omnetpp.ini --result-dir=examples/ieee80211ax/mac_features/dynamic_fragmentation/results/manual
```

```sh
opp_scavetool query -l \
  -f 'name =~ "packetSentToPeer:vector(packetBytes)" or name =~ "acknowledgmentFrameType:vector" or name =~ "acknowledgmentAirtime:vector" or name =~ "packetReceived:vector(packetBytes)"' \
  examples/ieee80211ax/mac_features/dynamic_fragmentation/results/manual/*.sca \
  examples/ieee80211ax/mac_features/dynamic_fragmentation/results/manual/*.vec
```

The retained packet-statistics session
[`20260724T175025Z`](results/packet-statistics/20260724T175025Z) contains the
`DynamicFragmentation` AP/host PCAPs and matching `.sca`/`.vec` files. The exact
table below records `6667` AP/global observations, including `5937` aggregated
QoS-data observations with mean size `376.0 B`, `429` BAR observations, `264`
BA observations, and `18` Ack observations. Those are capture-point
observations; delivery must be checked with the server packet-received vector.

```sh
tshark -n -r 'examples/ieee80211ax/mac_features/dynamic_fragmentation/results/packet-statistics/20260724T175025Z/DynamicFragmentation/DynamicFragmentation-#0Lan80211AxUlOfdma.ap.wlan[0].pcap' -c 20
```

<!-- REWRITE-PREFIX-END -->


<!-- BEGIN GENERATED: ieee80211ax-pcap-statistics -->
## 802.11 Packet Type Statistics
![802.11 Packet Type Statistics](packet_statistics.png)

This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from AP wireless-interface observation points. With multiple AP captures, one medium transmission may be observed at more than one AP; counts and airtime therefore represent recorded transmission observations, not de-duplicated application packets.

Capture session `20260724T175025Z` was generated from fresh PCAPng input with `TShark (Wireshark) 4.6.4.`. HE PPDU format, MCS, coding, bandwidth/RU, GI, and NSTS are decoded directly from standards-compliant radiotap HE fields; values not marked known by the recorder are omitted.

Two estimated airtime occupancy percentages are provided. HE-SU and HE-ER-SU use the modeled 36/44 µs preambles; a dissector-expanded A-MPDU is charged one shared preamble. HE MU/TB user-dependent signaling not exposed by radiotap remains approximate.
- **Air Time %**: This frame type's share of the sum of all estimated frame airtimes.
- **Air Time (Sim Time) %**: The sum of this frame type's estimated airtimes divided by the simulation time limit. Concurrent transmissions from multiple capture points are counted separately, so this value can exceed 100%; it is not the union of busy channel time.

### Evidence checks

| Status | Requirement | Observed evidence |
|---|---|---|
| **PASS** | DynamicFragmentation produced protocol-visible wireless observations | 6667 AP/global transmission observations |
| **INCONCLUSIVE** | Capability gate, fragment numbers, sizes, More Fragments and acknowledgment | The packet-type table is exchange evidence only; use the recorded feature vectors/results |

### Configuration: `DynamicFragmentation`
Total over-the-air frame/MPDU transmission observations (Global BSS/AP): **6667**

| Color | Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Mean Duration | Std Dev Duration | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#17cf23" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC, A-MPDU] | 5937 | 89.05% | 376.0 B | 176.8 B | 208.6 us | 99.3 us | 5010 MHz | -63.4 dBm | - | 97.26% | 61.91% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#2cce3f" /></svg> | Data: QoS Data [HE-SU, HE-MCS 1, 20 MHz, GI 3.2 us, LDPC] | 13 | 0.19% | 401.5 B | 187.1 B | 255.6 us | 102.3 us | 5010 MHz | -63.5 dBm | - | 0.26% | 0.17% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ab6c30" /></svg> | Control: Block Ack Request (BAR) | 429 | 6.43% | 24.0 B | 0.0 B | 28.0 us | 0.0 us | 5010 MHz | -62.8 dBm | - | 0.94% | 0.60% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#0946c8" /></svg> | Control: Block Ack (BA) | 264 | 3.96% | 152.0 B | 0.0 B | 70.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 1.47% | 0.93% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 12 | 0.18% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | - | 10.0 dBm | 0.02% | 0.01% |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#4799eb" /></svg> | Control: Ack | 6 | 0.09% | 14.0 B | 0.0 B | 24.7 us | 0.0 us | 5010 MHz | -63.7 dBm | 10.0 dBm | 0.01% | 0.01% |
| <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> | <hr> |
| <svg width="16" height="16"><rect width="16" height="16" rx="3" fill="#ec1313" /></svg> | Management: Action | 6 | 0.09% | 37.0 B | 0.0 B | 69.3 us | 0.0 us | 5010 MHz | -63.7 dBm | 10.0 dBm | 0.03% | 0.02% |

<!-- END GENERATED: ieee80211ax-pcap-statistics -->
