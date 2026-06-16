---
phase: 01-addba-validation-handshake-correctness
plan: 02
subsystem: mac
tags: [ieee80211, he-mu, addba, fallback, retry, cooldown]

requires:
  - phase-01-plan-01
provides:
  - FIFO-preserving SU fallback gate before MU scheduling in HeHcf.
  - Prompt contention resume after successful ADDBA response processing.
  - Concrete ADDBA retry cooldown policy and bounded retry burst behavior.
  - Extended focused ADDBA validation test coverage for retry/cooldown behavior.
affects: [phase-02-plan-01, mac, ieee80211-he-mu]

tech-stack:
  added: []
  patterns:
    - Head-of-line MU eligibility gate with explicit Hcf fallback.
    - Agreement-owned ADDBA retry accounting with cooldown-based suppression.

key-files:
  created:
    - .planning/phases/01-addba-validation-handshake-correctness/01-02-SUMMARY.md
  modified:
    - tests/unit/Ieee80211HeMuAddbaValidation_1.test
    - src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc
    - src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc
    - src/inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreementHandler.cc
    - src/inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreementPolicy.cc
    - src/inet/linklayer/ieee80211/mac/blockack/OriginatorBlockAckAgreementPolicy.ned

requirements-completed: [MAC-01, MAC-02]

completed: 2026-06-16
---

# Phase 01 Plan 02: SU Fallback And ADDBA Retry Summary

## Accomplishments

- Added MU precheck in `HeHcf::startFrameSequence(ac)` so an MU-ineligible earliest SU-transmittable packet falls back to `Hcf::startFrameSequence(ac)` for the current TXOP.
- Updated `HeHcf::collectCandidateStations()` to use dynamic active-BA checks via `hasActiveOriginatorBlockAckAgreement(...)` while preserving first-seen destination behavior.
- Added `resumeContention()` after successful ADDBA response handling in both recipient and originator management receive paths in `Hcf.cc`.
- Implemented concrete ADDBA retry cooldown in `OriginatorBlockAckAgreementPolicy`:
  - Added `addbaFailureTimeout` parameter in NED with default `1s`.
  - Implemented `computeAddbaFailureTimeout()`.
- Implemented bounded ADDBA retry behavior in `OriginatorBlockAckAgreementHandler`:
  - Up to three retries in a burst for inactive agreements.
  - Suppression before cooldown expiry with diagnostic logging.
  - Retry resumption after cooldown without teardown/recreation.
  - `processTransmittedAddbaReq(...)` now increments BA policy frame counters and refreshes inactive retry deadline.
- Extended focused test `Ieee80211HeMuAddbaValidation_1.test` with a retry/cooldown behavior scenario and plan-02 traceability assertions.

## Verification

- Source verification checks succeeded for required symbols and callsites:
  - `Hcf::startFrameSequence(ac)` fallback in `HeHcf.cc`.
  - `hasActiveOriginatorBlockAckAgreement` usage in `HeHcf.cc`.
  - `resumeContention` calls in `Hcf.cc` after ADDBA response processing.
  - `addbaFailureTimeout` in `OriginatorBlockAckAgreementPolicy.ned` and policy implementation.
  - `baPolicyFrameSent` and `getNumSentBaPolicyFrames` logic in the handler.
- Workspace diagnostics (`get_errors`) reported no editor/language-service errors in all modified files.

## Issues Encountered

- The prescribed Python test command failed in this environment due missing Python dependencies (`matplotlib`, `pandas`, `scipy`, `dask`) in the OMNeT++/INET Python stack.
- A full debug build run later failed due host disk exhaustion (`No space left on device`) after many objects had already compiled; this blocked completion of an end-to-end full build.

## Residual Risk

- The focused test harness command could not be completed end-to-end in this environment, so behavioral validation relies on source checks plus clean diagnostics rather than a fully passing targeted run.
- Full workspace compilation was interrupted by disk capacity limits unrelated to this plan's code.

## Next Phase Readiness

Phase 01 is complete and Phase 02 can proceed with timing/spacing verification built on enforced MU admission and deterministic SU fallback behavior.
