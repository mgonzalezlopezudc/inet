# 6LoWPAN review series

> **Kind:** report · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [publication plan](../../../../../../plan/pending/6lowpan-publication-plan.md), [actual commit and verification map](manifest.json)

P0–P15 remain the original implementation contracts. PR01–PR17 are review boundaries, and C00–C43 are publication commits. The table connects them without renumbering the original work. The local branches form a linear stack in the listed order; semantic prerequisites are documented in the plan.

| Review unit | Publication commits | Original contract | Contribution |
| --- | --- | --- | --- |
| PR01 | C00–C03 | P0/shared prerequisites | Publication map, initialization ordering, final-address ICMPv6 checksums/error size, decoded Fragment-header lifetime. |
| PR02 | C04–C05 | P1, P2a/P6 values | Legacy receive identity/opaque dispatch and independently usable native address/frame representations. |
| PR03 | C06–C07 | P2–P7 | Complete native MAC/adaptation, fragmentation, stateless IPHC/UDP NHC and first-release acceptance. |
| PR04 | C08 | P8 | Stateful context ownership, validity and fragment snapshots. |
| PR05 | C09–C12 | P9 | Extension/nested-IPv6 wire helpers, chain codec, production integration and constant rename. |
| PR06 | C13–C14 | P10/P11 prerequisites | Generic registration wire representation and address-validity queries. |
| PR07 | C15–C21 | P10 server | Owned bindings, native registration/discovery replies, malformed-input/validity guards and constant rename. |
| PR08 | C22–C23 | P10 legacy hosts | Automatic discovery/registration, refresh and retry timing. |
| PR09 | C24–C27 | P10 routed registration | Authority, bounds, native border admission and relay. |
| PR10 | C28–C29 | P10 context distribution | Bootstrap compression independence and authoritative advertisements. |
| PR11 | C30–C31 | P11 registrar | Updated ownership/transaction behavior and unsolicited Moved notifications. |
| PR12 | C32–C33 | P11 hosts | Common source eligibility and updated host transactions/lifecycle. |
| PR13 | C34 | P12 | Mesh-under forwarding. |
| PR14 | C35 | P13 | Paging and ESC interpretation/transit. |
| PR15 | C36–C37 | P14 prerequisites | Terminal native completion and paced source fragmentation. |
| PR16 | C38–C39 | P14 forwarding | Early fragment forwarding, fallback and original-size/actual-MTU correction. |
| PR17 | C40–C43 | P15 and combined acceptance | Selective recovery, evidence consolidation and separate plan archive/link commits. |

The manifest and per-unit records identify actual hashes and test cohorts. The [review records](reviews.md) explain metadata corrections and executable preservation. A checkpoint retains every old tested cut. The final external delivery index records all 44 commit IDs once they exist, including the final metadata commit's own hash.

This is a local reconstructed series with review descriptions. No remote PR creation or push is implied by this record. Original source history and the eleven pre-existing local edits remain preserved.
