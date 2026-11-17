# Native enhanced-frame profile and service mapping

> **Kind:** design · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [../../standard/ieee802154/enhanced-catalog.md](../../standard/ieee802154/enhanced-catalog.md), [../../protocol/ieee802154/enhanced-checks.md](../../protocol/ieee802154/enhanced-checks.md), [../../../../../plan/pending/802154-enhanced-frames-plan.md](../../../../../plan/pending/802154-enhanced-frames-plan.md)

E1-00 source audit, 2026-11-16. User authorization covers implementation and final push of the reviewed plan. This record owns the source/design decisions; current implementation and executed outcomes are recorded separately in the [execution record](followup-results.md). A closed structural stage does not enable later operational services. The source is IEEE Std 802.15.4-2024, hash pinned in the standard catalog. Canonical nodes, physical PDF pages and their continuation tables were retrieved from the local corpus; its four unresolved references and extraction warnings remain visible in the plan. No subsequent amendment or unverified erratum is used.

## Presence and encoder decisions

The following expands all 14 Table 7-2 rows (PDF80–81). N, S and E mean no, short and extended addressing; P means either S or E. D and S in the PAN columns mean the destination and source PAN fields are present. This table describes wire presence, independently of whether a particular native identity context is operationally supported.

| Destination mode | Source mode | Compression | Destination PAN | Source PAN |
| --- | --- | ---: | --- | --- |
| N | N | 0 | absent | absent |
| N | N | 1 | D | absent |
| P | N | 0 | D | absent |
| P | N | 1 | absent | absent |
| N | P | 0 | absent | S |
| N | P | 1 | absent | absent |
| E | E | 0 | D | absent |
| E | E | 1 | absent | absent |
| S | S | 0 | D | S |
| S | E | 0 | D | S |
| E | S | 0 | D | S |
| S | E | 1 | D | absent |
| E | S | 1 | D | absent |
| S | S | 1 | D | absent |

The pinned Table 7-2 footnote literally requires compression zero iff PANs are identical for dual addressing with at least one short address. Automatic version-2 emission follows that rule; structural decoding follows the transmitted field-presence row. A source PAN omitted while a destination PAN is carried is inferred from that destination PAN under 6.6.2. Where the literal automatic encoding cannot faithfully express a native request's distinct source-PAN context, refuse that operating combination explicitly rather than fabricate an on-wire source PAN. Fixed independent vectors must preserve this edition-specific choice; a change requires pinned corrective evidence and an updated interpretation.

Native DATA transmission retains version 1 as default and adds explicit version 2 selection; version-0 wire inspection/reception remains supported under its legacy rules. Version-2 operational DATA requires applicable local/peer PAN context and explicit source/destination identities for sequence-suppressed acknowledged exchanges. No-address/PAN-only layouts are structurally representable but do not establish an operational endpoint. Version 3 and reserved addressing are invalid. Multipurpose, Fragment and Extended frames remain opaque unsupported.

An unsecured version-2 Command requires its one-octet Command ID after the IE region
(7.3.4.1/Figure7-17, PDF92). The shared complete-frame inspection enforces that minimum
for raw decoding, generation and typed dissection. A complete IE list without that field
is truncated Command structure; an empty Beacon/DATA region is not subjected to the Command
rule. Protected remainder stays opaque rather than acquiring an invented Command ID.

For operational version-2 DATA, the receiver requires both endpoint identities. A carried
destination PAN is interpreted literally, including0xffff. If it is omitted, require a
configured nonbroadcast local PAN and an existing full-identity parent or coordinator-member
binding for the peer; no new peer registry is introduced. That proven local PAN supplies the
effective destination. A carried source PAN remains the optional wire value; when omitted,
its effective value is the carried destination PAN or the same proven local context when both
PAN fields are absent. Sender validation rejects unsupported unbound/no-address combinations
instead of emitting an unreceivable context. Full EUI comparison never aliases low48 bits.
This declared binding permits interpretation of omitted fields; it does not authenticate a
sender or infer an unobservable foreign PAN. Foreign-context tests must use an observable
wire PAN or peer-identity mismatch.

