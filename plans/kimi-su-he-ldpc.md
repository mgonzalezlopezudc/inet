# Plan: Add HE SU LDPC support parallel to the HE-MU path

## Goal
Make normal 802.11ax single-user (HE SU) frames use LDPC when the local MIB advertises `heLdpc` and the addressed STA also advertises HE LDPC support, mirroring the capability negotiation, header coding flag, standards-faithful airtime accounting, and error-model boost that already exist for HE MU/TB.

## Current state
- HE MU/TB already implement end-to-end LDPC: scheduler checks `localHeCapabilities.ldpc` and negotiated peer `intersection.ldpc`, writes `coding` into `Ieee80211HeMuCommonReq`/`Ieee80211HeMuReq`, `Ieee80211Radio` sets `Ieee80211HeMuPhyHeader::coding`, and `computeHePpduParameters` / `Ieee80211ErrorModelBase` apply LDPC accounting and a 1.5 dB SNR boost.
- HE SU uses `Ieee80211HeMode` / `Ieee80211HeDataMode`.  `Ieee80211HeCompliantModes::getCompliantMode()` has no `ldpc` parameter, `Ieee80211HeDataMode` always assumes BCC tail bits, `Ieee80211Transmitter::computeTransmissionMode()` has no `Ieee80211HeMode` LDPC branch, and `Ieee80211Radio::encapsulate()` never sets the SU header `coding` field for HE.  The result is that HE SU frames remain BCC-coded even when `heLdpc=true`.

## Proposed approach: extend `Ieee80211HeMode` with an LDPC variant (recommended)
Reuse the existing HE SU mode-set path and add LDPC the same way HT/VHT do, but use the existing `computeHeUserPhyParameters` helper for standards-faithful HE SU LDPC airtime.  This keeps the packet-level representation as an `Ieee80211HtPhyHeader` (which `Ieee80211HeSignalMode` already creates) and only adds the missing coding flag, mode selection, LDPC code object, and error-model boost.

### Files and changes

1. **`src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211VhtCode.h/cc`**
   - Extend `Ieee80211VhtCompliantCodes::getCompliantCode(...)` with an trailing `bool ldpc = false` parameter.
   - Pass `ldpc` to the `Ieee80211VhtCode` constructor so the code chain can report `isLdpc() == true`.

2. **`src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h`**
   - Add `bool ldpc` and a cached `const Ieee80211VhtCode *ldpcCode` to `Ieee80211HeDataMode`.
   - Update the `Ieee80211HeDataMode` constructor to take `bool ldpc`.
   - Update `getCode()` to return `ldpcCode` when `ldpc == true`.
   - Make `getTailFieldLength()` return `b(0)` for LDPC.
   - Add `isLdpc()` accessor.
   - Update `Ieee80211HeCompliantModes::getCompliantMode(...)` signature with `bool ldpc = false` and add `ldpc` to the cache key tuple.

3. **`src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.cc`**
   - In `Ieee80211HeDataMode`, create the LDPC `Ieee80211VhtCode` (with `isLdpc() == true`) when requested; delete it in the destructor.
   - In `getCompleteLength()`, omit the BCC tail when LDPC is selected.
   - In `getDuration()`, if `ldpc == true`, build a full-bandwidth `Ieee80211HeRu` from the mode bandwidth and call `computeHeUserPhyParameters` to obtain the exact number of LDPC data symbols; otherwise keep the existing BCC symbol calculation.
   - Update `Ieee80211HeCompliantModes::getCompliantMode()` to include `ldpc` in the cache key and pass it to `Ieee80211HeDataMode`.

4. **`src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.cc`**
   - In `computeTransmissionMode()`, add an `Ieee80211HeMode` branch to the `useLdpc` decision using `mib->localHeCapabilities.ldpc` and `mib->findNegotiatedHeCapabilities(receiverAddress)->intersection.ldpc` (matching the VHT pattern).
   - Add the corresponding remap to `Ieee80211HeCompliantModes::getCompliantMode(mcs, centerFreqMode, preambleFormat, gi, useLdpc)`.

5. **`src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.cc`**
   - In `encapsulate()`, after the existing HT and VHT `coding` branches, add an `Ieee80211HeMode` branch that sets `Ieee80211HtPhyHeader::coding` to `1` when the HE data mode is LDPC.

6. **`src/inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/Ieee80211YansErrorModel.cc` and `Ieee80211NistErrorModel.cc`**
   - In the existing `else if (auto heMode = dynamic_cast<const Ieee80211HeMode *>(mode))` branch of `getDataSuccessRate`, apply the same 1.5 dB SNR boost that HT/VHT LDPC use when `heMode->getDataMode()->getCode()->isLdpc()`.

7. **Tests**
   - Add a new unit test `tests/unit/Ieee80211HeSuLdpc_1.test` that:
     - Creates LDPC and BCC `Ieee80211HeMode` variants via `Ieee80211HeCompliantModes::getCompliantMode`.
     - Asserts `isLdpc()` differs, tail bits are omitted for LDPC, and LDPC duration is computed with `computeHeUserPhyParameters` (no tail, codeword accounting).
     - Verifies that `Ieee80211Radio::encapsulate` would set the PHY header `coding` field to 1 for the LDPC variant (or directly test the relevant helper if easier).
   - Update or extend `tests/unit/Ieee80211HeMode_1.test` to check that the `ax` mode set still loads correctly and that an LDPC variant has the same net bitrate as its BCC counterpart.

## Build and test
- Build the affected libraries with `make -j$(nproc)` after the OMNeT++/INET environment is sourced.
- Run the focused HE unit tests:
  ```sh
  export CCACHE_DISABLE=1
  source /home/user/omnetpp-6.4.0aipre2/setenv -f
  source setenv -q
  bin/inet_run_unit_tests -m release -f "Ieee80211He.*\\.test"
  ```
- Verify the existing `Ieee80211HeLdpcPacketExtension_1.test` and `Ieee80211HeUserPhyParameters_1.test` still pass (they exercise the shared `computeHeUserPhyParameters` path).

## Notes / non-goals
- HE SU will continue to use `Ieee80211HtPhyHeader` for the packet-level PHY header because `Ieee80211HeSignalMode::createHeader()` already does so; we only set its `coding` field.
- HE MU/TB paths are not changed; the fix is intentionally limited to the missing HE SU branch.
- The standards-mandatory LDPC cases (40/80/160 MHz-capable STAs, 484/996-tone RUs, HE MCS 10/11) will be reachable automatically once the capability negotiation above is wired in, because the same `heLdpc` capability controls selection.
