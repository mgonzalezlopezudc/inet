# Walkthrough - HE Dynamic Fragmentation

This walkthrough guides you through the HE dynamic fragmentation simulation example in the INET Framework.

## Background: HE Dynamic Fragmentation

IEEE 802.11ax introduces **Dynamic Fragmentation** to replace the legacy static MAC-layer fragmentation.
- **Legacy Static Fragmentation**: Divides MSDUs into fixed-size fragments (except the last one) based on a static fragmentation threshold. This cannot adapt to changing channel conditions or dynamic TXOP (Transmission Opportunity) limits.
- **HE Dynamic Fragmentation**: Allows the transmitter to dynamically adjust fragment boundaries for each fragment. It can choose different fragment sizes for eligible MPDUs to fit exactly within the available TXOP or RU allocations. It supports three levels (Level 1, 2, and 3) of complexity:
  - **Level 1**: Up to 4 fragments per MSDU.
  - **Level 2**: Up to 16 fragments per MSDU.
  - **Level 3**: Up to 64 fragments per MSDU.

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

---

## Running the Simulation

Execute the simulation using Cmdenv:
```sh
bin/inet -u Cmdenv -c DynamicFragmentation examples/ieee80211ax/mac_features/dynamic_fragmentation/omnetpp.ini
```

---

## Verifying Results and Model Limitations

After the simulation completes, query the results using `opp_scavetool` or standard grepping of the `.sca` file:
```sh
# Query packetSent at client applications
opp_scavetool query -l -f 'name =~ "packetSent:count" and module =~ "*.host*app*"' examples/ieee80211ax/mac_features/dynamic_fragmentation/results/*.sca

# Query packetDefragmented at the AP
opp_scavetool query -l -f 'name =~ "packetDefragmented:count" and module =~ "*.ap*recipientMacDataService*"' examples/ieee80211ax/mac_features/dynamic_fragmentation/results/*.sca
```

### Quantitative Summary:
- **`host[0..2].app[0] packetSent:count`**: 361 packets each (Total sent by hosts = 1083).
- **`ap.wlan[0].mac.hcf.recipientMacDataService packetDefragmented:count`**: 1079.
- **`server.app[0] packetReceived:count`**: 1079.

### Critical Model Realization:
In the current implementation of `HeDynamicFragmentationPolicy`, the policy checks for negotiated HE capabilities by peeking at the MAC header of the frame. However, at the point `computeFragmentSizes` is called during frame generation, the MAC header has not yet been prepended to the packet. As a result, the header lookup returns `nullptr`, and dynamic fragmentation is suppressed. Therefore, `packetFragmented:count` remains `0` on the stations.

However, the AP's `recipientMacDataService` still routes all incoming unfragmented packets through the defragmentation path, resulting in a `packetDefragmented:count` matching the total received packet count. This is a model limitation to be aware of when studying dynamic fragmentation in INET.
