# 6LoWPAN adaptation and link integration contracts

> **Kind:** design · **Status:** draft · **Seal:** none · **Owns:** — · **Stands on:** [implementation plan](../../../../../plan/pending/6lowpan-implementation-plan.md), [encoding profile](encoding-profile.md)

These are implementation contracts. They do not claim an implemented feature.

## Identity and dispatch

IEEE 802.15.4 owns `Ieee802154Address`: explicit unspecified/short/extended modes,
16-/64-bit value without truncation. Link requests/indications additionally carry source
and destination PAN identifiers. Broadcast is short 0xffff; aliases are legacy unicast
MacAddress values from a validated shared, immutable table. Codecs never see aliases.

The adapter implements the plan §3.1 mapping and PAN-isolation contract. Static neighbor
lookup uses outgoing interface plus IPv6 next hop, never final destination for routed
traffic. Unknown neighbors drop with an observable reason. Multicast selects native
short broadcast directly. MAC filtering/retries use aliases only in compatibility mode.

The package-owned `lowpanProtocol` registers through Protocol's constructor. The configured
MAC upper payload selector bypasses EtherType lookup in LoWPAN mode. Receive selection
uses local configuration and received bytes only; legacy default behavior is preserved.

## IPv6 reachability

`Ipv6::resolveMACAddressAndSendPacket()` calls `Ipv6NeighbourDiscovery::resolveNeighbour()`
before the interface can see a unicast packet. The latter returns a MacAddress and is not
virtual. Inserting adaptation alone cannot implement native static next-hop resolution.
`Ipv6::resolveMACAddressAndSendPacket()` is virtual and its existing protected
`fragmentPostRouting()` preserves IPv6 source fragmentation and forwarded Packet Too Big.

Use a feature-owned IPv6 specialization for statically configured LoWPAN interfaces:
retain the routed next hop in `NextHopAddressReq`, bypass ordinary legacy resolution on
those interfaces, then enter `fragmentPostRouting()`. Non-LoWPAN interfaces use the base
implementation. Compose this specialization in a feature-owned network layer through the
existing node `INetworkLayer` slot. No generic MacAddress widening, fake point-to-point
flag or fabricated resolved MAC address is permitted. Preserve forwarding counters,
post-routing hooks, Hop Limit handling and interface MTU. This composition must be proved
by P2b/P5 production tests before claiming IPv6 integration; MIPv6/IPsec are outside its
initial static profile.

Source fragmentation constructs fresh Packets without copying request tags. Override
`fragmentAndSend()` with scoped, nest-safe save/restore of the native next-hop context,
and `sendDatagramToOutput()` to restore that context onto each resulting IPv6 fragment.
The scope begins after deferred post-routing hooks, not around initial resolution.
Never substitute the final IPv6 destination for a routed next hop.

Use the existing NetworkInterface protocol property as the service marker: LowpanLayer
sets it to the package-owned lowpanProtocol and registers that service at its upper gates
at LINK_LAYER initialization. No marker-only InterfaceProtocolData or core field ID is
needed. LowpanIpv6 recognizes that exact protocol identity and delegates other interfaces.

Ordinary ND cannot be disabled completely by existing parameters: zero DAD attempts still
lead to router discovery and the first RS is unconditional even with zero retry count.
The static-profile composition therefore also selects LowpanIpv6NeighbourDiscovery.
It specializes existing virtual initiateDad/startRouterDiscovery/createRaTimer,
processNDMessage and sendRedirect seams for marked interfaces only; ordinary interfaces
retain base behavior. On LoWPAN, configured addresses become permanent without invoking
the base helper that schedules RS; received ND is rejected without SLAAC/cache/response
side effects, and RA/RS/Redirect generation is suppressed. Native link-local identity
must replace the legacy alias-derived interface token before IPv6 address assignment.
Startup and restart tests must prove this, including a mixed-interface node's unchanged ND.

