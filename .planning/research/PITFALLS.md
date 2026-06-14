# 802.11ax DL OFDMA Pitfalls Research

## Common Mistakes & Mitigation Strategies

### 1. Carrier Sensing & Contention on Sub-channels
- **Pitfall**: STAs performing virtual or physical carrier sensing on individual RUs instead of the main primary channel, leading to incorrect channel state assessments.
- **Mitigation**: Ensure that primary channel carrier sensing (CCA) is performed at the main bandwidth level (e.g., 20 MHz) before TXOP selection. The sub-channels (RUs) are only active once the AP has won the channel and transmits the MU PPDU.

### 2. Overlapping Resource Unit (RU) Interference
- **Pitfall**: When multiple STAs are assigned to parallel RUs, interference calculation might cross-pollute or fail to isolate the signals, leading to degraded packet reception success.
- **Mitigation**: The physical layer model must treat each RU as an independent sub-channel frequency band, ensuring that path loss and noise calculations are computed separately for each STA's RU band.

### 3. Sequential Acknowledgment Timing
- **Pitfall**: Multiple STAs transmitting Block Ack frames concurrently after a DL OFDMA frame, leading to collisions.
- **Mitigation**: Follow the sequential Ack procedure where the AP polls or schedules each STA's Ack sequentially (e.g., using SIFS offsets or explicit poll frames) so that only one STA transmits its Block Ack at any given time.

### 4. Coexistence with Legacy Nodes (802.11a/g/n/ac)
- **Pitfall**: Legacy nodes cannot parse HE-specific fields (HE-SIG-A/B) and may interrupt the transmission.
- **Mitigation**: Use the standard mixed-format preamble (L-STF, L-LTF, L-SIG) preceding the HE-specific fields. This ensures legacy nodes can parse the L-SIG duration field and set their NAV (Network Allocation Vector) accordingly.
