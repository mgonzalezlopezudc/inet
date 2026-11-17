# IEEE 802.15.4 — bounded frame codec

> **Kind:** design · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [applicability.md](applicability.md), [integration.md](integration.md)

Steps 2/3 introduced `Ieee802154Frame` and its registered serializer independently of the old
`Ieee802154MacHeader`. One chunk represents the complete MPDU. The native addresses retain
all 16/64 bits; field presence follows IEEE Std 802.15.4-2024, 7.2.2.6 (physical PDF p. 80,
`ieee802154-2024@367030:368561`). Legacy versions 0/1 use the DSN and optional destination
PAN/address then optional source PAN/address. Compression requires both addresses and equal
PAN IDs. Version2 instead uses Table7-2's presence matrix and supports optional sequence
and bounded Header/Payload/Nested IE framing. Its detailed wire, protected-content and
operational limits are owned by the [enhanced profile](enhanced-frame-profile.md); legacy
offsets are never applied to a version2 frame. Reserved
address mode/version and illegal legacy suppression/IE combinations are malformed. Reserved
FCF bit 7 is ignored on reception and retained for forensic reserialization (4.6). For secured
legacy data the common addressing is decoded while the protected remainder stays opaque; the
security-policy early return precedes any plaintext interpretation. Selected Beacon and
Command formats are implemented under the managed and enhanced profiles. Security
transformation remains unsupported; structural auxiliary parsing does not enable cryptography.

The codec owns little-endian integers, lengths and the selected two-octet FCS. It never stores
an upper protocol discriminator in a PAN field. CRC follows 7.2.11 (physical PDF pp. 84–85,
`ieee802154-2024@380294:384027`), initial remainder zero, reflected polynomial 0x8408,
no final XOR. Its independent ACK example is bytes `02 00 6a` with FCS `e4 79`; these are
the source's time-ordered bit strings converted into LSB-first octets. FCS covers MHR and
payload once. A decoded FCS is preserved even when invalid; absent FCS is a distinct input
mode and is never fabricated. Oversized O-QPSK PSDUs are rejected. Decode results distinguish
truncation, malformed and unsupported inputs without assertions on untrusted bytes.

The generated frame declaration, codec and registered serializer live under
`linklayer/ieee802154`, outside the sealed packet subsystem. The decoder bounds-checks before
each read, retains raw bytes for unsupported/malformed input, and exposes parse status separately
from FCS validity. Transmit callers finalize length before inserting the chunk. The production
dissector recognizes the new frame, reports fields and keeps unknown payload opaque; a typed
legacy chunk retains the existing legacy dissection path. Printer fields include full identity,
version, optional sequence and FCS status. The native interface uses this representation;
historical MAC modules retain their own header and serializer compatibility paths.

Direct unit cases `Ieee802154FrameWire_1.test` and `Ieee802154Fcs_1.test` cover
independent address/PAN byte vectors, all selected presence combinations, high EUI-64 bits,
254/255/0/1, immediate ACK, bad/truncated/reserved/unsupported input, CRC and invalid/absent FCS.
The registered serializer is exercised through `Packet` typed-to-bytes and bytes-to-typed paths.
Capture tooling, external-decoder limits and actual executed outcomes are recorded in
[results.md](results.md) and the [enhanced execution record](followup-results.md), separately
from these source contracts. Parser success does not establish operational support.