The existing indication's optional DSN/source-PAN fields and separate effective source PAN are reused. An omitted value never becomes zero/0xffff, and no DSN is added to DATA.confirm. A legal but unsupported local operating combination uses `UNSUPPORTED_FEATURE` after accepted semantic validation; local provider/profile absence uses `UNSUPPORTED_OPERATION` refusal with caller ownership retained. Invalid controls/ranges use `INVALID_PARAMETER`, invalid identities use `INVALID_ADDRESS`, oversize uses `FRAME_TOO_LONG`, and nonzero security retains the existing security outcomes.

The native indication packet carries only the original MSDU bytes after IE termination;
copied Header/Payload IE values travel separately in the indication and its result tag.
Table8-32 (PDF149–150) describes its `Msdu` octets as including Payload IEs as well as
providing separate IE-list parameters. This binding exposes those components separately,
rather than silently discarding them or claiming that the packet alone is a byte-for-byte
copy of that standard service parameter. Lists exclude termination/FCS, and legacy lists
remain empty. The raw/promiscuous path retains physical wire inspection separately.

## IE grammar and boundaries

Table 7-6, PDF97, gives the complete list-termination matrix. H/P/D indicate Header IEs, Payload IEs and ordinary data respectively. Optional means omitted by default for canonical native generation, while a legal explicit termination is preserved when decoding.

| H | P | D | Header termination | Payload termination |
| --- | --- | --- | --- | --- |
| no | no | no | none | none |
| yes | no | no | none required | none |
| no | yes | no | HT1 | optional |
| yes | yes | no | HT1 | optional |
| no | no | yes | none | none |
| yes | no | yes | HT2 | none |
| no | yes | yes | HT1 | required |
| yes | yes | yes | HT1 | required |

Canonical generation follows these inclusion rows and omits redundant empty termination
forms. Structural decoding preserves zero-content HT1-only, HT2-only and HT1-plus-Payload-
Termination-only forms as empty bounded regions. Clause7.2.9 defines termination descriptors
as IEs, while7.4.1/Table7-6's “None” and “no termination is required” wording does not
uniquely establish a receive-discard rule for redundant empty transitions. This is the selected
forensic decoding policy, not a claim that IEEE requires those forms to be emitted. Nonzero
termination content, overrun, missing HT1 at a Payload-IE transition, and an IE-present field
with no descriptor remain structural failures. No configurable strictness mode is added.

Descriptors are little-endian 16-bit words. Header IEs use length bits0–6, ID7–14, type15=0; Payload IEs use length0–10, group11–14, type15=1. Short Nested IEs use length0–7, sub-ID8–14, type15=0; long Nested IEs use length0–10, sub-ID11–14, type15=1. MLME is Payload group1. HT1/HT2 IDs are 0x7e/0x7f with zero content; Payload Termination is group0xf with zero content. Every length is checked against its immediate enclosing region and actual FCS-excluded bytes before advancing. Unknown identifiers preserve order/content without introducing semantics; arbitrary recursive containers are not supported.

The registered dissector retains one typed Frame callback and invokes shared codec inspection even on its typed fast path. A computed view exposes ordered values and nonoverlapping byte regions from the MPDU start; offsets are not stored again in MSG. The initial region is the fixed MHR prefix, followed by auxiliary/Header-IE regions where present, since these also belong to the complete MHR. This preserves existing frame serialization and consumer behavior without packet-region classes or core callback changes. Independent value-encoding tests complement the registered serializer and real dissector callback observations.

Nested inspection is confined to MLME Payload group1 and bounded by that immediate content
region. Preserve short/long form, unknown IDs, duplicates and order without recursive feature
interpretation. An empty MLME content region is retained as a structural empty list:
7.4.3.1 permits length0..2047, while7.4.3.3/7.4.4.1 (PDF101) define the container and
descriptors without an explicit nonempty-list minimum or receive-discard rule. This selected
structural policy does not establish a feature-valid MLME operation or an IEEE requirement
to emit/accept an empty feature exchange. The2047-byte helper enclosing limit is distinct
from the selected PHY's127-byte complete MPDU; descriptor bytes consume that enclosing limit.

