# Implementation Plan - IEEE 802.11 HT and VHT LDPC Coding

This plan describes the implementation of Low-Density Parity-Check (LDPC) coding at the packet level for both IEEE 802.11 HT (802.11n) and VHT (802.11ac) modes in the INET wireless framework.

## User Review Required

> [!IMPORTANT]
> All changes default to LDPC disabled, ensuring complete backward compatibility. When `htLdpc` or `vhtLdpc` is set to `true` in the MIB, the physical layer transmitter will resolve the LDPC variant of the transmission mode.

## Proposed Changes

### 1. Management Information Base (MIB)

#### [MODIFY] [Ieee80211Mib.ned](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mib/Ieee80211Mib.ned)
- Add `bool htLdpc = default(false);` parameter.

#### [MODIFY] [Ieee80211Mib.h](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mib/Ieee80211Mib.h)
- Add `bool localHtLdpc = false;` member to the `Ieee80211Mib` class.

#### [MODIFY] [Ieee80211Mib.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mib/Ieee80211Mib.cc)
- Read `localHtLdpc = par("htLdpc").boolValue();` during module initialization stage `INITSTAGE_LOCAL`.

---

### 2. Physical Layer Code Classes (HT and VHT)

#### [MODIFY] [Ieee80211HtCode.h](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtCode.h)
- Add `bool ldpc = false;` member variable.
- Update constructor to accept a `bool ldpc = false` parameter.
- Add getter: `bool isLdpc() const { return ldpc; }`.

#### [MODIFY] [Ieee80211HtCode.cc](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtCode.cc)
- Initialize the `ldpc` member in the `Ieee80211HtCode` constructor.

---

### 3. Physical Layer Data Modes and Duration Calculations

#### [MODIFY] [Ieee80211HtMode.h](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtMode.h)
- Update `Ieee80211HtDataMode` constructor signature to accept `bool ldpc = false`.
- Add members `bool ldpc = false;`, `mutable const Ieee80211HtCode *ldpcCode = nullptr;`, and `mutable std::vector<unsigned int> numberOfCodedBitsPerSpatialStreams;` (keeps stream bit-sizes vector alive for interleaving reference).
- Update virtual destructor: `virtual ~Ieee80211HtDataMode();`.
- Override `getCode()`: `virtual const Ieee80211HtCode *getCode() const override;`.
- Update `getTailFieldLength()` to return `b(0)` when `getCode()->isLdpc()` is true, and `b(6) * numberOfBccEncoders` otherwise.
- Update `Ieee80211HtCompliantModes::getCompliantMode` signature to accept `bool ldpc = false`.
- Update `modeCache` key in `Ieee80211HtCompliantModes` to include a `bool` for LDPC.

#### [MODIFY] [Ieee80211HtMode.cc](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211HtMode.cc)
- Implement the updated `Ieee80211HtDataMode` constructor: if `ldpc` is true, populate `numberOfCodedBitsPerSpatialStreams` from stream modulations and instantiate a custom `Ieee80211HtCode` with `ldpc = true`.
- Implement `Ieee80211HtDataMode::~Ieee80211HtDataMode()` to delete `ldpcCode`.
- Implement `Ieee80211HtDataMode::getCode()`.
- Update `Ieee80211HtCompliantModes::getCompliantMode` to use the 4-tuple key (incorporating `ldpc`) for cache lookup and creation.

#### [MODIFY] [Ieee80211VhtMode.h](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.h)
- Update `Ieee80211VhtDataMode` constructor signature to accept `bool ldpc = false`.
- Add members `bool ldpc = false;`, `mutable const Ieee80211VhtCode *ldpcCode = nullptr;`, and `mutable std::vector<unsigned int> numberOfCodedBitsPerSpatialStreams;`.
- Update virtual destructor: `virtual ~Ieee80211VhtDataMode();`.
- Override `getCode()`: `virtual const Ieee80211VhtCode *getCode() const override;`.
- Update `getTailFieldLength()` to return `b(0)` when `getCode()->isLdpc()` is true, and `b(6) * numberOfBccEncoders` otherwise.
- Update `Ieee80211VhtCompliantModes::getCompliantMode` signature to accept `bool ldpc = false`.
- Update `modeCache` key in `Ieee80211VhtCompliantModes` to include a `bool` for LDPC.

#### [MODIFY] [Ieee80211VhtMode.cc](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtMode.cc)
- Implement the updated `Ieee80211VhtDataMode` constructor: if `ldpc` is true, populate `numberOfCodedBitsPerSpatialStreams` from stream modulations and instantiate a custom `Ieee80211VhtCode` with `ldpc = true`.
- Implement `Ieee80211VhtDataMode::~Ieee80211VhtDataMode()` to delete `ldpcCode`.
- Implement `Ieee80211VhtDataMode::getCode()`.
- Update `Ieee80211VhtCompliantModes::getCompliantMode` to use the 5-tuple key (incorporating `ldpc`) for cache lookup and creation.

---

### 4. PHY Message Headers

#### [MODIFY] [Ieee80211PhyHeader.msg](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader.msg)
- Add `uint8_t coding = 0; // 0: BCC, 1: LDPC` to both `Ieee80211HtPhyHeader` and `Ieee80211VhtPhyHeader`.

---

### 5. Transmitter and Radio Integration

#### [MODIFY] [Ieee80211Transmitter.cc](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.cc)
- Include `Ieee80211HtMode.h`, `Ieee80211VhtMode.h`, `Ieee80211Mib.h`, and `Ieee80211Frame_m.h`.
- Update `computeTransmissionMode()` to check the destination MAC address of the packet and query the local MIB for HT or VHT LDPC capabilities. If LDPC is enabled (or negotiated with peer in VHT), resolve and return the LDPC variant of the transmission mode.

#### [MODIFY] [Ieee80211Radio.cc](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.cc)
- In `encapsulate()`, set the `coding` field of the instantiated `Ieee80211HtPhyHeader` or `Ieee80211VhtPhyHeader` to `1` if `mode->getDataMode()->getCode()->isLdpc()` is true, and `0` otherwise.

---

### 6. Physical Error Models

#### [MODIFY] [Ieee80211NistErrorModel.cc](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211NistErrorModel.cc)
- In `getDataSuccessRate()` when evaluating `Ieee80211HtMode`, boost the SNR by 1.5 dB if `htMode->getDataMode()->getCode()->isLdpc()` is true.

#### [MODIFY] [Ieee80211YansErrorModel.cc](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211YansErrorModel.cc)
- In `getDataSuccessRate()` when evaluating `Ieee80211HtMode`, boost the SNR by 1.5 dB if `htMode->getDataMode()->getCode()->isLdpc()` is true.

---

## Verification Plan

### Automated Tests

- **New Unit Tests**:
  - `tests/unit/HtErrorModelLdpc_1.test`: Verifies the 1.5 dB LDPC coding gain in the NIST and YANS error models for HT modes.
  - `tests/unit/HtVhtTransmissionDurationLdpc_1.test`: Verifies the tail-bit omission in the duration calculation of HT and VHT compliant modes.
  - `tests/unit/HtVhtTransmitterLdpc_1.test`: Verifies transmitter and radio-encapsulator correct selection of LDPC and PHY header coding fields.

- Run all IEEE 802.11 unit tests using the standard test runner command:
  ```sh
  export CCACHE_DISABLE=1
  source /home/user/omnetpp-6.4.0/setenv -f
  source setenv -q
  bin/inet_run_unit_tests -m release -f ".*Ldpc.*"
  ```
