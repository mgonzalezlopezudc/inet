# Completing and Fixing IEEE 802.11 WiFi 5 (802.11ac / VHT) in INET

This implementation plan details the steps required to complete and validate the IEEE 802.11ac (VHT) features in the INET framework, moving from the current partial/incomplete implementation to a fully validated WiFi 5 standard compliant model.

The plan is divided into Part A (completing existing features) and Part B (adding new features), following the sequencing and principles outlined in the workspace study.

---

## User Review Required

> [!IMPORTANT]
> **Operating Configuration over Negotiation**:
> By default, nodes will use user-set MIB configurations to determine their capabilities (bandwidth, NSS, GI, coding, MCS, etc.) without requiring a negotiation handshake. Negotiation (Part B2) is completely optional and opt-in via a configuration flag.

> [!NOTE]
> **MIMO Abstraction Model**:
> Rather than modeling a full C++ channel matrix, beamforming will be implemented as a configured SINR gain (e.g., +3 dB) on the link, and DL MU-MIMO will be modeled as concurrent streams with an inter-user cross-talk/interference penalty (e.g., -2 dB SINR degradation).

---

## Open Questions

> [!NOTE]
> **LDPC Coding Curves Model**:
> Based on your feedback, we will use standard theoretical approximation curves for the LDPC coded path. Specifically, we will apply a ~1.5 to 2.0 dB SNR coding gain offset to the SNR input when looking up error rates relative to the convolutional (BCC) code baseline.

---

## Proposed Changes

### Component 1: Physical Layer & Coding (Part A1, A3)

We will introduce first-class LDPC support in `Ieee80211VhtCode` and update error models (`Ieee80211NistErrorModel` and `Ieee80211YansErrorModel`) to adjust the SNR/PER calculation when LDPC is used. We will also clean up bandwidth and GI selection.

#### [MODIFY] [Ieee80211VhtCode.h](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtCode.h)
- Extend `Ieee80211VhtCode` to support an `isLdpc` flag.
- Add factory methods to construct LDPC-enabled codes.

#### [MODIFY] [Ieee80211VhtCode.cc](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtCode.cc)
- Update code instantiation in `getCompliantCode` to allow setting the coding type (BCC vs LDPC).

#### [MODIFY] [Ieee80211NistErrorModel.cc](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211NistErrorModel.cc)
- Update `getOFDMAndERPOFDMChunkSuccessRate` to apply a coding gain (e.g., +1.5 dB effective SNR) when the transmission uses LDPC coding.

#### [MODIFY] [Ieee80211YansErrorModel.cc](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211YansErrorModel.cc)
- Apply the same LDPC effective SNR gain scaling inside error calculations.

---

### Component 2: MAC Layer, TXOP & Aggregation (Part A2)

We will complete `HtTxOpFs` to correctly select and initiate protected frame sequences (L-SIG, NAV-protected, dual-CTS, etc.) and apply VHT A-MPDU aggregation/length bounds.

#### [MODIFY] [HtTxOpFs.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/framesequence/HtTxOpFs.cc)
- Replace the stub in `selectHtTxOpSequence` with the selection of correct sequences (e.g., RTS/CTS, block ack, or simple data transmission) according to the transmission protection requirements and remaining TXOP duration.

#### [NEW] [VhtMpduAggregationPolicy.h](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/aggregation/VhtMpduAggregationPolicy.h)
#### [NEW] [VhtMpduAggregationPolicy.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/aggregation/VhtMpduAggregationPolicy.cc)
#### [NEW] [VhtMpduAggregationPolicy.ned](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/aggregation/VhtMpduAggregationPolicy.ned)
- Implement VHT-aware MPDU aggregation limits (up to 1,048,575 bytes, exponent 0-7, and checking local/peer capabilities).

---

### Component 3: Operating Configuration Model & MIB (Part B1, B2)

Introduce direct user configuration of VHT/11ac capabilities (BW, NSS, GI, MCS, coding, beamforming, MU) in `Ieee80211Mib` and support optional capability negotiation in management frames.

#### [NEW] [Ieee80211VhtCapabilities.h](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mib/Ieee80211VhtCapabilities.h)
- Define `Ieee80211VhtCapabilities` and `Ieee80211VhtOperation` structs, along with a `negotiateVhtCapabilities` utility function.

#### [MODIFY] [Ieee80211Mib.h](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mib/Ieee80211Mib.h)
- Add VHT capability fields to BssAccessPointData and local/negotiated capabilities maps.

#### [MODIFY] [Ieee80211Mib.cc](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mib/Ieee80211Mib.cc)
- Parse VHT capabilities from the module parameters.
- Provide helper methods to register peer capabilities and lookup negotiated configurations.

#### [MODIFY] [Ieee80211Mib.ned](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mib/Ieee80211Mib.ned)
- Expose parameters for VHT capabilities (bandwidth, NSS, GI, LDPC, Beamforming, MU-MIMO).

---

### Component 4: MIMO & Sounding (Part B3, B4)

Implement VHT Sounding (NDPA -> NDP -> CBF feedback) and abstract beamforming/MU-MIMO gain adjustments in the PHY layer.

#### [MODIFY] [Ieee80211ErrorModelBase.cc](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211ErrorModelBase.cc)
- Read the beamforming/MU tags from the transmission packet.
- Adjust the SNR used for success rate calculation by applying the `beamformingGainDb` or subtracting `muMimoPenaltyDb`.

---

## Verification Plan

### Automated Tests
To adhere to a strict Test-Driven Development (TDD) cycle, we will create the following comprehensive unit test files under `tests/unit/`:

1. **`Ieee80211VhtCode_1.test`**:
   - Verify VHT LDPC coding flags, correct compliant code lookup, and net/gross bitrate calculation for all VHT MCS modes.
2. **`VhtErrorModelLdpc_1.test`**:
   - Verify that LDPC coding is correctly handled by `Ieee80211NistErrorModel` and `Ieee80211YansErrorModel`.
   - Assert that LDPC FEC yields the expected ~1.5–2.0 dB coding gain under identical SNR compared to Convolutional Code (BCC).
3. **`HtTxOpFs_1.test`**:
   - Verify that `HtTxOpFs::selectHtTxOpSequence` correctly chooses protected frame sequences (L-SIG protected, NAV-protected, dual-CTS protected, or simple sequence) based on context and remaining TXOP.
4. **`VhtMpduAggregation_1.test`**:
   - Verify that `VhtMpduAggregationPolicy` successfully aggregates packets up to VHT-defined limits (up to 1,048,575 bytes/exponent 7) and correctly respects individual station capability limits.
5. **`VhtMibCapabilities_1.test`**:
   - Verify that `Ieee80211Mib` parses, stores, and negotiates/intersects local and peer VHT capabilities (bandwidth, spatial streams, coding type, beamforming/MU flags) correctly.
6. **`VhtBeamformingMimo_1.test`**:
   - Verify that the MIMO abstraction layer applies the configured beamforming SINR gain and the MU-MIMO interference penalty correctly to the SNR/PER calculation.

Tests will be executed via:
```sh
export CCACHE_DISABLE=1
source /home/user/omnetpp-6.4.0/setenv -f
source setenv -q
bin/inet_run_unit_tests -m release -f "(Ieee80211VhtCode|VhtErrorModelLdpc|HtTxOpFs|VhtMpduAggregation|VhtMibCapabilities|VhtBeamformingMimo).*\\.test"
```

### Manual Verification
Run the 11ac example in `examples/wireless/lan80211ac` and assert that standard stats (throughput, airtime, frame counts) are generated correctly for 40, 80, and 160 MHz configurations.
