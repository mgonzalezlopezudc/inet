# Pitfalls Research

**Domain:** Wireless Simulation / IEEE 802.11ax DL MU OFDMA Correctness
**Researched:** 2026-06-16
**Confidence:** HIGH

## Critical Pitfalls

### Pitfall 1: Missing ADDBA Handshake check
**What goes wrong:**
AP schedules multi-user data packets on RUs, but no active Block Ack Agreement exists for the destination STA. The STA receives the frame but cannot process or acknowledge it correctly, leading to simulation mismatches or packet drops.

**Why it happens:**
Developers assume that any queued QoS packet can be scheduled via OFDMA, forgetting that standard IEEE 802.11ax requires an established Block Ack agreement before transmitting block-acknowledged data.

**How to avoid:**
Add a strict guard in the scheduler/frame-sequence initiator to check `originatorBAHandler->getAgreement(addr, tid)` and verify that `agreement->getIsAddbaResponseReceived()` is true. If not, fallback to single-user transmission.

**Warning signs:**
High packet drop rates, STAs failing to respond to BlockAckRequests, or protocol state errors during execution.

**Phase to address:**
Phase 1 (Validation of ADDBA state)

---

### Pitfall 2: Sequential Block Ack Spacing Drift
**What goes wrong:**
Collisions or timing gaps occur on the wireless medium during the sequential Block Ack phase.

**Why it happens:**
The AP computes the duration field or scheduled SIFS offsets using static frame sizes or incorrect MCS rates (e.g. not matching the rates computed by `rateSelection->computeResponseBlockAckFrameMode()`).

**How to avoid:**
Use the active `IQosRateSelection` submodule to dynamically calculate response block ack durations for each destination STA. Ensure SIFS and slot timing values are queried from the exact active `Ieee80211ModeSet` configuration.

**Warning signs:**
Channel overlaps or unexpected backoffs in the event logs.

**Phase to address:**
Phase 2 (Timing and spacing validation)

---

### Pitfall 3: Path Loss Leakage Across RUs
**What goes wrong:**
STAs receive and decode signals on an RU using the signal-to-noise ratio (SNR) or attenuation of the overall channel rather than the sub-channel band.

**Why it happens:**
The physical layer receiver fails to extract the specific sub-channel frequency band from `Ieee80211HeMuTag`, falling back to the default channel center frequency.

**How to avoid:**
Ensure that when a multi-user signal is processed, the path loss and noise calculations are indexed by the specific RU sub-channel band.

**Warning signs:**
Incorrect reception power or identical SNR values across different RUs despite frequency-selective noise.

**Phase to address:**
Phase 3 (Physical layer RU behavior check)

---

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Static Ack Duration | Speeds up frame sequence coding. | Breaks when MCS rates change or when packet sizes vary. | Never; standard requires dynamic rate-based duration. |
| Single-frequency noise modeling | Simplifies radio medium coding. | Fails to model realistic sub-channel frequency-selective interference. | Initial prototype only. |

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| ADDBA Missing Agreement | LOW | Halt transmission of multi-user container, trigger single-user ADDBA handshake sequence, and re-enqueue packet. |
| Timing Drift | MEDIUM | Re-calculate frame duration using `computeResponseBlockAckFrameMode` with correct rates. |

---
*Pitfalls research for: 802.11ax DL MU OFDMA correctness*
*Researched: 2026-06-16*
