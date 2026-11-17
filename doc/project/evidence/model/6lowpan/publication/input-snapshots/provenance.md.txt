# 6LoWPAN standards provenance

> **Kind:** report · **Status:** snapshot 2026-11-15 · **Seal:** none · **Owns:** — · **Stands on:** [standards.md](../../protocol/6lowpan/standards.md), [encoding-profile.md](encoding-profile.md)

The first-release profile uses RFC 4944 fragmentation, RFC 6282 stateless IPHC and carried-checksum UDP NHC, and the unsecured IEEE 802.15.4-2024 subset recorded in [coverage.md](coverage.md#native-subset-selected-for-implementation). This review closes P0's retrieval gap; it does not establish release acceptance or support for the deferred protocol extensions.

This snapshot remains pinned to the first-release source below. The [completion ledger](completion.md) records subsequent implementation and validation; it does not change this snapshot's retrieval or errata findings.

## Inputs and revision

Implementation source: `6a11f885acd503273abd53ab47d546f12b38b41e`, tree `067ce2a25597f99d2d38f2a2d78a32c149380a15`. The pre-existing local `AGENTS.md` edit is outside the model. The P7 continuation adds test and evidence files without modifying production source. Exact runtime commands, fixture hashes and verdicts belong to [release-validation.md](release-validation.md).

All eight cached RFC text hashes match [source-hashes.sha256](source-hashes.sha256). The shared IEEE PDF is `../inet-standards/802154-2024.pdf` relative to the INET checkout; its SHA-256 is `851d81127297f898c956fdd1ce92929324d68cd696fd338d1e876f6a372f22d3`, matching the original source inventory. Selected IEEE clauses and source-span locators are in the coverage ledger. The first-release RFC clause selections and deterministic stimuli are in the [plan's test matrix](../../../../../plan/done/6lowpan-implementation-plan.md#5-required-test-matrix) and [encoding profile](encoding-profile.md).

## Current errata snapshot

The complete [RFC Editor JSON export](https://www.rfc-editor.org/errata.json) was retrieved with HTTP 200 at `2026-11-15T16:13:56Z`: 11,912,336 bytes, SHA-256 `de78c0cc7545d3b2261f53583805c11fd75c84a3aa789c6a6b7dffff8f8b9b14`. The [filtered snapshot](errata-20261115.json) preserves every returned record for all eight selected RFCs, including rejected and held reports. The full download and HTTP metadata are retained in `/tmp/lowpan-p7-20261115/errata.json` and `errata-download.json`. Empty rows below mean no matching record in this retrieved export; they are not claims about future reports. The individual RFC search endpoints remained unavailable, so this report uses the complete export rather than inferring absence from a failed search.

| RFC | Reports in the export | Applicability |
| --- | --- | --- |
| 4944 | 4359, 6194 — Verified | 4359 corrects the ESC selector range; use the later dispatch assignments. 6194 corrects short-address autoconfiguration wording. Short-address unicast autoconfiguration remains outside the native profile; do not substitute it for RFC 6282's short-address decompression rule. |
| 6282 | 4814 — Rejected | The proposed Hop Limit change is rejected. Keep the published encoding and the distinction between a carried value of 255 and hop-limit forwarding checks. No verified correction to the EUI-64 U/L-bit inversion appears in this snapshot. |
| 6775 | 8338 — Verified | Removes nonexistent Reserved bits from ABRO. Applies to deferred P10 optimized ND, not static first-release neighbors. |
| 8505 | 5557 — Held for Document Update; 5574, 8433, 8484 — Verified | Abstract wording, Table 2 labels/flag allocation, and the registering node's SLLA in route-over NS(EARO). Carry these into deferred P11; no EARO path is implemented here. |
| 8025 | None | No errata record in the export. Paging remains deferred P13. |
| 8066 | None | No errata record in the export. Extended dispatch remains deferred P13; unsupported input is rejected explicitly. |
| 8930 | None | No errata record in the export. Fragment forwarding remains deferred P14. |
| 8931 | None | No errata record in the export. Selective recovery remains deferred P15. |

The current official information pages list RFC 6282, 6775, 8025, 8066 and 8931 as [updates to RFC 4944](https://www.rfc-editor.org/info/rfc4944/), and RFC 8066 as the [update to RFC 6282](https://www.rfc-editor.org/info/rfc6282/). These are already included in the selected family. Their existence does not enable the separately deferred capabilities.

## Fully elided extended addresses

RFC 6282 section 3.2.2 requires inversion of the EUI-64 U/L bit when reconstructing an IID. For source `0123456789abcdef` and destination `a123456789abcdef`, the link-local addresses are `fe80::323:4567:89ab:cdef` and `fe80::a323:4567:89ab:cdef`. The new native protocol fixture constructs those expected bytes independently and injects literal IPHC `7a 33 3b` through the MAC. The [reviewer document](../../../../../tests/protocol/6lowpan/native-reassembly.md) explains the path and assertions. A disagreement with a particular peer must remain visible; it does not authorize changing this normative mapping.
