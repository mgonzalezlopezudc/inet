# Walkthrough - HE NDP Feedback Report

This example configures NDP Feedback Report operation in a single-BSS uplink
scenario.

## Configuration

`omnetpp.ini` includes the UL-OFDMA example and supplies these directly
inspectable settings:

```ini
[General]
**.pcapRecorder.recordEmptyPackets = true
```

The included `../../ul_ofdma/omnetpp.ini` supplies the selected configuration:

```ini
[Config NdpFeedbackReport]
**.wlan[*].mib.heNdpFeedbackReport = true
**.ap.wlan[*].mac.hcf.enableNdpFeedbackReport = true
**.ap.wlan[*].mac.hcf.ulTriggerCheckInterval = 20ms
```

These are configuration inputs. The empty-packet recording setting permits
zero-byte records to be written to a capture, but the settings do not by
themselves identify a record's PHY format or the Trigger that elicited it.

## Run the configuration

```sh
bin/inet -u Cmdenv -c NdpFeedbackReport \
  examples/ieee80211ax/multi_user/ndp_feedback/omnetpp.ini
```

To create packet captures in a separate result directory:

```sh
bin/inet -u Cmdenv -c NdpFeedbackReport \
  examples/ieee80211ax/multi_user/ndp_feedback/omnetpp.ini \
  --result-dir=examples/ieee80211ax/multi_user/ndp_feedback/results/manual-run \
  --**.numPcapRecorders=1 \
  --**.checksumMode=\"computed\" \
  --**.fcsMode=\"computed\"
```

## Scalar/vector run evidence

The run-0 scalar/vector artifacts are in the retained
`results/scalar-vector/20260725T120411Z` session:

```sh
opp_scavetool query -l \
  -f 'name =~ "packetSent:count" or name =~ "packetReceived:count" or name =~ "packetSentToPeer:count"' \
  examples/ieee80211ax/multi_user/ndp_feedback/results/scalar-vector/20260725T120411Z/NdpFeedbackReport/NdpFeedbackReport-\#0.sca

opp_scavetool query -l \
  -f 'type =~ vector and module =~ "*.app[*]" and (name =~ "packetSent:vector(packetBytes)" or name =~ "packetReceived:vector(packetBytes)")' \
  examples/ieee80211ax/multi_user/ndp_feedback/results/scalar-vector/20260725T120411Z/NdpFeedbackReport/NdpFeedbackReport-\#0.vec
```

The run-0 files report:

| Result | Scalar count | Application-vector summary |
|---|---:|---|
| `host[0].app[0..1] packetSent` | 1, 341 | 1 × 1000 B, 341 × 1000 B |
| `host[1].app[0..1] packetSent` | 1, 341 | 1 × 1000 B, 341 × 1000 B |
| `host[2].app[0..1] packetSent` | 1, 341 | 1 × 1000 B, 341 × 1000 B |
| `server.app[0] packetReceived` | 1023 | 1023 × 1000 B |
| `ap.wlan[0].mac.hcf packetSentToPeer` | 1122 | Not an application vector |
| `host[0].wlan[0].mac.hcf packetSentToPeer` | 742 | Not an application vector |
| `host[1].wlan[0].mac.hcf packetSentToPeer` | 778 | Not an application vector |
| `host[2].wlan[0].mac.hcf packetSentToPeer` | 782 | Not an application vector |

Sources: [`NdpFeedbackReport-#0.sca`](results/scalar-vector/20260725T120411Z/NdpFeedbackReport/NdpFeedbackReport-%230.sca)
and [`NdpFeedbackReport-#0.vec`](results/scalar-vector/20260725T120411Z/NdpFeedbackReport/NdpFeedbackReport-%230.vec).
The application-vector summaries establish sample counts and packet-byte
values only. The application and HCF counts do not classify Trigger types,
identify zero-payload responses, or establish which packet caused another.

## Packet-statistics evidence

The `20260724T175025Z` packet-statistics directory contains `.sca`, `.vec`,
four wireless PCAP files, and Cmdenv output. Query its scalar file
independently with:

```sh
opp_scavetool query -l \
  -f 'name =~ "packetSent:count" or name =~ "packetReceived:count" or name =~ "packetSentToPeer:count"' \
  examples/ieee80211ax/multi_user/ndp_feedback/results/packet-statistics/20260724T175025Z/NdpFeedbackReport/NdpFeedbackReport-\#0.sca
```

Inspect the AP capture:

```sh
tshark -n \
  -r examples/ieee80211ax/multi_user/ndp_feedback/results/packet-statistics/20260724T175025Z/NdpFeedbackReport/NdpFeedbackReport-\#0Lan80211AxUlOfdma.ap.wlan[0].pcap \
  -c 25
```

Source:
[`NdpFeedbackReport AP PCAP`](results/packet-statistics/20260724T175025Z/NdpFeedbackReport/NdpFeedbackReport-%230Lan80211AxUlOfdma.ap.wlan[0].pcap)

| Decoded frame label | Observations |
|---|---:|
| QoS Data | 1382 |
| Trigger | 99 |
| Radiotap-only record (`frame.len = radiotap.length = 31`) | 39 |
| Ack | 1023 |
| **Total PCAP records** | **2543** |

The table establishes that the AP capture contains frames labeled `Trigger`
and 39 records for which TShark exposes only a 31-byte radiotap header. It does
not establish that every Trigger is an NDP Feedback Report Poll, classify the
radiotap-only records as feedback NDPs, pair a response with a Trigger, expose
feedback-resource allocation, or show how many stations transmitted
simultaneously.

## What remains inconclusive

The retained scalar file and frame-label table do not directly prove Trigger
Type 7, matching feedback allocation fields, SIFS timing, response station
identity, or causal pairing between Trigger and NDP records. They also provide
no matched control for an airtime or throughput comparison. Those conclusions
require decoded Trigger fields and timestamp-correlated per-station evidence.
