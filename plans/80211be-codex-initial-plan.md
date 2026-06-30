# 802.11be EHT + Integrated MLO Plan

## Summary
Implement IEEE 802.11be EHT for infrastructure AP/STA in INET with `opMode="be"`, 5/6 GHz operation, 20/40/80/160/320 MHz channels, packet-level EHT PHY behavior, and integrated MAC-level MLO. Preserve existing HE/AX behavior and reuse HE MU/OFDMA machinery where the model shape matches.

Use the existing standards pipeline as the source of truth: run `bin/inet_process_standards status`, use `search` and `show` against `standards/processed/index.sqlite`, and rebuild with `bin/inet_process_standards build` only if the corpus is missing or stale. Current status shows `80211be-2024` is already indexed and fresh.

## Key Changes
- Add EHT PHY support:
  - New `Ieee80211EhtMode` family, EHT MCS table 0..13 including 4096-QAM, 5/6 GHz band modes, 320 MHz mode-set entries, and `opMode="be"` in radio/MAC/interface NED enums.
  - Add EHT PPDU/header/tag types for packet-level EHT MU/TB/non-OFDMA operation, including U-SIG/EHT-SIG fields, per-user metadata, puncturing, RU/MRU allocation, and link ID metadata.
  - Extend RU allocation from HE RU-only layouts to EHT RU/MRU layouts, including 320 MHz tone plans, MRU combinations, 3984-tone full-width allocations, and validated puncturing masks.
  - Add `Ieee80211EhtPhyCalculator` and EHT hooks in transmitter, receiver, radio medium, and error models; add 4096-QAM BER/PER handling to Yans/Nist paths.

- Add standards-backed implementation workflow:
  - For every EHT constant/table/field layout, first locate the indexed source with commands like `bin/inet_process_standards search "EHT Operation element"` and inspect exact chunks with `bin/inet_process_standards show 80211be-2024:chunk:NNNNN`.
  - Record relevant chunk IDs in code comments or test descriptions where constants are non-obvious.
  - Do not read or regenerate PDFs directly unless `status` reports stale/missing artifacts.

- Add EHT capabilities and management:
  - New `Ieee80211EhtCapabilities`, `Ieee80211EhtOperation`, negotiated EHT capability structs, and MIB/NED parameters for EHT widths, MCS/NSS, puncturing, 4096-QAM, MU-MIMO/OFDMA, EMLSR/EMLMR, STR/NSTR, and MLD identity.
  - Extend management frames and serializers with spec-field-full EHT Capabilities, EHT Operation, Multi-Link, TID-to-Link Mapping, and related AP discovery elements needed for AP/STA association.
  - Update explicit and simplified AP/STA management flows so beacons/probe responses/association requests/responses negotiate HE + EHT + MLO consistently.

- Implement integrated MLO inside the MAC:
  - Refactor `Ieee80211Mac` internals around per-link contexts: link ID, channel/radio, HCF/DCF state, queues, rate control, BA/reorder state, NAV, and link-local address, plus shared MLD address and association state.
  - Implement spec-field-full MLO setup, per-link association state, link enable/disable, TID-to-link mapping, STR/NSTR conflict handling, EMLSR/EMLMR link-state behavior, and shared retry/BA policy.
  - Default traffic steering uses airtime/backlog selection among eligible mapped links; TID-to-link mapping constrains eligibility before load-based selection.

## Test Plan
- Add unit tests for standards pipeline lookups used by EHT work where needed, and keep `python/inet/standards/test_processor.py` passing.
- Add unit tests for EHT mode tables, MCS 12/13 rates, 320 MHz duration calculations, EHT guard intervals, RU/MRU validation, puncturing masks, and EHT per-user PHY parameters.
- Add serializer round-trip tests for EHT PHY headers, EHT management elements, Multi-Link elements, TID-to-Link Mapping, trigger/control extensions, and malformed/unsupported field combinations.
- Add AP/STA association tests for single-link EHT, multi-link setup, link add/remove, negotiated capability rejection, and simplified-management parity.
- Add MAC behavior tests for STR vs NSTR blocking, EMLSR/EMLMR state transitions, airtime/backlog steering, TID mapping constraints, shared BA/retry behavior, and per-link queue isolation.
- Verification commands:
  - `rtk bin/inet_process_standards status`
  - `rtk bin/inet_run_unit_tests -m release -f "(Ieee80211Eht|Eht|Mlo|Ieee80211He).*\\.test"`
  - `rtk make -j$(nproc)` after sourcing OMNeT++ and INET env with `CCACHE_DISABLE=1`.

## Assumptions
- Target is AP/STA infrastructure only; ad hoc and mesh EHT/MLO are out of scope.
- EHT is enabled for 5/6 GHz only; 2.4 GHz remains non-EHT.
- MLO field/state-machine fidelity is spec-full, but PHY remains packet-level like the existing HE model rather than a new bit-level EHT PHY.
- Existing `ax` behavior and HE tests must remain unchanged unless explicitly updating shared generalized helpers.
