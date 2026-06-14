# Roadmap: 802.11ax DL OFDMA Support

## Overview

This roadmap lays out the development phases to implement Downlink OFDMA support in the INET Framework under the IEEE 802.11ax standard. Using a Vertical MVP approach, we will progressively add functional slices—starting with basic HE mode enablement, followed by physical sub-channel partition, MAC multi-user scheduling, user payload reception, sequential Block Acks, and ending with system-level examples and automated testing.

## Phases

- [x] **Phase 1: High Efficiency (HE) Mode Enablement** - Define 802.11ax HE timing parameters, slot times, and MCS tables.
- [x] **Phase 2: PHY Resource Unit (RU) Sub-channel Representation** - Partition the channel bandwidth into abstract RU frequency sub-channels.
- [x] **Phase 3: AP MAC DL OFDMA Scheduler Foundation** - Implement user/RU mapping and packet aggregation in the AP MAC scheduler.
- [x] **Phase 4: Multi-User Reception at STAs** - Support parsing of SIG-B user metadata and payload extraction on assigned RUs at STAs.
- [x] **Phase 5: Sequential Block Acknowledgments** - Implement collision-free sequential Block Ack sequences separated by SIFS spacing.
- [ ] **Phase 6: System Integration and Verification** - Provide a complete DL OFDMA example simulation and automated regression tests.

## Phase Details

### Phase 1: High Efficiency (HE) Mode Enablement
**Goal**: Enable basic 802.11ax HE physical modes, MCS tables, and slot/SIFS timing configurations.
**Mode**: mvp
**Depends on**: Nothing
**Requirements**: MODE-01, MODE-02, MODE-03
**Success Criteria**:
  1. MAC/NIC simple modules parse the new `"ax"` modeSet parameter.
  2. Timing parameters for 802.11ax (SIFS = 16µs, Slot = 9µs) are verified and retrievable in C++.
  3. HE MCS levels (0-11) can be successfully selected during initialization.
**Plans**: 2 plans

Plans:
- [x] 01-01: Update NED parameter definitions and C++ parsing for `"ax"` modeSet.
- [x] 01-02: Implement HE timing parameters and HE MCS tables in `Ieee80211ModeSet` and `Ieee80211AxMode` logic.

### Phase 2: PHY Resource Unit (RU) Sub-channel Representation
**Goal**: Partition the channel bandwidth into abstract parallel RU frequency sub-channels on the radio medium.
**Mode**: mvp
**Depends on**: Phase 1
**Requirements**: PHY-01, PHY-02
**Success Criteria**:
  1. Transmissions can be split into multiple parallel Resource Unit (RU) bands.
  2. Noise and path loss calculations are computed independently for each RU sub-channel band.
**Plans**: 2 plans

Plans:
- [x] 02-01: Define RU sub-channel structures and frequency partitioning rules.
- [x] 02-02: Integrate sub-channel noise and path loss calculation on the radio medium.

### Phase 3: AP MAC DL OFDMA Scheduler Foundation
**Goal**: Implement the multi-user scheduling logic at the AP to aggregate packets from the winning EDCAF queue.
**Mode**: mvp
**Depends on**: Phase 2
**Requirements**: SCHED-01, SCHED-02, SCHED-03
**Success Criteria**:
  1. The new DL OFDMA scheduler class compiles and links inside the MAC layer.
  2. When the winning AC queue contains packets for multiple STAs, the scheduler extracts up to N packets for concurrent transmission.
**Plans**: 2 plans

Plans:
- [x] 03-01: Implement the basic DL OFDMA Scheduler class and queue inspection logic.
- [x] 03-02: Map extracted packets to assigned RUs based on destination addresses.

### Phase 4: Multi-User Reception at STAs
**Goal**: Support parsing multi-user SIG-B metadata and decoding assigned RU payload at destination STAs.
**Mode**: mvp
**Depends on**: Phase 3
**Requirements**: SCHED-04, PHY-03
**Success Criteria**:
  1. Destination STAs correctly parse the HE MU PPDU header containing RU allocations.
  2. STAs decode only their assigned RU sub-channel payload and discard others.
**Plans**: 2 plans

Plans:
- [x] 04-01: Define HE MU PPDU header layout and SIG-B allocation fields.
- [x] 04-02: Update the receiver (`Rx`) logic at STAs to filter and decode assigned RU sub-channels.

### Phase 5: Sequential Block Acknowledgments
**Goal**: Coordinate sequential Block Ack responses from STAs with proper SIFS spacing to prevent collisions.
**Mode**: mvp
**Depends on**: Phase 4
**Requirements**: ACK-01, ACK-02
**Success Criteria**:
  1. Receiving STAs send Block Ack frames back to the AP in sequential order.
  2. The transmission offset of each STA prevents overlaps on the medium.
**Plans**: 2 plans

Plans:
- [x] 05-01: Implement sequential Ack timing offset calculations at receiving STAs.
- [x] 05-02: Add MAC recovery and retry handling for sequential Block Acks at the AP.

### Phase 6: System Integration and Verification
**Goal**: Finalize test coverage and provide a working example simulation of DL OFDMA.
**Mode**: mvp
**Depends on**: Phase 5
**Requirements**: TEST-01, TEST-02
**Success Criteria**:
  1. A multi-user DL OFDMA simulation runs to completion using a new configuration in `examples/`.
  2. All newly added unit and integration tests execute and pass successfully.
**Plans**: 2 plans

Plans:
- [ ] 06-01: Build and configure a DL OFDMA simulation example in `examples/ieee80211/ofdma/`.
- [ ] 06-02: Implement and run automated unit tests in `tests/` to verify correctness.

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5 → 6

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. High Efficiency (HE) Mode Enablement | 2/2 | Complete | 2026-06-14 |
| 2. PHY Resource Unit (RU) Sub-channel Representation | 2/2 | Complete | 2026-06-14 |
| 3. AP MAC DL OFDMA Scheduler Foundation | 2/2 | Complete | 2026-06-14 |
| 4. Multi-User Reception at STAs | 2/2 | Complete | 2026-06-14 |
| 5. Sequential Block Acknowledgments | 2/2 | Complete | 2026-06-14 |
| 6. System Integration and Verification | 0/2 | Not started | - |
