# 802.11ax DL MU OFDMA Correctness

## What This Is

This project focuses on verifying and ensuring the functional and physical correctness of the Downlink Multi-User Orthogonal Frequency Division Multiple Access (DL MU OFDMA) implementation in the INET Framework. It validates that standard-compliant IEEE 802.11ax frame sequences, block acknowledgment timings, and physical layer Resource Unit (RU) signal reception/noise calculations are accurately executed and collision-free.

## Core Value

Ensure high-fidelity, standard-compliant packet-level simulation of 802.11ax DL MU OFDMA scheduling, transmission, and reception by verifying both protocol state machines and physical sub-channel behavior.

## Requirements

### Validated

- ✓ Existing IEEE 802.11ax DL OFDMA MAC and PHY layers — existing codebase
- ✓ Abstract Resource Unit (RU) sub-channel band representation — existing codebase
- ✓ Queue-based multi-user packet scheduling at the Access Point (AP) — existing codebase
- ✓ HE MU PPDU structure with SIG-B allocation parsing at destination STAs — existing codebase
- ✓ Sequential Block Ack transmission after DL MU PPDU — existing codebase

### Active

- [ ] Ensure ADDBA handshake is correctly performed and maintained for all target STAs before sending any DL MU PPDUs.
- [ ] Verify SIFS spacing, BAR transmission, and sequential Block Ack timing offsets to prevent channel collisions and guarantee compliance.
- [ ] Verify that channel noise, path loss, and bit/packet error rate calculations are computed correctly and independently for each RU sub-channel band.
- [ ] Implement and run automated unit/integration tests to verify correct MAC frame sequences and PHY sub-channel calculations.

### Out of Scope

- Uplink OFDMA (UL OFDMA) — Excluded to focus on validating the stability and correctness of downlink multi-user scheduling.
- Multi-User Block Ack Requests (MU-BAR) — Excluded in favor of standard sequential Block Acks to keep implementation scope focused.
- Detailed subcarrier-level fading/interference simulation — Keep abstract parallel RU sub-channels as the physical representation.

## Context

- The INET Framework uses modular simple C++ components and NED compound modules for layering.
- As of June 2026, Downlink OFDMA support is fully integrated with complete PHY-level Resource Unit partitioning, dynamic multi-user MAC scheduling, SIG-B reception filtering, and collision-free sequential Block Ack sequences.
- This project will verify, audit, and fix any correctness issues, timing drift, or physical calculations in these existing components.

## Constraints

- **Language**: C++17, NED, and MSG message definitions.
- **Environment**: OMNeT++ 6.3.0 simulation engine.
- **Safety**: Safe casting must use `check_and_cast<T*>()`.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Validate both MAC and PHY | Standard frame sequences and physical RU calculations are tightly coupled and both must be correct for valid simulation. | — Pending |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-06-16 after initialization*
