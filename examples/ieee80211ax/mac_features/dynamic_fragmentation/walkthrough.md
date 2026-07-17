# Walkthrough - HE Dynamic Fragmentation

This walkthrough shows why 802.11ax negotiates dynamic fragmentation. Static
fragmentation commits to one boundary in advance; dynamic fragmentation can
choose a boundary that fits the current TXOP or RU budget, avoiding the choice
between wasting the remainder of an opportunity and deferring the whole MSDU.

## Background: HE Dynamic Fragmentation

IEEE 802.11ax introduces **Dynamic Fragmentation** to replace the legacy static MAC-layer fragmentation.
- **Legacy Static Fragmentation**: Divides MSDUs into fixed-size fragments (except the last one) based on a static fragmentation threshold. This cannot adapt to changing channel conditions or dynamic TXOP (Transmission Opportunity) limits.
- **HE Dynamic Fragmentation**: Allows the transmitter to adjust fragment
  boundaries so eligible data fits a constrained PPDU duration. The negotiated
  levels govern how fragments may be included in an A-MPDU:
  - **Level 1**: one dynamic fragment as a non-A-MPDU.
  - **Level 2**: dynamic fragments may appear in an A-MPDU, with no more than
    one fragment of a given MSDU or A-MSDU in that A-MPDU.
  - **Level 3**: up to four fragments of a given MSDU or A-MSDU may appear in
    the A-MPDU.

---

## Network Topology and Configuration

The simulation runs in a single-BSS network (`Lan80211AxUlOfdma`) where:
- **`ap`**: The Access Point.
- **`host[0..2]`**: Three wireless stations.
- **`server`**: A wired server connected to the AP.
- **Traffic**: Uplink traffic is generated from the hosts to the server. The hosts send large packets (1400-byte payloads) every 5ms.

The `DynamicFragmentation` config in `omnetpp.ini` is defined as:
```ini
[Config DynamicFragmentation]
description = "Negotiated HE level-1 dynamic fragmentation divides 1400-byte QoS MSDUs into approximately 500-byte MPDUs and reassembles them at the AP."
**.ap.wlan[*].mib.heDynamicFragmentationLevel = 1
**.host[*].wlan[*].mib.heDynamicFragmentationLevel = 1
**.host[*].wlan[*].mac.hcf.originatorMacDataService.fragmentationPolicy.typename = "HeDynamicFragmentationPolicy"
**.host[*].wlan[*].mac.hcf.originatorMacDataService.fragmentationPolicy.fragmentationThreshold = 500B
**.host[*].wlan[*].mac.hcf.originatorMacDataService.fragmentationPolicy.requiredLevel = 1
**.host[*].app[0].messageLength = 1400B
**.wlan[*].mac.hcf.enableUlMuOfdma = false
```

### Key Parameters:
1. **`heDynamicFragmentationLevel = 1`**: AP and stations advertise Level-1 dynamic fragmentation support.
2. **`typename = "HeDynamicFragmentationPolicy"`**: Activates the HE-specific fragmentation policy that checks negotiated peer capabilities.
3. **`fragmentationThreshold = 500B`**: Targets a nominal fragment size of 500 bytes.
4. **`requiredLevel = 1`**: Specifies that dynamic fragmentation requires Level-1 support.

The 1400-byte MSDU is intentionally larger than the 500-byte policy threshold,
so every eligible packet exercises fragmentation and reassembly. Level 1 is
the minimum negotiated mode and keeps the exchange easy to inspect.
The static and unfragmented configurations are essential controls: negotiation
alone is not evidence that fragment sizing changed.

---

## Running the Simulation

Execute the simulation using Cmdenv:
```sh
bin/inet -u Cmdenv -c DynamicFragmentation examples/ieee80211ax/mac_features/dynamic_fragmentation/omnetpp.ini
```

---

## Verifying Results

After the simulation completes, query the results using `opp_scavetool` or standard grepping of the `.sca` file:
```sh
# Query packetSent and packetFragmented at the client hosts
opp_scavetool query -l -f 'name =~ "packetSent:count" or name =~ "packetFragmented:count"' examples/ieee80211ax/mac_features/dynamic_fragmentation/results/*.sca

# Query packetDefragmented at the AP
opp_scavetool query -l -f 'name =~ "packetDefragmented:count"' examples/ieee80211ax/mac_features/dynamic_fragmentation/results/*.sca
```

