# 6LoWPAN native single-hop exchange

Enable `Lowpan` and `LowpanExamples`, build INET, then run:

```
inet --debug -u Cmdenv -f omnetpp.ini -c General -r 0
inet --debug -u Cmdenv -f omnetpp.ini -c Uncompressed -r 0
```

Two IPv6 hosts exchange a 1280-octet echo datagram using native IEEE 802.15.4
extended addresses, FCS, acknowledgments and RFC 4944 fragmentation. `General`
uses stateless IPHC; `Uncompressed` uses the 0x41 dispatch. The IPv6 MTU stays
1280 regardless of the destination-dependent MAC payload capacity.

The domain supplies static IPv6 next-hop bindings. Native interfaces suppress
ordinary ND and require stationary radios on a dedicated medium. The initial
MAC profile is unsecured, same-PAN, 250 kbit/s, with extended unicast addresses
and short broadcast. It does not implement association or security. UDP NHC
carries its checksum; stateful compression, extension-header NHC and mesh-under
are outside this profile.

The legacy compatibility interface remains available as
`Ieee802154LowpanInterface`, with `LowpanLinkDomain` and explicit collision-checked
48-bit aliases. It is not a native wire format. Native capture decoding and a
pinned ns-3 exchange of fragmented global-address UDP vectors pass independently.
The peer reconstructs fully elided EUI addresses differently; broader interoperability
and release gates remain open. See the
[peer evidence](../../doc/project/evidence/protocol/6lowpan/checks/peer/README.md).

The two-PAN route-over example uses explicit IPv6 addresses and routes in
`route.xml` and distinct native neighbor tables on each PAN:

```
inet --debug -u Cmdenv -f route.ini -c General -r 0
```

Its UDP sink receives one 1232-byte application payload (1280 bytes including
IPv6 and UDP headers). The router reassembles before forwarding with a decreased
Hop Limit, then compresses and fragments for its other interface.

Keep transport and ICMPv6 checksum modes set to `computed` when validating wire
bytes. INET's declared checksum mode is simulation metadata and does not produce
a checksum suitable for an external decoder or peer.