The auxiliary Security Control octet determines security level, key-ID mode, frame-counter suppression and ASN-in-nonce selection. A nonsuppressed counter occupies four octets; key modes0/1/2/3 occupy 0/1/5/9 key-identifier octets (9.4.1, PDF173). The structural decoder establishes those extents before Header IEs. Table9-6 and its continuation (PDF173–174) give MIC lengths0/4/8/16 for levels0/(1,5)/(2,6)/(3,7). Reserve the known MIC extent when bounding clear Header IEs and reject a missing required extent; this is a structural minimum, not authentication. Protected payload and MIC remain one opaque remainder under the unsupported-security outcome; E1 never authenticates, interprets protected Payload IEs or creates keys/counters/replay state. Level4 is reserved/deprecated in this edition: a structurally inspectable frame may expose that reserved level, with its post-auxiliary remainder opaque and no assumed MIC extent, while native operation discards it as unsupported security. Structural OK is not security validity or an encryption-only capability. HT/IE parsing and FCS verification never consume beyond the enclosing MPDU.

## Sequence and ACK contract

Enhanced ACK response content is explicit model configuration: default-empty typed NED
Header/Payload lists and payload octets, copied into the existing MAC ACK owner at
initialization. It is not an invented IEEE PIB. Object-valued interface forwarding clones
the NED values before the child parameter owns them. Invalid descriptor/applicability/vendor
content fails initialization. The encoded body is conservatively limited to104 octets so
the largest selected fixed MHR21 plus FCS2 always fits127, even when the triggering address
layout changes. Actual window-dependent optional-content omission remains scheduled integration.

The native ACK producer supports vendor Header0, Payload Vendor2 or MLME1 with vendor short
sub-ID0x40/long0x8, each vendor content carrying at least three identifier octets. ACK Header
applicability is0/0x1a/0x1d/0x1e/0x26; applicable nonvendor formats are unimplemented. ESDU
Payload group0 is forbidden in ACK. Short MLME ACK applicability is the discrete set
0x37/0x38/0x39/0x40;0x3a–0x3f remain reserved. Configured ordinary lists exclude termination,
which the MAC produces. This producer subset never becomes a filter that rejects a
structurally valid received ACK merely because its IE is unrecognized.

DSN serves DATA/Command generation with a sequence field, BSN serves legacy beacons, and EBSN serves enhanced beacons (6.6.1, PDF69). Allocate/increment only when the selected frame carries its sequence field; suppressed frames do not allocate a dummy number. Retries reuse the constructed bytes/counter value. Counters wrap at 255. EBSN is initialized randomly only in the enhanced-capable profile so default legacy RNG consumption remains unchanged; nondefault reset retains it, default reset redraws it under that profile.

| Trigger / control | Response and matching rule |
| --- | --- |
| Legacy DATA/Command, unicast AR1 | Existing version0 immediate ACK path; legacy sequence behavior unchanged. |
| Version2 addressed DATA/Command, unicast AR1, security success | Version2 enhanced ACK; AR0, destination mode/address copied from triggering source, source mode/address from actual responder. |
| Trigger compression/suppression | Copy both control bits. Derive ACK PAN presence from its own address modes and Table7-2. Carry the triggering sequence iff present. A carried Source PAN identifies the actual responder; Destination PAN derives from the trigger's effective Source PAN. |
| AR0 or all-ones broadcast destination | No ACK response; normal broadcast/group-addressed transmission never sets AR. Preserve the existing receive-side rule for an externally injected group-addressed AR1 frame instead of inferring a new ACK filter from the sender restriction. |
| Suppressed sequence | Match explicit expected peer, destination, applicable PANs, suppression/compression, channel and arrival interval; do not compare a dummy DSN. Unsupported ambiguous contexts are refused locally. |
| Wrong peer/sequence/control/PAN/channel, malformed ACK | Does not complete the outstanding exchange successfully. |
| Valid unknown ACK IE | Ignore semantic content for ACK acceptance and complete SUCCESS; preserve actual received values for the service consumer. |
| DATA.confirm after actual enhanced ACK | Optional copied Header IE list, Payload IE list and ACK payload, excluding termination IEs/FCS. Empty actual enhanced ACK differs from no enhanced ACK. |

An exact pending peer identity needs no alias lookup. A different ACK source address form is
accepted only when the existing committed parent/member binding proves that it identifies
the same peer in the applicable local PAN. There is no new alias registry. A broadcast PAN
in the triggering destination does not require a responder to claim PAN0xffff. These rules
follow Figure7-16's continuation (physical PDF92), while the existing destination/control,
sequence, channel and physical-interval checks still apply.

