# Selected O-QPSK PPDU representation

> **Kind:** design · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [phy-provider.md](phy-provider.md), [external-validation.md](external-validation.md)

The native packet-level radio already accounts for the 128-us preamble,
64-us SFD/PHR interval and complete PSDU exactly once. Its inherited transmission
class exposes lengths, but no encoded SHR/PHR view. This leaves catalog PHY-6/7's
basic encoding observation unproven even though durations pass.

The selected radio will use a dedicated transmitter producing an
Ieee802154StandardTransmission subclass. Its existing base fields remain the
sole timing/length authority. An immutable view derives the six PHY octets:
four zero preamble octets, SFD 0xa7 (13.1.2.3/Figure 13-3's time-ordered bits),
and a PHR with reserved bit zero and the actual PSDU byte count (13.1.3.2).
The complete PPDU byte view combines those octets with the actual serialized
PSDU, including its FCS. No parallel stored length or PHY-header state is added.

The radio/medium's existing packet body remains its PSDU. This preserves MAC
dissector, TAP and PCAP boundaries. PHY overhead is represented by the
transmission's encoded view and timed signal parts; it is not inserted into the
PSDU and charged twice. The existing receiver/error model accepts the derived
transmission through its unchanged base interface. Legacy narrowband/UWB use
their original transmitter and model. New source is in the unsealed PHY subtree.

The transmitter owns packet-to-signal construction and validates byte alignment,
PSDU bounds and the selected rate before producing its transmission. Packet and
analog-model ownership retain the existing medium/radio contracts. The proposed
external PHY fixture will inspect actual emitted transmission objects at lengths
2, 5, 18, 20 and 127 bytes, compare header/complete PPDU octets and durations,
and run the existing five-SNIR BER/PER oracle on those same objects. The full
typed/raw protocol suite must retain its actual MAC/ACK timing and filtering.
This adds packet-level PHY encoding visibility, not chip or pulse waveforms.

The same release increment tightens the new broadcast path: only a nonbroadcast
device identity can select a per-device Data Request extraction. A malicious
poll rewritten to all-ones source must not release queued broadcast DATA before
its beacon. It may receive the ordinary command ACK with Frame Pending clear.
Coordinator recognition also requires an actual extended/allocated-short source;
the absent-address sentinel is not a coordinator identity. Existing association
and poll inference retain their separate contexts. The Broadcast protocol
fixture will inject the source mutation with recomputed FCS and verify both the
empty poll and subsequent actual beacon release. This contract was checked
against receive filtering, canonical queue keys and the PHY factory before edit.
