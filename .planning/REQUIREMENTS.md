# Requirements: 802.11ax DL OFDMA Support

**Defined:** 2026-06-14
**Core Value:** Enable high-fidelity packet-level simulation of multi-user DL OFDMA scheduling and transmission under the 802.11ax standard, prioritizing robust queuing integration and realistic abstract PHY layer sub-channel behavior.

## v1 Requirements

Requirements for the initial release. Each maps to roadmap phases.

### High Efficiency (HE) Mode Enablement

- [ ] **MODE-01**: Access Point (AP) and Station (STA) MAC modules support the `"ax"` modeSet parameter in NED configuration.
- [ ] **MODE-02**: `Ieee80211ModeSet` registers HE-specific timing parameters (SIFS = 16µs, Slot Time = 9µs, CWmin = 15, CWmax = 1023).
- [ ] **MODE-03**: Support standard 802.11ax MCS values (MCS 0 to 11) for 20 MHz channels under the new HE mode.

### MAC Layer Multi-User (MU) Scheduling

- [ ] **SCHED-01**: Implement a DL OFDMA MAC scheduler class in the AP to handle Resource Unit (RU) user mappings.
- [ ] **SCHED-02**: When an Access Category (AC) wins channel access, the scheduler extracts packets from that winning AC's queue.
- [ ] **SCHED-03**: The scheduler groups extracted packets by destination address, selecting up to N distinct destination STAs for concurrent transmission.
- [ ] **SCHED-04**: Build a multi-user C++ physical frame metadata structure containing the assigned RU mapping (HE-SIG-B emulation).

### PHY Layer Resource Unit (RU) Sub-channels

- [ ] **PHY-01**: Represent Resource Units (RUs) as independent parallel frequency sub-channels on the radio medium.
- [ ] **PHY-02**: Compute path loss, noise, and signal-to-noise ratio (SNR) independently for each STA's assigned RU sub-channel.
- [ ] **PHY-03**: Destination STAs decode only the C++ physical frame chunks belonging to their assigned RU sub-channel, ignoring other concurrent RU transmissions.

### Multi-User Acknowledgments

- [ ] **ACK-01**: Receiving STAs construct Block Ack responses after successfully decoding their assigned RU payload.
- [ ] **ACK-02**: Implement sequential Block Ack transmissions back to the AP, separated by SIFS timing intervals, to prevent collision on the channel.

### Verification and Testing

- [ ] **TEST-01**: Create a set of automated test cases in the INET test suite validating correct DL OFDMA packet reception and sequential acknowledgment sequence.
- [ ] **TEST-02**: Provide an example simulation scenario in the `examples/` directory configuring an AP and multiple STAs using the 802.11ax DL OFDMA MAC scheduler.

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### Uplink OFDMA

- **UL-01**: Multi-user Uplink transmission coordinated by AP trigger frames.
- **UL-02**: Trigger frame generation and parsing in MAC.

### Multi-User Block Ack Requests (MU-BAR)

- **MBAR-01**: Support multi-user Block Ack Request frame formatting to request sequential block acknowledgments in a single frame.

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| Per-STA EDCA queuing | Aggregate queue per AC is kept to maintain compatibility and minimize restructuring overhead. |
| Subcarrier-level fading/interference | Excluded in favor of abstract sub-channels (RUs) to preserve simulation performance. |
| Trigger-based random access | Excluded as it is only applicable for Uplink OFDMA. |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|

**Coverage:**
- v1 requirements: 12 total
- Mapped to phases: 0
- Unmapped: 12 ⚠️

---
*Requirements defined: 2026-06-14*
*Last updated: 2026-06-14 after initial definition*