The packet-level response policy remains AIFS=12 selected-PHY symbols after receive end. Accept a peer's additional permitted CAP backoff-boundary start up to AIFS plus one unit backoff period (6.6.3.3, PDF72). Wait for a conservative maximum-legal-PSDU ACK duration because the originator cannot know receiver-selected IEs. Reuse twice the existing one-way `maxPropagationDelay` in completion and permitted-arrival envelopes. Scheduled deadlines leave required IFS inside CAP/GTS; optional response IEs yield when necessary. Physical PPDU timestamps and durations remain simulation time. No second ACK owner/timer, packet pointer in confirmation, arbitrary timeout parameter or knowledge of a peer's actual receive timestamp is added.

The GTS/ATI fit proof uses the minimum appropriate ACK and final SIFS under6.6.4 (physical
PDF74), with the applicable guard. It does not substitute the unknown receiver's maximum
optional ACK body or the long-frame LIFS into that proof. Clause6.3.1 (PDF62–63) separately
constrains the same device's next transmission; retaining that LIFS eligibility does not
extend the current reservation's RX duty. E1-07 must verify the shared legacy calculation
before its bounded correction and clip scheduled waiting to the reservation allowance.

## Enhanced discovery and integration

Enhanced Beacon Request is command7 with frame version2, no command content, FP0 and AR0. Requested Header/Payload/Nested elements are sent with zero-length content in their correct descriptor grammar (6.5.2 and7.5.3, PDF67–68,108–109). At most one Enhanced Beacon Filter IE, short Nested sub-ID0x1e inside the MLME group, is permitted (Table7-9, PDF102). Its content control bits select Permit Joining, an optional unsigned minimum-LQI byte, an optional 0–100 percent-response byte and a zero-to-three-octet requested-attribute bitmap (7.4.4.2 andFigure7-28, PDF106). Unsupported feature-specific requested attributes cannot create TSCH/DSME/security capability. A zero/100 percent filter is deterministic; intermediate values use one configured MAC RNG draw per applicable request after deterministic filters pass.

The request's PAN and destination select either a specific coordinator or broadcast short destination. With `macBeaconAutoRespond=false`, deliver the actual filter/IE request to the upper management client; a later explicit beacon request carries the chosen supported values. The native active-scan service receives an additive legacy-default request-format/IE/filter selection, so discovery is runnable through production services. Received enhanced-beacon lists belong in native notifications/scan descriptors, not an invented legacy beacon body. E1-07 requires completed B1/B2/B3 and maps Header IE0x23 (Simplified Superframe) and0x24 (Simplified GTS) to the existing schedule/grant owners; it adds no parallel database or generic IE registry.

Table6-1 (PDF68) maps requested attribute bits0–6 to TRLE, TSCH, DSME, low energy, hopping, deterministic access and coexistence features respectively; bits7–23 are reserved. None of those semantic features is selected by E1 core, so a filter requiring one cannot be satisfied by manufacturing its feature IEs or a success-valued PIB entry. Preserve its raw request/upper notification, and do not emit an automatic response claiming that capability. Supported arbitrary IE values come from the explicit native upper configuration; the MAC builds only the selected structural/filter elements and, in E1-07, its real simplified schedule/GTS IEs. Unknown IE semantics do not create a producer. Native generation rejects reserved filter bits/ranges; raw forensic reception preserves uninterpreted values without enabling a feature.

Operational filter reception ignores reserved control bits5–7 and requested-attribute
bits7–23 under4.6 (PDF51–52); neither6.5.2 nor7.4.4.2 overrides that rule. Preserve
the actual received IE/request values for upper notification while applying only defined
bits to automatic-response decisions. Structural errors, duplicate filters and invalid
defined values remain invalid. Strict local generation and tolerant reception are separate.

Outside scans, apply6.6.2's carried destination PAN/address and beacon source PAN validity
before8.2.4.3's native-notification policy. Resolve an omitted Source PAN from an actually
carried Destination PAN where applicable without inventing wire presence. Scan collection
under6.4.1.2 remains broad across PANs; do not impose the ordinary local-PAN predicate before
that scan branch. These rules are separate from promiscuous observation.

### EBR response address binding

