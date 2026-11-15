# Enhanced-frame English checks

> **Kind:** what · **Status:** current · **Seal:** none · **Owns:** `IEEE802154-C-ENHANCED-*` · **Stands on:** [../../standard/ieee802154/enhanced-catalog.md](../../standard/ieee802154/enhanced-catalog.md)

These checks state protocol observations independently of an implementation. They do not report executable results. Implementation mapping and test ownership are in the [enhanced-frame profile](../../model/ieee802154/enhanced-frame-profile.md).

## IEEE802154-C-ENHANCED-LAYOUT

Encode independently chosen version-2 frame-control/address combinations from every Table 7-2 row. Inspect field positions and total octets, including no-address/PAN-only and both-extended cases. Decode zero and 0xffff values separately from omitted fields. Reject reserved control/address combinations without reading past the frame.

Obligations: ENHANCED-1, ENHANCED-2.

## IEEE802154-C-ENHANCED-SEQUENCE

Interleave legacy DATA, suppressed DATA, commands, legacy beacons and enhanced beacons near counter rollover. Inspect independent DSN/BSN/EBSN allocation, absent fields, retries and reset retention/defaulting.

Obligations: ENHANCED-2, ENHANCED-3.

## IEEE802154-C-ENHANCED-IE

Use all eight Header/Payload/data presence combinations with independent bytes. Include unknown identifiers, both nested formats, zero/maximum length and every truncation point in representative frames. Require exact MSDU extraction, termination semantics, unknown ordering and bounded rejection of malformed extents.

Obligations: ENHANCED-4, ENHANCED-5.

## IEEE802154-C-ENHANCED-SECURITY

Place auxiliary headers with every key-identifier mode and counter-suppression choice before Header IEs. Truncate each extent. Preserve protected content for raw inspection while denying ordinary unauthenticated delivery and security-derived changes.

Obligations: ENHANCED-6.

## IEEE802154-C-ENHANCED-FCS

Calculate independent FCS vectors across IE/auxiliary/data boundaries; corrupt each region. Exercise 127 versus 128 total octets including FCS under the selected O-QPSK profile.

Obligations: ENHANCED-7.

## IEEE802154-C-ENHANCED-DATA

Two addressed peers exchange version-2 unacknowledged DATA with present/suppressed sequence and explicit/inferred PANs. Inspect receiver-owned payload/identity; foreign PANs and unsupported identity contexts must not acquire fabricated metadata.

Obligations: ENHANCED-1, ENHANCED-2, ENHANCED-8.

## IEEE802154-C-ENHANCED-ACK

Exchange acknowledged DATA with legal sequence/address combinations and variable-size enhanced ACKs. Inject wrong peer/sequence/control/address/PAN and malformed IEs. Observe AR zero, copied fields, applicable AIFS response, actual received ACK content, SUCCESS for unknown well-formed IEs, and correct loss/retry behavior.

Obligations: ENHANCED-8 through ENHANCED-11.

## IEEE802154-C-ENHANCED-DISCOVERY

Request an enhanced beacon with empty, matching and nonmatching filters and requested IEs. Observe the existing command identity/version, no legacy beacon-body prefix, correct IE response and scan descriptor, and upper-directed behavior when automatic response is disabled.

Also vary enhanced scan suppression/address inputs, supplied versus generated IE lists, automatic source-address NONE/SHORT/FULL, and filter enablement. Read/write/reset the enhanced PIB attributes; require valid defaults/retention, value-copy lifetime and the permitted SET status for invalid or unavailable attributes. No unspecified identity or unsupported attribute may become a fabricated successful response.

Obligations: ENHANCED-12 through ENHANCED-14, ENHANCED-16 and ENHANCED-17 excluding periodic selection.

## IEEE802154-C-ENHANCED-SCHEDULE

Repeat synchronization, CAP DATA and both GTS directions with enhanced beacons. Observe actual IE-derived windows/grants, length-dependent CAP fit and loss/reacquisition; no second schedule may be inferred from unrelated data.

Before scheduled integration, require the periodic enhanced-selection attribute to be unavailable. Once implemented, write macUseEnhancedBeacon true/false and observe the corresponding actual periodic beacon formats, retaining the same schedule/grant owner. Default/nondefault reset must apply the selected default/retention behavior.

Obligations: ENHANCED-15, ENHANCED-16 and ENHANCED-17 periodic selection.

ACK timing checks include nonzero propagation, unknown receiver-selected optional IE lengths, and starts/completions exactly at and one representable tick outside the applicable bound. Scheduled checks require all supporting scheduled mechanisms; parser success alone establishes none of these exchanges. Protected, TSCH and DSME semantics remain separate checks outside the selected unsecured profile.
