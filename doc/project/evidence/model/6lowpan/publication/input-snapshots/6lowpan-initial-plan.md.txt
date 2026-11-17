**6LoWPAN should be implemented as a new adaptation layer between IPv6 and the IEEE 802.15.4 MAC, not as functionality inside `Ipv6` or `Ieee802154Mac`**. INET already has most of the infrastructure needed, but several assumptions in the present IPv6/802.15.4 boundary need to be dealt with explicitly.

## 1. What is in INET master today

There is **no actual 6LoWPAN implementation**. A repository-wide search for `6lowpan`, `lowpan`, RFC 6282, and LOWPAN_IPHC finds only documentation/reference material. The 802.15.4 user guide even says the model can “alternatively” be used with 6LoWPAN, but there is no corresponding source package, protocol, compressor, fragmenter, or interface implementation. [Current 802.15.4 user guide](https://github.com/inet-framework/inet/blob/98117c3257b2e11661d2baf685c18911c8639b24/doc/src/users-guide/ch-802154.rst)

The relevant current architecture looks like this:

```text
Ipv6
 │
 │ IPv6 packet
 ▼
NetworkInterface
 │
 ▼
Ieee802154NarrowbandInterface
 │
 ├── queue
 ├── Ieee802154Mac
 └── Ieee802154NarrowbandRadio
```

`Ieee802154NarrowbandInterface` is therefore a very natural place to insert the adaptation layer.

There are, however, some important incompatibilities.

| Area                       | Current master                                                                            | Consequence for 6LoWPAN                                                               |
| -------------------------- | ----------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------- |
| 6LoWPAN                    | Not implemented                                                                           | New adaptation layer needed                                                           |
| IPv6 MTU                   | `Ipv6::fragmentAndSend()` uses `NetworkInterface::getMtu()`                               | 6LoWPAN interface must advertise **1280 B**, not the IEEE 802.15.4 frame payload size |
| 802.15.4 MTU               | `Ieee802154Mac.mtu` defaults to `0B` and is copied to `NetworkInterface`                  | Current MTU model cannot directly represent 6LoWPAN                                   |
| 802.15.4 addresses         | Uses generic 48-bit `MacAddress`                                                          | RFC 4944/6282 require 64-bit extended and 16-bit short addresses                      |
| Received L2 metadata       | `Ieee802154Mac::decapsulate()` sets source MAC but not destination                        | Insufficient for fully correct IPHC address elision and fragment identification       |
| IPv6 multicast             | `Ipv6` maps multicast to Ethernet `33:33:xx:xx:xx:xx`                                     | 802.15.4 6LoWPAN requires link broadcast for ordinary IPv6 multicast                  |
| 802.15.4 wire format       | Simplified header; addresses padded to 64 bits; Source PAN ID abused for payload protocol | Not wire-compatible enough for real 6LoWPAN PCAP/interoperability                     |
| Generic INET fragmentation | Available, but generic defragmenter expects one sequential stream                         | Cannot implement RFC 4944 reassembly directly                                         |
| IPv6 ND                    | RFC 4861-style, tightly based on `MacAddress`                                             | Usable as an initial approximation, but not RFC 6775/8505 6LoWPAN ND                  |

The MTU distinction is particularly important. RFC 4944 specifies an IPv6 MTU of **1280 octets**, while the physical IEEE 802.15.4 frame is at most 127 octets; fragmentation/reassembly therefore occurs **below IP**. ([IETF Datatracker][1])

Current `Ipv6::fragmentAndSend()` does this:

```text
packet <= interface MTU
        -> send

packet > interface MTU
        -> IPv6 fragmentation if locally generated
        -> ICMPv6 Packet Too Big if being forwarded
```

[Current Ipv6.cc](https://github.com/inet-framework/inet/blob/98117c3257b2e11661d2baf685c18911c8639b24/src/inet/networklayer/ipv6/Ipv6.cc)

Therefore the correct configuration is conceptually:

```text
IPv6 interface MTU              = 1280 B

IEEE 802.15.4 maximum frame     = 127 B
       - MAC header
       - security overhead
       - FCS/etc.
       = actual LoWPAN payload capacity

6LoWPAN fragmentation operates here.
```

Setting the interface MTU to ~100 B and letting `Ipv6` fragment would be incorrect.

## 2. Architecture I would implement

I would create:

```text
src/inet/linklayer/lowpan/
```

and a new network interface:

```text
Ieee802154LowpanInterface
```

with approximately this composition:

```text
                       NetworkInterface
 ┌───────────────────────────────────────────────────────┐
 │                                                       │
 │ IPv6                                                  │
 │  │                                                    │
 │  ▼                                                    │
 │ LowpanLayer                                           │
 │  │                                                    │
 │  ▼                                                    │
 │ queue                                                 │
 │  │                                                    │
 │  ▼                                                    │
 │ Ieee802154Mac                                         │
 │  │                                                    │
 │  ▼                                                    │
 │ radio                                                 │
 └───────────────────────────────────────────────────────┘
```

RFC 4944 explicitly defines LoWPAN encapsulation as the payload of the IEEE 802.15.4 MAC PDU and places the adaptation function between IP and MAC. ([IETF Datatracker][1])

I would make `LowpanLayer` one NED simple module initially, with testable C++ helper classes rather than a long chain of tiny NED modules:

```text
LowpanLayer
 ├── LowpanDispatchParser
 ├── LowpanIphcCompressor
 ├── LowpanIphcDecompressor
 ├── LowpanFragmenter
 ├── LowpanReassemblyTable
 └── LowpanContextTable
```

This keeps packet flow simple while keeping the algorithms separately testable.

A corresponding packet-model package should contain chunks such as:

```text
LowpanFragmentHeader
LowpanMeshHeader
LowpanBroadcastHeader
LowpanIphcHeader
LowpanNhcUdpHeader
LowpanPagingHeader
```

plus serializers, dissector, printer, and a `Protocol::lowpan` registration.

## 3. Addressing needs an abstraction

I would **not let LOWPAN_IPHC directly use `MacAddress`**.

RFC 4944 permits IEEE 802.15.4 64-bit extended addresses and 16-bit short addresses, and RFC 6282 explicitly uses them to reconstruct IPv6 IIDs. ([IETF Datatracker][1])

Introduce something along the lines of:

```cpp
class LowpanLinkAddress {
    enum Type {
        SHORT_16,
        EXTENDED_64
    };
    ...
};
```

The compression algorithms consume `LowpanLinkAddress`, not `MacAddress`.

For an initial development version there can be a compatibility mapping:

```text
current INET MacAddress
        ↓
temporary deterministic LowpanLinkAddress
```

but that mapping must be clearly marked as a simulation compatibility mechanism.

A standards-faithful implementation should subsequently repair `Ieee802154MacHeader`. Its current comments already acknowledge that the representation is not a faithful IEEE 802.15.4 frame. [Current Ieee802154MacHeader.msg](https://github.com/inet-framework/inet/blob/98117c3257b2e11661d2baf685c18911c8639b24/src/inet/linklayer/ieee802154/Ieee802154MacHeader.msg)

That work should eventually provide real:

```text
16-bit short addresses
64-bit extended addresses
PAN IDs
PAN-ID compression
addressing modes
frame version
variable MAC header length
security header
proper FCS semantics
```

and stop using the Source PAN ID as an ersatz EtherType.

Crucially, this IEEE 802.15.4 cleanup need not block development of the 6LoWPAN algorithms if the latter are written against `LowpanLinkAddress`.

## 4. Fix the L2 metadata boundary

There is a small prerequisite fix I would submit independently.

Current `Ieee802154Mac::decapsulate()` does essentially:

```cpp
packet->addTagIfAbsent<MacAddressInd>()
      ->setSrcAddress(csmaHeader->getSrcAddr());
```

but does not copy the destination.

It should expose both source and destination addresses. [Current Ieee802154Mac.cc](https://github.com/inet-framework/inet/blob/98117c3257b2e11661d2baf685c18911c8639b24/src/inet/linklayer/ieee802154/Ieee802154Mac.cc)

The destination is needed because address elision can depend on the encapsulating link-layer addresses, and RFC 4944 reassembly identifies a datagram using source/originator, destination/final destination, `datagram_size`, and `datagram_tag`. ([IETF Datatracker][1])

That is an excellent small standalone PR because it improves the existing model without introducing 6LoWPAN yet.

## 5. Multicast requires special treatment

This is another real incompatibility I found.

Current INET IPv6 calls:

```cpp
Ipv6Address::mapToMulticastMacAddress()
```

which always produces:

```text
33:33:xx:xx:xx:xx
```

That is the Ethernet mapping.

RFC 4944 says that ordinary IPv6 multicast on IEEE 802.15.4 **must be carried as link-layer broadcast**, using PAN broadcast address `0xffff`; mesh-under multicast is a distinct case. ([IETF Datatracker][1])

For the initial implementation I would handle this in `LowpanLayer`:

```text
IPv6 destination is multicast
        ↓
override underlying 802.15.4 destination
        ↓
broadcast
```

Longer term, INET would benefit from moving L3→L2 multicast mapping behind an interface-specific abstraction instead of embedding the Ethernet rule in `Ipv6Address`.

I would keep that generic refactoring out of the first 6LoWPAN PR unless the maintainers specifically prefer it.

## 6. RFC 4944 fragmentation

This should be implemented with dedicated code rather than `FragmentTagBasedFragmenter` / `DefragmenterBase`.

RFC 4944 defines FRAG1 and FRAGN; non-final fragments must correspond to multiples of eight octets, `datagram_tag` is 16 bits and wraps, and reassembly state is keyed by the two link addresses plus size and tag. The receiver must deal with fragments arriving before FRAG1, overlaps, duplicates, and a reassembly timer of at most 60 seconds. ([IETF Datatracker][1])

The current generic `DefragmenterBase` has effectively one sequentially assembled packet and an `expectedFragmentNumber`; that is not sufficient for this.

I would use:

```cpp
struct LowpanReassemblyKey {
    LowpanLinkAddress originator;
    LowpanLinkAddress destination;
    uint16_t datagramSize;
    uint16_t datagramTag;
};

struct LowpanReassemblyBuffer {
    std::vector<...> receivedRanges;
    Packet reconstructedDatagram;
    simtime_t expiry;
};
```

and maintain:

```cpp
std::map<LowpanReassemblyKey, LowpanReassemblyBuffer>
```

with configurable limits for total buffered bytes and simultaneous datagrams.

## 7. The subtle RFC 6282 fragmentation issue

This is probably the easiest place to introduce a difficult-to-find bug.

It is **not correct to simply compress the IPv6 packet and then split the resulting byte string at arbitrary positions**.

RFC 6282 explicitly says that RFC 4944's `datagram_size` and `datagram_offset` refer to the **IPv6 datagram before compression**, and payload in fragments after the first corresponds to the uncompressed datagram coordinate system. Headers that cannot fit into FRAG1 must not be compressed. ([IETF Datatracker][2])

I would therefore make the fragmenter operate on a **virtual uncompressed datagram coordinate space**.

Conceptually:

```text
Original IPv6 datagram

 offset 0                                      offset N
 ├──────── IPv6/header chain ────────┬──────── payload ────────┤
                                     │
        IPHC/NHC compressor          │
               ↓                     │

FRAG1:
    FRAG1 | IPHC | NHC | first payload portion
                    ↓
        decompressed coverage = [0 ... K)

FRAGN:
    FRAGN | bytes corresponding to original IPv6 offset K ...
```

On reception, FRAG1 is decompressed into positions starting at zero in the reassembly buffer. FRAGN payloads are then inserted using their RFC 4944 offsets.

This should be designed before writing either the compressor or fragmenter.

## 8. Header compression implementation order

Do **RFC 6282 LOWPAN_IPHC**, not the original RFC 4944 HC1/HC2 compressor. RFC 6282 explicitly replaced that format and says new implementations should not transmit the old RFC 4944 compression format. ([IETF Datatracker][2])

I would implement compression incrementally:

| Stage  | Functionality                                    |
| ------ | ------------------------------------------------ |
| IPHC-1 | Version elision, TF, NH inline, HLIM compression |
| IPHC-2 | Stateless link-local unicast address compression |
| IPHC-3 | Stateless multicast address compression          |
| NHC-1  | UDP NHC and all four UDP port encoding modes     |
| IPHC-4 | CID + stateful source/destination contexts       |
| NHC-2  | IPv6 extension-header NHC                        |
| Later  | Additional registered NHCs                       |

The context table should be a first-class object:

```text
CID
prefix
prefixLength
compressionAllowed
validUntil
```

Initially configure it statically. Later RFC 6775's 6LoWPAN Context Option can update the same table dynamically.

The UDP checksum should initially be transmitted normally. Checksum elision can be implemented later as an explicit option rather than silently enabled.

## 9. Dispatch parser: design for modern extensions now

Even if the first implementation handles only Page 0, I would not write a single hard-coded `switch(firstByte)` that assumes the original 2007 dispatch space forever.

RFC 8025 added the Paging Dispatch and 16 parsing pages, while RFC 8066 defined ESC extension semantics. ([IETF Datatracker][3])

Use something conceptually like:

```text
LowpanDispatchParser
    currentPage = 0

    parseNextHeader()
       Mesh
       Broadcast
       Fragment
       Paging
       ESC
       LOWPAN_IPHC
       IPv6
       ...
```

This also naturally enforces RFC 4944's ordering:

```text
Mesh
Broadcast
Fragment
payload dispatch
```

([IETF Datatracker][1])

The first release can reject unsupported Pages/ESC types correctly without implementing their associated protocols.

## 10. Neighbor Discovery should be a separate milestone

I would **not make RFC 6775/8505 a prerequisite for the initial 6LoWPAN data plane**.

Initially, let INET's existing RFC 4861 ND operate through the adaptation layer, with IPv6 multicast translated to 802.15.4 broadcast. That is useful for bringing up and testing IPv6+UDP+ICMPv6.

Then implement proper 6LoWPAN ND.

RFC 6775 adds host-driven registration, eliminates multicast address resolution for hosts, distributes compression contexts, and defines ARO, ABRO, and 6CO. ([IETF Datatracker][4]) RFC 8505 substantially extends that registration architecture, including 6LR registration, mobility, proxy registration, and ROVR. ([IETF Datatracker][5])

I would preferably refactor the current `Ipv6NeighbourDiscovery` enough that regular RFC 4861 and 6LoWPAN ND share the normal RA/NS/NA machinery rather than copying the whole module.

## 11. Route-over first; mesh-under later

For the first implementation I strongly favor **route-over**.

With route-over:

```text
802.15.4
   ↓
6LoWPAN reassembly
   ↓
IPv6 decompression
   ↓
IPv6 routing
   ↓
IPHC compression
   ↓
new 6LoWPAN fragmentation
   ↓
next 802.15.4 hop
```

That makes excellent use of INET's existing IPv6 stack.

RFC 8930 confirms that this reassemble→route→recompress→refragment sequence is the baseline route-over behavior and introduces fragment forwarding as an optimization. ([IETF Datatracker][6])

Mesh-under can subsequently add RFC 4944 Mesh Addressing and Broadcast headers. RFC 8930 fragment forwarding and RFC 8931 recoverable fragmentation should come much later, after the basic implementation is stable. ([IETF Datatracker][7])

## 12. Proposed PR sequence

For upstream INET I would deliberately keep the PRs small:

| PR                                       | Content                                                                                                       | Key acceptance criterion                  |
| ---------------------------------------- | ------------------------------------------------------------------------------------------------------------- | ----------------------------------------- |
| **1. 802.15.4 L2 indications**           | Add destination to receive indication; tests                                                                  | No behavioral regression                  |
| **2. 6LoWPAN skeleton/interface**        | `LowpanLayer`, `Ieee802154LowpanInterface`, dispatch parser, uncompressed IPv6, MTU=1280, multicast→broadcast | ICMPv6/UDP works for single-frame packets |
| **3. RFC 4944 fragmentation**            | FRAG1/FRAGN, tag generation, out-of-order reassembly, timeout, overlap handling                               | 1280-byte datagram round-trip             |
| **4. RFC 6282 IPHC core**                | Stateless IPHC, multicast forms, NHC-UDP                                                                      | All compression modes tested              |
| **5. IEEE 802.15.4 addressing fidelity** | Real extended/short addresses and removal of 48-bit assumptions                                               | PCAPs decode correctly as 802.15.4        |
| **6. Stateful IPHC**                     | Context table, CID, stateful unicast/multicast, extension-header NHC                                          | Static-context tests                      |
| **7. RFC 6775/8505 ND**                  | ARO/EARO, 6CO, registration, 6LR/6LBR roles                                                                   | No multicast address resolution for 6LN   |
| **8. Mesh-under**                        | Mesh header, BC0, L2 forwarding                                                                               | Multi-hop mesh-under scenario             |
| **9. Modern extensions**                 | Paging/ESC support, RFC 8930/8931 where useful                                                                | Fragment forwarding/recovery tests        |

PR 5 can actually progress in parallel with PRs 2–4 because the compressor should see `LowpanLinkAddress`, not `MacAddress`.

## 13. Testing and standards evidence

I would integrate this with the relatively new `doc/project/evidence` structure already used by current master for IPv6/RFC 8200.

For example:

```text
doc/project/evidence/standard/rfc4944/catalog.md
doc/project/evidence/standard/rfc6282/catalog.md
doc/project/evidence/standard/rfc6775/catalog.md
doc/project/evidence/standard/rfc8505/catalog.md

doc/project/evidence/protocol/lowpan/checks/
doc/project/evidence/model/lowpan/coverage.md
doc/project/evidence/model/lowpan/results.md

tests/protocol/lowpan/
```

The test set should cover serializer bit-level vectors, compress→decompress identity, fragment→reassemble identity, every length around fragment boundaries, FRAGN-before-FRAG1, duplicates, overlaps, timeout, tag wraparound, all IPHC SAM/DAM/TF/HLIM modes, UDP NHC port modes, link-local/global/multicast addresses, 1280-byte packets, and multi-hop route-over.

For interoperability validation I would also generate real IEEE 802.15.4 PCAPs and verify them independently with Wireshark and at least one external 6LoWPAN implementation. That is where fixing the current simplified `Ieee802154MacHeader` becomes essential.

## Recommended target

I would define the first major milestone as:

> **RFC 4944 adaptation + RFC 6282 IPHC/NHC-UDP, route-over, 64-bit addresses initially, IPv6 MTU 1280, correct fragmentation/reassembly, and ordinary RFC 4861 ND transported over the LoWPAN.**

That gives INET a genuinely useful IPv6-over-802.15.4 implementation without immediately making the project depend on the much larger RFC 6775/8505 ND work.

The two architectural points I would treat as non-negotiable are **keeping IPv6 MTU 1280 while link-fragmenting below IP**, and **using an explicit 16/64-bit LoWPAN link-address abstraction rather than embedding the current 48-bit `MacAddress` assumption into RFC 6282**. Those two decisions will determine whether the initial implementation can evolve cleanly into a standards-faithful one.

[1]: https://datatracker.ietf.org/doc/html/rfc4944 "https://datatracker.ietf.org/doc/html/rfc4944"
[2]: https://datatracker.ietf.org/doc/html/rfc6282 "https://datatracker.ietf.org/doc/html/rfc6282"
[3]: https://datatracker.ietf.org/doc/html/rfc8025 "https://datatracker.ietf.org/doc/html/rfc8025"
[4]: https://datatracker.ietf.org/doc/html/rfc6775 "https://datatracker.ietf.org/doc/html/rfc6775"
[5]: https://datatracker.ietf.org/doc/html/rfc8505 "https://datatracker.ietf.org/doc/html/rfc8505"
[6]: https://datatracker.ietf.org/doc/html/rfc8930 "https://datatracker.ietf.org/doc/html/rfc8930"
[7]: https://datatracker.ietf.org/doc/html/rfc8931 "https://datatracker.ietf.org/doc/html/rfc8931"