Clause7.3.1.2 (PDF87) requires copying a present requester source address into the response
destination, using `macPanId` for any required Destination PAN, omitting Source PAN and
following Table7-2. The selected literal equal-PAN footnote remains applicable. Native
enhanced EBRs therefore carry the existing local EUI; legacy requests retain their format.
Request compression follows the actual local/requested PAN relationship, independently of
the false-default all-PAN-suppression control: equal PANs use0, different PANs use1. With
SHORT destination and EUI source, the selected request always carries Destination PAN.
`PanIdSuppressed=true` yields one accepted SCAN.confirm INVALID_PARAMETER before RF/channel
preparation under8.2.2/Table8-19; profile absence remains a provider refusal.

| Requester source | Configured automatic source | Selected response |
| --- | --- | --- |
| EUI | FULL | Copy EUI destination, full local source, PC0 and Destination PAN only. |
| EUI or SHORT | NONE | Copy destination, omit source addressing, PC0 and Destination PAN only; no fabricated join identity. |
| EUI | SHORT | Suppress automatic response: Source PAN omission needs PC1, conflicting with the pinned equal-PAN short-address footnote. |
| SHORT | FULL or SHORT | Suppress automatic response for the same unrepresentable layout. |
| Absent | NONE | Retain Table8-25's short-broadcast fallback, PC0 and Destination PAN only. |
| Absent | FULL or SHORT | Suppress the unrepresentable fallback response. |

Valid PIB values remain readable/writable; an unsupported automatic response has no accepted
native request requiring a confirmation. AutoRespond=false still delivers actual request
values to the upper client. Do not substitute another configured source mode or introduce a
second anonymous-response policy. These limitations concern EBR responses; explicit one-shot
beacons use their own valid layouts. Table8-25 (PDF135–136) supplies the received-request
address context;7.5.3 does not waive the response destination-copy rule.

### Scheduled-beacon eligibility in E1-07

Periodic slot0 begins with its beacon's first PPDU symbol and uses boundary priority without
CSMA (10.2.2.1/10.2.4, PDF182–183/187–188). Conventional Beacon-PAN EBR responses instead use
slotted CSMA in the next CAP under10.19/Table10-98 (PDF351–352). Header0x23's16-bit Timestamp
has implementation-dependent initial value, resolution and accuracy; it is not a standardized
phase-offset/remote-clock map. An arbitrary response receive start cannot become an epoch.

The selected E1-07 producer generates operational0x23/0x24 only through the existing periodic
snapshot at actual boundary TX, using DestinationNONE as the periodic wire profile. One-shot
raw/generated schedule copies are INVALID_PARAMETER under8.2.2/Table8-22; automatic EBR
requests for those outputs are outside the selected response profile. Preserve them for
AutoRespond=false and never send an empty substitute or call a deferred boundary beacon a
Table10-98 CAP response. Other one-shot discovery remains independent.

Receive normalization requires valid unsecured periodic shape/geometry, configured PAN,
channel/coordinator identity and existing acquisition/tracking-window eligibility. Only that
beacon's copied first-PPDU-start sample establishes/renews the shared epoch. Header0x24 alone
and addressed discovery responses cannot update phase, windows, grants, advertisement aging
or loss counters. Their actual IE values can remain visible to scan/notification clients.
This is a declared wire-profile boundary, not a universal claim about every enhanced beacon;
no internal radio transmission-kind tag or additional clock/timer is used.

## Enhanced PIB and service inputs

Table8-39, including its continuation (PDF158–160), has no upper-read-only dagger on the following attributes. All are GET/SET through the existing authoritative PIB owner when the enhanced profile is enabled; disabled profile reads/writes retain UNSUPPORTED_ATTRIBUTE. A default reset restores the selected defaults and redraws EBSN; a nondefault reset retains all values. Source dashes mean the standard does not give that default; values identified below are selected model defaults, not quoted IEEE defaults. E1-06 adds these values together with its actual consumers; E1-07 enables periodic enhanced selection.

