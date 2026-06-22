# Walkthrough - IEEE 802.11ax BSS Coloring & Spatial Reuse

We have successfully implemented and verified the IEEE 802.11ax (Wi-Fi 6) BSS Coloring capability and created a new example simulation showing spatial reuse.

## Changes Made

### 1. Core Framework Modifications
- **MIB Expose & Set**: Exposed the `heBssColor` parameter in [Ieee80211Mib.ned](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mib/Ieee80211Mib.ned) and validated/applied it inside [Ieee80211Mib.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mib/Ieee80211Mib.cc).
- **PHY Header Serialization**: Updated [Ieee80211Radio.cc](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.cc) to fetch the NIC MIB and inject the configured BSS Color into outgoing `Ieee80211HeMuPhyHeader`s.
- **Dynamic BSS Color Sync**: Modified [Ieee80211MgmtSta.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtSta.cc) and [Ieee80211MgmtStaSimplified.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtStaSimplified.cc) to copy the associated AP's BSS Color dynamically upon connection and reset it to 0 upon disassociation.

### 2. New Simulation Example
Created a complete demonstration simulation in [examples/ieee80211/bss_coloring/](file:///home/user/omnetpp_ws/inet/examples/ieee80211/bss_coloring/):
- [BssColoringNetwork.ned](file:///home/user/omnetpp_ws/inet/examples/ieee80211/bss_coloring/BssColoringNetwork.ned): Two overlapping BSSs (AP1 at 150m, AP2 at 400m, and their associated STAs 50m away).
- [omnetpp.ini](file:///home/user/omnetpp_ws/inet/examples/ieee80211/bss_coloring/omnetpp.ini): Configurations for the baseline (`BssColoringDisabled`) and Wi-Fi 6 spatial reuse (`BssColoringEnabled`) scenarios.
- [walkthrough.md](file:///home/user/omnetpp_ws/inet/examples/ieee80211/bss_coloring/walkthrough.md): A detailed, user-facing explanation of the BSS Coloring theory, geometry, and expected simulation results.

---

## Verification Results

### 1. Automated Unit Tests
All 23 HE/OFDMA unit tests completed successfully:
```
◉ 23 inet unit tests (concurrently) Multiple unit tests: 23 PASS in 4.153
```

### 2. Simulation Output & Throughput Metrics

We ran both simulation configurations and collected the received packet counts at the stations.

#### BssColoringDisabled (Baseline):
- **BSS 1** (STA1[0..1]) received: **342 packets**
- **BSS 2** (STA2[0..1]) received: **3 packets**
- **Total aggregate packets**: **345 packets**
- *Observation*: The two APs defer to each other via legacy CCA sensitivity CS/ED thresholds because they are on the same channel and overlap. Due to channel access contention, BSS 1 completely starves BSS 2.

#### BssColoringEnabled (Spatial Reuse):
- **BSS 1** (STA1[0..1]) received: **228 packets**
- **BSS 2** (STA2[0..1]) received: **289 packets**
- **Total aggregate packets**: **517 packets**
- *Observation*: Both BSSs ignore each other's transmissions because their signals fall below the $-62$ dBm OBSS/PD and energy detection thresholds. They transmit concurrently, resulting in a **50% increase** in total aggregate network throughput and resolving the starvation of BSS 2.
