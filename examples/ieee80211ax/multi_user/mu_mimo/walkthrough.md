# Walkthrough - HE MU-MIMO

This example demonstrates how IEEE 802.11ax assigns several users to one
frequency resource while separating them by spatial stream. OFDMA separates
users in frequency; MU-MIMO reuses the same RU in space.

The retained evidence has two scopes:

- `results/scalar-vector/20260725T120411Z/` contains five matched runs of the
  20 MHz downlink `DlMuMimo` and `EqualSizedRUs_fBW` configurations.
- `results/packet-statistics/20260724T175025Z/` contains AP and station
  captures for `DlMuMimo` and `UlMuMimo`.

The scalar/vector session establishes the downlink application comparison and
the AP's full-bandwidth spatial-stream allocations. The packet session
establishes protocol-visible sounding and Trigger exchanges. These are
packet-level model results, not a general real-world MU-MIMO capacity claim.

## Configurations

- `DlMuMimo`: 20 MHz, three stations, a four-antenna AP, downlink sounding,
  and full-bandwidth MU-MIMO.
- `EqualSizedRUs_fBW`: the matched 20 MHz, three-station OFDMA control. It
  separates the users into frequency-domain RUs.
- `SuEdcaBaseline`: the 20 MHz single-user downlink control.
- `DlMuMimo80MHz`: 80 MHz, eight stations, and an eight-antenna AP.
- `UlMuMimo`: 20 MHz, three saturated uplinks, a four-antenna AP, and
  full-bandwidth uplink MU-MIMO.
- `EdcaBaseline`: the saturated uplink workload using EDCA.

The focused settings are in [downlink.ini](downlink.ini) and
[uplink.ini](uplink.ini). `DlMuMimo` and `EqualSizedRUs_fBW` both use a
1,000-byte application payload every 1 ms and
`dlMuAckMethod = "sequentialBar"`. The three downlink flows therefore offer
24 Mbit/s in total under the same acknowledgment policy.

## Reproduce scalar/vector runs

Run one configuration and repetition at a time:

```sh
mkdir -p results/validation/dl-mu results/validation/dl-ofdma

bin/inet -u Cmdenv -c DlMuMimo -r 0 \
  --result-dir=results/validation/dl-mu \
  examples/ieee80211ax/multi_user/mu_mimo/downlink.ini

bin/inet -u Cmdenv -c EqualSizedRUs_fBW -r 0 \
  --result-dir=results/validation/dl-ofdma \
  examples/ieee80211ax/multi_user/mu_mimo/downlink.ini
```

Repeat with run numbers 1 through 4 and distinct result directories.

Query the application payload vectors:

```sh
opp_scavetool query -l \
  -f 'module =~ "*.host*.app[0]" and name =~ "packetReceived:vector(packetBytes)"' \
  results/validation/dl-mu/*.vec results/validation/dl-ofdma/*.vec
```

Query the HE allocation vectors:

```sh
opp_scavetool query -l \
  -f 'module =~ "*.ap.wlan[0].radio" and (name =~ "heRuToneSize:vector" or name =~ "heStaId:vector" or name =~ "heSpatialStreams:vector" or name =~ "heStreamStartIndex:vector")' \
  results/validation/dl-mu/*.vec
```

## Five-run downlink result

Goodput is the sum of received `packetBytes` at the three station applications
over `[0.55 s, 0.88 s)`, divided by the 0.33 s interval. Each received vector
sample is 1,000 bytes. The inputs are the ten `.vec` files under
`results/scalar-vector/20260725T120411Z/DlMuMimo/` and
`results/scalar-vector/20260725T120411Z/EqualSizedRUs_fBW/`.

