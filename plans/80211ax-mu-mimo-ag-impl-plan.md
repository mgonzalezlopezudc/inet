# HE DL MU-MIMO with TDD Implementation Plan

Implement opt-in 802.11ax HE downlink full-bandwidth MU-MIMO support in the INET Framework. This includes AP sounding sequences, CSI manager, generalized scheduler, and PHY/reception validation.

## User Review Required

> [!IMPORTANT]
> The DL MU-MIMO implementation is completely opt-in and defaults to disabled. If the capability flags are not set, all existing simulations will run with identical results.

> [!NOTE]
> The sounding sequence is a standard multi-step sequence within a single TXOP: NDP Announcement -> SIFS -> NDP -> SIFS -> BFRP Trigger -> SIFS -> Compressed Beamforming Feedback.

## Proposed Changes

### 1. Capabilities and Management Elements

#### [MODIFY] [Ieee80211HeCapabilities.h](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mib/Ieee80211HeCapabilities.h)
- Extend `Ieee80211HeCapabilities` with fields:
  - `bool dlMuMimoBeamformer = false`
  - `bool dlMuMimoBeamformee = false`
  - `int soundingDimensions = 0`
  - `int beamformeeSts20Mhz = 0`
  - `int beamformeeStsAbove20Mhz = 0`
  - `int feedbackMode = 0` // 0=None, 1=SU, 2=MU, 3=Both
- Implement helper function `isDlMuMimoEligible` to validate AP/STA roles, NSS limits, sounding dimensions, bandwidth-specific beamformee STS, and feedback modes.
- Implement helper function `getMaxNss` to determine maximum spatial streams from MCS maps.

#### [MODIFY] [Ieee80211Mib.ned](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mib/Ieee80211Mib.ned)
- Expose NED parameters for the new MIB capabilities, defaulted to off/0.

#### [MODIFY] [Ieee80211Mib.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mib/Ieee80211Mib.cc)
- Initialize the new capabilities in `initialize` stage from the NED parameters.

#### [MODIFY] [Ieee80211MgmtFrame.msg](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame.msg)
- Extend `Ieee80211HeCapabilitiesElement` with the new capability fields.
- Define `Ieee80211HeNdpStaInfo` struct.
- Define action frames `Ieee80211HeNdpAnnouncement` and `Ieee80211HeCompressedBeamformingFeedback`.

#### [MODIFY] [Ieee80211HeMgmtElements.h](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mgmt/Ieee80211HeMgmtElements.h)
- Map new fields in `makeHeCapabilitiesElement` and `makeHeCapabilities`.

#### [MODIFY] [Ieee80211MgmtFrameSerializer.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrameSerializer.cc)
- Encode/decode new capabilities in `phyCapabilities` (bits 75-87).
- Serialize/deserialize NDPA and Compressed Beamforming Feedback frames.

---

### 2. Coordination Function and Sounding

#### [NEW] [HeMuMimoCsiManager.h](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/coordinationfunction/HeMuMimoCsiManager.h)
- Class to manage CSI entries by MAC address and bandwidth.
- Properties: CSI validity duration, default leakage, and overrides.
- Parsers for overrides e.g. `AID1-AID2:val,AID3-AID4:val`.
- Snapshot leakages upon successful feedback receipt.

#### [NEW] [HeSoundingFs.h](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/framesequence/HeSoundingFs.h)
- Implements `IFrameSequence` for the sounding exchange:
  - Step 0: Broadcast `Ieee80211HeNdpAnnouncement`.
  - Step 1: Transmit sounding `"HE-NDP"` packet with empty MAC payload.
  - Step 2: Broadcast BFRP Trigger.
  - Step 3: Wait for simultaneous feedbacks (`ReceiveCollectionStep`).

#### [MODIFY] [HeHcf.h](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h)
- Own a pointer to `HeMuMimoCsiManager`.
- Add members to track NDPA receipt state (`ndpAnnouncementReceived`, `ndpReceived`, etc.).

#### [MODIFY] [HeHcf.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc)
- Instantiate CSI manager.
- In `collectScheduleContext`, populate `ScheduleContext` with `csiManager` and `numApAntennas`.
- In `startFrameSequence`, check if sounding is needed (on-demand when at least two MU-capable backlogged STAs lack fresh CSI) and run `HeSoundingFs`.
- In `recipientProcessReceivedFrame`:
  - Handle receiving NDPA and NDP (setting state flags).
  - Handle BFRP trigger: transmit `Ieee80211HeCompressedBeamformingFeedback` after SIFS.
  - Handle received feedback frames: feed them to the CSI manager.

---

### 3. Generalized Scheduler and PHY/Error Model

#### [MODIFY] [HeDlSchedulerEqualSizedRUs.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/scheduler/HeDlSchedulerEqualSizedRUs.cc)
- Add opt-in check for `apCapabilities.dlMuMimoBeamformer`.
- If eligible and $\ge 2$ users have fresh CSI, build a MU-MIMO group:
  - Anchor selection and stable round-robin compatibility check.
  - Group stream apportionment up to 8 total and 4 per user.
  - Scale SNR by stream share, select MCS and clamp to negotiated support.

#### [MODIFY] [Ieee80211PhyHeader.msg](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader.msg)
- Add `muMimo`, `spatialConfiguration`, `totalNsts` to `Ieee80211HeMuPhyHeader` and `Ieee80211HeMuRuPayloadHeader`.
- Add `streamStartIndex` to `Ieee80211HeMuUserInfo`.

#### [MODIFY] [Ieee80211PhyHeaderSerializer.cc](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeaderSerializer.cc)
- Add serialization for the new PHY header fields when `extended` is true.

#### [MODIFY] [Ieee80211HePhyCalculator.h](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h)
- Add `streamStartIndex` to `Ieee80211HeUserPhyParameters`.
- In `computeHePpduParameters`, validate MU-MIMO constraints: unique users, Table 27-31 layout, total/per-user NSS, contiguous stream mappings.

#### [MODIFY] [Ieee80211Transmitter.cc](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.cc)
- If PPDU is MU-MIMO, enforce `totalNsts` against transmitter antenna count.

#### [MODIFY] [Ieee80211Radio.cc](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.cc)
- Update `collectHeMuUsers` and `encapsulate` to handle NDP sounding and MU-MIMO parameters.

#### [MODIFY] [Ieee80211ErrorModelBase.cc](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211ErrorModelBase.cc)
- In `computePacketErrorRate`, scale desired signal power by stream share and calculate SINR using sounded pairwise leakage overrides or default leakage.

---

## Verification Plan

### Automated Tests
- Build in release/debug mode: `make -j$(nproc) MODE=release`
- Run existing and new unit tests:
  ```sh
  bin/inet_run_unit_tests -m release -f "(Ieee80211He|HeDlScheduler).*\\.test"
  ```

### Manual Verification
- Write new unit tests validating:
  - Capability gating, management element serialization.
  - NDP Announcement -> NDP -> BFRP -> feedback sequence.
  - Scheduler RR grouping, stream apportionment, and fallback.
  - SINR and error model calculation with leakage.
