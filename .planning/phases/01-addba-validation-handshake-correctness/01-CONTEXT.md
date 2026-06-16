# Phase 1: ADDBA Validation & Handshake Correctness - Context

**Gathered:** 2026-06-16
**Status:** Ready for planning

<domain>
## Phase Boundary

Verify and enforce Block Ack agreements before scheduling DL MU OFDMA frames. The AP coordination function must fall back to standard single-user EDCA transmission for STAs that do not have active Block Ack agreements.

</domain>

<decisions>
## Implementation Decisions

### Single-user fallback strategy
- **D-01:** When a packet is destined for a STA lacking an active Block Ack agreement, the AP will transmit it via standard single-user EDCA transmission and initiate an ADDBA handshake sequence (ADDBA Request) in the background. This ensures immediate packet delivery and prepares the STA for future DL MU OFDMA transmissions.
- **D-02:** If the ADDBA handshake fails or times out, the AP will retry the ADDBA request up to a limit of 3 times, after which it will apply a cooldown period before retrying to prevent handshake floods.
- **D-03:** ADDBA Request frames will be scheduled and transmitted via standard single-user EDCA (keeping coordination logic clean and avoiding complex management scheduling in OFDMA).
- **D-04:** ADDBA Request frames will use the AC_VO Access Category to ensure fast delivery, matching the existing INET behavior.

### Packet-level agreement validation
- **D-05:** During DL MU OFDMA container assembly in `HeDlMuTxOpFs::buildMuContainerPacket`, the AP will strictly validate both the destination STA address and the Traffic Identifier (TID). Packets whose TID does not have an active Block Ack agreement will be skipped and left in the pending queue for subsequent single-user or multi-user transmission.
- **D-06:** During candidate station collection in `HeHcf::collectCandidateStations`, the AP will validate only the head-of-line (first) packet for each destination. If it lacks an active agreement for its TID, that destination is ineligible for DL MU scheduling this round, preserving strict FIFO queue order.
- **D-07:** When a packet is skipped during container assembly due to a missing Block Ack agreement, a detailed diagnostic warning (`EV_WARN`) will be logged indicating the skipped packet, the destination STA, the TID, and the reason.

### Dynamic agreement state changes
- **D-08:** When a Block Ack agreement is torn down mid-simulation (e.g. DELBA frame or inactivity timeout), the AP will immediately disable DL MU OFDMA scheduling for the affected STA and TID. Currently queued packets for that TID will fall back to single-user EDCA.
- **D-09:** `HeHcf` will continue to dynamically query the Block Ack agreement handler at the start of each TXOP rather than caching candidates, ensuring a robust, simple, and self-correcting candidates list.
- **D-10:** If a Block Ack agreement is torn down while a DL MU frame sequence is actively in progress, the active sequence will be allowed to complete normally. State changes will apply only to subsequent frame sequences.
- **D-11:** When a new Block Ack agreement is successfully established, if there are pending packets in the queue, the AP will immediately trigger channel contention/access checks to ensure prompt transmission.

### the agent's Discretion
- The exact value of the cooldown period after ADDBA handshake failures.
- The precise formatting of the warning logs.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### INET MAC Coordination and Frame Sequences
- `src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc` — Dynamic collection of candidate stations and initiation of MU frame sequences.
- `src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc` — DL MU OFDMA frame sequence coordination, packet assembly, and sequential ACK spacing calculation.
- `src/inet/linklayer/ieee80211/mac/scheduler/HeDlSchedulerEqualSizedRUs.cc` — DL MU OFDMA scheduler assigning resource units to candidate STAs.
- `src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc` — Recipient management frame processing (ADDBA and DELBA handling).

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `getOriginatorBlockAckAgreementHandler()->getAgreement(dest, tid)`: Retrieves the Block Ack agreement for a specific destination and TID.
- `agreement->getIsAddbaResponseReceived()`: Checks if the ADDBA response has been received (active agreement).
- `Hcf::startFrameSequence(ac)`: Initiates single-user transmission sequence.

### Established Patterns
- Head-of-line checking in candidate collection using `pendingQueue->getPacket(i)`.
- Re-queueing or dropping packets inside EDCA coordination functions.

### Integration Points
- `HeHcf::collectCandidateStations` — Entry point for filtering candidate STAs.
- `HeDlMuTxOpFs::buildMuContainerPacket` — Location where individual queued packets are fetched and validated before inserting into the HE-MU-PPDU container.
- `Hcf::recipientProcessReceivedManagementFrame` — Receives ADDBA/DELBA management frames and updates agreement state.

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 01-addba-validation-handshake-correctness*
*Context gathered: 2026-06-16*