Initialization correction: IPv6 creates link-local addresses at LINK_LAYER too, so native
token assignment must precede that stage. LowpanCompatibilityMac overrides
configureNetworkInterface at NETWORK_INTERFACE_CONFIGURATION, calls the base and then
sets the native EUI-64-derived token (U/L flipped, InterfaceToken low word first), MTU
1280 and lowpan protocol. The first static profile requires extended local identities;
short broadcast remains supported. Token/protocol survive NetworkInterface reset and
are reused when IPv6 data is recreated on restart. LowpanLayer registers the service
at LINK_LAYER without depending on intra-stage module order. The domain fixture checks
the effective MTU and token; P2b must add IPv6 address/restart observations.

## Budget, queue and lifecycle

The lower boundary prepares an immutable transmission envelope and returns usable octets.
No LoWPAN helper calculates MAC overhead. Fixed native same-PAN envelopes budget 104
(unicast) or 110 (broadcast) octets, including the two-octet FCS subtraction; the legacy
path uses its own actual overhead. Packet slicing preserves region metadata.

One processing module owns input datagrams, pending fragments and reassembly timers.
Bound pending datagrams and buffered bytes. Queue admission uses existing push/pull
contracts; refusal drops unsent fragments of that datagram with one reasoned outcome.
No unbounded loop emits fragments regardless of queue capacity. Stop/crash cancels timers
and clears transient state; restart reloads immutable static bindings. No peer-specific
association event is synthesized on the legacy path.

Use bounded `PacketQueue` with no dropper: DropTailQueue reports push capability even
when full, and PacketQueue does not notify producers after pulling. Check admission
for each prepared packet, then push synchronously; refusal discards the unsent remainder.
Do not wait for a capacity callback. Remove owned pending state before handoff because
the queue may synchronously notify the MAC and the MAC may immediately pull.

The compatibility serializer emits 23 header octets and no FCS; its default simulated
9-octet header is unsuitable for a truthful compatibility budget. The compatibility
profile will require a 23-octet simulated header, 127-octet frame limit and no FCS,
yielding 104 payload octets for both aliases and broadcast. These happen to equal the
selected native unicast budget but have different overhead semantics. A feature-owned
MAC validates each immutable prepared envelope before encapsulation, including its
local alias, destination alias and profile. Native wire conformance remains P6.

## Reassembly and reuse policy selected for P3

