# Add Config illustrating the Multi-TID Block Ack Feature

Add a simulation configuration to both the Downlink OFDMA (`dl_ofdma`) and Uplink OFDMA (`ul_ofdma`) simulations that enables and negotiates the HE Multi-TID Block Ack (BA) capability parameters. This requires first exposing the HE Multi-TID aggregation parameters in the 802.11 MIB module (`Ieee80211Mib`).

## User Review Required

> [!NOTE]
> The Multi-TID Block Ack serialization and packet header representation are already implemented and validated in INET unit tests. Adding these parameters to the MIB enables the simulation to negotiate the feature during association, demonstrating capability advertisement.

## Open Questions

No open questions.

## Proposed Changes

---

### Component: 802.11 MIB Module

#### [MODIFY] [Ieee80211Mib.ned](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mib/Ieee80211Mib.ned)
- Add the `heMultiTidAggregationRx` and `heMultiTidAggregationTx` parameters:
  ```ned
  bool heMultiTidAggregationRx = default(false);
  bool heMultiTidAggregationTx = default(false);
  ```

#### [MODIFY] [Ieee80211Mib.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mib/Ieee80211Mib.cc)
- Read the parameters in `initialize(int stage)` and assign them to `localHeCapabilities`:
  ```cpp
  localHeCapabilities.multiTidAggregationRx = par("heMultiTidAggregationRx").boolValue();
  localHeCapabilities.multiTidAggregationTx = par("heMultiTidAggregationTx").boolValue();
  ```

---

### Component: OFDMA Simulation Examples

#### [MODIFY] [omnetpp.ini](file:///home/user/omnetpp_ws/inet/examples/ieee80211ax/dl_ofdma/omnetpp.ini)
- Add a new config block `[Config MultiTidBlockAck]` inheriting from `[General]`:
  ```ini
  [Config MultiTidBlockAck]
  description = "HE Multi-TID Block Ack: AP and stations negotiate Multi-TID Aggregation support"
  **.ap.wlan[*].mib.heMultiTidAggregationRx = true
  **.ap.wlan[*].mib.heMultiTidAggregationTx = true
  **.host[*].wlan[*].mib.heMultiTidAggregationRx = true
  **.host[*].wlan[*].mib.heMultiTidAggregationTx = true
  ```

#### [MODIFY] [omnetpp.ini](file:///home/user/omnetpp_ws/inet/examples/ieee80211ax/ul_ofdma/omnetpp.ini)
- Add a new config block `[Config MultiTidBlockAck]` inheriting from `[General]`:
  ```ini
  [Config MultiTidBlockAck]
  description = "HE Multi-TID Block Ack: AP and stations negotiate Multi-TID Aggregation support"
  **.ap.wlan[*].mib.heMultiTidAggregationRx = true
  **.ap.wlan[*].mib.heMultiTidAggregationTx = true
  **.host[*].wlan[*].mib.heMultiTidAggregationRx = true
  **.host[*].wlan[*].mib.heMultiTidAggregationTx = true
  ```

## Verification Plan

### Automated Tests
- Run unit tests to check that the modification compiles and existing HE tests pass successfully:
  ```sh
  bin/inet_run_unit_tests -m release -f "(Ieee80211He|HeDlScheduler|Ieee80211MultiTid).*\\.test"
  ```

### Manual Verification
- Run the simulation configs `MultiTidBlockAck` in both `dl_ofdma` and `ul_ofdma` using command-line mode (`Cmdenv`) to ensure they compile, load parameters correctly, negotiate capabilities, and run without warnings:
  ```sh
  # Run Downlink OFDMA with the new config
  opp_run -u Cmdenv -c MultiTidBlockAck -n ../../src:../../examples:../../tutorials:../../showcases -l ../../src/libINET.so omnetpp.ini
  
  # Run Uplink OFDMA with the new config
  opp_run -u Cmdenv -c MultiTidBlockAck -n ../../src:../../examples:../../tutorials:../../showcases -l ../../src/libINET.so omnetpp.ini
  ```
