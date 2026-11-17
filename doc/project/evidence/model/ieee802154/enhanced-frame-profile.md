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

The existing indication's optional DSN/source-PAN fields and separate effective source PAN are reused. An omitted value never becomes zero/0xffff, and no DSN is added to DATA.confirm. A legal but unsupported local operating combination uses `UNSUPPORTED_FEATURE` after accepted semantic validation; local provider/profile absence uses `UNSUPPORTED_OPERATION` refusal with caller ownership retained. Invalid controls/ranges use `INVALID_PARAMETER`, invalid identities use `INVALID_ADDRESS`, oversize uses `FRAME_TOO_LONG`, and nonzero security retains the existing security outcomes.

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

Descriptors are little-endian 16-bit words. Header IEs use length bits0–6, ID7–14, type15=0; Payload IEs use length0–10, group11–14, type15=1. Short Nested IEs use length0–7, sub-ID8–14, type15=0; long Nested IEs use length0–10, sub-ID11–14, type15=1. MLME is Payload group1. HT1/HT2 IDs are 0x7e/0x7f with zero content; Payload Termination is group0xf with zero content. Every length is checked against its immediate enclosing region and actual FCS-excluded bytes before advancing. Unknown identifiers preserve order/content without introducing semantics; arbitrary recursive containers are not supported.

The auxiliary Security Control octet determines security level, key-ID mode, frame-counter suppression and ASN-in-nonce selection. A nonsuppressed counter occupies four octets; key modes0/1/2/3 occupy 0/1/5/9 key-identifier octets (9.4.1, PDF173). The structural decoder establishes those extents before Header IEs. Protected payload and any MIC remain opaque under the unsupported-security outcome; E1 never authenticates, interprets encrypted Payload IEs or creates keys/counters/replay state. HT/IE parsing and FCS verification never consume beyond the enclosing MPDU.

## Sequence and ACK contract

DSN serves DATA/Command generation with a sequence field, BSN serves legacy beacons, and EBSN serves enhanced beacons (6.6.1, PDF69). Allocate/increment only when the selected frame carries its sequence field; suppressed frames do not allocate a dummy number. Retries reuse the constructed bytes/counter value. Counters wrap at 255. EBSN is initialized randomly only in the enhanced-capable profile so default legacy RNG consumption remains unchanged; nondefault reset retains it, default reset redraws it under that profile.

| Trigger / control | Response and matching rule |
| --- | --- |
| Legacy DATA/Command, unicast AR1 | Existing version0 immediate ACK path; legacy sequence behavior unchanged. |
| Version2 addressed DATA/Command, unicast AR1, security success | Version2 enhanced ACK; AR0, destination mode/address copied from triggering source, source mode/address from actual responder. |
| Trigger compression/suppression | Copy both control bits. Derive ACK PAN presence from its own reversed address modes and Table7-2. Carry the triggering sequence iff present. |
| AR0 or all-ones broadcast destination | No ACK response; normal broadcast/group-addressed transmission never sets AR. Preserve the existing receive-side rule for an externally injected group-addressed AR1 frame instead of inferring a new ACK filter from the sender restriction. |
| Suppressed sequence | Match explicit expected peer, destination, applicable PANs, suppression/compression, channel and arrival interval; do not compare a dummy DSN. Unsupported ambiguous contexts are refused locally. |
| Wrong peer/sequence/control/PAN/channel, malformed ACK | Does not complete the outstanding exchange successfully. |
| Valid unknown ACK IE | Ignore semantic content for ACK acceptance and complete SUCCESS; preserve actual received values for the service consumer. |
| DATA.confirm after actual enhanced ACK | Optional copied Header IE list, Payload IE list and ACK payload, excluding termination IEs/FCS. Empty actual enhanced ACK differs from no enhanced ACK. |

The packet-level response policy remains AIFS=12 selected-PHY symbols after receive end. Accept a peer's additional permitted CAP backoff-boundary start up to AIFS plus one unit backoff period (6.6.3.3, PDF72). Wait for a conservative maximum-legal-PSDU ACK duration because the originator cannot know receiver-selected IEs. Reuse twice the existing one-way `maxPropagationDelay` in completion and permitted-arrival envelopes. Scheduled deadlines leave required IFS inside CAP/GTS; optional response IEs yield when necessary. Physical PPDU timestamps and durations remain simulation time. No second ACK owner/timer, packet pointer in confirmation, arbitrary timeout parameter or knowledge of a peer's actual receive timestamp is added.

## Enhanced discovery and integration

Enhanced Beacon Request is command7 with frame version2, no command content, FP0 and AR0. Requested Header/Payload/Nested elements are sent with zero-length content in their correct descriptor grammar (6.5.2 and7.5.3, PDF67–68,108–109). At most one Enhanced Beacon Filter IE, short Nested sub-ID0x1e inside the MLME group, is permitted (Table7-9, PDF102). Its content control bits select Permit Joining, an optional unsigned minimum-LQI byte, an optional 0–100 percent-response byte and a zero-to-three-octet requested-attribute bitmap (7.4.4.2 andFigure7-28, PDF106). Unsupported feature-specific requested attributes cannot create TSCH/DSME/security capability. A zero/100 percent filter is deterministic; intermediate values use one configured MAC RNG draw per applicable request after deterministic filters pass.