| Run | `DlMuMimo` packets | `DlMuMimo` goodput | `EqualSizedRUs_fBW` packets | `EqualSizedRUs_fBW` goodput |
|---:|---:|---:|---:|---:|
| 0 | 1,044 | 25.309 Mbit/s | 440 | 10.667 Mbit/s |
| 1 | 1,041 | 25.236 Mbit/s | 440 | 10.667 Mbit/s |
| 2 | 1,044 | 25.309 Mbit/s | 440 | 10.667 Mbit/s |
| 3 | 1,043 | 25.285 Mbit/s | 440 | 10.667 Mbit/s |
| 4 | 1,044 | 25.309 Mbit/s | 440 | 10.667 Mbit/s |
| Mean | -- | 25.290 Mbit/s | -- | 10.667 Mbit/s |

In this table, the mean goodput ratio is 2.37. The `DlMuMimo` mean has a
95% t-confidence interval of approximately ±0.039 Mbit/s; the five
`EqualSizedRUs_fBW` values are identical. This comparison is limited to the
matched configurations, five repetitions, and stated measurement interval.

The AP-radio vectors in
[`DlMuMimo-#0.vec`](results/scalar-vector/20260725T120411Z/DlMuMimo/DlMuMimo-%230.vec)
contain 727 records for each of `heRuToneSize`, `heStaId`,
`heSpatialStreams`, and `heStreamStartIndex`. `heRuToneSize` is 242 for every
record, `heStaId` ranges from 1 to 3, `heSpatialStreams` is one for every
record, and `heStreamStartIndex` ranges from 0 to 2. These vectors directly
show that the AP assigned one full-bandwidth RU to several station IDs using
disjoint one-stream ranges.

Application goodput alone does not prove MU-MIMO. The result is supported by
both the received-payload vectors and the HE allocation vectors named above.

## Reproduce packet captures

Use PCAPng, nanosecond timestamps, and computed checksums/FCS:

```sh
mkdir -p results/validation/pcap-dl results/validation/pcap-ul

bin/inet -u Cmdenv -c DlMuMimo -r 0 \
  --result-dir=results/validation/pcap-dl \
  '--**.numPcapRecorders=1' \
  '--**.pcapRecorder[*].moduleNamePatterns="wlan[0]"' \
  '--**.pcapRecorder[*].dumpProtocols="ieee80211mac"' \
  '--**.pcapRecorder[*].fileFormat="pcapng"' \
  '--**.pcapRecorder[*].timePrecision=9' \
  '--**.pcapRecorder[*].alwaysFlush=true' \
  '--**.pcapRecorder[*].verbose=false' \
  '--**.checksumMode="computed"' \
  '--**.fcsMode="computed"' \
  examples/ieee80211ax/multi_user/mu_mimo/downlink.ini

bin/inet -u Cmdenv -c UlMuMimo -r 0 \
  --result-dir=results/validation/pcap-ul \
  '--**.numPcapRecorders=1' \
  '--**.pcapRecorder[*].moduleNamePatterns="wlan[0]"' \
  '--**.pcapRecorder[*].dumpProtocols="ieee80211mac"' \
  '--**.pcapRecorder[*].fileFormat="pcapng"' \
  '--**.pcapRecorder[*].timePrecision=9' \
  '--**.pcapRecorder[*].alwaysFlush=true' \
  '--**.pcapRecorder[*].verbose=false' \
  '--**.checksumMode="computed"' \
  '--**.fcsMode="computed"' \
  examples/ieee80211ax/multi_user/mu_mimo/uplink.ini
```

The retained AP captures are:

- [`DlMuMimo AP PCAP`](results/packet-statistics/20260724T175025Z/DlMuMimo/DlMuMimo-%230Lan80211AxDlOfdma.ap.wlan%5B0%5D.pcap),
  containing 1,770 packets from 0.200148 s through 0.998391 s.
- [`UlMuMimo AP PCAP`](results/packet-statistics/20260724T175025Z/UlMuMimo/UlMuMimo-%230Lan80211AxUlOfdma.ap.wlan%5B0%5D.pcap),
  containing 3,777 packets from 0.001048 s through 1.999351 s.

These are AP wireless-interface observations. They are not de-duplicated
end-to-end application packets.

