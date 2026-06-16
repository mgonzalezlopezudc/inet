# Feature Research

**Domain:** Wireless Simulation / IEEE 802.11ax DL MU OFDMA Correctness
**Researched:** 2026-06-16
**Confidence:** HIGH

## Feature Landscape

### Table Stakes (Users Expect These)

Features users assume exist. Missing these = product feels incomplete.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| ADDBA Handshake Validation | Required by standard before QoS block acks can be sent. | MEDIUM | Ensure AP refuses to transmit OFDMA frames to STAs without active agreements. |
| Collision-Free Sequential Ack | Prevents stations from transmitting Block Acks simultaneously. | HIGH | Needs SIFS-based spacing and precise transmission duration offsets. |
| Independent RU PHY Calculations | Path loss, noise, and SNR must be computed per RU band. | HIGH | Abstract PHY layer must separate the main channel into independent bands. |
| MAC Scheduler RU Matching | Map packets to corresponding RUs based on dest STA addresses. | MEDIUM | AP scheduler extracts packets matching available RU capacities. |

### Differentiators (Competitive Advantage)

Features that set the product apart. Not required, but valuable.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Dynamic MCS Adaptation per RU | Maximizes throughput based on path loss of individual sub-channels. | HIGH | Requires rate selection module to support per-RU MCS estimation. |
| Multi-User BAR (MU-BAR) support | Reduces overhead by triggering all Block Acks in a single frame. | HIGH | Requires trigger frame structure and timing alignment. |

### Anti-Features (Commonly Requested, Often Problematic)

Features that seem good but create problems.

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Subcarrier-level fading model | Realistic frequency-selective fading. | Massive CPU overhead, slows down packet-level network simulation. | Abstract parallel RU sub-channels with separate flat-fading/noise variables. |
| Inter-AC Multi-User Scheduling | Transmit packets from different ACs in one TXOP. | Violates standard EDCA TXOP ownership rules, complex state machinery. | Aggregate packets only from the winning AC queue. |

## Feature Dependencies

```
[ADDBA Agreement]
    └──requires──> [EDCA Queue Access]
                        └──requires──> [MAC DL OFDMA Scheduler]
                                            └──requires──> [Sequential BAR & BA sequence]

[Independent RU PHY Calculations] ──enhances──> [MAC DL OFDMA Scheduler]
```

### Dependency Notes

- **ADDBA Agreement requires EDCA Queue Access:** Block Ack Agreements are negotiated on a per-TID (Traffic Identifier) basis, which maps to specific Access Category queues.
- **Sequential BAR & BA sequence requires MAC DL OFDMA Scheduler:** The AP scheduler must compute the total duration of the TXOP (including BAR and Block Ack frames) and set it in the PPDU duration field.

## MVP Definition

### Launch With (v1)

Minimum viable product — what's needed to validate the concept.

- [ ] ADDBA Check Guard — AP refuses DL MU OFDMA to STAs without active Block Ack agreements.
- [ ] Collision-Free Sequential BlockAck Timing — Precise SIFS offsets and transmission durations.
- [ ] PHY RU Separation Validation — Correct path loss and noise calculations per RU sub-channel.

### Add After Validation (v1.x)

Features to add once core is working.

- [ ] Rate selection feedback loop integration.

### Future Consideration (v2+)

Features to defer until product-market fit is established.

- [ ] Uplink Trigger-based OFDMA.
- [ ] Multi-User Block Ack Request (MU-BAR) frame support.

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| ADDBA Check Guard | HIGH | MEDIUM | P1 |
| Collision-Free Sequential Timing | HIGH | HIGH | P1 |
| Independent RU PHY Calculations | HIGH | HIGH | P1 |
| Dynamic MCS Adaptation per RU | MEDIUM | HIGH | P2 |

**Priority key:**
- P1: Must have for launch
- P2: Should have, add when possible
- P3: Nice to have, future consideration

## Competitor Feature Analysis

| Feature | Competitor A (ns-3) | Competitor B (INET prior) | Our Approach |
|---------|---------------------|---------------------------|--------------|
| DL OFDMA scheduling | Yes (very detailed) | Basic single-user only | Dynamic queue-based RU scheduling with sequential BAR. |
| Physical RU modeling | Spectrum channel models | Single flat channel | Abstract parallel sub-channels representing RUs. |

## Sources

- IEEE 802.11ax-2020 Standard Specifications.
- INET Framework physical layer documentation.

---
*Feature research for: 802.11ax DL MU OFDMA correctness*
*Researched: 2026-06-16*
