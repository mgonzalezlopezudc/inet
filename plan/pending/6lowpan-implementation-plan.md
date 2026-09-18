# Implement 6LoWPAN adaptation in INET

Status: **proposed; implementation not started**. Written 2026-09-19 against
`c913a63a8335ca39a319cb2089daf835ea5e95cd` and the local, uncommitted standards inventory.
Starting point: [6lowpan-initial-plan.md](6lowpan-initial-plan.md).

## 1. Deliverable and completion boundary

Deliver an optional adaptation layer between IPv6 and IEEE 802.15.4. Its first release
supports route-over, an IPv6 MTU of 1280 octets, RFC 4944 uncompressed dispatch and
fragmentation, and RFC 6282 stateless IPHC plus UDP NHC with the checksum carried.
IPv6 extension headers can travel inline without NHC compression. Unicast initially uses
extended addresses; multicast uses the mandatory short broadcast destination. Codec tests
also cover short-address IID reconstruction before short-address unicast is enabled.

There are two distinct acceptance points:

1. **Simulation data plane:** deterministic single-hop and route-over delivery using the
   existing MAC through an explicitly documented compatibility adapter and static neighbors.
   This proves adaptation behavior, not IEEE 802.15.4 wire interoperability.
2. **First release:** the same behavior over a pinned, unsecured IEEE 802.15.4 frame subset,
   with real extended addresses, short broadcast, correct frame lengths/FCS and external
   packet validation. A statically configured neighbor mode is sufficient. Dynamic ordinary
   ND is included only after its link-address options and resolution boundary are verified.

Do not advertise full RFC 4944/6282 support: stateful compression, extension-header NHC,
mesh-under, legacy HC1 decoding, optimized ND and advanced fragmentation are separate
deliverables. Do not transmit HC1. Do not enable UDP checksum elision through a Boolean
alone: RFC 6282 §4.3.2 requires authorization and qualifying integrity protection.

The eight cached RFCs and their relationships are indexed in the
[standards map](../../doc/project/evidence/protocol/6lowpan/standards.md). That map's
capability levels are broader than this first release and are not test-depth levels.

## 2. Verified starting state and design corrections

The source inspection below is a planning baseline, not a runtime audit.