| Attribute | Domain, access and default | Consumer and refusal boundary |
| --- | --- | --- |
| `macEbAutoSa` | Read/write NONE/SHORT/FULL; IEEE default FULL; explicitly assigned local enum values0/1/2. | Automatic EBR response source mode. SHORT requires an allocated short identity; FULL uses the full EUI. NONE omits source addressing according to the versioned layout and cannot fabricate a join candidate's coordinator identity. |
| `macEbFilteringEnabled` | Read/write Boolean; selected default true. | When true, apply joining/LQI/percent/attribute filter conditions before automatic response. When false, bypass admission filtering, while retaining valid structural/size/security checks and fulfillment of requested output IEs under6.5.2. A requested unsupported producer still prevents an automatic response; never send an empty substitute that drops required requested content. Upper-directed notification preserves the actual request. |
| `macEbHeaderIeList` | Read/write copied Header IE list excluding termination; selected default empty. | Authoritative configured automatic/periodic enhanced-beacon Header values; semantic generated values remain owned by the relevant MAC feature. Invalid descriptor kind/termination/range is INVALID_PARAMETER. |
| `macEbPayloadIeList` | Read/write copied Payload IE list excluding termination; selected default empty. | Authoritative configured automatic/periodic Payload values. Native GET returns a value copy; no borrowed chunk or second cached IE list. |
| `macEbsn` | Read/write integer0–255; random initial/default-reset value under6.6.1. | Enhanced beacons with a sequence field only; never consume DSN/BSN or an initialization draw in the disabled profile. |
| `macUseEnhancedBeacon` | E1-07 exposes read/write Boolean; selected default false. | E1-07's periodic scheduler format choice. Before that consumer lands, leave the attribute absent and report UNSUPPORTED_ATTRIBUTE for GET/SET under the existing SET vocabulary; no incomplete periodic path is enabled. |

Malformed/range-invalid PIB writes return INVALID_PARAMETER; a valid stored IE list that makes a later complete beacon exceed127 octets produces FRAME_TOO_LONG at generation. A short identity unavailable for explicit SHORT beacon emission gives INVALID_PARAMETER under Table8-22 without inventing an extended replacement. Optional profile absence remains a local admission refusal where the provider itself is absent; this does not change established accepted-service ownership. SET uses Table8-11's existing SUCCESS/READ_ONLY/UNSUPPORTED_ATTRIBUTE/INVALID_INDEX/INVALID_PARAMETER outcomes (PDF122), not an invented enhanced-feature status.

| Standard service inputs | Additive native mapping and defaults | Consumer |
| --- | --- | --- |
| Table8-29/8-30 LegacyTx | Copied Boolean `legacyTx=true` preserves native version1 transmission; false selects the enabled version2 DATA profile. No version0 transmit selector is added. | Existing DATA construction and admission owner. |
| Table8-29 PanIdSuppressed/SeqNumSuppressed | Copied Boolean `panIdSuppressed=false` and sequence-suppression false defaults; modern controls must satisfy Table7-2 and the pinned encoder footnote. Both-extended addressing can omit both PAN fields only with applicable peer/local context. A structurally valid but nonrepresentable requested PAN context is UNSUPPORTED_FEATURE, not a fabricated field. | DATA presence calculation, DSN allocation and optional receive metadata. |
| Table8-30 HeaderIeList/PayloadIeList | Copied typed values excluding termination, empty defaults. Validate descriptor/content/parent bounds and complete MPDU length before packet generation; exceptions from malformed local values must become native accepted-service error terminals. | Existing DATA packet/confirmation owner and shared IE/frame codecs. |
| Table8-32 HeaderIeList/PayloadIeList | Empty-default copied received lists excluding termination/FCS. Populate from the received view, including uninterpreted values, and copy through the native indication tag. | Existing DATA indication callback/adapter; source request metadata is not a substitute. |
| Table8-18 ScanType | Add explicitly assigned ENHANCED_ACTIVE_SCAN while preserving ED/ACTIVE/PASSIVE/ORPHAN values and PASSIVE default. RIT/MPM scan remain explicitly unsupported. | Existing scan owner sends version2 EBR and uses the same channel/dwell/capacity/restore path. |
| Table8-18 PanIdSuppressed/SeqNumSuppressed | Copied Boolean request values, false defaults. Reject incompatible legacy combinations; omitted PAN retains its presence distinction. | EBR frame layout and sequence allocation. |
| Table8-18 copied HeaderIeList/PayloadIeList | Copied values excluding termination; empty defaults. | EBR request body and actual native scan descriptor/notification. |
| Table8-18 HeaderIeIdList/NestedIeSubIdList | Copied typed IDs, empty defaults; selected MAC producers only. Unsupported semantic requests never synthesize a capability IE. LinkQualityScan remains false by default; unsupported selected behavior reports the existing explicit unsupported-feature outcome. | Existing scan request validation and EBR construction. |
| Table8-22 BeaconType/ChannelInfo/SuperframeOrder | Existing enhanced flag selects BEACON versus ENHANCED_BEACON, with legacy false default; existing channel/SO/security fields retain meaning. | Existing one-shot beacon service. |
| Table8-22 SrcAddrMode/DstAddrMode/DstAddr | Add explicit native address choices with legacy-preserving unspecified defaults. A modern explicit source mode selects NONE/SHORT/EXTENDED; a response destination may identify the requester, otherwise normal broadcast selection applies. | The actual beacon MHR and versioned presence calculation; no address tag is treated as wire data. |
| Table8-22 BsnSuppression | Add copied Boolean false default; only enhanced beacon selection permits suppression. | EBSN presence/allocation and FCF suppression. |
| Table8-22 copied Header/Payload lists and generated Header/Nested IDs | Copied request values, empty defaults. Explicit empty one-shot lists mean no supplied IEs. Generated requested IDs use only actual supported feature owners. | Manual one-shot generation; automatic EBR responses use the authoritative configured PIB lists instead. |

