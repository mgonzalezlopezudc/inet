# Requirements: 802.11ax DL MU OFDMA Correctness

**Defined:** 2026-06-16
**Core Value:** Ensure high-fidelity, standard-compliant packet-level simulation of 802.11ax DL MU OFDMA scheduling, transmission, and reception by verifying both protocol state machines and physical sub-channel behavior.

## v1 Requirements

Requirements for initial release. Each maps to roadmap phases.

### MAC Protocol & ADDBA Validation

- [x] **MAC-01**: AP MAC Coordination function checks if an active Block Ack agreement with a received ADDBA Response exists for all destination STAs before scheduling multi-user OFDMA transmissions.
- [ ] **MAC-02**: AP MAC Coordination function falls back to standard single-user EDCA transmission for STAs that do not have an active Block Ack agreement.

### MAC Timing & Spacing Verification

- [ ] **TIM-01**: AP calculates the precise duration field value of the multi-user container packet to fully cover the expected sequential BAR and Block Ack responses.
- [ ] **TIM-02**: Spacing between consecutive Block Ack responses from receiving STAs is exactly SIFS, avoiding any overlapping transmissions.

### Physical Layer RU Behavior Auditing

- [ ] **PHY-01**: Signals on separate Resource Units (RUs) are attenuated and received based on frequency-selective path loss of their corresponding sub-channel band, rather than the main channel.
- [ ] **PHY-02**: Channel noise is calculated independently per RU sub-channel band.

### Automated Testing & Verification

- [ ] **TST-01**: Implement automated unit/integration tests that verify protocol frame sequences (ADDBA checks, BARs, sequential BAs) under varying traffic loads.
- [ ] **TST-02**: Provide a validation simulation example and assert stats match standard-compliant timings and zero collisions.

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### MAC Enhancements

- **MAC-03**: Dynamic MCS adaptation based on individual RU path loss feedback.
- **MAC-04**: Multi-User Block Ack Request (MU-BAR) frame support.

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| Uplink OFDMA (UL OFDMA) | Focus is on validating downlink multi-user scheduling and reception. |
| Subcarrier-level fading/interference model | Excluded for simulation execution speed, using abstract flat-fading parallel sub-channels instead. |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| MAC-01 | Phase 1 | Complete |
| MAC-02 | Phase 1 | Pending |
| TIM-01 | Phase 2 | Pending |
| TIM-02 | Phase 2 | Pending |
| PHY-01 | Phase 3 | Pending |
| PHY-02 | Phase 3 | Pending |
| TST-01 | Phase 4 | Pending |
| TST-02 | Phase 4 | Pending |

**Coverage:**

- v1 requirements: 8 total
- Mapped to phases: 8
- Unmapped: 0 ✓

---
*Requirements defined: 2026-06-16*
*Last updated: 2026-06-16 after initial definition*
