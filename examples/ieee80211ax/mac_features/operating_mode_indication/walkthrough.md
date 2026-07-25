# Walkthrough - HE Operating Mode Indication

This example configures one station to send an Operating Mode Indication (OMI)
while uplink application traffic is active.

## Configuration

`omnetpp.ini` defines `OperatingModeIndication` by extending
`ScheduledOnly`. Its directly inspectable settings include:

```ini
**.ap.wlan[*].mib.heOmControl = true
**.host[*].wlan[*].mib.heOmControl = true
**.host[0].wlan[*].mac.hcf.sendOperatingModeIndication = true
**.host[0].wlan[*].mac.hcf.operatingModeRxNss = 2
**.host[0].wlan[*].mac.hcf.operatingModeChannelWidth = 0
**.host[0].wlan[*].mac.hcf.operatingModeUlMuDisable = true
**.ap.wlan[*].mac.hcf.ulTriggerCheckInterval = 0.5s
```

These are configuration inputs. Their presence does not by itself establish
the transmitted OM Control bits, receiver state, or scheduler behavior.

## Run the configuration

```sh
bin/inet -u Cmdenv -c OperatingModeIndication \
  examples/ieee80211ax/mac_features/operating_mode_indication/omnetpp.ini
```

To create packet captures in a separate result directory:

```sh
bin/inet -u Cmdenv -c OperatingModeIndication \
  examples/ieee80211ax/mac_features/operating_mode_indication/omnetpp.ini \
  --result-dir=examples/ieee80211ax/mac_features/operating_mode_indication/results/manual-run \
  --**.numPcapRecorders=1 \
  --**.checksumMode=\"computed\" \
  --**.fcsMode=\"computed\"
```

## Scalar/vector run evidence

The run-0 scalar/vector artifacts are in the retained
`results/scalar-vector/20260725T120411Z` session:

```sh
opp_scavetool query -l \
  -f 'name =~ "packetSent:count" or name =~ "packetReceived:count"' \
  examples/ieee80211ax/mac_features/operating_mode_indication/results/scalar-vector/20260725T120411Z/OperatingModeIndication/OperatingModeIndication-\#0.sca

opp_scavetool query -l \
  -f 'type =~ vector and module =~ "*.app[*]" and (name =~ "packetSent:vector(packetBytes)" or name =~ "packetReceived:vector(packetBytes)")' \
  examples/ieee80211ax/mac_features/operating_mode_indication/results/scalar-vector/20260725T120411Z/OperatingModeIndication/OperatingModeIndication-\#0.vec
```

The run-0 files report:

| Application result | Scalar count | Vector count and packet-byte range |
|---|---:|---|
| `host[0].app[0] packetSent` | 1 | 1 × 1000 B |
| `host[0].app[1] packetSent` | 341 | 341 × 1000 B |
| `host[1].app[0] packetSent` | 1 | 1 × 1000 B |
| `host[1].app[1] packetSent` | 341 | 341 × 1000 B |
| `host[2].app[0] packetSent` | 1 | 1 × 1000 B |
| `host[2].app[1] packetSent` | 341 | 341 × 1000 B |
| `server.app[0] packetReceived` | 1023 | 1023 × 1000 B |

Sources: [`OperatingModeIndication-#0.sca`](results/scalar-vector/20260725T120411Z/OperatingModeIndication/OperatingModeIndication-%230.sca)
and [`OperatingModeIndication-#0.vec`](results/scalar-vector/20260725T120411Z/OperatingModeIndication/OperatingModeIndication-%230.vec).
The scalar counts and application-vector sample summaries establish only the
recorded application traffic, not OMI processing or a scheduling decision.

## Packet-statistics evidence

The `20260724T175025Z` packet-statistics directory contains `.sca`, `.vec`,
four wireless PCAP files, and Cmdenv output. Query its scalar file
independently with:

```sh
opp_scavetool query -l \
  -f 'name =~ "packetSent:count" or name =~ "packetReceived:count"' \
  examples/ieee80211ax/mac_features/operating_mode_indication/results/packet-statistics/20260724T175025Z/OperatingModeIndication/OperatingModeIndication-\#0.sca
```

Inspect the AP capture:

```sh
tshark -n \
  -r examples/ieee80211ax/mac_features/operating_mode_indication/results/packet-statistics/20260724T175025Z/OperatingModeIndication/OperatingModeIndication-\#0Lan80211AxUlOfdma.ap.wlan[0].pcap \
  -c 20
```

Source:
[`OperatingModeIndication AP PCAP`](results/packet-statistics/20260724T175025Z/OperatingModeIndication/OperatingModeIndication-%230Lan80211AxUlOfdma.ap.wlan[0].pcap)

| Decoded frame label | Observations |
|---|---:|
| QoS Data | 1481 |
| Trigger | 3 |
| Block Ack | 3 |
| Ack | 1020 |
| **Total PCAP records** | **2507** |

These are observations at the AP wireless capture point. The frame-label table
does not expose an OM Control subfield, its Rx NSS/channel-width/UL-MU-disable
values, the AP's stored peer state, or the contents of Trigger User Info
fields.

## What remains inconclusive

The retained artifacts cited above do not directly demonstrate that an OM
Control subfield was decoded, that the AP applied Rx NSS 2 or channel width 0,
or that host 0 was excluded from UL-MU scheduling. They also do not measure
power, thermal, latency, throughput, or airtime effects. Establishing those
points requires explicit decoded OM Control and Trigger fields or dedicated
receiver/scheduler telemetry correlated by simulation time.
