# Project Research Summary

**Project:** 802.11ax DL MU OFDMA Correctness
**Domain:** Wireless Simulation / IEEE 802.11ax
**Researched:** 2026-06-16
**Confidence:** HIGH

## Executive Summary

This research establishes the verification and correction strategies for the Downlink Multi-User OFDMA (DL MU OFDMA) implementation in the INET Framework. The current implementation supports core functions such as parallel RU scheduling and sequential Block Acks, but lacks strict guards and timing/physical validation checks required for standard compliance.

The recommended approach focuses on auditing three key areas: ADDBA negotiation guards at the MAC layer, SIFS-spaced sequential Block Ack timing calculations, and independent path loss/SNR calculations at the PHY layer. By resolving these concerns, we ensure high-fidelity, standard-compliant wireless simulations.

## Key Findings

### Recommended Stack

We recommend adhering to the standard INET simulation environment:
- **Core technologies:**
  - OMNeT++ 6.3.0: Event loop and discrete-event scheduling engine.
  - C++17: Implementation of protocol state machines and calculations.
  - NED & MSG: Module connectivity and message definition layouts.

### Expected Features

**Must have (table stakes):**
- Strict ADDBA handshake verification before OFDMA scheduling.
- Precise SIFS timing and offset calculations to prevent channel collisions.
- Correct independent attenuation and noise figures per RU.

**Defer (v2+):**
- Multi-User Block Ack Requests (MU-BAR) and Uplink OFDMA.

### Architecture Approach

The architecture relies on the interaction between:
1. `HeHcf`: coordination function driving TXOP access.
2. `HeDlSchedulerEqualSizedRUs`: scheduling algorithms mapping queues to RUs.
3. `HeDlMuTxOpFs`: frame sequence handler coordinating BARs and Block Acks.
4. `Ieee80211Radio`: physical layer handling sub-channel power levels.

### Critical Pitfalls

1. **Missing ADDBA agreement check:** AP schedules packets to STAs before the Block Ack session is established.
2. **Timing drift in sequential Block Acks:** Incorrect spacing calculations leading to channel collisions.
3. **Path loss leakage:** Sub-channels using the main channel's center frequency for SNR calculations.

## Implications for Roadmap

Based on research, suggested phase structure:

### Phase 1: ADDBA Validation & Handshake Correctness
**Rationale:** Standard-compliant OFDMA requires Block Ack agreements. This phase ensures agreements are validated before scheduling.
**Delivers:** Strict ADDBA status check guards in the scheduler and frame sequence.
**Avoids:** Scheduling failures and dropped frames due to missing agreements.

### Phase 2: Sequential Block Ack Spacing & Timing Verification
**Rationale:** SIFS timing must be respected to prevent wireless packet collisions.
**Delivers:** Dynamic duration and spacing calculations based on computed rate modes.
**Avoids:** Collision on the channel during block ack replies.

### Phase 3: Physical Layer RU Behavior & Attenuation Auditing
**Rationale:** RUs behave as independent sub-channels. Frequency-selective path loss and noise must be correctly separated.
**Delivers:** Audited and corrected attenuation calculations in `Ieee80211Radio`.
**Avoids:** SNR calculations bleeding across adjacent RU sub-channels.

### Phase 4: Automated Testing & Example Verification
**Rationale:** Regression tests are needed to lock in correctness.
**Delivers:** Unit test coverage and validation scenarios checking MAC and PHY statistics.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | OMNeT++ 6.3.0 is a stable and standard platform. |
| Features | HIGH | Table stakes map directly to the 802.11ax standard requirements. |
| Architecture | HIGH | INET's modular coordinate function/frame sequence structure is well-defined. |
| Pitfalls | HIGH | Pitfalls represent direct failure modes seen in network simulations. |

**Overall confidence:** HIGH

---
*Research completed: 2026-06-16*
*Ready for roadmap: yes*
