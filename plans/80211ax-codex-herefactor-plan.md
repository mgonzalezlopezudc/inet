# HE Refactor Implementation Plan

## Summary
- Use the confirmed choices: preserve existing MCS symbols, add AC-specific UORA overloads, and extract DL MU packing into a testable planner with side effects left in `HeDlMuTxOpFs`.
- Keep external behavior compatible except where new per-AC UORA behavior is explicitly added.

## Key Changes
- Add `HeDlMuPackingPlanner`.
  - Inputs: scheduler allocations, schedule context, queue lookup, BA availability, AID/multi-TID capability data, rate/duration limits, and packing limits.
  - Output: a deterministic plan containing selected users, packets per user, PSDU lengths, AID, RU/MCS/NSS/DCM, stream-start metadata, final PPDU duration, and rejection reason.
  - `HeDlMuTxOpFs::buildMuContainerPacket()` will call the planner, then perform queue removal, sequence assignment, ACK tracking, trailer edits, tagging, and final packet assembly.

- Move HE helper implementations from headers into `.cc` files.
  - Move `Ieee80211HeSigCodec.h` implementation-heavy encode/decode functions into `Ieee80211HeSigCodec.cc`.
  - Move `Ieee80211HeRu.h` implementation-heavy RU catalog/layout/allocation/validation functions into `Ieee80211HeRu.cc`.
  - Leave structs, declarations, trivial operators, and tiny accessors in headers.

- Compact the HE MCS table while keeping symbols.
  - Keep all existing `Ieee80211HemcsTable::heMcsXBWYMHzNssZ` public static names.
  - Replace repetitive declarations/definitions with macro/table expansion driven by one MCS descriptor table and bandwidth/NSS expansion.

- Add per-AC UORA state.
  - Store OCW/OBO as `std::array<int, 4>` indexed by `AccessCategory`.
  - Add `selectRandomAccessRu(AccessCategory ac, int count)` and `reportRandomAccessResult(AccessCategory ac, bool success)`.
  - Keep old no-AC methods as `AC_BE` compatibility wrappers.
  - Update watches and summaries to show all AC states.

## Test Plan
- Add planner unit tests for multi-user packing, BA-window exhaustion, single-survivor rejection, TXOP/PPDU trimming, multi-TID eligibility, and duplicate AID rejection.
- Update `Ieee80211HeDlMuTxOpFs_1.test` to verify side effects still happen through the frame sequence.
- Extend `Ieee80211HeUora_1.test` for independent AC state and legacy wrapper behavior.
- Run targeted HE unit tests:
  - `bin/inet_run_unit_tests -m release -f "(Ieee80211HeUora|Ieee80211HeDlMuTxOpFs|HeDlMuPacking|Ieee80211HeRu|Ieee80211HeWireSigBEncoding|Ieee80211HeMode|Ieee80211HeFixesBandSifsInvalidMcs).*\\.test"`
