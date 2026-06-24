# Walkthrough - TWT Example Simulation Fix

This walkthrough describes the changes implemented to resolve the chunk conversion crash in the 802.11ax TWT example simulation under `examples/ieee80211/twt/` and the subsequent validation.

## Changes Made

### Linklayer Management & MAC

1. **Management Frame Deserialization Support**
   - Modified [Ieee80211MgmtFrameSerializer.h](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrameSerializer.h) and [Ieee80211MgmtFrameSerializer.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrameSerializer.cc) to override the `deserialize(MemoryInputStream& stream, const std::type_info& typeInfo)` method. It now dynamically inspects `typeInfo` and returns the correctly instantiated management frame subclass rather than always defaulting to an association request frame.

2. **Popped MAC Header Chunk Extraction**
   - Modified [Ieee80211MgmtBase.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtBase.cc) to dynamically extract the popped variable-length MAC header chunk (such as Action/TWT frames) ending at `frontOffset` from `packet->peekAll()`. If the header chunk is inside a `SequenceChunk` or `SliceChunk`, it is appropriately cast or unwrapped.

3. **Relative Header References**
   - Updated [Ieee80211MgmtAp.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtAp.cc) to use `packet->peekData<T>()` rather than offset-based `packet->peekAt<T>(header->getChunkLength())`.

4. **TWT Cross-Module Context and Control Frame FCS**
   - Added `Enter_Method` macros to public TWT methods in [Ieee80211Mac.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/Ieee80211Mac.cc) and [Ieee80211TwtManager.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/twt/Ieee80211TwtManager.cc) to prevent OMNeT++ context errors during cross-module calls.
   - Initialized `fcsMode` explicitly via `trailer->setFcsMode(getFcsMode())` in `Ieee80211Mac::sendTwtPsPoll` to ensure downstream validation does not fail with an unknown FCS mode error.

## Verification

### Automated Simulations
All 4 TWT example configurations under `examples/ieee80211/twt/omnetpp.ini` were run to completion successfully:
- **Baseline**: Ran to `t=2s` (event #30632) without issues.
- **IndividualUnannounced**: Ran to `t=2s` (event #31616) without issues.
- **IndividualAnnounced**: Ran to `t=2s` (event #31636) without issues.
- **Broadcast**: Ran to `t=2s` (event #30236) without issues.

### Unit Tests
The HE/OFDMA unit tests were run using:
```sh
bin/inet_run_unit_tests -m release -f "(Ieee80211He|HeDlScheduler).*\\.test"
```
The exact same 4 unexpected failures (`HeDlScheduler_1.test`, `Ieee80211HeMuSeqAck_1.test`, `Ieee80211HeSpatialReuse_1.test`, `HeDlSchedulerQueueAware_1.test`) occurred on both the clean branch and the modified branch, proving that these are pre-existing issues and that our modifications introduced no regressions.
