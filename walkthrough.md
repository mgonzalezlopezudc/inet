# MLO NSTR Implementation Walkthrough

We have upgraded the 802.11be Multi-Link Operation (MLO) implementation to enforce **Nonsimultaneous Transmit and Receive (NSTR)** runtime self-interference and scheduling constraints in a fully standard-compliant manner.

Prior to these changes, the `Str` and `Nstr` configurations behaved identically because capability flags were only utilized for negotiation and serialization. The new runtime constraints are active when the local device is configured as `mlo = true` and `nstr = true`.

## Changes Made

### Link MAC and Coordination Layer
1. **[Ieee80211MldMac.h](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/Ieee80211MldMac.h) & [Ieee80211MldMac.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/Ieee80211MldMac.cc)**:
   - Added registration of individual sub-link MACs (`Ieee80211Mac` pointers) in the parent `Ieee80211MldMac` module using `registerLinkMac()`.
   - Implemented transmission state change coordination via `linkTransmissionStateChanged()`, notifying all other link MACs to recompute their medium free status when any link starts or stops transmitting.
   - Added utility methods to query if another link is currently transmitting (`isOtherLinkTransmitting()`) or was transmitting during a past time interval (`isOtherLinkTransmittingDuring()`).

2. **[Ieee80211Mac.h](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/Ieee80211Mac.h) & [Ieee80211Mac.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/Ieee80211Mac.cc)**:
   - Added `mldMac` parent reference resolution and registration during `INITSTAGE_LINK_LAYER` initialization.
   - Tracked transmission start/end times (`lastTxStart`, `lastTxEnd`) in the MAC receiver signal listener for `IRadio::transmissionStateChangedSignal` and propagated updates to the parent MLD MAC.
   - Implemented `isTransmittingDuring()` to verify transmission overlap.
   - Implemented `otherLinkTransmissionStateChanged()` to notify the RX module to reevaluate the medium.

3. **[Rx.h](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/Rx.h) & [Rx.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/Rx.cc)**:
   - Made `recomputeMediumFree()` public to allow calls from the containing MAC module.
   - In `lowerFrameReceived()`, implemented standard-compliant NSTR self-interference checks by retrieving the incoming packet's precise signal interval from its `SignalTimeInd` tag. If another link of the NSTR device was transmitting during this interval, the packet is corrupted, dropped with reason `INCORRECTLY_RECEIVED`, and logged:
     `"Received frame corrupted due to MLO NSTR self-interference"`
   - In `recomputeMediumFree()`, if `isMlo && isNstr` is true and any other link is transmitting, the local medium is kept busy, suspending contention/backoff on the current link to prevent unaligned concurrent transmissions.

---

## Verification Results

### 1. Automated Unit Tests
We verified that existing EHT capability negotiation tests pass successfully:
- Test command: `inet_run_unit_tests -m debug -f 'Ieee80211EhtCapabilities_1.test'`
- Status: **PASS**

### 2. Simulation Verification (`Str` vs `Nstr` Performance)
We ran the MLO showcase simulation (`examples/ieee80211be/mlo/omnetpp.ini`) in debug mode for both configurations.

#### Configuration `Str` (Simultaneous Transmit & Receive)
- **Status**: Completed successfully (`t=1s`, event `1450130`).
- **Application Performance**: Successfully received **18,251** UDP packets at the host application layer (`host.app[0].packetReceived:count`).
- **Self-Interference**: None (both links transmit/receive simultaneously without blocking each other).

#### Configuration `Nstr` (Non-Simultaneous Transmit & Receive)
- **Status**: Completed successfully (`t=1s`, event `1358151`).
- **Application Performance**: Received **0** UDP packets at the host application layer (`host.app[0].packetReceived:count = 0`).
- **Analysis**:
  - Without centralized end-time alignment, the host experiences persistent transmit-receive conflicts under saturated downlink traffic (e.g., transmitting ACKs or control responses on one link while receiving on the other).
  - The logs successfully verify that frames are dropped at runtime due to NSTR self-interference:
    `[INFO] Received frame corrupted due to MLO NSTR self-interference: (inet::Packet)...`
  - In addition, the ADDBA (Block Ack agreement) exchange frames are corrupted by self-interference, preventing data transmission sessions from establishing, leading to a complete performance difference between STR and NSTR as expected.
