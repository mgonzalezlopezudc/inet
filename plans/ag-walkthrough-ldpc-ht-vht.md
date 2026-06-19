# Walkthrough - IEEE 802.11 HT and VHT LDPC Coding

We have successfully implemented and validated packet-level Low-Density Parity-Check (LDPC) coding for both IEEE 802.11 HT (802.11n) and VHT (802.11ac) modes in the INET framework.

## Changes Made

### 1. Management Information Base (MIB)
- **[Ieee80211Mib.ned](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mib/Ieee80211Mib.ned)**: Added `bool htLdpc = default(false)` parameter.
- **[Ieee80211Mib.h](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mib/Ieee80211Mib.h)** & **[Ieee80211Mib.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mib/Ieee80211Mib.cc)**: Extended to parse and store `localHtLdpc`.

### 2. Mode Classes & Duration Calculations
- **[Ieee80211HtCode](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtCode.h)**: Introduced `ldpc` flag support in codes.
- **[Ieee80211HtMode](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtMode.h)** & **[Ieee80211VhtMode](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.h)**:
  - Updated compliant modes creation to resolve LDPC modes.
  - Updated `getTailFieldLength()` to return `b(0)` when LDPC is in use, successfully omitting the 6 tail bits per encoder.
  - Corrected `Ieee80211VhtSignalMode::createHeader()` in `Ieee80211VhtMode.h` to return `Ieee80211VhtPhyHeader` instead of `Ieee80211HtPhyHeader`.

### 3. PHY Message Headers
- **[Ieee80211PhyHeader.msg](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader.msg)**: Added `uint8_t coding = 0` field (0: BCC, 1: LDPC) to `Ieee80211HtPhyHeader` and `Ieee80211VhtPhyHeader`.

### 4. Transmitter and Radio Integration
- **[Ieee80211Transmitter.cc](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.cc)**:
  - Implemented automatic mapping of transmission modes to their LDPC variant in `computeTransmissionMode` when LDPC is enabled in the MIB (or negotiated in VHT).
  - Designed type-safe header peeking utilizing `dynamic_cast` checks on `frontChunk` to safely inspect the MAC receiver address without triggering reinterpretation conversions.
- **[Ieee80211Radio.cc](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.cc)**: Populated the `coding` field of `Ieee80211HtPhyHeader` / `Ieee80211VhtPhyHeader` on encapsulation according to the resolved mode's coding type.

### 5. Error Models Updates
- **[Ieee80211NistErrorModel.cc](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211NistErrorModel.cc)** & **[Ieee80211YansErrorModel.cc](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211YansErrorModel.cc)**: Applied a 1.5 dB effective SNR boost for HT modes utilizing LDPC coding.

---

## Verification & Automated Tests

We ran the automated test suite specifically for LDPC. All four tests compiled and executed successfully:

```
[1/5]   ⏺ HtErrorModelLdpc_1.test PASS in 0.511
[2/5]   ⏺ HtVhtTransmissionDurationLdpc_1.test PASS in 1.64
[3/5]   ⏺ VhtErrorModelLdpc_1.test PASS in 1.837
[4/5]   ⏺ HtVhtTransmitterLdpc_1.test PASS in 2.05
[5/5] ◉ 4 inet unit tests (concurrently) Multiple unit tests: 4 PASS in 2.189
Multiple unit test results: PASS, summary: 4 PASS in 2.189
```

- **[HtErrorModelLdpc_1.test](file:///home/user/omnetpp_ws/inet/tests/unit/HtErrorModelLdpc_1.test)**: Verified the 1.5 dB SNR boost in YANS/NIST error models.
- **[VhtErrorModelLdpc_1.test](file:///home/user/omnetpp_ws/inet/tests/unit/VhtErrorModelLdpc_1.test)**: Verified the VHT error model integration.
- **[HtVhtTransmissionDurationLdpc_1.test](file:///home/user/omnetpp_ws/inet/tests/unit/HtVhtTransmissionDurationLdpc_1.test)**: Verified correct getTailFieldLength() returning 0 for LDPC and correctly shortening the transmission duration.
- **[HtVhtTransmitterLdpc_1.test](file:///home/user/omnetpp_ws/inet/tests/unit/HtVhtTransmitterLdpc_1.test)**: Verified transmitter mode mapping and correct populating of the coding field in encapsulation.
