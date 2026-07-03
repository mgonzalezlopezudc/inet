---
name: inet-fingerprint-regression
description: Diagnose and manage INET fingerprint regression tests. Use when Codex needs to run fingerprint tests, interpret fingerprint mismatches, decide whether changed fingerprints are expected, update fingerprints with evidence, or distinguish harmless simulation-event changes from behavioral regressions.
---

# INET fingerprint regression work

Use this skill when a change affects INET fingerprints or when the user asks about fingerprint tests, fingerprint mismatches, deterministic regression checks, or updating expected fingerprints.

## Core rule

Treat a fingerprint change as evidence that the simulation trajectory changed. Do not update fingerprints until the changed trajectory is explained and accepted.

## Workflow

1. Identify the exact test, simulation, configuration, run number, seed-set, and working directory.
2. Run the smallest relevant fingerprint test or simulation first; do not start a broad campaign unless requested.
3. Preserve the exact command, exit status, old fingerprint, new fingerprint, and first mismatch location reported by the tool.
4. Determine whether the code change was expected to alter event ordering, packet contents, timing, random-stream consumption, result recording, or topology/module construction.
5. Use `inet-cmdenv-log-analysis`, `omnetpp-eventlog-analysis`, `inet-pcap-tshark-analysis`, or `omnetpp-result-analysis` to explain the first behavioral divergence when the mismatch is not obvious.
6. Update expected fingerprints only when the behavioral change is intentional or demonstrably irrelevant to the tested contract.
7. Rerun the same narrow test after updating expectations.
8. For 802.11 changes, also consider `inet-80211-regression-testing` and `inet-80211-packet-debugging` before accepting changed fingerprints.

## Common causes to check

* Changed event scheduling order or timer timing.
* Added, removed, renamed, or reordered modules/gates/signals/statistics.
* Changed packet headers, tags, encapsulation, aggregation, retransmission, or drops.
* Changed random-number stream consumption.
* Changed logging/result recording that affects event processing.
* Different release/debug build, library, working directory, NED path, or INI inheritance.
* Nondeterminism from iteration order, uninitialized state, wall-clock input, file ordering, or address-dependent behavior.

## Do not

* Treat a fingerprint update as a fix.
* Update many fingerprints to hide an unexplained first divergence.
* Compare runs with different binaries, NED paths, seeds, command-line overrides, or dirty source states unless that difference is intentional.
* Assume a small fingerprint difference is harmless without finding what changed first.

## Reporting

Include the exact command, working directory, test/filter, configuration, run, seed-set, old and new fingerprints, first mismatch evidence, explanation of the trajectory change, files updated if any, rerun result, and remaining uncertainty.
