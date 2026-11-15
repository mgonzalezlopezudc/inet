# Independent adaptation vector exchange

> **Kind:** report · **Status:** snapshot 2026-09-19 · **Seal:** none · **Owns:** — · **Stands on:** [6LoWPAN features](../../features.md)

Peer: ns-3 `5e35cfbc28bd43aa9ceb0d1edea44078d6e76830` (`3-dev`), available from the canonical [ns-3 GitLab repository](https://gitlab.com/nsnam/ns-3-dev). Use a clean checkout at that exact revision and a default-profile build with the `sixlowpan` module and its dependencies. The adapter compiles the peer's existing test MockNetDevice directly; no peer production code is modified or copied. The GitHub mirror did not provide this revision during the 2026-11-15 reproduction.

```
doc/project/evidence/protocol/6lowpan/checks/peer/run.sh /path/to/ns-3 /tmp/lowpan-peer
opp_run_module_tests --load . -p inet -m debug --no-build -f 'LowpanPeerReceive\.test$'
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

The [current release validation record](../../../../model/6lowpan/release-validation.md) records the fresh canonical-peer build, exact byte comparisons and INET native tests. RFC 6282 section 3.2.2 requires the EUI-64 U/L-bit inversion. The pinned peer's `src/network/utils/ipv6-address.cc`, `MakeAutoconfiguredLinkLocalAddress(Mac64Address)`, copies the eight address octets without that inversion. INET's independently constructed native-wire expectation passes; preserving the peer's incompatible result does not imply changing INET's normative mapping.

The native-EUI limitation was also reproduced directly: decoding
`elided-input.hex` yields `elided-ns3-observed.hex`, differing from the independently
constructed `elided-expected.hex` only at IPv6 byte16 and byte32 (0x00 instead of
0x02 in each IID). This is a recorded interoperability failure for that encoding,
not a passing test or a reason to alter INET's RFC6282 U/L-bit derivation.
