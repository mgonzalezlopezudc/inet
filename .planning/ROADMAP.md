# Roadmap: 802.11ax DL MU OFDMA Correctness

## Overview

This roadmap defines the verification and development phases to ensure standard-compliant behavior of Downlink Multi-User OFDMA in the INET Framework. Using a Vertical MVP approach, each phase delivers a fully testable slice of protocol correctness, addressing ADDBA verification, sequential ack timing, physical layer sub-channel modeling, and automated regression testing.

## Phases

- [ ] **Phase 1: ADDBA Validation & Handshake Correctness** - Ensure AP refuses OFDMA when agreements are missing.
- [x] **Phase 2: Sequential Block Ack Spacing & Timing Verification** - Verify precise SIFS spacing and duration calculations.
- [x] **Phase 3: PHY Layer RU Behavior & Attenuation Auditing** - Audit path loss and independent sub-channel calculations.
- [ ] **Phase 4: Automated Testing & Example Verification** - Run simulation validation scenarios and unit tests.

## Phase Details

### Phase 1: ADDBA Validation & Handshake Correctness

**Goal**: Verify and enforce Block Ack agreements before scheduling DL MU OFDMA frames.
**Mode**: mvp
**Depends on**: Nothing
**Requirements**: MAC-01, MAC-02
**Success Criteria**:

  1. AP checks for active Block Ack agreements before scheduling packets to STAs.
  2. Fallback to standard single-user transmission works seamlessly for STAs lacking active agreements.

**Plans**: 2 plans
Plans:
**Wave 1**

- [x] 01-01-PLAN.md - Strict active Block Ack admission for `HeDlMuTxOpFs` container creation.

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 01-02-PLAN.md - FIFO single-user fallback, ADDBA retry cooldown, and prompt contention after agreements.

### Phase 2: Sequential Block Ack Spacing & Timing Verification

**Goal**: Enforce correct SIFS-spaced sequential block ack scheduling and duration calculations.
**Mode**: mvp
**Depends on**: Phase 1
**Requirements**: TIM-01, TIM-02
**Success Criteria**:

  1. The container frame's duration field correctly protects the full sequential exchange.
  2. SIFS timing between sequential Block Acks is respected, preventing channel collisions.

**Plans**: 2 plans

Plans:

- [x] 02-01: Implement dynamic duration calculations using `IQosRateSelection` in `HeDlMuTxOpFs`.
- [x] 02-02: Verify sequential Block Ack transmission offsets at STAs.

### Phase 3: PHY Layer RU Behavior & Attenuation Auditing

**Goal**: Audit and verify path loss and noise calculations per RU sub-channel band.
**Mode**: mvp
**Depends on**: Phase 2
**Requirements**: PHY-01, PHY-02
**Success Criteria**:

  1. Sub-channel noise is computed independently per RU band.
  2. Frequency-selective path loss is correctly applied per sub-channel.

**Plans**: 1 plan

Plans:

- [ ] 03-01: Audit RU geometry, per-RU attenuation, and noise isolation in `Ieee80211RadioMedium` with dedicated RU regression tests.

### Phase 4: Automated Testing & Example Verification

**Goal**: Implement unit/integration tests and run simulation examples to assert correctness.
**Mode**: mvp
**Depends on**: Phase 3
**Requirements**: TST-01, TST-02
**Success Criteria**:

  1. All automated tests in `tests/` compile and pass.
  2. Simulation examples run without collisions or spacing drift.

**Plans**: 2 plans

Plans:

- [x] 04-01: Create unit and integration test scripts.
- [ ] 04-02: Build and run validation scenarios in `examples/`.

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. ADDBA Validation | 2/2 | Completed | 2026-06-16 |
| 2. Timing Verification | 2/2 | Completed | 2026-06-17 |
| 3. PHY RU Auditing | 1/1 | Completed | 2026-06-17 |
| 4. Testing & Verification | 1/2 | In Progress|  |
