# IEEE 802.15.4 nested MLME IE framing

Plan: [E1-03](../../../plan/pending/802154-enhanced-frames-plan.md#e1-03--ieee802154-add-nested-mlme-ie-framing). Logical tests: `Ieee802154NestedElements`, the extended enhanced serializer fixture and `Ieee802154EnhancedFrameGate`.

An MLME Payload IE is group 1. Its declared content length is the immediate
parent bound for each two-octet short or long Nested IE descriptor and its
content. Unknown sub-IDs and repeated entries remain ordered values; parsing
does not recursively interpret their contents or enable TSCH, DSME or other
feature semantics.

```text
HT1 | Payload IE group 1 [ short descriptor | bytes | long descriptor | bytes ] | PT | MSDU
                         <------------- declared parent content ------------->
```

The unit fixture uses independent descriptor octets for mixed forms, duplicate
IDs and zero-length entries. It exercises every prefix of representative
short/long elements, the immediate enclosing bound, invalid widths and the
2047-byte Payload IE maximum. A long Nested IE can carry at most 2045 bytes
inside that parent because its descriptor consumes two bytes; the native PHY's
127-byte complete PSDU is a separate, smaller limit. The Frame codec rejects a
nested overrun instead of reading the following Payload Termination or MSDU.

The extended real-radio gate adds an MLME group with one unknown short and
one unknown long sub-IE, followed by a separate two-byte MSDU. Its production
dissector callback checks forms, IDs, contents, parent regions and MSDU bytes
in typed and raw runs. The independent synthetic serializer captures add two
MLME vectors and preserve exact bytes in both FCS modes; their generator and
provenance are documented in [ieee802154-e102.md](../../serializer/pcap/generated/ieee802154-e102.md).
The preceding E1-02 code retained the MLME group as opaque content but did not
expose nested values. Secured remainders remain opaque.

From the checkout root, after a fresh debug build:

```sh
make MODE=debug -j4
opp_run_unit_tests --load . -p inet -m debug --no-build -f 'Ieee802154(NestedElements|InformationElements|FrameLayout|FrameWire|Fcs)_1[.]test$'
opp_run_opp_tests --load . -p inet -m debug --no-build -f 'serializer_ieee802154(_enhanced)?[.]test$'
opp_run_protocol_tests --load . -p inet -m debug --no-build -w '^tests/protocol/ieee802154$' -f 'Ieee802154EnhancedFrameGate[.]test$'
```

The first fresh debug diagnostic passed nine unit fixtures, both serializer
fixtures (26 enhanced and four legacy capture frames) and all 202 selected
protocol cases, without errors, skips or failures. These test framing and
representation, not feature-specific MLME semantics, cryptography or enhanced
operational delivery. Mermaid rendering is not applicable to the ASCII region
diagram.
