# Walkthrough - IEEE 802.11ax Downlink MU-MIMO

Completed the implementation of 802.11ax HE Downlink MU-MIMO with TDD sounding.

## Changes Made

### 1. Coordination Function & Sounding
- **[HeHcf.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc)**:
  - Resolved `transmitter->getAntenna()` compilation issue by fetching it from `radio->getAntenna()`.
  - Replaced the direct lookup `mib->findHeCapabilities` with maps querying `mib->bssAccessPointData.advertisedHeCapabilities`.
  - Added casting of incoming MAC headers to `Ieee80211TwoAddressHeader` to extract transmitter address for CSI updates.
  - Corrected `Ieee80211HeCompressedBeamformingFeedback` transmission inside HCF: wrapped the payload inside a proper `Ieee80211MgmtHeader` and passed the management header to `transmitFrame()`.
- **[HeSoundingFs.h](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/framesequence/HeSoundingFs.h)**:
  - Added `apAddress` member variable to resolve the undeclared variable error.
- **[HeSoundingFs.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/framesequence/HeSoundingFs.cc)**:
  - Added missing includes: `FrameSequenceContext.h` and `Ieee80211Tag_m.h`.
  - Corrected `Ieee80211HeMuCommonReq` namespace qualification to `physicallayer::Ieee80211HeMuCommonReq`.
  - Cast the NDP packet's peeked chunk to `SequenceChunk` to safely iterate over and update the spatial stream count on payload headers.
  - Used `Ieee80211DataHeader` for the NDP frame header instead of `Ieee80211MacHeader` to allow transmitter address settings.

### 2. Scheduler Robustness & Serialization
- **[HeDlSchedulerEqualSizedRUs.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/scheduler/HeDlSchedulerEqualSizedRUs.cc)**:
  - Robustified `hcf`, `mac`, and `mib` retrieval to prevent crashes in unit tests running the scheduler in isolation.
  - Corrected parameters to `estimateHeMuUserDuration` by wrapping PSDU length in a `B(...)` unit cast.
  - Updated capabilities map lookup.
- **[Ieee80211MgmtFrameSerializer.h](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrameSerializer.h)** / **[Ieee80211MgmtFrameSerializer.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrameSerializer.cc)**:
  - Isolated NDPA and feedback frames serialization/deserialization into a separate class `Ieee80211HeSoundingMgmtFrameSerializer`. This ensures that they serialize and deserialize correctly on a direct packet pop (such as during unit tests), resolving standard management element malformation errors.

### 3. Unit Tests
- **[Ieee80211HeMuMimo_1.test](file:///home/user/omnetpp_ws/inet/tests/unit/Ieee80211HeMuMimo_1.test)**:
  - Cleaned up syntax: replaced `addr.toString()` with `addr == MacAddress(...)` and `ASSERT_NEAR` with `std::abs(...) < eps`.
  - Removed setting/asserting sender/receiver addresses directly on the feedback frame chunk, as those fields are part of the `Ieee80211MgmtHeader` rather than the feedback payload itself.
- **[Ieee80211HeLdpcPacketExtension_1.test](file:///home/user/omnetpp_ws/inet/tests/unit/Ieee80211HeLdpcPacketExtension_1.test)**:
  - Updated chunk length specification to `B(39)` (byte-aligned size) to accommodate the new MU-MIMO fields and avoid truncation errors on serialization.

## Verification Results
All unit tests in the suite successfully passed:
```
Multiple unit test results: PASS, summary: 21 PASS in 4.543
```
- Capability Gating checks: **PASS**
- CSI Manager & Leakage Overrides: **PASS**
- Feedback serialization round-trip: **PASS**
- Equal-Sized RU Scheduling fallbacks and MU-MIMO grouping: **PASS**
- Packet-extension timing and LDPC tail accounting: **PASS**
