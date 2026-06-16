# Phase 1: ADDBA Validation & Handshake Correctness - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-06-16
**Phase:** 1-ADDBA Validation & Handshake Correctness
**Areas discussed:** Single-user fallback strategy, Packet-level agreement validation, Dynamic agreement state changes

---

## Single-user fallback strategy

| Option | Description | Selected |
|--------|-------------|----------|
| Transmit via SU EDCA + background ADDBA | Transmit via single-user EDCA and initiate ADDBA handshake in background (ensures immediate delivery and prepares for future OFDMA). | ✓ |
| Transmit via SU EDCA (no ADDBA) | Transmit via single-user EDCA without initiating ADDBA (relies on external/upper-layer setup to establish agreements). | |
| Defer transmission | Defer transmission, initiate ADDBA handshake immediately, and transmit once agreement is active. | |

**User's choice:** Transmit via SU EDCA + background ADDBA (Option 1)
**Notes:** Ensures immediate packet delivery and prepares the STA for future DL MU OFDMA transmissions.

### Sub-question: Handshake Failure Handling

| Option | Description | Selected |
|--------|-------------|----------|
| Retry limit + cooldown | Retry ADDBA request up to a limit (e.g., 3 times), then apply a cooldown period before retrying (prevents handshake flood). | ✓ |
| Retry indefinitely | Retry ADDBA request indefinitely on every new packet arrival. | |
| Permanent fallback | Permanently disable OFDMA for this STA after a single handshake failure (no further ADDBA requests). | |

**User's choice:** Retry limit + cooldown (Option 1)

### Sub-question: ADDBA Request Transmission Mode

| Option | Description | Selected |
|--------|-------------|----------|
| SU EDCA for Requests | Transmit ADDBA Requests via standard single-user EDCA (highly standard-compliant and keeps coordination logic clean). | ✓ |
| Bundle inside MU | Attempt to bundle ADDBA Requests inside a DL MU OFDMA frame (complex, but saves channel time). | |

**User's choice:** SU EDCA for Requests (Option 1)

### Sub-question: Access Category Selection

| Option | Description | Selected |
|--------|-------------|----------|
| Re-use AC_VO | Re-use existing INET behavior where all Management frames (including ADDBA Requests) use AC_VO (ensures fast delivery). | ✓ |
| Map to data AC | Map the ADDBA Request to the same Access Category (AC) as the data frame that triggered it. | |

**User's choice:** Re-use AC_VO (Option 1)

---

## Packet-level agreement validation

| Option | Description | Selected |
|--------|-------------|----------|
| Validate STA & TID strictly | Validate both STA address and Traffic Identifier (TID) strictly. Skip packets whose TID does not have an active Block Ack agreement (leaving them in the queue for single-user transmission). | ✓ |
| Validate STA only | Validate only the destination STA address. If the STA has any active agreement, schedule the packet regardless of TID. | |
| Mixed packets in MU container | Allow mixed packets in the container; if a packet lacks an agreement for its TID, still schedule it but fallback to standard ACK. | |

**User's choice:** Validate STA & TID strictly (Option 1)

### Sub-question: Candidate Selection Validation

| Option | Description | Selected |
|--------|-------------|----------|
| Head-of-line only | Validate only the head-of-line (first) packet for each destination in the queue. If it lacks an active agreement for its TID, that destination is ineligible for DL MU scheduling this round (preserves strict FIFO queue order). | ✓ |
| Full queue scan | Scan the entire queue to select only packets that have active Block Ack agreements, bypassing non-conforming head-of-line packets. | |

**User's choice:** Head-of-line only (Option 1)

### Sub-question: Skipped Packet Fate

| Option | Description | Selected |
|--------|-------------|----------|
| Keep in queue | Leave non-conforming packets in the queue (they will be sent via single-user fallback or subsequent MU opportunities). | ✓ |
| Drop packet | Drop non-conforming packets immediately and record a simulation drop event. | |

**User's choice:** Keep in queue (Option 1)

### Sub-question: Diagnostic Logging

| Option | Description | Selected |
|--------|-------------|----------|
| EV_WARN diagnostic log | Log a detailed warning (EV_WARN) indicating the packet was skipped and the reason (TID/agreement mismatch). | ✓ |
| Silent skip | Skip silently without logging to keep the console clean. | |

**User's choice:** EV_WARN diagnostic log (Option 1)

---

## Dynamic agreement state changes

| Option | Description | Selected |
|--------|-------------|----------|
| Immediate fallback | Disable DL MU OFDMA scheduling immediately for the affected STA and TID. Any currently queued packets for that TID will fall back to single-user EDCA transmission. | ✓ |
| Complete active TXOP | Allow any currently active TXOP or frame sequence to complete first before disabling the agreement for scheduling. | |

**User's choice:** Immediate fallback (Option 1)

### Sub-question: Coordination Function State Tracking

| Option | Description | Selected |
|--------|-------------|----------|
| Dynamic query | Continue using the dynamic query approach (querying the Block Ack handler at the start of each TXOP is robust, simple, and self-correcting). | ✓ |
| Cache candidates | Cache the candidate list and update it via OMNeT++ signals (blockAckAgreementAdded/Deleted). | |

**User's choice:** Dynamic query (Option 1)

### Sub-question: Mid-Sequence Teardown Handling

| Option | Description | Selected |
|--------|-------------|----------|
| Complete current sequence | Let the currently running sequence complete normally. Changes in agreement state apply to subsequent frame sequences only (prevents mid-flight timing issues and channel collisions). | ✓ |
| Abort mid-flight | Attempt to abort the sequence mid-flight. | |

**User's choice:** Complete current sequence (Option 1)

### Sub-question: Channel Contention Trigger

| Option | Description | Selected |
|--------|-------------|----------|
| Immediate trigger | Immediately trigger channel access / contention check when an agreement is established if there are pending packets (ensures prompt transmission). | ✓ |
| Wait for packet / periodic | Wait for subsequent packet arrivals or periodic checks to trigger channel contention. | |

**User's choice:** Immediate trigger (Option 1)

---

## the agent's Discretion

- Cooldown duration for ADDBA handshake failures.
- Log formatting for skipped packets.

## Deferred Ideas

None.

---

*Phase: 01-addba-validation-handshake-correctness*
*Discussion log generated: 2026-06-16*