The request's PAN and destination select either a specific coordinator or broadcast short destination. With `macBeaconAutoRespond=false`, deliver the actual filter/IE request to the upper management client; a later explicit beacon request carries the chosen supported values. The native active-scan service receives an additive legacy-default request-format/IE/filter selection, so discovery is runnable through production services. Received enhanced-beacon lists belong in native notifications/scan descriptors, not an invented legacy beacon body. E1-07 requires completed B1/B2/B3 and maps Header IE0x23 (Simplified Superframe) and0x24 (Simplified GTS) to the existing schedule/grant owners; it adds no parallel database or generic IE registry.

Table6-1 (PDF68) maps requested attribute bits0–6 to TRLE, TSCH, DSME, low energy, hopping, deterministic access and coexistence features respectively; bits7–23 are reserved. None of those semantic features is selected by E1 core, so a filter requiring one cannot be satisfied by manufacturing its feature IEs or a success-valued PIB entry. Preserve its raw request/upper notification, and do not emit an automatic response claiming that capability. Supported arbitrary IE values come from the explicit native upper configuration; the MAC builds only the selected structural/filter elements and, in E1-07, its real simplified schedule/GTS IEs. Unknown IE semantics do not create a producer. Native generation rejects reserved filter bits/ranges; raw forensic reception preserves uninterpreted values without enabling a feature.

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
| Table8-18 ScanType | Add explicitly assigned ENHANCED_ACTIVE_SCAN while preserving ED/ACTIVE/PASSIVE/ORPHAN values and PASSIVE default. RIT/MPM scan remain explicitly unsupported. | Existing scan owner sends version2 EBR and uses the same channel/dwell/capacity/restore path. |
| Table8-18 PanIdSuppressed/SeqNumSuppressed | Copied Boolean request values, false defaults. Reject incompatible legacy combinations; omitted PAN retains its presence distinction. | EBR frame layout and sequence allocation. |
| Table8-18 copied HeaderIeList/PayloadIeList | Copied values excluding termination; empty defaults. | EBR request body and actual native scan descriptor/notification. |
| Table8-18 HeaderIeIdList/NestedIeSubIdList | Copied typed IDs, empty defaults; selected MAC producers only. Unsupported semantic requests never synthesize a capability IE. LinkQualityScan remains false by default; unsupported selected behavior reports the existing explicit unsupported-feature outcome. | Existing scan request validation and EBR construction. |
| Table8-22 BeaconType/ChannelInfo/SuperframeOrder | Existing enhanced flag selects BEACON versus ENHANCED_BEACON, with legacy false default; existing channel/SO/security fields retain meaning. | Existing one-shot beacon service. |
| Table8-22 SrcAddrMode/DstAddrMode/DstAddr | Add explicit native address choices with legacy-preserving unspecified defaults. A modern explicit source mode selects NONE/SHORT/EXTENDED; a response destination may identify the requester, otherwise normal broadcast selection applies. | The actual beacon MHR and versioned presence calculation; no address tag is treated as wire data. |
| Table8-22 BsnSuppression | Add copied Boolean false default; only enhanced beacon selection permits suppression. | EBSN presence/allocation and FCF suppression. |
| Table8-22 copied Header/Payload lists and generated Header/Nested IDs | Copied request values, empty defaults. Explicit empty one-shot lists mean no supplied IEs. Generated requested IDs use only actual supported feature owners. | Manual one-shot generation; automatic EBR responses use the authoritative configured PIB lists instead. |

Table8-22's prose names a combined `macEbIeList`, whereas Table8-39 defines separate Header/Payload attributes. This binding treats the prose as the combined applicable lists; it does not introduce an invented third PIB attribute or ignore the explicitly supplied list parameters. Periodic generation uses the two PIB lists plus actual scheduler/GTS producers. Reject conflicting upper-supplied copies of MAC-owned scheduled semantic elements rather than creating two authoritative schedules. Legacy frame controls, defaults and wire bytes remain unchanged. Unknown legal IE values remain structurally preservable without a semantic generator.

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

| Obligations | Owning implementation / direct future fixtures | Executed outcome |
| --- | --- | --- |
| ENHANCED-1,2 | E1-01; FrameLayout/FrameWire unit and EnhancedFrameGate protocol | NOT_RUN |
| ENHANCED-4–7 | E1-02/03; InformationElements/NestedElements units, enhanced serializer, EnhancedFrameGate observations | NOT_RUN |
| ENHANCED-1–3,8 | E1-04; EnhancedData protocol and native-gate/sequence controls | NOT_RUN |
| ENHANCED-8–11 | E1-05; EnhancedAck protocol, ownership/lifecycle and nonzero-propagation cases | NOT_RUN |
| ENHANCED-3,12–14,16–17 excluding periodic selection | E1-06; EnhancedBeacon protocol, native scan, PIB GET/SET/reset controls and managed application | NOT_RUN |
| ENHANCED-15,16–17 periodic selection and scheduled ACK boundaries | E1-07; EnhancedSuperframe plus scheduled EnhancedAck extension, including macUseEnhancedBeacon exposure/selection | NOT_RUN |

New enum/code values are explicitly assigned while existing values retain meaning. All concrete client/serializer/dissector/printer/tag consumers change in their owning source commit. Codec success remains gated from operational dispatch until the corresponding behavior commit. Stop/reset/crash, PHY unavailability, accepted/refused packet ownership and inline callback reentry preserve the existing native exactly-once contract. E1-99 closes evidence only after actual DATA/ACK/EBR production paths and the separately required scheduled gate pass.