Key: interface, source/destination PAN and native source/destination identities, original
size and tag. Exact same-range bytes are ignored without extending expiry. Partial overlap
resets accumulated state; restart from the newly received valid fragment with a fresh
first-received deadline (the RFC's permitted restart behavior). Same-range conflicting
bytes discard the context and quarantine that key until its original deadline. Quarantine
occupies a bounded context slot and cannot extend its deadline. Capacity exhaustion rejects
new keys; it does not evict an unrelated active datagram. Expire before accepting arrivals
at the deadline, independently of event insertion order. Timeout default is 60 seconds.

Sender tags increment modulo 65536. Keep per-source/PAN reuse guards: a tag becomes
available only after the configured maximum lower transit lifetime plus 60 seconds from
its last fragment's handoff. Reject a new datagram if the next tag is still guarded; do
not skip tags or spin. Preserve guards across stop/start; a new simulation has no prior
state. Tests must state the lower transit bound and verify the configured envelope makes
it valid. A restart that discards guard history requires a quiet interval of that bound.
This policy mitigates reuse only under its stated delay bound; nonconflicting generations
sharing a complete wire key remain indistinguishable. Default lower transit bound and its
MAC/queue enforcement must be resolved before P3 code, not inferred from reassembly timeout.

## P2a pre-write contract: native values and metadata

- Owner/invariant: IEEE 802.15.4 domain stores unspecified, short and extended addresses
  distinctly; construction/parsing rejects invalid width/mode rather than truncating.
- Entry/control path: codec and adapter callers construct values and carry them in native
  request/indication tags; this first substep does not claim production adapter reachability.
- Artifacts: `Ieee802154Address.h/.cc/.msg`, `Ieee802154AddressTagBase.msg` under
  `src/inet/linklayer/ieee802154/`; `tests/unit/LowpanNativeAddress.test`.
- Sibling/terminal paths: invalid parse throws without modifying an existing value;
  metadata duplication preserves both PANs and identities. No timers, packets or module
  ownership are introduced by value operations.
- Boundaries: unsigned 16/64-bit address values; mode 0 unspecified, 2 short, 3 extended;
  all address bits survive comparison, parsing and message serialization.
- Verification: fresh debug build and `inet_run_unit_tests -m debug -f
  'LowpanNativeAddress\.test$'`; native asymmetric high bits, short/extended distinction,
  invalid syntax/width, broadcast, tag duplication. Production reachability is explicitly
  still owed by the adapter substep, not implied by this helper test.

Self-validation: generated-value conventions were checked against MacAddress.msg and
MacAddressTag.msg; existing IEEE 802.15.4 feature owns these paths; no sealed path changes.

## P2a pre-write contract: configured legacy payload selection

- Owner: Ieee802154Mac selects one configured upper payload protocol on an opt-in
  interface; empty configuration preserves the current EtherType behavior.
- Production path: initialize resolves the configured protocol by registered name;
  encapsulate checks the request's PacketProtocolTag without an EtherType lookup;
  decapsulate uses receiver configuration, not the received networkProtocol field.
- Artifacts: Ieee802154Mac.h/.cc/.ned and LowpanOpaquePayload module test.
- Wire/terminal semantics: configured mode writes a constant 0xffff into the legacy
  Source PAN field, never a protocol number; it remains a non-native compatibility
  representation. Wrong TX payload protocol is a configuration error before insertion.
  Existing ACK/filter/queue/lifecycle paths and default MAC behavior remain unchanged.
- Evidence: inject headers whose protocol field is deliberately unmapped and without
  TX tags, then transmit a registered protocol absent from the EtherType group. Observe
  configured RX metadata and actual radio transmissionStarted after production MAC encapsulation (the MAC data
  path uses sendDelayed and does not emit packetSentToLower).
  Fresh debug build; module filter `LowpanOpaquePayload\.test$`; rerun P1 and the
  same three existing fingerprints after this MAC change.

Self-validation: Protocol::getProtocol(name) is the existing registry lookup; configured
selection needs no core protocol member, PAN discriminator or serializer edit.

## P2a pre-write contract: alias and static-neighbor value table

The package-owned `LowpanLinkAddressMap` owns immutable-after-configuration peer values
for one PAN/delivery domain. `addPeer()` rejects unspecified/broadcast native identities,
non-unicast legacy aliases and duplicate native/alias values before mutation. `bind()`
accepts only known native peers and unicast, specified IPv6 next hops; duplicate IP keys
are rejected. Forward/inverse/next-hop queries never insert state and report absence.
The adapter must validate shared domain ownership separately; helper success alone does
not prove PAN isolation. Production entry remains the P2a adapter configuration path.

Artifacts: `src/inet/linklayer/lowpan/LowpanLinkAddressMap.h/.cc`, package-owned protocol,
package.ned and `.oppfeatures` core entry; `tests/unit/LowpanLinkAddressMap.test`.
Verification: debug build, that explicit unit filter, naming and feature-off compilation.
Allocation is standard container ownership; rejected configuration never transfers Packet
ownership. The lowpan package depends on IEEE 802.15.4/IPv6, not applications or examples.

## P2a pre-write contract: shared compatibility domain

`LowpanLinkDomain` owns one immutable alias table, PAN and explicit member list. XML
peers name NetworkInterface modules, native identities and aliases; neighbor entries
name the outgoing interface, IPv6 next hop and native peer. The lookup key uses the
globally unique interface **module** ID, not the node-local interface ID. Each member
must contain the configured compatibility MAC and radio, use the same medium, and
have its declared alias. At LINK_LAYER initialization traverse the simulation module
tree and reject every unlisted radio sharing the medium and every second domain
sharing it. Thus channel or range does not silently stand in for PAN isolation.
Configuration is parsed at LOCAL; cross-module checks run after physical/interface
initialization. Invalid configuration aborts initialization before traffic. The module
owns values only, no packets/timers; static bindings survive node stop/start unchanged.

Artifacts: LowpanLinkDomain.h/.cc/.ned and LowpanLinkAddressMap.h/.cc (interface-scoped
neighbors), unit map test and forthcoming real-radio domain configuration module tests.
The production boundary queries this domain for translation; helper tests alone still
do not close P2a. Verification: fresh debug compilation, map test including identical
IPv6 next hops on distinct interfaces, configuration rejection tests with real radios,
and scoped naming/architecture checks. Self-validation: IRadio::getMedium(), module
tree iteration and NetworkInterface identity/alias APIs support the stated validation;
all new paths remain in the optional unsealed lowpan package.

## P2a pre-write contract: per-request compatibility preparation

`LowpanCompatibilityMac` implements feature-owned `ILowpanMac`. Preparation takes only
native source/destination/PAN metadata and returns an immutable `LowpanTransmissionReq`
with the validated tuple, profile and payload limit. The initial static profile has
no mutable envelope options: same PAN, registered source, registered unicast destination
or short broadcast, 23-byte header, 127-byte frame and no FCS. It returns 104 octets.
The caller copies the prepared request onto every queued packet. At encapsulation the
MAC checks the request, profile, capacity and actual initialized source alias again,
then creates the legacy address request immediately before calling base encapsulation.
Thus adaptation/fragment planning never sees or calculates legacy aliases/overhead.
Unsupported preparation requests fail before handoff. At every CCA opportunity, including
retries, validate the head prepared frame before the base MAC can transmit it. Invalid
queued envelopes or changed local identities produce `LOWPAN_ENVELOPE_INVALIDATED`
packet drops, clear the current TX frame/retry state and resume ordinary queue service.
No stale-budget transmission occurs; invalid remaining fragments are rejected on their
own queue turn. This adds a handleSelfMessage override around the existing CCA entry;
it does not duplicate the MAC state machine or delete the MAC-owned CCA timer.
Native P6 preparation may offer other envelopes.

Artifacts: contract/ILowpanMac.h, LowpanTransmissionReq.msg, LowpanCompatibilityMac.h/.cc/.ned.
No timers/packet ownership are introduced by prepare; encapsulation retains base MAC
ownership, ACK/filter/retry/lifecycle paths. Verification: debug build and production
module tests of unicast/broadcast preparation, boundary size, queue delay and invalid
request rejection. Self-validation: encapsulate is virtual; its caller retains
currentTxFrame ownership. Use dropCurrentTxFrame for cancellation and manageQueue for
the terminal transition. LowpanPacketDropDetails carries the named lower reason.

## P2a pre-write contract: production compatibility boundary

`LowpanCompatibilityBoundary` owns native/legacy receive translation and queue handoff;
`ILowpanLink` exposes native identity, static neighbor lookup and per-request preparation
alongside the existing passive-push contract. PacketPusherBase provides the concrete
queue references and callback forwarding. Its in/out gates form the transmission path;
separate lowerLayerIn/upperLayerOut gates form the receive path. Incoming messages on
in use the same checked push path. Direct callers retain packets on a failed canPush
query; push transfers ownership and may synchronously trigger MAC activity.

Before handoff validate the complete prepared native tuple and profile; reject queue
refusal with the queue-overflow reason. RX uses only MacAddressInd and the shared map,
reconstructs both native addresses/PANs, strips legacy/request metadata, and delivers
opaque lowpan payload. Unknown aliases drop with address-resolution-failed reason.
No buffering/timers; stop/crash state remains with adaptation and MAC. Artifacts:
contract/ILowpanLink.h, LowpanCompatibilityBoundary.h/.cc/.ned and real queue/MAC tests.
Verification must cover zero-capacity refusal, real radio delivery with reconstructed
native indications and tag-free receive injection. Self-validation: PacketPusherBase
does not buffer; PacketQueue admission is reliable only without a dropper, which the
boundary validates at initialization. Existing core classes need no changes.

Review correction: initialization also verifies local NIC membership, the selected MAC's
domain and NIC, and that the queue actually feeds that MAC. The domain admits only
LowpanCompatibilityMac peers bound back to it. External PacketBuffer use is rejected:
its post-admission drops would bypass the selected refusal contract. New negative
fixtures cover cross-domain binding, plain-MAC membership and external-buffer use.

Retry correction: the shared compatibility profile defines header/frame/payload sizes
and profile ID once. Native request validation is shared by preparation and queued/retry
checks. An encapsulated retry additionally matches its retained header source/destination,
length and constant PAN placeholder against the prepared native tuple, and checks the
packet protocol. LowpanRetryEnvelope mutates the retained destination after a real first
attempt, expects cancellation before a retry, then exercises service of a valid successor.

## P2b pre-write contract: uncompressed dispatch and tooling

Owner: package-owned LowpanHeader represents only RFC 4944 §5.1 LOWPAN_IPV6 (0x41).
LowpanHeaderSerializer emits/parses its one octet; malformed/unsupported field values
cannot silently become IPv6. LowpanProtocolDissector consumes the recognized dispatch
before delegating a sufficiently long payload to IPv6; unknown formats remain opaque,
and empty/truncated input is marked incorrect. LowpanProtocolPrinter names the format.
These register through existing registries, without core switches or IEEE EtherTypes.

Artifacts: LowpanHeader.msg, LowpanHeaderSerializer.h/.cc,
LowpanProtocolDissector.h/.cc, LowpanProtocolPrinter.h/.cc and
tests/unit/LowpanUncompressedDispatch.test. These helpers are consumed by the subsequent
LowpanLayer production step; helper tests alone do not close P2b. No timers or packet
ownership transfer in the serializer/printer; the dissector follows existing temporary
pop/callback ownership. Verification: fresh debug compilation and the explicit unit
filter for literal 0x41 bytes, independent decoding and invalid value rejection;
production IPv6 exchange remains a separate required test. Self-validation: existing
FieldsChunkSerializer and ProtocolDissector/Printer registry signatures were checked;
all artifacts live in the optional unsealed lowpan package.

## P2b pre-write contract: processing and IPv6 production composition

LowpanLayer is the sole adaptation processing/lifecycle owner. It fills ILowpanLayer's
four gates and uses IActivePacketSource plus ILowpanLink for synchronous bounded handoff.
It accepts IPv6 packets only, validates version/base length/payload length, resolves the
routed NextHopAddressReq (multicast selects short broadcast), prepares the native envelope,
and adds LOWPAN_IPV6 without replacing payload chunks. Oversize single-frame datagrams
drop with LOWPAN_FRAGMENTATION_UNAVAILABLE; MTU remains 1280. Admission refusal drops once
at the adaptation caller and does not push to the boundary. RX validates native link
indications and dispatch/IPv6 lengths, strips stale request/dispatch metadata, and sends
IPv6 with InterfaceInd and a fresh IPv6 dispatch request. No P2b pending buffers/timers;
stop/crash reject packets while down, and existing MAC lifecycle owns its queued frames.

Artifacts: contract/ILowpanLayer.ned and ILowpanLink.ned, LowpanLayer.h/.cc/.ned,
Ieee802154LowpanInterface.ned; LowpanIpv6.h/.cc/.ned,
LowpanIpv6NeighbourDiscovery.h/.cc/.ned and LowpanIpv6NetworkLayer.ned. The interface
composes adaptation, configurable link boundary/MAC, bounded queue and radio. The static
network layer explicitly selects both IPv6 specializations and rejects mobility/IPsec/MLD
options outside this profile. LowpanIpv6 preserves routed next-hop context through the
base source-fragmentation path and avoids emitting the internal unspecified MacAddressReq
placeholder onto the adaptation-facing interface. Ordinary interfaces delegate unchanged.
ND hooks and native token initialization follow the earlier reachability section.

Signals: datagramAccepted at valid IPv6 TX admission, datagramCompleted at successful RX
delivery, packetDropped with core or LowpanPacketDropDetails reason. A handoff is not
radio delivery/ACK success. Chunks/region metadata remain on the original packet in P2b;
no new packet is made for the dispatch byte. Each terminal drop deletes exactly once.
Invalid local composition throws during initialization; malformed received bytes drop.

Verification to add (NOT_RUN): LowpanUncompressedExchange production module fixture for
UDP/ICMPv6 bytes, native link-local source, MTU, multicast, oversize failure, mixed-interface
ND and lifecycle; explicit debug unit/module filters, then protocol-level scenarios.
Existing runner commands are available; these new production fixtures are still owed.
Self-validation: OperationalMixin<PacketProcessorBase> is already instantiated; its
full initialization range covers link/network stages. ND and IPv6 overridden seams are
virtual with the signatures checked in the current tree. All source changes stay in the
feature-owned unsealed package, with no generic IPv6/ND/packet-core edits.

P2b startup correction from the first production echo: base ND delays its link-local
assignment/DAD timer by the host boot interval (0.4–1s). Native static addresses must be
usable when the network layer starts, without changing the ordinary interface boot
policy. LowpanIpv6NeighbourDiscovery::start delegates base timer setup then permanently
assigns configured native addresses immediately, on initialization and restart. Its
existing per-interface hooks suppress native DAD/RS/RA only. The echo test starts at10ms
to prove it does not accidentally rely on the delayed ordinary boot timer.

P2b lifecycle correction: the production restart fixture exposed that legacy MAC start
resets its FSM but leaves an idle receiver's radio OFF after stop/crash. The compatibility
subclass now restores RECEIVER in its start hook after base startup, so receive-only peers
resume without requiring their own transmission. This is confined to the opt-in profile.

P2a/P2b packaging completion: LowpanExamples owns examples/lowpan and LowpanTests owns
an independent tests/networks/lowpan simulation package. Both depend on Lowpan plus their actual
host/application composition (Loopback and Udp); neither is a core dependency. The .test
unit/module runner has no package feature selection, so those fixtures remain explicitly
filtered with Lowpan enabled as recorded in coverage.md. Standalone package simulations
exercise the same public interface/network-layer types without a test C++ driver.

## P3 pre-write contract: fragmentation coordinates and bounded reassembly

LowpanFrag1Header and LowpanFragnHeader own the RFC 4944 §5.3 four/five-octet wire
fields with paired serializers; size is the original IPv6 size (40..2047), offset
is in eight-octet units. Serializer validation rejects unrepresentable fields before
emitting bytes. Independent vectors include c5 00 12 34 and e5 00 12 34 0c.

LowpanFragmenter is a stateless planner. Its inputs distinguish the encoded header
length from original header coverage, and first/subsequent payload budgets. Each output
range is in original IPv6 coordinates. An uncompressed dispatch contributes one wire
octet and zero original coverage; compressed IPv6/UDP headers later contribute 40/48
original octets. Non-final covered endpoints are multiples of eight. Capacity failure
returns no partial plan. No MAC layout calculations enter this helper.

LowpanReassemblyTable owns bounded contexts keyed by interface ID, source/destination
PAN and native identities, original size and tag. Admission reserves the declared size
against a configured byte budget; context count includes quarantines. No unrelated
context is evicted. It accepts borrowed range packets, keeps owned duplicates, and
returns an owned complete packet only after FRAG1 and full coverage. Chunk data and
packet region tags are copied by their original-coordinate ranges; sender request tags
are never used to identify fragments. Equal-range equal-byte duplicates keep the original
deadline. Equal-range conflicting bytes quarantine until that deadline and release data.
Partial overlap restarts with the incoming range and a new deadline, as permitted by
RFC 4944 §5.3. Expiry at time <= now precedes arrival processing, including FRAGN-first.
Each context expires timeout seconds after its first accepted range. Invalid lengths,
zero coverage, bad alignment and out-of-bounds offsets are rejected before storage.
Module lifecycle clears contexts and cancels its expiry timer; native bindings remain.

The sender's 16-bit increment-and-wrap allocator retains reuse guards across stop/start.
No skipping a guarded next tag: exhaustion is an explicit datagram drop. Lower delivery
is bounded by an enforced prepared deadline checked at every CCA attempt, including
retries, plus a derived tail bound for turnaround, configured narrowband PHY duration
and stationary constant-speed propagation. The compatibility domain validates that
restricted lower profile and direct undelayed connections. Per-packet bitrate overrides
are unsupported. Reuse guard ends at the last prepared delivery deadline plus reassembly
timeout; this mitigates, but cannot distinguish, nonconflicting stale generations sharing
a complete wire key. Exact lower implementation/edge-time tests precede production P3
completion. A sender cannot infer the bound from a radio-medium cache parameter.

Verification: helper unit vectors/ranges; duplicate, conflict, partial overlap, FRAGN-first,
resource and exact-expiry cases; production 1280-octet echo/UDP byte equality, mixed senders,
loss/reorder, lifecycle flush, wrap/guard exhaustion, and queued/retry deadline cancellation.
All new owners stay in the optional Lowpan package; no packet-core edits are required.

P3 review correction: the deadline profile also rejects channels on each receiver's
compound radioIn-to-radio gate path. RadioMedium sends at the path start, so such a
channel would otherwise add latency outside the derived propagation/PHY tail.

## P4 pre-write contract: stateless IPHC codec

LowpanIphcHeader declares the RFC 6282 base fields, optional CID octet and ordered inline
bytes; its paired serializer derives the exact wire length from TF/NH/HLIM/address modes.
LowpanIphcCodec selects and accepts the separate encoding-profile.md matrices. It derives
IIDs only from native IEEE 802.15.4 metadata, never compatibility aliases. Encoder output
uses CID=0 and inline NH in this step. Decoder accepts a consumed, unused CID extension,
SAC=1/SAM=00 unspecified source, and rejects all context-dependent and reserved modes.
It reconstructs payload length from the original datagram size supplied by the caller,
not from compressed wire length. Unsupported/malformed input returns no decoded header.
No state, timers or owned packets live in the codec. TF ECN rotation, flow-label placement,
HLIM values, exact link-local prefixes and multicast zero ranges follow RFC 6282 §§3.1–3.2.

LowpanLayer will choose IPHC for valid IPv6 packets when enabled. The fragment planner
receives encoded header size and 40-byte original coverage. IPHC must fit wholly in FRAG1;
otherwise the existing uncompressed dispatch/range planner is the fallback. FRAGN remains
opaque. Reassembly expands only FRAG1's header to original coordinates before inserting
its range; unfragmented receive computes original size from decoded header coverage and
remaining wire payload. Payload chunks and packet region tags retain their original byte
meaning while header regions are replaced. P5 extends header coverage to48 for UDP NHC.

Verification: independently specified TF/HLIM/source/destination/multicast bytes, decode-only
inline/CID cases, all truncation prefixes, missing native identity, reserved/context modes;
production compressed single-frame and fragmented traffic, boundary budgets and existing
uncompressed mode. Extend dissector/printer with bounded header parsing in the same step.

## P5 pre-write contract: UDP NHC

LowpanUdpNhcHeader and its serializer own the RFC 6282 UDP NHC shape. The codec
operates on eight serialized UDP header bytes, avoiding a mandatory dependency on
INET's optional UDP transport implementation. Select the shortest supported port mode,
carry the checksum unchanged and infer UDP length from the complete original IPv6
payload length. The initial profile never emits or accepts checksum elision. Invalid
or incomplete NHC is rejected before replacing packet bytes. Compression failure leaves
UDP inline; unknown next-header compression is rejected on receive. No context, packet
ownership or timers are added to this codec.

LowpanLayer replaces 48 original bytes for direct IPv6/UDP NHC, or 40 for inline-NH
IPHC. A complete IPHC plus NHC chain must fit FRAG1; fall back to inline UDP before
considering uncompressed IPv6 dispatch. Fragment offsets and reconstructed lengths stay
in original IPv6 coordinates. Decoded UDP bytes are provided to the existing UDP parser,
which retains responsibility for transport checksum verification. Golden port-boundary
vectors and production unicast/multicast, single-frame/fragmented UDP tests verify this
contract. Route-over and IPv6 MTU evidence remain separate P5 acceptance requirements.

## P5 prerequisite correction: ICMPv6 error budget

The production forwarded-oversize fixture exposed an existing error-message quote
budget that omitted the outer IPv6 base header. Correct the budget in the existing
Icmpv6 owner to include IPv6_HEADER_BYTES, keeping the eventual IPv6 error packet
within IPv6_MIN_MTU. The PTB test must observe the carried MTU1280 and original
quoted destination/length, without a new IPv6 Fragment header on the response.
This is a shared IPv6 prerequisite, not a LoWPAN-only replacement of ICMP behavior.
Run the existing ICMPv6 delivery/PTB fixtures as well as the new two-link case.

## P6 pre-write contract: native wire and MAC profile

Add IEEE-owned Ieee802154FrameHeader and paired serializer for the pinned unsecured
version01 data frames, extended source and extended unicast/shortffff destination,
same-PAN compression and present DSN; immediate ACK carries only frame control and
DSN. Unsupported control bits/modes are rejected. Address/PAN integers serialize
least-significant octet first; model header lengths are21/15/3 octets. A two-byte
FCS covers actual serialized header and payload using the existing reflected
CRC16-CCITT helper (initial0). The final PSDU, including FCS, is at most127 octets.

Use a separate native MAC on MacProtocolBase: the existing legacy FSM hardcodes
48-bit identities and addressed ACK assumptions and cannot be reused by changing
only encapsulation. Native filtering and retry state use native addresses/PAN and eight-bit DSN.
IEEE2024 section6.6.1 assigns a device-wide DSN usable for immediate ACK correlation;
it cannot prove duplicate identity across wrap or frames to other destinations.
Valid retries are delivered as MSDUs; there is no fabricated DSN replay window.
Adaptation reassembly handles exact fragment duplicates while a context exists. ACK matching uses outstanding DSN and exchange
state, without fabricated ACK addresses. Bounded CSMA/backoff/retries, owned
turnaround/ACK timers and pre-attempt deadlines are required; broadcast never ACKs.
Lifecycle cancellation drops owned pending work and restores receiver mode on start.

Native link domain/boundary peers require no aliases. They implement the existing
adaptation-facing contracts with per-request104/110-octet capacity and native
indications derived from received headers. Extract only shared physical deadline
validation into a helper; retain distinct native and compatibility configurations.
Local wire/unit/radio/capture evidence remains distinct from the required external
peer exchange. The sibling IEEE802154 standards branch has no reusable native
codec/MAC yet; its independently proposed address API differs and is not imported.

The native PHY profile fixes250kbps,128us preamble,16-bit PHR and zero radio switching
matrix, with explicit192us MAC turnaround. Maximum round-trip propagation must be
strictly less than16us to fit the560us immediate-ACK wait; longer propagation is rejected
at initialization. CCA tracks any busy indication throughout its128us observation,
and data/retry scheduling respects a conservative640us interframe interval.

## Native capture prerequisite: ICMPv6 pseudo-header checksum

Independent TShark decoding found the shared ICMPv6 implementation computes/verifies
only ICMP bytes. RFC4443 sections2.2–2.3 require resolved source/destination addresses
and an IPv6 pseudo-header. Keep declared checksum modes unchanged. Computed insertion
is deferred from the existing public helper to an ICMPv6-owned POST_ROUTING hook,
after source selection and before IPv6 source fragmentation. Walk supported extension
headers, use ICMP length excluding those headers, and never recalculate forwarded
fragments. Pre-existing source fragments and active routing-header final-destination
semantics are outside this bounded hook's computed profile and must be explicit.

Receive verification uses IPv6 L3AddressInd; a computed checksum without address context
cannot be asserted valid. Raw protocol dissection without that context must retain
structural validation without claiming checksum verification. No sealed packet-core
changes or network-to-transport dependency are needed. Validate independent pseudo-header
bytes, native request/reply/error captures, source fragmentation and existing declared
ICMP tests. ND/MLD computed transmit should traverse the same hook; receive validation
for those independently dispatched protocols is not claimed by this correction.

Final checksum inspection found that an unspecified `peekDataAt` length can return
only the first payload chunk. Checksum finalization must serialize the explicit
full remaining interval. The computed local-error regression deliberately contains
separate quoted IPv6 and payload chunks and checks actual local delivery.
Native interface module types are defaults, allowing verified subclasses for
controlled fault injection; the boundary/domain still validate their concrete MAC
contract and domain ownership.

Independent byte-only peer fragments exposed a packet representation edge case:
removing the compressed header from a raw BytesChunk after popping FRAG1 exercises
the sealed packet core's nonzero-offset erase branch. Normalize the front by
popping the complete encoded header and trimming consumed bytes before inserting
reconstructed IPv6/UDP headers. Preserve remaining region tags through the packet
API. No sealed packet-core edit is made; the peer fixture checks every output byte.

`datagramAccepted` always observes the submitted IPv6 datagram, including in the
single-frame path. Keep a temporary Packet duplicate (shared immutable chunks) until
queue admission is known, then emit it; fragmented acceptance already emits the
original datagram. The observable must not switch between IPv6 and encoded bytes
based on whether fragmentation was needed.
