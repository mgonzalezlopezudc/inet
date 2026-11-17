# IEEE 802.15.4 enhanced DATA

This fixture exercises the native version-2 DATA service between two nodes on
the scalar O-QPSK medium. Each scenario runs once with typed frames and once
with raw bytes. Both nodes explicitly enable the profile except for the
profile-refusal control. No enhanced ACK or security processing is selected.

| Scenarios | Setup and expected observation |
| --- | --- |
| 0–5 | Extended and short source/destination combinations, broadcast, optional sequence suppression, and vendor Header plus ESDU payload IEs deliver the original MSDU without an ACK. |
| 6–12 | Profile absence preserves caller ownership; unsupported ACK/GTS/no-address options, unsupported security and malformed legacy/IE combinations produce their specified native terminals. |
| 13–21 | The 127-byte boundary, including IE descriptors and terminators, succeeds and the next byte fails; missing association context, address mismatch with equal low 48 EUI bits, invalid short address, incompatible options and foreign PAN are checked. Actual association permits a PAN-omitted exchange. |
| 22–26 | Beacon-only Header IEs, unsupported DATA-applicable features, incomplete vendor identifiers and reserved Payload groups are rejected at the native request. |
| 27 | Legacy, suppressed version-2 and sequence-present version-2 frames cross 254, suppressed, 255 and leave the PIB counter at zero. |
| 28–34 | Vendor short and long MLME nested IEs and vendor Payload IEs pass; reserved, forbidden, unimplemented and incomplete nested values receive the selected error terminals. |
| 35 | A real association is followed by a PAN-omitted frame whose source EUI is changed in transit, preserving length and valid FCS; the receiver does not deliver it under the old binding. |
| 36–37 | A scripted PHY sends from a real associated identity with reception ending exactly at, or one tick before, an indirect disassociation expiry. The fixture observes the RF end and distinguishes retired from still-live context. |
| 38–39 | A reserved Header IE is rejected for local emission, while an independently mutated valid frame carrying an unknown Header IE is received with copied IE metadata and the original MSDU. |

The local vendor identifier is treated as caller-supplied bytes; no OUI
registry or feature-specific semantics are inferred. The generic codec retains
unknown values for forensic inspection independently of the operational DATA
admission policy. The test checks actual received frames, delivered MSDUs and copied IE lists;
it does not establish enhanced management, ACK, encryption or scheduled DATA.

From the checkout root, after a fresh debug build:

```sh
opp_run_protocol_tests --load . -p inet -m debug --no-build -w '^tests/protocol/ieee802154$' -f 'Ieee802154EnhancedData[.]test$'
```

The fresh matching debug build passed all 80 typed/raw fixture runs and the
adapter module test. The subsequent no-build acceptance used the same library
and passed nine unit fixtures, five module fixtures, two serializer fixtures and
512 protocol cases across thirteen fixtures. In both typed/raw managed Enhanced
example runs the device completed two cycles and each role recorded two
deliveries and confirmations. The earlier unchanged-source expiry-boundary red reached its exact RF
end and disassociation terminal, then showed the stale-context delivery; the
one-tick-earlier control passed. A bounded repair moved context resolution
after expiry, and both cases passed in the matching build. The initial four-case
plumbing diagnostic predates the completed operational IE policy.
