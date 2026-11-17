# IEEE 802.15.4 enhanced IE framing

Plan: [E1-02](../../../plan/pending/802154-enhanced-frames-plan.md#e1-02--ieee802154-add-bounded-header-and-payload-ie-codecs). Logical tests: `Ieee802154InformationElements`, `serializer_ieee802154_enhanced`, and the extended `Ieee802154EnhancedFrameGate`.

The frame keeps one authoritative sequence of payload octets. A computed view
identifies nonoverlapping byte regions; offsets are relative to the first FCF
octet. The fixed MHR prefix precedes the auxiliary security header and Header
IEs. FCS, when present, is outside every parsed IE region.

```text
FCF/address prefix | auxiliary | Header IEs | Payload IEs | MSDU | FCS
                                  HT1 -> Payload IEs -> PT -> MSDU
                                  HT2 ---------------------> MSDU
```

For security-enabled frames, auxiliary control/counter/key-identifier extents
and any clear Header IEs are bounded before an opaque protected remainder.
The indicated MIC length is reserved when checking those bounds; payload and
MIC remain opaque and are never authenticated or decrypted. Reserved security
level 4 leaves the complete post-auxiliary remainder opaque. No security or
enhanced DATA service is enabled by a successful structural view.

The unit vectors independently cover all eight Table 7-6 presence combinations,
canonical generation, optional Payload Termination, bounded noncanonical empty
terminators, unknown and duplicate elements in order, descriptor transitions,
declared-length overruns, representative every-offset truncation, both FCS
modes and corruption. Auxiliary vectors cover all key modes, counter presence,
security levels and MIC minima. Header/Payload descriptor-width limits are
helper checks; the separate 127-byte frame test is the selected PHY's limit.
The test calls the production dissector on valid and malformed typed frames;
the callback receives the same typed frame and inspects the computed view.

The extended [real-radio gate](enhanced-frame-gate.md) adds a valid Header IE,
an overrun, a secured frame and a reserved-level opaque frame. Its callback
also checks independent Header-IE, Payload-IE and MSDU spans and MSDU bytes in
a combined frame. It checks values reported by the production dissector; malformed input
is marked incorrect and none of these version-2 frames receives an operational
ACK or DATA indication. The first five gate scenarios remain the E1-01
parent-compatible control.

The independent synthetic PCAP generation method and fixed inputs are in
[ieee802154-e102.md](../../serializer/pcap/generated/ieee802154-e102.md).
The serializer fixture admits both FCS modes only when the production typed
frame dissector and registered serializer reproduce all capture bytes.

From the checkout root, after a fresh debug build:

```sh
make MODE=debug -j4
opp_run_unit_tests --load . -p inet -m debug --no-build -f 'Ieee802154(InformationElements|FrameLayout|FrameWire|Fcs|Capture)_1[.]test$'
opp_run_opp_tests --load . -p inet -m debug --no-build -f 'serializer_ieee802154(_enhanced)?[.]test$'
opp_run_protocol_tests --load . -p inet -m debug --no-build -w '^tests/protocol/ieee802154$' -f 'Ieee802154(EnhancedFrameGate|ReceiveSecurity)[.]test$'
```

The first fresh debug diagnostic passed eight unit fixtures, both serializer
fixtures (22 enhanced and four legacy capture frames), and all 198 selected
protocol cases across four fixtures, with no errors, skips or failures. Scapy
2.8.0's IEEE 802.15.4 parser assumes an unconditional sequence field and does
not validate these suppressed-sequence/IE layouts; its partial FCF inspection
is not an independent IE decode. Packet dissection and serializer replay are
structural evidence; they do not establish enhanced MAC operation,
feature-specific IE semantics or physical interoperability.

Two pre-fix unit reds exposed a Command frame containing only IEs and no
mandatory Command ID, first through raw decoding and then through the typed
dissector path. The shared view now checks the unsecured Command payload after
the IE list rather than treating an IE octet as its ID. The final fresh debug
verification passed eight unit fixtures, both serializer fixtures and all 200
selected protocol cases, including the combined-span proof, without errors,
skips or failures.
