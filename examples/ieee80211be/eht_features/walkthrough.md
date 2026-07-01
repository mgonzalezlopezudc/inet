# 802.11be EHT Features Example

This example demonstrates the foundational features of IEEE 802.11be (Wi-Fi 7) on the physical and MAC layers, specifically operating in the 6 GHz band with a 320 MHz channel width and using 4096-QAM modulation.

## Topology

The network consists of a wired server (`StandardHost`), a wireless access point (`AccessPoint`), and one or more wireless stations (`WirelessHost`). The AP acts as a bridge between the wired Ethernet link and the wireless LAN.

## Configurations

The `omnetpp.ini` file provides two primary configurations for comparative analysis:

1. **`BaselineAx`**: Operates using 802.11ax (HE) on a 160 MHz channel in the 5 GHz band. This serves as a baseline to measure the throughput improvements introduced by EHT.
2. **`EhtFeatures`**: Operates using 802.11be (EHT) on a 320 MHz channel in the 6 GHz band, maximizing the capabilities up to 4096-QAM.

Both configurations simulate a heavy UDP (CBR) traffic load from the server to `host[0]` to saturate the wireless medium and observe maximum achievable throughput.

## Observations

By running both configurations and comparing the results, you should observe a significant increase in MAC-level throughput for the `EhtFeatures` scenario compared to the `BaselineAx` scenario, reflecting the doubled channel bandwidth and the denser modulation scheme.
