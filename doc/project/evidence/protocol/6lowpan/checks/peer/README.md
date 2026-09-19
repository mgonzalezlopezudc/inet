# Independent adaptation vector exchange

Peer: ns-3 `5e35cfbc28bd43aa9ceb0d1edea44078d6e76830` (`3-dev`), clean local checkout
`/home/user/ns-3-dev`, existing default build. The adapter compiles the peer's
existing test MockNetDevice directly; no peer production code is modified or copied.

```
doc/project/evidence/protocol/6lowpan/checks/peer/run.sh /home/user/ns-3-dev
MPLCONFIGDIR=/tmp/lowpan-matplotlib inet_run_module_tests -m debug -f 'LowpanPeerReceive\.test$' --no-concurrent
```

`LowpanNativeRouteOver.test`, payload1232 run, exports the original1280-byte IPv6
UDP datagram and each source adaptation fragment. `inet-encoded.hex` contains
LoWPAN payloads without MAC headers/FCS. Native metadata is EUI64
0011223344556601→0011223344556602; the IPv6 global addresses travel inline.
ns-3 receives those bytes and reconstructs exactly `inet-original.hex`.
Both implementations use104-octet lower payload budgets. Checksums are enabled;
UDP NHC uses four-bit ports with the checksum carried. The reverse ns-3 encoding
is embedded in `LowpanPeerReceive.test`; production INET adaptation reconstructs
the exact original twice with reversed, interleaved fragment tags.

Results: INET→ns-3 exact comparison PASS; ns-3→INET production receive PASS.
The initial reverse comparison exposed a nonzero-front-offset packet erase issue;
adaptation now consumes/trims encoded headers before inserting reconstructed ones.
This fixture must remain byte-only so sender chunk structure cannot hide the issue.

Scope: one fragmented UDP datagram, global addresses, stateless IPHC and carried
checksum. This is serialized-vector interoperability, not a live MAC/radio exchange.
The pinned peer's Mac64 link-local derivation copies the EUI without the U/L flip;
fully elided native EUI address interoperability is not established by these vectors.
Short broadcast and other encoding forms require separate peer evidence. These
limits keep the broader first-release validation gate open.

The native-EUI limitation was also reproduced directly: decoding
`elided-input.hex` yields `elided-ns3-observed.hex`, differing from the independently
constructed `elided-expected.hex` only at IPv6 byte16 and byte32 (0x00 instead of
0x02 in each IID). This is a recorded interoperability failure for that encoding,
not a passing test or a reason to alter INET's RFC6282 U/L-bit derivation.
