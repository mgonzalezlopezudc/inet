# IEEE 802.15.4 enhanced-frame structural gate

Plan: [E1-01](../../../plan/pending/802154-enhanced-frames-plan.md#e1-01--ieee802154-add-version-aware-mac-frame-layouts). Logical test: `Ieee802154EnhancedFrameGate`.

The test uses two native interfaces one metre apart on the scalar O-QPSK
medium. The sender submits one acknowledged legacy DATA request. A medium
tap changes the delivered bytes into a structurally valid version-2 Beacon,
DATA, ACK or Command and recomputes FCS without changing the PSDU length. The
fifth case suppresses the DATA sequence number and moves one octet into the
opaque payload to keep that length. Each case runs in typed and raw modes.

The receiver's lower-packet observation checks the delivered raw control bits
and FCS and requires version 2, the selected frame type and expected sequence
presence. The operational MAC must not send an ACK or deliver
normal DATA; the legacy sender consequently receives NO_ACK. This establishes
structural visibility and the native capability gate, not enhanced operation,
IE semantics or security processing.

`Ieee802154FrameLayout_1.test` separately expands all 14 Table 7-2 rows to
18 concrete address-mode/compression combinations, both sequence choices and
four frame types. It checks actual field presence, truncation and independent
both-extended, PAN-only and shortest-frame octets. An on-wire `0xffff` PAN is
present; it is not an absence marker. Existing FrameWire/Fcs controls retain
legacy wire and FCS behavior.

From the checkout root, after a fresh debug build:

```sh
make MODE=debug -j4
opp_run_unit_tests --load . -p inet -m debug --no-build -f 'Ieee802154(FrameLayout|FrameWire|Fcs|Address|Commands|Beacons)_1[.]test$'
opp_run_protocol_tests --load . -p inet -m debug --no-build -w '^tests/protocol/ieee802154$' -f 'Ieee802154(EnhancedFrameGate|ReceiveSecurity|DataAck|GtsData)[.]test$'
```

The fresh debug diagnostic passed all six selected unit fixtures, the legacy
serializer control and 188 protocol cases across the new gate and existing
ReceiveSecurity, DataAck and GtsData controls. The first gate build selected
zero cases because its test client declared two non-existent callbacks; after
that fixture-only correction, all ten typed/raw gate cases passed against the
unchanged production library. Later legacy classification controls exposed two
regressions: short legacy prefixes must remain incomplete, while forbidden
legacy ACK/beacon controls must remain malformed. Both were reproduced before
the bounded fix. A final fresh debug run then passed the same six units, legacy
serializer and all 188 selected protocol cases. The parent MAC already rejected
operational version-2 traffic; successful structural decoding is new unit
coverage, which the parent's codec returned as unsupported. The protocol
fixture uses APIs available on both revisions and expresses a parent-pass
compatibility control; parent runtime execution has not been claimed. The medium
observation is a MAC-delivered packet; actual RF transmission is provided by
the real sender and radio medium. No enhanced management or DATA service is
enabled by these structural changes.
