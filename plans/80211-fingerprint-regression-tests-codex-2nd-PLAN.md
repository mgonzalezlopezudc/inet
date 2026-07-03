# Plan: Fix Pre-HE 802.11 Fingerprint Regressions

## Summary

Fix the current pre-HE 802.11 regressions by preventing HE/EHT management capability elements from being advertised or negotiated in non-HE/non-EHT modes such as `g(mixed)`. Then rerun the same baseline/current fingerprint matrix and update only fingerprints whose remaining changes are proven metadata-only.

The main bug candidate is that `Ieee80211MgmtAp` and `Ieee80211MgmtSta` unconditionally add HE capabilities to beacons, association responses, association requests, and probe requests. In pre-HE configs this changes management-frame byte lengths and showcase behavior.

## Implementation Changes

- Add explicit mode gates for management capabilities:
  - Reuse `Ieee80211Mac::isAxMode()` for HE.
  - Add `Ieee80211Mac::isBeMode()` for EHT.
  - Add shared management helpers, preferably in `Ieee80211MgmtBase`, to query `macModule` and answer:
    - `isHeManagementSupported()`: true only when MAC mode set is `ax` or `be`.
    - `isEhtManagementSupported()`: true only when MAC mode set is `be`.
  - If `macModule` cannot be resolved, fail closed for capability advertisement and log at debug level.

- Gate frame construction:
  - In `Ieee80211MgmtAp.cc`, make `addApHeManagementElements()` conditional on `isHeManagementSupported()`.
  - In `Ieee80211MgmtSta.cc`, set `heCapabilitiesPresent` in association requests and probe requests only when `isHeManagementSupported()`.
  - Ensure chunk length calculations continue to use `getHeMgmtElementsLength(body)` so omitted elements naturally restore pre-HE lengths.
  - Do not change existing HE/EHT MIB parameters; they may remain visible as model parameters and fingerprint metadata.

- Gate peer capability acceptance:
  - In AP association/reassociation request handling, ignore/remove peer HE capabilities if local mode is not HE-capable.
  - In STA association response handling and AP-info storage, only store/negotiated HE capabilities when local mode is HE-capable and the received frame has HE elements.
  - Apply the same pattern for EHT if any EHT management-element setters are added or already reachable.

- Keep HE/EHT behavior intact:
  - `ax` mode must still advertise HE capabilities and negotiate HE features.
  - `be` mode must advertise HE where required and EHT where implemented.
  - Existing HE MU/OFDMA/TWT code paths should remain enabled only through negotiated capabilities and existing mode checks.

## Verification Plan

- First run the narrow showcase regression:
  - Current tree: `cd tests/fingerprint && ./fingerprinttest -t 1 -F tyf -m "ieee80211|Ieee80211" showcases.csv`
  - Expected after fix: `showcases/visualizer/canvas/ieee80211/*` no longer changes management-frame byte totals due to HE elements in `g(mixed)`.

- Recheck representative evidence before updating fingerprints:
  - Compare `showcases/visualizer/canvas/ieee80211/OneNetwork` `.sca` against baseline.
  - Confirm management packet byte sums return to baseline or differ only for explained metadata.
  - Capture short PCAPng for `OneNetwork` if scalar byte totals still differ.

- Rerun the original matrix:
  - `wireless-combo.csv` with `-m Ieee80211Radio`
  - `examples.csv` with `-m "ieee80211|lan80211|layered80211|Ieee80211"`
  - `showcases.csv` with `-m "ieee80211|Ieee80211"`
  - Use `/tmp/inet-prehe-f833` as baseline with explicit `INET_ROOT`, `PATH`, and `PYTHONPATH`.

- Acceptance criteria:
  - No current-only runtime errors.
  - Pre-HE management-frame byte totals no longer grow from HE/EHT capability advertisement.
  - Remaining `wireless-combo` and `examples` fingerprint changes, if any, are documented as parameter/enum metadata-only with unchanged PCAP/scalar evidence.
  - Do not update checked-in fingerprints until each remaining changed trajectory is classified.

## Assumptions

- Desired behavior: pre-HE modes such as `a`, `b`, `g(erp)`, `g(mixed)`, `n(mixed-2.4Ghz)`, `p`, and `ac` must not advertise HE/EHT management elements.
- HE advertisement starts at `ax`; EHT advertisement starts at `be`.
- The existing `/tmp/inet-prehe-f833` checkout remains the accepted baseline, including its unrelated IPv6 source modification.
- Fingerprint updates are a separate follow-up after this fix and evidence pass.
