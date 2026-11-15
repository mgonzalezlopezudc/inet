# Complete the 6LoWPAN protocol family

> **Kind:** design · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [6lowpan-implementation-plan.md](../done/6lowpan-implementation-plan.md), [standards.md](../../doc/project/evidence/protocol/6lowpan/standards.md)

Status: **executing P8–P15**. The user requested continued implementation without stopping at intermediate milestones on 2026-11-15. The bounded P0–P7 profile remains validated and archived. This plan implements the separately listed follow-ons; no package is complete until its production paths, direct protocol tests, reviewer document, builds and regression checks pass. Preserve the unrelated local `AGENTS.md` edit. No packet-core change is required or authorized.

## Completion sequence and contracts

| Package / proposed commit | Responsible state and production path | Protocol test and reviewer artifact | Acceptance |
| --- | --- | --- | --- |
| P8 `lowpan: add: stateful IPHC compression contexts` | Per-interface `LowpanLayer` owns a bounded context table; codec selects valid compressible contexts and accepts valid receive-only contexts. First-fragment decompression snapshots addresses; prefix replacement clears pending RX. | `LowpanStatefulContexts.test`, `stateful-contexts.md` | Independent unicast/multicast vectors, CID 0/nonzero unequal SCI/DCI, missing/expired contexts, permission distinction, fragmentation and context replacement. |
| P9 `lowpan: add: extension-header next-header compression` | Adaptation NHC chain codec handles supported RFC 6282 EIDs, lengths/padding and nested IPv6; unsupported headers remain inline. | `LowpanExtensionNhc.test`, `extension-nhc.md` | Every supported EID and NH chain, independent bytes, 255/256-byte length boundary, malformed input and first-fragment fallback. |
| P10 `lowpan: add: optimized neighbor registration` | LoWPAN ND owns 6LN/6LR/6LBR roles and registration/cache timers; ICMPv6 options retain actual native identities. Context advertisements update the adaptation owner. | `LowpanNeighborRegistration.test`, `neighbor-registration.md` | ARO registration, duplicates, bounded cache/expiry, 6CO/ABRO distribution, restart and multihop registration without static-neighbor masking. |
| P11 `lowpan: add: extended address registration` | Registration adds EARO/ROVR, transaction recency and ownership/movement/proxy behavior to P10. Pin later-update applicability before implementation. | `LowpanExtendedRegistration.test`, `extended-registration.md` | Mixed base/extended peers, stale transactions, conflict, refresh, expiry and movement. |
| P12 `lowpan: add: mesh-under forwarding` | Adaptation dispatch owns Mesh/BC0 wire headers, originator/final destination, bounded hop count and duplicate cache; reassembly uses mesh identities. | `LowpanMeshForwarding.test`, `mesh-forwarding.md` | Multihop unicast/broadcast, loops/duplicates/hop exhaustion and header-order checks. |
| P13 `lowpan: add: paging and extended dispatch` | Per-packet dispatch parser tracks RFC 8025 page state and RFC 8066 ESC extension types; transit forwarding preserves opaque unsupported types where required. | `LowpanPagingDispatch.test`, `paging-dispatch.md` | Page reset, switches/order, IPHC on supported pages and explicit unknown-type endpoint/transit behavior. |
| P14 `lowpan: add: virtual fragment forwarding` | Interface forwarding state owns bounded RFC 8930 VRBs, route/next-hop selection and outgoing tag mapping before full reassembly. | `LowpanFragmentForwarding.test`, `fragment-forwarding.md` | Concurrent flows, observed early forwarding, tag collisions, route changes, resource limits and expiry. |
| P15 `lowpan: add: selective fragment recovery` | Distinct RFC 8931 RFRAG/RFRAG-ACK codec and sender/receiver recovery windows use their own offset/sequence rules; timers and congestion behavior have bounded ownership. | `LowpanSelectiveRecovery.test`, `selective-recovery.md` | Controlled loss, selective retransmission, retry/window bounds, congestion signals and cleanup in compressed-datagram coordinates. |

Each named protocol artifact is proposed until created under `tests/protocol/6lowpan/`. Execute explicit `opp_run_protocol_tests --load . -p inet -m debug -w '^tests/protocol/6lowpan$' -f '<named-fixture>\.test$'` selections against a matching build, preserving configuration/run/seed, exact assertions, case counts, status and artifacts. Unit vectors supplement production-path proof. Each behavior commit includes its protocol fixture and reviewer document; helper preparation may be split when independently useful and tested. No baseline update is implied.

