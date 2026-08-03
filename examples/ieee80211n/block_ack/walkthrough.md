# Walkthrough: 802.11n Block Acknowledgement

<!-- BEGIN SCRIPT RESULTS SESSIONS -->
`[script]` results sessions:

- Scalar/vector: `NOT RUN`
- PCAP: `NOT RUN`
<!-- END SCRIPT RESULTS SESSIONS -->

`[agent]` results sessions: `NOT RECORDED`.

This walkthrough demonstrates the IEEE 802.11n Block Acknowledgement (Block ACK) mechanisms, comparing legacy per-frame ACK policy against Immediate Block ACK, Compressed Block ACK bitmaps, and HT Implicit Block ACK.

## [agent] Learning objectives and feature primer

After completing this walkthrough, the reader can:

- Trace the ADDBA (Add Block ACK Request/Response) management exchange used to establish Block ACK agreements per TID.
- Understand how Compressed Block ACK bitmaps represent up to 64 MPDU sequence numbers in a compact 8-byte field.
- Explain the SIFS timing and overhead reduction when replacing multiple discrete ACKs with a single Block ACK frame.
- Compare throughput and delay performance across standard ACK and Block ACK configurations.

## [agent] Scenario description

The topology uses the common single-BSS network [`SingleBssNetwork`](../../ieee80211/common/SingleBssNetwork.ned). Traffic flows from `server` to `host[0]` at 0.5 ms packet intervals.

Four configurations are evaluated:

1. `StandardAck`: Legacy Stop-and-Wait ACK after each transmitted MPDU.
2. `ImmediateBlockAck`: ADDBA negotiated Block ACK with explicit BAR/BA exchange.
3. `CompressedBlockAck`: HT Compressed Block ACK bitmap optimization.
4. `HtImplicitBlockAck`: SIFS implicit Compressed Block ACK automatically returned after an A-MPDU burst without requiring an explicit BAR.

## [agent] Standards and INET model boundary

- **IEEE Std 802.11n-2009 / 802.11-2020 Clause 9.21 & 9.3.1.9**: Defines ADDBA management frames, BlockAckReq/BlockAck control structures, compressed bitmaps, and implicit BA rules.
- **INET Model Boundary**: ADDBA negotiation and Block ACK bitmap accounting are implemented in `inet::ieee80211::Hcf`.
