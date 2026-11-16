# IEEE 802.15.4 enhanced-frame obligations

> **Kind:** what · **Status:** current · **Seal:** none · **Owns:** `IEEE802154-ENHANCED-*` · **Stands on:** [source.md](source.md), [catalog.md](catalog.md)

This extraction extends the selected base-standard catalog for unsecured version-2 Beacon, DATA, ACK and Command frames. The pinned source is IEEE Std 802.15.4-2024 without amendments, SHA-256 `851d81127297f898c956fdd1ce92929324d68cd696fd338d1e876f6a372f22d3`. Physical PDF pages are one-based. Canonical IDs below have the prefix `ieee802154-2024:`. Statements are paraphrased normative requirements; notes and implementation choices are not promoted to requirements. The [English checks](../../protocol/ieee802154/enhanced-checks.md) describe independent observations; [model decisions](../../model/ieee802154/enhanced-frame-profile.md) define the bounded implementation profile and its unresolved execution evidence.

## IEEE802154-ENHANCED-1

For frame version 2, address modes and PAN compression determine the presence of each PAN field through Table 7-2, rather than the legacy equality rule.

Source: `clause:7.2.2.6`, `table:7-2`; 80–81.

## IEEE802154-ENHANCED-2

Sequence-number suppression controls whether a sequence field is carried; absent is distinct from sequence zero. Legacy versions cannot use suppression or IE-present control.

Source: `clause:7.2.2.7`, `clause:7.2.2.8`, `clause:7.2.3`; 81–82.

## IEEE802154-ENHANCED-3

Generated DATA, Command and Beacon frames with a sequence field use the appropriate device-wide DSN, BSN or EBSN; counters wrap at their representable maximum and initialize randomly. A retransmission preserves the frame's sequence. ACKs instead copy the triggering sequence under ENHANCED-9; Multipurpose is outside this profile.

Source: `clause:6.6.1`, `clause:6.6.3.4`; 69, 72–73.

## IEEE802154-ENHANCED-4

Header, Payload and Nested IE descriptors use their own length/identifier/type fields. The enclosing frame/group bounds each element. Header and Payload lists follow the Table 7-6 termination combinations.

Allocated IE types have the frame-type applicability specified by Tables7-7 through7-10;
structural descriptor width alone does not establish permitted emission. Vendor Specific
Header/Payload/Nested content begins with a three-octet assigned Vendor OUI/CID before any
vendor-defined information.

Source: `clause:7.4.1`, `table:7-6`, `clause:7.4.2.1`, `table:7-7`, `clause:7.4.2.2`, `clause:7.4.3.1`, `table:7-8`, `clause:7.4.4.1`, `table:7-9`, `table:7-10`, `clause:7.4.4.3`; 96–106.

## IEEE802154-ENHANCED-5

HT1 and HT2 have zero-length content. HT1 changes to Payload IEs; HT2 changes to ordinary MAC data. Payload Termination separates payload IEs from data when required by Table 7-6.

Source: `clause:7.4.2.4`, `clause:7.4.2.5`, `clause:7.4.3.4`; 100–102.

## IEEE802154-ENHANCED-6

The auxiliary security header precedes Header IEs. Its control/key/counter fields determine its structural extent; security processing governs whether protected bytes can be interpreted or delivered. Table9-6 defines MIC lengths0/4/8/16 for levels0/(1,5)/(2,6)/(3,7); level4 is reserved and receiving devices discard it.

Source: `clause:7.2.8`, `clause:9.2`, `clause:9.4`, `clause:9.4.2.2`, `table:9-6`; 83, 160–180, including Table9-6's continuation on174.

## IEEE802154-ENHANCED-7

FCS verification covers the complete MHR and MAC payload, including actual IE and auxiliary bytes. All overhead contributes to the selected PHY's PSDU bound.

Source: `clause:7.2.11`, `table:8-31`; 84, 146–148.

## IEEE802154-ENHANCED-8

Valid nonbroadcast version-2 DATA/Command frames requesting acknowledgment use an enhanced ACK. Failed incoming security does not require an enhanced ACK.

Source: `clause:6.6.2`; 70–71.

## IEEE802154-ENHANCED-9

An enhanced ACK has AR zero, copies suppression/compression and the triggering sequence when present, addresses the triggering source, and uses the responding device's appropriate source identity.

Source: `clause:7.3.3`, `figure:7-16`; 91–92.

## IEEE802154-ENHANCED-10

ACK response begins AIFS after reception outside CAP; inside CAP it may also begin at a permitted backoff boundary no later than AIFS plus one backoff period. Unknown well-formed ACK IEs do not prevent SUCCESS.

Source: `clause:6.6.3.3`; 72.

## IEEE802154-ENHANCED-11

DATA.confirm carries received enhanced-ACK Header IE list, Payload IE list and ACK payload, excluding termination IEs.

Source: `table:8-31`; 146–148.

## IEEE802154-ENHANCED-12

An Enhanced Beacon Request uses the existing Beacon Request command with frame version 2 and no command content; FP and AR are zero. Requested IEs use zero-length requests in their applicable IE grammar.

Source: `clause:6.5.2`, `clause:7.5.3`; 67–68, 108–109.

## IEEE802154-ENHANCED-13

At most one Enhanced Beacon Filter IE occurs per request. Joining, minimum LQI, response probability and requested-attribute bitmap govern the response under their stated conditions.

Source: `clause:7.4.4.2`, `figure:7-28`, `table:6-1`; 67–68, 106.

## IEEE802154-ENHANCED-14

With automatic response disabled, Enhanced Beacon Request IEs/filter are delivered to the next higher layer. Otherwise a matching request obtains the applicable requested supported IEs in the enhanced beacon.

Source: `clause:6.5.2`; 67–68.

## IEEE802154-ENHANCED-15

Simplified Superframe and Simplified GTS IEs carry the applicable scheduled information in enhanced beacons; their presence does not supply unrelated TSCH/DSME semantics.

Source: `clause:10.2.8.1`, `clause:10.25.8.1`; 190–191, 389–390.

## IEEE802154-ENHANCED-16

Enhanced beacon source selection, filter enable, configured IE lists, EBSN and periodic enhanced selection are authoritative MAC PIB attributes with the Table 8-39 domains and access.

Source: `table:8-39`; 158–160.

## IEEE802154-ENHANCED-17

Enhanced active scan and one-shot beacon generation carry their specified suppression, address and copied/generated IE inputs; unsupported or out-of-range beacon parameters report INVALID_PARAMETER, and an oversized beacon reports FRAME_TOO_LONG.

Source: `table:8-18`, `table:8-22`; 128–129, 133–134.

The Table 7-2 continuation's footnote is included in the interpretation record; the selected edition's literal encoder rule is not replaced with an older edition. The complete field-presence and termination matrices are recorded in the model decision document, separately from these normative statements.