Table8-22's prose names a combined `macEbIeList`, whereas Table8-39 defines separate Header/Payload attributes. This binding treats the prose as the combined applicable lists; it does not introduce an invented third PIB attribute or ignore the explicitly supplied list parameters. Periodic generation uses the two PIB lists plus actual scheduler/GTS producers. Reject conflicting upper-supplied copies of MAC-owned scheduled semantic elements rather than creating two authoritative schedules. Legacy frame controls, defaults and wire bytes remain unchanged. Unknown legal IE values remain structurally preservable without a semantic generator.

For an explicitly empty enhanced-beacon IE list, compute IE Present as zero under
7.3.1.1/Figure7-6 (physical PDF86). Table8-22's list-construction prose says to set
that bit, but does not justify an IE-present frame with no descriptor bytes. This
binding emits neither that malformed combination nor an invented capability IE.
Requested zero-length EBR elements remain a separate request grammar under6.5.2;
their IDs identify desired response content, rather than ordinary command producers.

### Operational DATA IE generation

Structural preservation is broader than native emission. Tables7-7 through7-10 define
frame-type applicability; a descriptor fitting its bit width does not authorize its use in DATA.
The bounded DATA producer supports HeaderID0 Vendor Specific, Payload group0 ESDU, group2
Vendor Specific, and group1 MLME containing vendor short sub-ID0x40 or long sub-ID0x8.
Each vendor content carries at least three octets for its caller-supplied assigned OUI/CID;
the MAC checks that structural minimum without an OUI registry. ESDU content is opaque
higher-layer octets and has no vendor prefix.

DATA-applicable HeaderIDs0x1a/0x1b/0x25/0x26/0x27/0x2c and other DATA-applicable MLME
features are valid but unimplemented here, giving UNSUPPORTED_FEATURE. Reserved IDs,
allocated elements without DATA applicability, malformed vendor content and caller-supplied
termination elements give INVALID_PARAMETER. HeaderID0x2a is externally assigned with no
established DATA applicability in the pinned table and is explicitly unsupported, rather
than relabelled reserved. Termination is generated by the MAC. Classification uses the
Nested descriptor's form and ID together. Unknown/reserved forensic parsing and replay
remain unchanged; these restrictions govern the selected native producer.

## Concrete affected-consumer inventory

The following bounded inventory was traced in the current tree before E1 interface edits. B-01's separate PHY callback migration is not counted as E1 behavior. Each owning E1 commit must update all applicable entries together, and add new concrete clients to this inventory if its implementation introduces them.

