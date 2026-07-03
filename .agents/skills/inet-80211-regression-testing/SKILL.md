---
name: inet-80211-regression-testing
description: Design, run, and interpret focused IEEE 802.11 regression tests in INET. Use when Codex needs to create or diagnose deterministic Wi-Fi reproductions, choose seeds, compare before/after behavior, validate HE/EHT or MAC/PHY fixes, add unit or simulation coverage, or avoid overfitting an 802.11 change to one run.
---

# IEEE 802.11 regression testing in INET

Use this skill after, or alongside, `inet-80211-packet-debugging` when the task is to prove an 802.11 change is covered and does not regress important behavior.

## Core rule

A passing single run is evidence, not coverage. Verify the specific frame exchange, timing, counters, and configuration feature gate that the change was meant to protect.

## Workflow

1. State the 802.11 behavior under test: management, association, DCF/EDCA, RTS/CTS, ACK/Block Ack, retry, aggregation, rate control, PHY reception, interference, AP forwarding, HE/EHT feature, or packet tags.
2. Build the smallest deterministic scenario that exercises that behavior.
3. Use one configuration and run number first; record seed-set and all command-line overrides.
4. Capture or log the protocol-visible invariant using PCAPng/TShark, Cmdenv logs, event logs, result vectors/scalars, unit-test assertions, or source-level checks.
5. Compare before/after using the same seed, binary mode, NED path, and configuration.
6. Expand to multiple seeds or parameter points only after the narrow failing seed is understood.
7. For fingerprint changes, use `inet-fingerprint-regression` before accepting updates.
8. For NED/INI uncertainty, use `inet-ned-ini-analysis` before attributing behavior to C++.

## Good 802.11 invariants

* Association state reaches the expected AP before data exchange.
* Unicast DATA expecting ACK either receives ACK or retries/drops for the demonstrated reason.
* RTS/CTS appears only when the configured policy requires it.
* Retry counters, sequence numbers, fragment numbers, and Retry bit evolve consistently.
* QoS traffic maps to the intended TID/access category.
* Aggregation and Block Ack windows advance as expected.
* Receiver power/SNIR/error decision explains PHY success or failure.
* AP forwarding preserves the correct 802.11 address interpretation and wired/wireless handoff.
* HE/EHT-specific configuration is implemented and enabled, not just present in the standard.

## Avoid

* Replacing the radio with an ideal model as final verification unless that is the intended abstraction.
* Accepting a fix because throughput improved without checking the targeted frame exchange.
* Updating fingerprints before explaining the first changed event.
* Testing only one lucky seed for contention, interference, mobility, rate control, or randomized traffic.
* Adding broad logging or captures permanently to normal experiment configs.

## Reporting

Include scenario path, INI config, run/seed, expected invariant, exact command, evidence files, before/after comparison, pass/fail outcome, fingerprint impact if any, seeds or parameter points covered, and remaining model-fidelity limitations.