### Downlink sounding evidence

```sh
tshark -n -r results/packet-statistics/20260724T175025Z/DlMuMimo/DlMuMimo-#0Lan80211AxDlOfdma.ap.wlan[0].pcap \
  -Y 'wlan.trigger.he.trigger_type == 1' \
  -T fields -E header=y -E separator=, -E occurrence=a \
  -e frame.number -e frame.time_epoch \
  -e wlan.trigger.he.user_info.aid12 -e _ws.col.Info
```

That exact PCAP contains seven decoded BFRP Triggers. Frame 22 at
0.300989 s polls AIDs 2 and 3; the later six BFRP Triggers poll AIDs 1, 2,
and 3. The capture proves protocol-visible sounding feedback polling. Native
MAC capture does not expose every HE-SIG-B stream-allocation field, so the
downlink stream indices must be read from the AP vectors named above.

### Uplink Trigger evidence

```sh
tshark -n -r results/packet-statistics/20260724T175025Z/UlMuMimo/UlMuMimo-#0Lan80211AxUlOfdma.ap.wlan[0].pcap \
  -Y 'wlan.trigger.he.trigger_type == 0' \
  -T fields -E header=y -E separator=, -E occurrence=a \
  -e frame.number -e frame.time_epoch \
  -e wlan.trigger.he.user_info.aid12 \
  -e wlan.trigger.he.ru_allocation \
  -e wlan.trigger.he.ru_starting_spatial_stream \
  -e wlan.trigger.he.ru_number_of_spatial_stream \
  -e wlan.trigger.he.tid_aggregation_limit \
  -e wlan.trigger.he.preferred_ac -e _ws.col.Info
```

That exact PCAP contains 49 decoded Basic Triggers. Frame 16 at 0.202272 s
assigns AIDs 1, 2, and 3 to RU allocation 61, with starting stream indices
0, 1, and 2. The encoded NSS value is zero for each user, representing one
spatial stream per user.

```sh
tshark -n -r results/packet-statistics/20260724T175025Z/UlMuMimo/UlMuMimo-#0Lan80211AxUlOfdma.ap.wlan[0].pcap \
  -Y 'frame.number >= 16 && frame.number <= 20' \
  -T fields -E separator=, \
  -e frame.number -e frame.time_epoch -e wlan.fc.type_subtype \
  -e wlan.sa -e wlan.da -e _ws.col.Info
```

Frames 17, 18, and 19 are simultaneous QoS Null responses at 0.203788 s from
the three stations, followed by the AP's Block Ack in frame 20 at
0.203857 s. This capture directly supports the multi-user Trigger and
simultaneous-response structure. It does not establish uplink application
goodput.

## Packet-type summary

The table below is derived from the two AP PCAPs named above. It counts AP
wireless-interface observations and therefore must not be read as application
delivery.

| Configuration | Total observations | Selected decoded observations |
|---|---:|---|
| `DlMuMimo` | 1,770 | 7 Trigger, 724 BAR, 724 Block Ack, 262 HE-MU aggregates |
| `UlMuMimo` | 3,777 | 49 Basic Trigger plus 2 other Trigger, 1,939 QoS Data/Null observations, 51 Block Ack |

Packet totals show that the configured exchanges executed, but subtype totals
alone cannot prove disjoint stream allocation. Use the Trigger fields and the
HE vectors for that claim.

## Standards and model boundary

IEEE Std 802.11-2024 Clause 27.3.2.5 defines full-bandwidth DL MU-MIMO user
allocation, and Clause 27.3.3.2.4 constrains non-overlapping UL spatial-stream
ranges. Trigger User Info carries RU allocation and spatial-stream assignment.

INET's packet-level model uses configured CSI freshness and leakage constants
and ideal separation for disjoint scalar stream ranges. It does not derive the
reported gain from a waveform channel matrix. The evidence therefore
demonstrates the mechanism and benefit in these configurations only.