| Current paths / consumers | Required ownership and owning E1 commits |
| --- | --- |
| `Ieee802154Frame.msg`, `Ieee802154FrameCodec.{h,cc}`, `Ieee802154FrameSerializer.{h,cc}`, `Ieee802154ProtocolDissector.cc`, `Ieee802154ProtocolPrinter.cc` under `src/inet/linklayer/ieee802154/` | Single wire chunk/layout and truthful optional fields/IE boundaries; E1-01/02/03. |
| `Ieee802154StandardMac.{h,cc,ned}`, `Ieee802154MacPib.{h,cc}`, `Ieee802154BeaconCodec.h`, `Ieee802154CommandCodec.h`, `Ieee802154PendingTransactions.{h,cc}` | Operational gating, frame lifetime, ACK matching/retry and authoritative enhanced PIB/indirect storage; E1-04/05/06 and scheduled E1-07. No second queue. |
| `Ieee802154MacDataRequest.h`, `Ieee802154MacBeaconRequest.h`, `Ieee802154MacScanRequest.h`, `Ieee802154MacGetRequest.h`, `Ieee802154MacServiceTag.h`, `contract/IIeee802154MacService{Provider,Client}.{h,ned}` | Additive copied request/confirm/notification and PIB value surfaces; preserve optional indication metadata and pure roles; E1-04/05/06. |
| `Ieee802154MacServiceAdapter.{h,cc,ned}`, `Ieee802154StandardInterface.ned` | Concrete client/tag adapter and explicit enhanced profile parameter binding; E1-04/05/06. |
| `src/inet/applications/ieee802154/Ieee802154ManagementApp.{h,cc,ned}` | Concrete service client; configured modern DATA/discovery policy and examples; E1-04/06. |
| `tests/module/Ieee802154{MacServiceContract,NativeInterface,Start,Discovery,PassiveScan}_1.test` | Concrete service clients; compile with the additive roles/records and preserve legacy assertions. |
| All17 existing `tests/protocol/ieee802154/Ieee802154*.test` fixtures | Concrete generated service clients/peers and frame observations: Admission, Association, Broadcast, Contention, DataAck, Indirect, InterframeSpacing, Lifecycle, Orphan, PanConflict, PhyLifecycle, PhyPib, Reassociation, ReceiveSecurity, ReceiverEnable, SequenceWrap, UnslottedAccess. Extend the new owning fixtures without relaxing these controls. |
| `tests/unit/Ieee802154{FrameWire,Fcs,Capture,Commands,Beacons,PendingTransactions}_1.test`, `tests/serializer/serializer_ieee802154.test` | Existing chunk/codec/serializer consumers and raw replay controls; E1-01/02/03 and their operational consumers. |

Generated `_m` outputs are rebuilt from MSG inputs rather than hand-edited. Generic packet/capture code is sealed and is not an E1 edit surface. Existing registered serializer/dissector identity and receiver-owned protocol dispatch remain the integration boundary.

## Execution mapping

| Obligations | Owning implementation / direct fixtures | Executed outcome |
| --- | --- | --- |
| ENHANCED-1,2 | E1-01; FrameLayout/FrameWire unit and EnhancedFrameGate protocol | PASS for the selected structural layouts; [executed evidence](followup-results.md#e1-01-version-aware-layouts). |
| ENHANCED-4–7 | E1-02/03; InformationElements/NestedElements units, enhanced serializer, EnhancedFrameGate observations | PASS for bounded framing/FCS and unsupported-security policy; [IE evidence](followup-results.md#e1-02-bounded-ie-framing), [nested evidence](followup-results.md#e1-03-nested-mlme-framing). No cryptographic claim. |
| ENHANCED-1–3,8 | E1-04; EnhancedData protocol and native-gate/sequence controls | PASS for selected AR0 DATA/DSN/service binding; [executed evidence](followup-results.md#e1-04-native-data-services). EBSN remains E1-06. |
| ENHANCED-8–11 | E1-05; EnhancedAck/EnhancedAckTiming protocol, ownership/lifecycle and nonzero-propagation cases | PASS for the selected unscheduled ACK profile; repaired full acceptance and bounded C++ dispositions are in the [execution record](followup-results.md#e1-05-initial-enhanced-ack-diagnostic). Scheduled CAP/GTS extensions remain E1-07. |
| ENHANCED-3,12–14,16–17 excluding periodic selection | E1-06; EnhancedBeacon protocol, native scan, PIB GET/SET/reset controls and managed application | NOT_RUN |
| ENHANCED-15,16–17 periodic selection and scheduled ACK boundaries | E1-07; EnhancedSuperframe plus scheduled EnhancedAck extension, including macUseEnhancedBeacon exposure/selection | NOT_RUN |

New enum/code values are explicitly assigned while existing values retain meaning. All concrete client/serializer/dissector/printer/tag consumers change in their owning source commit. Codec success remains gated from operational dispatch until the corresponding behavior commit. Stop/reset/crash, PHY unavailability, accepted/refused packet ownership and inline callback reentry preserve the existing native exactly-once contract. E1-99 closes evidence only after actual DATA/ACK/EBR production paths and the separately required scheduled gate pass.
