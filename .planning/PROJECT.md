# 802.11ax DL OFDMA Support

## What This Is

This project implements Downlink Orthogonal Frequency Division Multiple Access (DL OFDMA) support for the IEEE 802.11ax (High Efficiency, HE) standard in the INET Framework. It introduces physical layer Resource Unit (RU) abstractions as independent sub-channels and a dynamic queue-based MAC scheduler at the Access Point (AP) to transmit multi-user frames concurrently to multiple associated stations.

## Core Value

Enable high-fidelity packet-level simulation of multi-user DL OFDMA scheduling and transmission under the 802.11ax standard, prioritizing robust queuing integration and realistic abstract PHY layer sub-channel behavior.

## Requirements

### Validated

- ✓ Existing L2 IEEE 802.11 stack support, including 802.11a/b/g/p/n/ac physical and MAC layers — existing codebase
- ✓ Single-user channel access coordination functions (DCF and EDCA/HCF) — existing codebase
- ✓ Queueing system with compound pending queues per Access Category (AC) — existing codebase
- ✓ Define IEEE 802.11ax (HE) physical modes and MCS tables in `Ieee80211ModeSet` and related files to enable the `"ax"` modeSet — Phase 1
- ✓ Implement an abstract physical layer Resource Unit (RU) model representing RUs as sub-channels with independent bandwidth, path loss, noise, and reception calculations — Phase 2
- ✓ Implement a DL OFDMA MAC scheduler at the Access Point (AP) — Phase 3
- ✓ Support dynamic queue-based RU scheduling: when an Access Category (AC) wins a TXOP, schedule packets from that winning AC's queue destined to up to N different stations (STAs) — Phase 3

### Active

- [ ] Support sequential multi-user acknowledgment (sequential Block Ack responses from the receiving STAs).
- [ ] Verify the DL OFDMA implementation with automated test runs and a new example simulation configuration.

### Out of Scope

- Uplink OFDMA (UL OFDMA) — Excluded to simplify coordination complexity and focus on downlink multi-user scheduling.
- Multi-User Block Ack Requests (MU-BAR) — Excluded in favor of standard sequential Block Acks to reduce frame formatting overhead and protocol complexity.
- Per-STA EDCA queuing system — Keep the current aggregate AC queue system to maintain compatibility with existing INET queuing modules.
- Detailed subcarrier-level fading/interference simulation — Excluded in favor of abstract sub-channels (RUs) for simulation performance and styling consistency.

## Context

- The INET Framework uses modular simple C++ components and NED compound modules for layering.
- The 802.11 MAC implementation is heavily decomposed, utilizing coordination functions (like `Hcf`), channel access modules (like `Edcaf`), and data services (like `OriginatorQosMacDataService`).
- Packets are passed as `inet::Packet` objects containing dynamic headers.

## Constraints

- **Language**: C++17, NED, and MSG message definitions.
- **Environment**: OMNeT++ 6.3.0 simulation engine.
- **Safety**: Safe casting must use `check_and_cast<T*>()`.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Downlink OFDMA only with sequential Ack | Simplifies multi-user coordination and avoids complex trigger-frame handshake logic. | — Pending |
| Abstract RU sub-channel model | Represents resource partition effectively without the massive computational overhead of subcarrier-level modeling. | — Pending |
| Winning AC queue aggregation only | Aligns with standard EDCA TXOP ownership rules and avoids inter-AC scheduling complexity. | — Pending |
| Aggregate AC queues (current system) | Reuses the existing queue implementation instead of refactoring it to a per-STA queue system. | — Pending |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd:complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-06-14 after initialization*