| Evidence in this checkout | Consequence / action |
| --- | --- |
| No adaptation implementation found under `src/inet` or feature entry in `.oppfeatures`; the [user guide](../../doc/src/users-guide/ch-802154.rst) nevertheless mentions 6LoWPAN | Add a feature and correct the published support statement with its implementation scope. |
| [Ieee802154NarrowbandInterface.ned](../../src/inet/linklayer/ieee802154/Ieee802154NarrowbandInterface.ned) connects upper input → queue → MAC | Insert adaptation before the transmit queue and after receive decapsulation. Define bounded fragment production and queue refusal behavior. |
| [Ieee802154Mac.cc](../../src/inet/linklayer/ieee802154/Ieee802154Mac.cc), `configureNetworkInterface()`, sets MTU from `mac.mtu`, whose default is zero | Set and verify 1280 after initialization; never use frame capacity as the IPv6 MTU. |
| The same MAC's `decapsulate()` sets only the source in `MacAddressInd` | Add destination metadata as an independent prerequisite fix. |
| MAC encapsulation/decapsulation resolves payload protocol through the EtherType group | Merely inserting a module will fail for a new protocol. Define the lower payload contract before bring-up; do not invent a standards EtherType. |
| [MAC header](../../src/inet/linklayer/ieee802154/Ieee802154MacHeader.msg) and [serializer](../../src/inet/linklayer/ieee802154/Ieee802154MacHeaderSerializer.cc) use padded 48-bit addresses and Source PAN ID as protocol metadata | A successful local round trip cannot establish real-wire correctness. Make wire fidelity a first-release prerequisite. |
| [Ipv6.cc](../../src/inet/networklayer/ipv6/Ipv6.cc), `fragmentAndSend()`, uses interface MTU; multicast paths use the Ethernet mapping | Override multicast at the adaptation boundary; preserve normal IPv6 source fragmentation and forwarded Packet Too Big behavior. |
| [Ipv6NeighbourDiscovery.cc](../../src/inet/networklayer/icmpv6/Ipv6NeighbourDiscovery.cc) uses `MacAddress` in resolution and link-address options; MAC creates the IPv6 interface token from that type | Ordinary ND is an integration task, not automatic evidence that native 64-bit neighbors work. Bring up with static bindings first. |
| [Protocol constructor](../../src/inet/common/Protocol.cc) registers instances dynamically | Use a package-owned protocol object through this extension point. The draft's literal new `Protocol::lowpan` member would require a core edit. |
| [Naming rules](../../doc/project/rule/naming.md#nr-ned-role) reserve `*Layer` for compound modules | Use `LowpanLayer` as a compound shell, with a processing module and separately testable helpers. Do not make it a simple module with a conflicting role name. |

Follow [add-a-protocol](../../doc/project/guide/add-a-protocol.md),
[architecture](../../doc/project/rule/architecture.md), and
[test derivation](../../doc/project/guide/derive-tests-from-a-standard.md).
`src/inet/common/packet/` is recursively sealed in the
[seal registry](../../doc/project/audit/seal-list.md); this plan needs no edits there.
If implementation discovers a missing extension point, document that gap and resolve its
design/protection requirements before changing core code. Planning does not authorize a
sealed-source change.

## 3. Architecture and contracts to establish before coding

Proposed package: `src/inet/linklayer/lowpan/`, namespace `inet::lowpan`.
Keep standards/evidence and protocol tests under the existing family name **`6lowpan`**;
do not create a competing `evidence/protocol/lowpan/` inventory.

```text
IPv6 ↔ Ieee802154LowpanInterface
          TX: LowpanLayer → bounded queue → IEEE 802.15.4 MAC → radio
          RX: LowpanLayer ← MAC decapsulation ← radio
```

| Owner | Contract to implement |
| --- | --- |
| `LowpanLayer` / processing module | NED-declared gates, parameters, signals and lifecycle; one owner of datagrams and timers; IPv6 above, LoWPAN below. Use existing queueing contracts where applicable. |
| Dispatch helper | Bounded parsing with positive byte progress; Page 0 initially; classify current IPHC/ESC/paging values without interpreting unsupported encodings as IPv6. FRAGN payload is opaque data, not another dispatch stream. |
| IPHC/NHC codec helpers | Inputs are IPv6 bytes/fields, typed link identities and an immutable context view. Return encoded headers, uncompressed coverage and explicit failure/fallback results. No lookup by module path. |
| Fragment planner | Inputs include per-frame payload budget and codec coverage. Outputs describe original datagram byte ranges and required wire headers. No silent IP-level fragmentation. |
| Reassembly table | Per-interface/PAN-scoped identity, byte/range coverage, original size, tag, expiry and bounded storage. No reuse of a single sequential-stream defragmenter as the RFC algorithm. |
| Link address adapter | Tagged 16-/64-bit identity with PAN scope; no truncation into `MacAddress`. Compatibility mapping is isolated from the codec and never used as external-wire evidence. |
| Static neighbor provider | Explicit `(interface, IPv6 next hop) → native link address` bindings; distinguish next hop from final IPv6 destination. Unknown bindings have a defined drop/resolve outcome. |
| MAC boundary | Report source and destination, PAN, actual addressing modes and payload protocol locally. Derive frame capacity from actual wire overhead. No protocol marker hidden in a PAN field on the native path. |
| Packet model/tooling | Chunks, serializers, dissector and printer live with the new protocol and register through existing APIs. Packet tags carry local metadata, never surrogate wire fields. |

Define NED parameters with units and validation: IPv6 MTU, frame-size profile, addressing,
static bindings, reassembly timeout `(0, 60 s]`, maximum buffered bytes/datagrams, and
compression enablement. Pin concrete defaults in the first implementation PR. Declare
stable datagram, fragment and reassembly outcome signals with typed drop/rejection reasons
for malformed input, unsupported format, context failure and resource limits. Define each
event's owner and counting boundary. Observe queue drops through the selected queue's
existing signals; do not count the same loss again as a separate adaptation loss.

Separate `127-octet PSDU limit` from PHY overhead. For every frame compute
`LoWPAN capacity = PSDU limit − MAC header − security overhead − FCS`.
FRAG1, FRAGN and dispatch overhead consume that capacity too. Initial native frames are
unsecured; reject unsupported security modes explicitly instead of pretending to model them.

Tag contract tests must verify `PacketProtocolTag`, dispatch requests, interface and link
address indications at each boundary. After reconstruction the upper packet is IPv6, with
the correct interface indication and no stale lower-layer dispatch request. Each outgoing
fragment has the correct next-hop request. Region metadata and ownership survive slicing
and reassembly according to the existing packet API.

### 3.1 Adaptation-facing link contract — required by P2a

Native address values and request/indication tags belong to the IEEE 802.15.4 domain and
are available before the native frame codec. They identify address mode, full 16-/64-bit
value and PAN scope. LoWPAN codecs, static neighbors and fragment/reassembly logic use
these values from the outset; `MacAddressReq`/`MacAddressInd` occur only on the legacy
side of the compatibility adapter. P1 repairs that legacy receive boundary alone.

The initial compatibility mode uses an explicit, immutable table mapping each configured
native unicast identity to a legacy unicast `MacAddress`, with an inverse receive mapping.
Require uniqueness throughout the legacy MAC delivery/filtering domain, not just within
one node's neighbor table. Reject collisions, reserved/broadcast aliases and inconsistent
peer tables at initialization; never truncate or derive native identities from alias bits.
An unknown next-hop binding or received alias produces a named drop. The legacy MAC uses
aliases for filtering, sequence/retry state and ACK behavior; adaptation uses native values.

Configure one PAN per compatibility link. Map native short `0xffff` to legacy broadcast;
recover destination PAN from the configured receiving link and source identity from the
inverse table. Reject configurations where different PANs share a legacy delivery domain
without enforced isolation: aliases and configuration do not create real PAN filtering.
Bindings remain fixed while TX/RX state exists; reconfiguration requires interface teardown.
Native operation in P6 replaces this mapping with indications decoded from actual headers.

Use a configured single upper payload protocol for this interface. In LoWPAN mode the MAC
bypasses EtherType lookup and the PAN-as-protocol convention, delivers opaque payload bytes
to the adapter, and selects local dispatch tags from receiver configuration. LoWPAN parses
its dispatch from those bytes. No added wire discriminator or sender-only request tag is
used to classify receive traffic. Mixed upper MAC clients are unsupported in this mode;
existing configurations retain their legacy payload-selection behavior.

Above adaptation, `PacketProtocolTag` identifies IPv6. Below adaptation it identifies the
package-owned LoWPAN protocol; separate tags carry native next-hop requests. The adapter translates
those requests to legacy requests; the MAC owns encapsulation and its IEEE 802.15.4 protocol
tag. On receive, the adapter reconstructs native indications and removes legacy requests;
reassembly delivers IPv6 with `InterfaceInd` and no stale lower dispatch request. Tests
must inject received bytes without preserved transmit tags.

### 3.2 Lower payload-budget contract — required by P2a

The lower boundary owns frame-overhead calculations. Prepare a transmission using its
native source/destination/PAN and supported MAC options; return usable payload octets and
an immutable envelope describing the addressing modes, frame version, PAN compression,
security and FCS accounting that make that budget valid. The fragment planner consumes
the budget without duplicating MAC-header arithmetic. Compatibility budgets describe the
actual legacy representation; native budgets describe the selected IEEE frame subset.

Initially enforce one homogeneous envelope for every fragment of a datagram, including
retries, and retain it through queueing. Preparation fails explicitly for unsupported
options. If configuration invalidates the envelope before transmission, cancel pending
fragments with a named outcome; never transmit using a stale budget. A future boundary
allowing different envelopes must supply each fragment's budget and replan accordingly.
A fixed profile is sufficient only when it validates and freezes these assumptions.
Pin the native frame subset in P0 so the contract can express its requirements before P6.

## 4. Implementation sequence and exit gates

Each numbered item is a reviewable work package, normally one PR. Keep behavior, wire
representation and its direct tests together; split oversized packages by the substeps
listed here. All checkboxes are initially open.

### P0 — Pin scope and derive the checks

Dependencies: none.

- [ ] Record the exact source revision, local changes, RFC text hashes and selected clauses.
  Use the cached texts; check applicable updates/errata before asserting conformance.
  Pin the IEEE 802.15.4 edition and frame subset before P2a; the RFC cache does not supply it.
- [ ] Extend the existing RFC catalogs for the complete supported TF/address-mode matrices,
  malformed input, disassociation cleanup and compression/fragmentation interaction.
  Existing catalogs explicitly state that they are selective.
- [ ] Publish separate IPHC compressor-selection and decompressor-acceptance matrices:
  TF, HLIM, NH, SAC/SAM, M/DAC/DAM and CID combinations; inline lengths; link-local and
  non-link-local addresses; unspecified source; IID derivation prerequisites; multicast
  forms; and reserved/unsupported outcomes. Cover legal encodings the compressor does not
  select. Distinguish the context-independent unspecified source from stateful compression.
- [ ] Create standards-only `features.md` and `checks/` under
  `doc/project/evidence/protocol/6lowpan/`. Put implementation mappings and planned/run
  status under `doc/project/evidence/model/6lowpan/`, following the derivation guide.
- [ ] Record the contracts in §3, especially protocol registration, lower dispatch,
  interface initialization, queue ownership and static neighbor addressing.

Exit: every first-release claim has a clause, a planned check, a test category and an
implementation owner. Missing framework reachability is recorded explicitly. No run or
test-depth achievement is inferred from the inventory.

### P1 — Repair existing receive metadata

Dependencies: P0 scope; independent of the new feature.

- [ ] Set destination as well as source in `Ieee802154Mac::decapsulate()`.
  This fixes legacy `MacAddressInd`; it does not define native LoWPAN link identity.
- [ ] Add a production-MAC receive test observing both addresses, interface and payload
  protocol; include unicast and broadcast. Exercise the real receive/decapsulation path.
- [ ] Run focused existing IEEE 802.15.4 regressions and record any changed expectations.

Exit: upper receive indications contain the transmitted destination and source, with no
unexplained change in existing MAC behavior.

### P2a — Native link contracts and compatibility boundary

Dependencies: P0, P1.

- [ ] Add `Lowpan`, `LowpanExamples`, `LowpanTests` feature entries with discovered dependencies.
  Keep the core independent of optional applications/examples; use the feature decomposition
  prescribed by the add-a-protocol guide and verify the actual dependency graph.
- [ ] Implement IEEE 802.15.4-owned native address values and request/indication metadata,
  the static next-hop provider, and the compatibility mapping contract in §3.1.
- [ ] Implement the lower preparation/budget contract in §3.2; test extended unicast and
  short broadcast requests, envelope validity, unsupported options and collision rejection.
- [ ] Register package-owned protocol identity and implement configured opaque lower
  payload selection. Test receive classification from bytes without transmit-side tags.
- [ ] Pin C++/NED contract names, ownership, validation and failures. Demonstrate that the
  future native boundary can supply the same adaptation-facing semantics without aliases.

Exit: contract tests prove reversible configured mappings, PAN isolation, tag transitions
and budget validity; feature-off build succeeds. No adaptation helper depends on legacy
address width or header math.
P2b cannot start with unresolved mapping, payload-selection or capacity contracts.

### P2b — Uncompressed adaptation and simulation bring-up

Dependencies: P2a.

- [ ] Add `LowpanLayer`, its processing contract and `Ieee802154LowpanInterface`.
- [ ] Register packet tooling. Implement LOWPAN_IPV6
  dispatch (`0x41`) and bounded rejection of malformed/unsupported input.
- [ ] Integrate the tested P2a adapter, payload-selection and preparation contracts.
- [ ] Configure MTU 1280 and test the effective value after all initialization stages.
  Apply multicast-to-link-broadcast mapping. Start with static IPv6 addresses, routes and
  next-hop bindings through the explicitly named compatibility mode.
- [ ] Add small UDP and ICMPv6 exchanges and test start/stop/crash/restart ownership.

Exit: single-frame packets enter through IPv6, cross the actual MAC/radio path and return
as the original IPv6 bytes. Packets exceeding single-frame capacity produce an adaptation
drop with reason `fragmentationUnavailable` until P3; do not lower the advertised IPv6 MTU
or report a smaller IP MTU. Examples use verified fitting packets. The intermediate feature
documentation explicitly states that this bring-up milestone is not release-capable and
does not yet deliver all packets admitted by MTU 1280. Feature-off build succeeds.

### P3 — RFC 4944 fragmentation and bounded reassembly

Dependencies: P2b; agree the compressed-header coverage contract and the reassembly/reuse
policies below before implementing.

- [ ] Add FRAG1/FRAGN chunks and serializers with independent golden vectors. Use original
  IPv6 size and eight-octet offset units; strip encapsulation dispatch from reconstructed IP.
- [ ] Fragment only when needed. Produce non-final coverage aligned to eight octets and
  increment the 16-bit tag per fragmented datagram, including wrap.
- [ ] Key initial route-over reassembly by receiving interface/PAN, native link source,
  native link destination, size and tag. Accept FRAGN before FRAG1 and interleaved senders.
  Mesh originator/final-destination substitution belongs to P12; fabricate no such metadata.
- [ ] Track ranges and reject out-of-bounds/invalid lengths before allocating or writing.
  Implement RFC 4944 §5.3 overlap discard precisely. Ignore byte-identical same-range
  duplicates without extending expiry. On partial overlap discard accumulated fragments;
  pin whether to restart with the incoming fragment (permitted by the RFC) or reject it.
  For same-range conflicting bytes, specify a separate local hardening policy. Before P3
  coding, record whether rejected keys permit later fragments to create a fresh context or
  remain quarantined, with bounded storage and a fixed expiry for any quarantine. Quarantine
  is not an RFC requirement and must not silently replace its overlap semantics.
- [ ] Pin a deterministic tag-reuse policy before P3 coding: sender guard lifetime and its
  scope, behavior at guard exhaustion, restart handling, and receiver behavior when an active
  key is reused. Preserve increment-and-wrap semantics. State the assumed maximum stale
  fragment lifetime; a sender guard only mitigates collisions under that assumption.
  Nonconflicting fragments from different generations with the same complete key cannot
  be distinguished on the wire. Do not claim that a guard or quarantine solves that ambiguity.
- [ ] Start expiry on the first received fragment, including FRAGN; duplicates do not
  extend it. Bound total bytes and datagrams with an explicit admission/eviction policy.
- [ ] Flush partial RX and pending TX on disassociation where modeled and on interface
  teardown; cancel timers and release packets on stop/crash/restart. If no association
  event exists in the chosen MAC, state that scope limit and test the lifecycle path available.
  Pin the actual event provider and affected interface/PAN scope; do not infer peer-specific
  disassociation. Honor RFC 4944 §5.3 cleanup within that scope. Static neighbor configuration
  is separate from transient TX/RX state; define its restart retention explicitly. Future
  compression-context invalidation belongs to P8.

Exit: 1280-octet IPv6 datagrams are delivered byte-exactly through multiple MAC frames;
loss, overlap, exhaustion and timeout never produce partial upper-layer delivery. Tests
cover all rows marked P3 in §5, including observable timer/storage cleanup.

### P4 — Stateless IPHC

Dependencies: P3, P2a native link contracts and completed P0 encode/accept matrices.

- [ ] Implement all TF and HLIM modes; inline NH; stateless unicast/multicast forms;
  unspecified source; fully inline fallback when IID elision is invalid. Test all supported
  legal encodings and reserved combinations, not only encodings selected by the compressor.
- [ ] Derive extended-address IIDs by the RFC 6282 U/L-bit rule and short-address IIDs by
  its `0000:00ff:fe00:XXXX` rule. Do not conflate that rule with RFC 4944 §6's PAN-based
  short-address autoconfiguration procedure.
- [ ] Reconstruct IPv6 payload length from the complete uncompressed datagram coverage,
  never from the compressed FRAG1 length. Validate every inline-field read.
- [ ] Integrate compression with fragmentation: all compressed headers fit wholly in
  FRAG1; later fragments contain original datagram bytes at original IPv6 offsets.
  If a header cannot fit there, leave affected headers uncompressed or select LOWPAN_IPV6,
  then recompute coverage and alignment. Uncompressed header bytes may extend into later
  fragments; a larger total encoding can therefore satisfy the placement constraint.
  Reject transmission only when no supported representation yields a valid fragment plan
  (RFC 6282 §2). Distinguish this placement fallback from invalid-IID/unsupported-compression
  fallback, and test both.

Exit: independent golden bytes and production-path tests agree for every selected mode,
including a compressed packet whose original size requires several fragments. Context-based
forms remain explicitly unsupported except the context-independent unspecified-source form.

### P5 — UDP NHC and route-over data-plane acceptance

Dependencies: P4.

- [ ] Implement all four port modes, infer UDP length and preserve the carried checksum.
  C=1 input without the required integrity support is dropped; no checksum-elision option
  is offered in this release. Use computed checksums in byte/capture validation.
- [ ] Test port-range endpoints and values just outside each compression range.
- [ ] Add a three-node, two-link route-over network with two distinguishable flows. Observe
  complete reassembly before IPv6 forwarding, Hop Limit decrement, and new per-hop
  compression/fragmentation with the outgoing next-hop identity.
- [ ] Test forwarded IPv6 packets larger than 1280 for ICMPv6 Packet Too Big, and locally
  originated packets larger than 1280 through existing IPv6 source fragmentation. Datagrams
  at or below 1280 acquire no IPv6 Fragment header solely because they span MAC frames.
  LoWPAN fragmentation operates on each resulting complete IPv6 packet's bytes, including
  any IPv6 Fragment header already supplied by source fragmentation.

Exit: the **simulation data plane** acceptance point in §1 is met. Its evidence still
labels the compatibility MAC as non-interoperable.

### P6 — Native IEEE 802.15.4 address and wire boundary

Dependencies: P0 pinned IEEE edition/subset and P2a contracts; algorithm development P2b–P5 can proceed
before this package completes. P6 must complete before first-release acceptance.

- [ ] Integrate the P2a IEEE 802.15.4-owned native address representation and request/
  indication metadata through native MAC filtering, retries,
  ACK handling, neighbor bindings and IPv6 interface-token construction. Avoid widening
  generic `MacAddress` as an incidental change.
- [ ] In a separate reviewable substep, implement the chosen data/ACK frame subset's frame
  control, PAN/address modes, PAN compression, byte order, variable header length and
  two-octet FCS. Remove the PAN-as-protocol convention on this path. Align simulated chunk
  length, serialized byte count and the MAC/radio size budget.
- [ ] Support extended unicast and short `0xffff` broadcast with the actual destination
  PAN. Explicitly reject unsupported frame versions/security/forms. Broader short-unicast
  operation may follow after its allocation and neighbor semantics are tested.
- [ ] Replace compatibility bindings in the release examples with native bindings. Preserve
  or explicitly migrate existing 802.15.4 configurations and document changed fingerprints.
- [ ] Produce captures with the correct link type and FCS convention. Decode them with
  an independent dissector and exchange serialized vectors in both directions with a pinned
  external 6LoWPAN implementation. Record peer version, setup and exact supported subset.

Exit: all selected address bits survive round trips, every selected frame fits 127 octets,
PAN fields carry PANs, and external byte validation agrees. Wireshark decoding alone is
not peer interoperability. If no external peer is available, record the missing evidence
and leave the first-release gate open.

### P7 — First-release integration and documentation

Dependencies: P5, P6.

- [ ] Re-run the complete first-release matrix on the native MAC path, including feature-off
  and existing IPv6/802.15.4 regression selections. Keep compatibility results separate.
- [ ] Assess ordinary ND as its own substep: encode native link-address options, resolve
  64-bit next hops without truncation, and test link-local creation, DAD, RS/RA and NS/NA.
  If this needs a wider neighbor-cache refactor, retain documented static-neighbor operation
  for the first release and track dynamic ND as outstanding. Do not call that RFC 6775 ND.
- [ ] Publish minimal single-hop and route-over examples, limitations, feature/configuration
  instructions and release/migration notes. Update model coverage with actual commands,
  verdicts and artifacts, and correct the old user-guide statement.

Exit: every first-release acceptance row passes, or the release scope explicitly excludes
the behavior before a claim is made. An unexplained failure in a claimed behavior blocks
completion. Move this plan to `plan/done/` only when its agreed scope is completed; do not
silently mark the later milestones below implemented.

## 5. Required test matrix

Use standards-derived expected bytes independently of the implementation. Round-trip tests
are useful but can conceal a compressor and decompressor sharing the same error.

| Package / claim | Deterministic stimulus and invariant | Evidence category |
| --- | --- | --- |
| P1 metadata | Real MAC receives unicast and broadcast; upper source, destination and interface equal the transmitted values | Module, plus protocol receive observation |
| P2a address boundary | Asymmetric 64-bit identities survive alias/inverse mapping; duplicate/reserved aliases, inconsistent tables, unknown peers and unisolated PAN configurations fail explicitly; no native identity comes from alias bits | Unit + module contract checks |
| P2a payload/tags | Inject opaque received bytes without TX tags; configured protocol reaches dispatch parsing; observe IPv6/LoWPAN/MAC protocol transitions and native request/indication reconstruction | Production MAC/adapter injection |
| P2a budget | Unicast/broadcast preparation returns capacity for the actual lower envelope; unsupported options fail; queued fragments retain the envelope or are cancelled on invalidation | Unit + module frame-length assertions |
| P2b dispatch | Valid `0x41`, empty/truncated fields, unknown dispatch, `0x7f` recognized as IPHC; unsupported ESC/page input is not delivered as IPv6 | Unit vectors + production protocol injection |
| P2b temporary limit | Effective MTU remains 1280; a valid packet exceeding single-frame capacity has exactly one `fragmentationUnavailable` drop and no lower transmission | Production adaptation boundary |
| P2b/P6 MTU and multicast | Initialized interface reports 1280; `ff02::1` emits link broadcast; native mode has short destination `0xffff` and correct PAN | Module + protocol |
| P3 frame boundaries | For each supported overhead profile, enumerate sizes around no-fragment/FRAG1/FRAGN capacity (`boundary−1`, `boundary`, `boundary+1`) and each eight-byte alignment edge through 1280 | Unit planner + protocol frame-length/offset assertions |
| P3 reassembly | In-order, reverse order, FRAGN first, duplicate, missing last, two interleaved peers, same tag across interfaces/PANs; exactly one delivery on completion | Protocol with controlled injection/delay |
| P3 invalid input | Partial overlap, conflicting same-range duplicate, offset past size, inconsistent size, truncated header, invalid non-final alignment; later fragments exercise the selected restart/quarantine policy and its expiry | Unit + protocol; no partial delivery or out-of-range writes |
| P3 resource/lifecycle | Fill each configured limit including any quarantine; late fragment after timeout; events just before/at/after expiry; stop/crash/restart with pending TX/RX; modeled disassociation scope and static-binding retention | Module state/ownership checks + protocol absence, with a guard proving stimulus arrived |
| P3 tag wrap/reuse | Tags 65534, 65535, 0; active-key reuse, sender guard expiry/exhaustion and restart under the pinned policy; document indistinguishable nonconflicting generations | Unit + protocol |
| P4 IPHC | Exhaust legal supported TF, HLIM, SAM/DAM, multicast and unspecified-source forms; asymmetric addresses, IID mismatch, reserved combinations | Independent byte vectors + production codec paths |
| P4/P5 compressed fragmentation | Known IPv6/header/payload bytes; FRAG1 compressed size differs from original coverage; compressed header exactly fits/exceeds capacity; legal uncompressed fallback spanning fragments, NHC fallback and no-valid-plan rejection | Protocol; size and FRAGN offsets compared with original IPv6 bytes |
| P5 UDP | Four port modes including range boundaries; fragmented/unfragmented UDP; checksum preserved; C=1 without integrity rejected | Unit vectors + protocol payload/checksum assertions |
| P5 routing/IP MTU | Two hops, different next hops, mixed compressed/inline headers; exact bytes except forwarded Hop Limit; 1280 and 1281-byte IPv6 inputs | Network behavior + protocol boundary observations |
| P6 wire | Extended unicast, short broadcast, asymmetric PAN/address values, ACK/retry behavior and 127-octet limit | Serialization + protocol + external validation |
| P7 optional ordinary ND | Native link-address options and dynamic resolution; no static binding hides failure | Protocol; required only if dynamic ND is claimed |

Pin configuration, seed (initially 0), source/destination addresses, PANs, data bytes and
fault schedule. Deterministic byte/state claims need no broad random campaign. If a later
claim concerns contention, mobility or recovery probability, first define a finite campaign,
parameters, seeds and acceptance statistic; do not use repeated reruns to hide a failure.

Core normative anchors in the cached texts:

- [RFC 4944](../../doc/project/evidence/standard/rfc4944/rfc4944.txt): §3 lines 217–221
  (multicast), §4 lines 241–255 (MTU), §5 lines 358–371 (encapsulation order), §5.3
  lines 569–687 (fragment fields, overlap, disassociation and timeout).
- [RFC 6282](../../doc/project/evidence/standard/rfc6282/rfc6282.txt): §2 lines 235–244
  (uncompressed offset coordinates and first-fragment constraint), §3.1.1 (encoding
  matrices), §3.2.2 lines 645–697 (IID derivation), §4.3.2 lines 979–1032 (checksum
  elision prerequisites), §4.3.3 lines 1089–1107 (UDP ports and length).
- [RFC 8025 catalog](../../doc/project/evidence/standard/rfc8025/catalog.md) and
  [RFC 8066 catalog](../../doc/project/evidence/standard/rfc8066/catalog.md): current page,
  ordering and unknown-ESC processing rules. An endpoint dropping unsupported extensions
  does not establish full paging support or justify dropping opaque transit extensions
  in a future forwarding implementation.

## 6. Follow-on deliverables

These have separate exit gates and must not expand the initial critical path implicitly.

| Package | Dependencies and actionable scope | Acceptance gate |
| --- | --- | --- |
| P8 static stateful IPHC | P7; table of context ID, prefix/length, compression permission and validity; CID=0 and nonzero SCI/DCI; stateful unicast/multicast; keep decompression eligibility distinct from permission to compress | Golden vectors and production cases for valid/missing/expired contexts, unequal source/destination contexts and context changes during reassembly; pin the context version or invalidate affected partial datagrams explicitly |
| P9 extension-header NHC | P8; RFC 6282 §4.2 EIDs, NH chaining, lengths, padding and nested IPv6; leave unsupported headers inline | Every supported EID decoded independently; 255/256-octet compressed-length boundary and first-fragment fit/fallback cases pass |
| P10 RFC 6775 ND | P7 native addresses, P8 context owner; catalog dependencies including RFC 4861/4862; 6LN/6LR/6LBR roles, ARO, 6CO, ABRO, registration lifetimes, duplicate handling and multihop registration; reuse existing ND machinery where its contracts fit | Registration success/duplicate/cache exhaustion/expiry, context distribution and restart; registered hosts do not rely on multicast address resolution |
| P11 RFC 8505 registration | P10; EARO, ROVR, transaction recency, refresh/movement and proxy behavior; select applicable later updates explicitly | Mixed base/extended peers per §6; stale registrations cannot supersede newer ownership; conflict, renewal, expiry and movement cases |
| P12 mesh-under | P7; Mesh/BC0 serializers and forwarding, originator/final-destination identities, Hops Left/Deep Hops Left, broadcast duplicate suppression | Multi-hop unicast/broadcast, loop/duplicate bounds, hop exhaustion, correct reassembly identities and header ordering |
| P13 paging/ESC | P7 dispatch contract; implement selected RFC 8025/8066 page/extension semantics and explicit unsupported-type outcomes | Page reset per packet, switch/order cases, IPHC on supported pages, unknown EET processing vs opaque forwarding |
| P14 fragment forwarding | P7 plus required dispatch subset; RFC 8930 VRB allocation, outgoing tag mapping, resource/timeout paths and routing changes | Observe forwarding before full reassembly; two concurrent flows, loss, collisions and bounded cleanup |
| P15 selective recovery | P14 where shared forwarding is used; separate RFC 8931 RFRAG/RFRAG-ACK codec, retransmission window, recovery and congestion state | Selective retransmission under controlled loss, retry/window limits, cleanup and compressed-datagram coordinates; never reuse RFC 4944 offset semantics accidentally |

## 7. Execution and review procedure

For each implementation package, record changed path/symbol → named case mapping first.
Follow the current [gate commands](../../doc/project/guide/run-the-gates.md) and
[protocol runner instructions](../../tests/protocol/lib/AUTHORING.md#10-running-tests).
Planned protocol suite: `tests/protocol/6lowpan/`; the runner discovers it from `.test`
files. Unit and module cases follow the existing flat suite conventions with `Lowpan*`
names. Discover their supported case selectors before executing them.

Example future commands, from the checkout root after those cases exist:

```sh
make -j$(nproc) MODE=debug
inet_run_protocol_tests -p inet -m debug -w '^6lowpan$' -f 'LowpanFragment.*'
doc/project/enforcement/check-architecture.sh src/inet/linklayer/lowpan
doc/project/enforcement/check-naming.sh src/inet/linklayer/lowpan
```

Replace the example file selector with each package's actual cases; verify nonzero
selection. Also scope gates to IEEE 802.15.4 or IPv6/ND when changed. Before submission,
run debug/release compilation, feature-off compilation, the project-wide mechanical
gates and the recorded focused tests required by the linked guide. Fingerprints are
additional regression evidence, never the correctness proof for new behavior.

Each package closes with: exact command/filter, mode, configuration/run/seed, source
revision, exit status, actual case count/verdicts and artifact paths. Record build/setup
errors separately from behavioral failures. Baseline changes require an explained behavior
change and the project's baseline procedure. Packet captures retain the link type, FCS
mode, peer version and generated input bytes needed to reproduce external validation.

This planning change requires no simulation build. The supplied root `AGENTS.md` also
requests skill-package validation, but this checkout has no `scripts/validate_skill_suite.py`,
`scripts/package_skill_suite.py` or `tests/skill-suite/`. Record those checks as unavailable;
do not substitute tests from another checkout or claim a skill-package pass.
