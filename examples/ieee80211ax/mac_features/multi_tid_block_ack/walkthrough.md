# Walkthrough - HE Multi-TID Block Ack

This walkthrough guides you through the HE Multi-TID Block Ack simulation example in the INET Framework.

## Background: Multi-TID Block Ack

In high-QoS wireless networks, stations (STAs) generate traffic belonging to different traffic classes or Traffic Identifiers (TIDs). In legacy 802.11, Block Ack agreements are negotiated separately for each TID. If an Access Point (AP) or station wants to acknowledge packets from multiple TIDs, it has to send separate Block Ack Request (BAR) and Block Ack (BA) frames for each TID. This introduces significant channel overhead and increases latency.

802.11ax introduces **Multi-TID Block Ack**:
- **Multi-TID ADDBA Negotiation**: Allows stations and APs to establish a shared acknowledgment context covering multiple TIDs.
- **Combined Feedback**: A single Multi-TID Block Ack frame can acknowledge MAC Service Data Units (MSDUs) belonging to multiple TIDs, reducing control frame overhead and SIFS gaps.

---

## Network Topology and Configuration

The simulation runs in a single-BSS network (`Lan80211AxUlOfdma`) where:
- **`ap`**: The Access Point.
- **`host[0..2]`**: Three wireless stations.
- **`server`**: A wired server connected to the AP.
- **Traffic**: Uplink traffic is generated from two separate UDP applications on each host:
  - `app[0]` generates **TID 0 (Best Effort)** traffic (1000B packets sent every 5ms).
  - `app[1]` generates **TID 6 (Voice)** traffic (200B packets sent every 10ms).

The `MultiTidBlockAck` config in `omnetpp.ini` is defined as:
```ini
[Config MultiTidBlockAck]
description = "HE Multi-TID Block Ack: AP and stations negotiate Multi-TID Aggregation and transmit voice/video/BE traffic concurrently"
**.ap.wlan[*].mib.heMultiTidAggregationRx = true
**.ap.wlan[*].mib.heMultiTidAggregationTx = true
**.host[*].wlan[*].mib.heMultiTidAggregationRx = true
**.host[*].wlan[*].mib.heMultiTidAggregationTx = true
```

### Key Parameters:
1. **`heMultiTidAggregationRx = true`**: Declares that the node's receiver supports receiving aggregated Multi-TID A-MPDUs.
2. **`heMultiTidAggregationTx = true`**: Declares that the node's transmitter supports building and transmitting Multi-TID A-MPDUs.

---

## Running the Simulation

Execute the simulation using Cmdenv for either downlink or uplink direction:
```sh
# Run Downlink Multi-TID Block Ack
bin/inet -u Cmdenv -c MultiTidBlockAck examples/ieee80211ax/mac_features/multi_tid_block_ack/downlink.ini

# Run Uplink Multi-TID Block Ack
bin/inet -u Cmdenv -c MultiTidBlockAck examples/ieee80211ax/mac_features/multi_tid_block_ack/uplink.ini
```

---

## Verifying Results and Model Limitations

After the simulation completes, check the results using `opp_scavetool`:
```sh
# Query packetSent at host applications (Uplink run)
opp_scavetool query -l -f 'name =~ "packetSent:count" and module =~ "*.host*app*"' examples/ieee80211ax/mac_features/multi_tid_block_ack/results/*.sca

# Query packetReceived at server applications (Uplink run)
opp_scavetool query -l -f 'name =~ "packetReceived:count" and module =~ "*.server.app*"' examples/ieee80211ax/mac_features/multi_tid_block_ack/results/*.sca
```

### Quantitative Summary:
- **`host[0..2].app[0] packetSent:count` (TID 0)**: 361 packets each (Total sent = 1083).
- **`host[0..2].app[1] packetSent:count` (TID 6)**: 176 packets each (Total sent = 528).
- **`server.app[0] packetReceived:count` (TID 0)**: 1030.
- **`server.app[1] packetReceived:count` (TID 6)**: 512.

### Critical Model Realization:
The INET 802.11ax MAC model successfully negotiates Multi-TID capability flags and routes the internal BAR/Block Ack state machinery correctly. However, the current DL/UL path creates a separate Block Ack record per request rather than packing multiple TIDs into a single physical Multi-TID Block Ack frame. Thus, this scenario is a verification of the capability negotiation and internal queue prioritization structures, not a proof of aggregated physical multi-TID transmission.