## P8 implementation contract

`LowpanContextTable` stores at most 16 canonical IPv6 prefixes of lengths 0–128, absolute validity deadlines and a compression permission. `LowpanLayer` owns one table per interface and loads an optional XML `compressionContexts` parameter at initialization. Codec lookups take explicit simulation time; missing/expired contexts reject context-dependent decoding, while context-independent unspecified source and unused CID values preserve current behavior. The context identifier octet follows RFC 6282 section 3.1.2. Multicast context prefixes are limited to the 64-bit prefix field of the RFC 3306/3956 representation.

The owner exposes a typed context-install operation for future ND updates. Prefix/length replacement clears partial receive state and cancels its timer; validity/permission refresh does not reinterpret stored bytes. A FRAG1 decoded while its context is valid already contains the reconstructed IPv6 address, so later expiry cannot change that snapshot. FRAGN-first storage is invalidated by prefix replacement before a later first fragment can mix generations. Static configuration persists through ordinary lifecycle cleanup, with absolute validity continuing to run.

The affected source is restricted to unsealed `src/inet/linklayer/lowpan/` codecs, owner and NED configuration. Existing stateless calls retain defaults. Proposed direct verification: `LowpanStatefulContexts` unit and protocol cases, then existing `LowpanIphc*`, `LowpanNativeReassembly`, `LowpanNativeFragmentBoundaries` and the focused enabled/feature-off matrix. Packet ownership and first-fragment normalization retain the existing packet API; no sealed source is modified.

## Progress

P10/P11 use RFC 6775 and RFC 8505 individual unicast-address registration, including the RFC 8505 base-peer compatibility flow. Later updates are explicitly outside this package: RFC 8928 cryptographic AP-ND, RFC 8929 backbone federation, RFC 9010 RPL signaling, RFC 9685 multicast/anycast subscriptions and RFC 9926 prefix registration. ROVR comparison is an ownership identifier match, not cryptographic proof. The generic ICMPv6 wire prerequisite precedes native registration policy; it preserves variable-length options and decodes DAR/DAC subclasses without a dependency on the optional LoWPAN feature.

P10/P11 first add the separately reviewable generic ICMPv6 wire prerequisite: `Ipv6DuplicateAddressMessage` supports DAR/DAC and extended 8/16/24/32-byte ROVRs through the ordinary ICMPv6 serializer and `Ipv6NdMessage` dispatch. Raw ND options, including variable-sized link-layer addresses, retain their bytes without optional LoWPAN dependencies; existing eight-byte Ethernet options remain typed. Proposed direct coverage is `tests/unit/Ipv6NdRegistrationWire.test`, selected by `opp_run_unit_tests --load . -p inet -m debug --no-build -f 'Ipv6NdRegistrationWire\.test$'` after a fresh debug build. This prerequisite does not claim registration policy or live native ND integration.

P9 is split into a wire-representation/nested-IID preparation commit with `LowpanExtensionNhcWire` unit vectors, a chain-codec commit with independent chain vectors, and production integration with `LowpanExtensionNhc.test` and its reviewer document. Preparation does not claim live extension compression before the production caller is connected.

P9 runtime regression exposed a shared IPv6 prerequisite: local delivery stored a decoded Fragment header's raw pointer after its scan-local owning chunk was destroyed. Preserve a shared chunk reference through reassembly in a separate `ipv6: fix:` commit, verified by literal serialized fragments and the affected route-over/source-fragmentation cases, before enabling NHC reconstruction.

- [x] P8 stateful contexts and production evidence: [p8-verification.json](../../doc/project/evidence/model/6lowpan/p8-verification.json).
- [ ] P9 extension/nested-header NHC and production evidence.
- [ ] P10 optimized ND roles, options and registration.
- [ ] P11 extended registration and later-update profile.
- [ ] P12 mesh-under forwarding and duplicate/hop bounds.
- [ ] P13 page/ESC parsing and transit behavior.
- [ ] P14 virtual fragment forwarding.
- [ ] P15 selective recovery/congestion behavior.
- [ ] Full feature matrix, regression checks, evidence reconciliation, commit audit and final documentation.
