# Walkthrough - IEEE 802.11ac (WiFi 5) VHT Completion and Fixes

Completed the implementation and verification of IEEE 802.11ac / VHT support in the INET framework under a test-driven development (TDD) model.

## Changes Made

### 1. Tag & MIMO Abstraction
- **Transmission Tag**: Added [Ieee80211VhtTransmissionTag](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag.msg) representing the beamforming status, MIMO settings, beamforming gain (+3.0 dB), and concurrent stream MU-MIMO interference penalty (-2.0 dB).
- **Error Model SNR Adjustments**: Integrated tag lookups and SNR adjustments (+3.0 dB for beamforming, -2.0 dB for MU-MIMO) into [Ieee80211ErrorModelBase::computePacketErrorRate](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211ErrorModelBase.cc).

### 2. MIB, Capabilities & Aggregation
- **MIB Capabilities & Configuration**: Parsed, negotiated, and intersected capabilities using `Ieee80211VhtCapabilities` and `Ieee80211VhtOperation` in [Ieee80211Mib](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mib/Ieee80211Mib.cc).
- **Aggregation & Exponent Limits**: Enforced maximum A-MPDU exponent limits (up to exponent 7 / 1,048,575 bytes) via the VHT-specific policy in [VhtMpduAggregationPolicy](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/aggregation/VhtMpduAggregationPolicy.cc).
- **80+80 MHz Support**: Added support for 80+80 MHz bandwidth channels in [Ieee80211Band](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211Band.cc).
- **LDPC Gain**: Integrated standard +1.5 dB coding gain to linear SNR when LDPC is in use across both [Ieee80211NistErrorModel](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211NistErrorModel.cc) and [Ieee80211YansErrorModel](file:///home/user/omnetpp_ws/inet/src/inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211YansErrorModel.cc).
- **TXOP Selection**: Added predicates and frame sequence logic to support NAV protection in [HtTxOpFs](file:///home/user/omnetpp_ws/inet/src/inet/linklayer/ieee80211/mac/framesequence/HtTxOpFs.cc).

---

## Verification Results

### Automated Tests
Ran the unit test suite containing our 6 new VHT tests as well as the 19 pre-existing HE/OFDMA tests. All tests pass successfully.

#### 1. VHT / WiFi 5 Unit Tests (6/6 Pass)
```sh
bin/inet_run_unit_tests -m release -f "(Ieee80211VhtCode|VhtErrorModelLdpc|HtTxOpFs|VhtMpduAggregation|VhtMibCapabilities|VhtBeamformingMimo).*\\.test"
```
Output snippet:
```
[1/7]   ⏺ VhtBeamformingMimo_1.test PASS in 0.54
[2/7]   ⏺ VhtMibCapabilities_1.test PASS in 0.544
[3/7]   ⏺ VhtErrorModelLdpc_1.test PASS in 0.585
[4/7]   ⏺ HtTxOpFs_1.test PASS in 0.587
[5/7]   ⏺ Ieee80211VhtCode_1.test PASS in 0.596
[6/7]   ⏺ VhtMpduAggregation_1.test PASS in 0.606
[7/7] ◉ 6 inet unit tests (concurrently) Multiple unit tests: 6 PASS in 0.74
Multiple unit test results: PASS, summary: 6 PASS in 0.74
```

#### 2. Regression Tests (19/19 Pass)
```sh
bin/inet_run_unit_tests -m release -f "(Ieee80211He|HeDlScheduler).*\\.test"
```
Output snippet:
```
[20/20] ◉ 19 inet unit tests (concurrently) Multiple unit tests: 19 PASS in 7.96
Multiple unit test results: PASS, summary: 19 PASS in 7.96
```