### Quantitative Summary:
- **`host[0..2].app[0] packetSent:count`**: 361 packets each (Total sent by hosts = 1083).
- **`host[0..2].wlan[0].mac.hcf.originatorMacDataService packetFragmented:count`**: 359, 359, 360 (Total fragmented = 1078).
- **`ap.wlan[0].mac.hcf.recipientMacDataService packetDefragmented:count`**: 1068.
- **`server.app[0] packetReceived:count`**: 1068.

---

## PCAP Tshark Packet Exchange Analysis

To record PCAP traces and inspect them with TShark, run the simulation with PCAP recording and checksum computation enabled:

```sh
bin/inet -u Cmdenv -c DynamicFragmentation examples/ieee80211ax/mac_features/dynamic_fragmentation/omnetpp.ini --result-dir=examples/ieee80211ax/mac_features/dynamic_fragmentation/results --**.numPcapRecorders=1 --**.checksumMode=\"computed\" --**.fcsMode=\"computed\"
```

Use TShark to print the timeline of packet exchanges:

```sh
tshark -n -r examples/ieee80211ax/mac_features/dynamic_fragmentation/results/DynamicFragmentation-#0Lan80211AxUlOfdma.ap.wlan[0].pcap -c 20
```

The decoded output timeline shows:
1. **Dynamic Frame Fragments**: Standard data frames are dynamically split into smaller fragments of approximately 504 bytes (corresponding to the `fragmentationThreshold = 500B` configuration) to fit channel opportunities (e.g. frames 1, 2, 4, 6).
2. **Fragment ACKs**: The AP acknowledges each received fragment (e.g. frames 3, 5, 7) individually.
3. **Reassembly**: Once the last fragment is received, the MAC layer defragments and reassembles the original QoS MSDU before passing it up to UDP, decoded by TShark as the fully reassembled UDP packet (e.g. frame 8).

## Interpreting the comparison

Across five seeds, dynamic and static policies both produce a mean transmitted
MAC-frame size of `293.89 B` and `51.792 ms` of ACK airtime; the unfragmented
control produces `1070 B` frames and `69.360 ms` of ACK airtime. Dynamic and
static overlap because this example gives both the same 500-byte sizing rule.
The HE-specific advantage demonstrated here is negotiated eligibility and the
ability to choose boundaries per opportunity; showing adaptive sizing would
require varying the available TXOP or RU budget during the run.

## 802.11 Packet Type Statistics
This section provides a statistical overview of the 802.11 frames transmitted over the wireless medium during the simulation. The packet counts were gathered from the Access Point's wireless interface (`ap.wlan[0]`), which captures all uplink, downlink, and management traffic in the BSS without duplication.

Two airtime occupancy percentages are provided:
- **Air Time %**: The percentage of the total transmission airtime of all packets occupied by this frame type.
- **Air Time (Sim Time) %**: The percentage of the total simulation time occupied by the transmission of this frame type (defined as the sum of physical airtimes of this frame type w.r.t. the total simulation time limit).

### Configuration: `DynamicFragmentation`
Total over-the-air packets captured (Global BSS/AP): **6764**

| Frame Type & Subtype | Count | Percentage | Mean Size | Std Dev | Freq | Mean RX Sig | Mean TX Pwr | Air Time % | Air Time (Sim Time) % |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Data: QoS Data | 6043 | 89.34% | 376.1 B | 176.8 B | 5010 MHz | -63.3 dBm | - | 98.44% | 99.63% |
| Control: Block Ack Request (BAR) | 435 | 6.43% | 24.0 B | 0.0 B | 5010 MHz | -62.8 dBm | - | 0.60% | 0.61% |
| Control: Block Ack (BA) | 261 | 3.86% | 152.0 B | 0.0 B | 5010 MHz | - | 10.0 dBm | 0.91% | 0.92% |
| Control: Ack | 18 | 0.27% | 14.0 B | 0.0 B | 5010 MHz | -63.7 dBm | 10.0 dBm | 0.02% | 0.02% |
| Management: Action | 7 | 0.10% | 37.0 B | 0.0 B | 5010 MHz | -63.0 dBm | 10.0 dBm | 0.02% | 0.02% |

### Analysis of Packet Distribution
In dynamic fragmentation scenarios, large application layer packets are dynamically fragmented into smaller MAC-layer **QoS Data** frames depending on channel conditions. This results in a higher count of QoS Data frames for fragmented configurations compared to non-fragmented baselines. The corresponding **Block Ack (BA)** count also reflects the fragment-level acknowledgment bitmap.
