# 6LoWPAN implementation coverage

> **Kind:** ledger · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [publication plan](../../../../../plan/done/6lowpan-publication-plan.md), [standards capabilities](../../protocol/6lowpan/features.md)

The selected implementation covers original P0–P15 through the publication cuts below. [Publication verification](publication/manifest.json) identifies every actual commit/review tip, its original source commitments, focused and combined checks, final acceptance and preserved limitations. Historical input records retain their original source/library identities; they are not executions of the reconstructed series.

| Original contract | Publication owner | Direct evidence |
| --- | --- | --- |
| P0/P1 standards, profile and receive boundaries | C00, PR01–PR03 | Standards catalogs, generic IPv6 controls, legacy metadata modules, native address/frame vectors and receive matrix. |
| P2–P7 native IPv6 data plane | PR03 | Dispatch, IPHC/UDP NHC and fragment units; five native protocol fixtures for radio boundaries, byte-only reassembly, identity, restart and tag guards; six independently decoded captures and global-address vectors. |
| P8 stateful IPHC | PR04 | Context unit/protocol vectors for CID, prefixes, multicast, permission, validity and partial-reassembly snapshots. |
| P9 extension and nested-IPv6 NHC | PR05 | Wire/chain units and production transmission/reconstruction, malformed input and first-fragment fallback. |
| P10 discovery, legacy registration, authority, relay and contexts | PR06–PR10 | Generic wire/validity units; literal-peer and real native/routed fixtures for local responders, automatic hosts, deadlines, ownership and authoritative propagation. |
| P11 updated registrar and host transactions | PR06, PR11–PR12 | Identity/TID comparison, registrar races, unsolicited Moved correction, common source guard, host registration and lifecycle fixtures. |
| P12 mesh-under | PR13 | Mesh/BC0 wire units and multihop identity, hop/duplicate, fragment, resource and lifecycle observations. |
| P13 paging/ESC | PR14 | Packet-local parser/wire vectors, endpoint interpretation and opaque mesh transit. |
| P14 completion, source pacing and virtual forwarding | PR15–PR16 | Twenty completion-module cases, real ACK-relative pacing, early forwarding/fallback, lifecycle/resource bounds and original-size/actual-MTU refusal and cancellation. |
| P15 selective recovery | PR17 | Partition/tag/wire units; three protocol fixtures for bounded source/relay/receiver behavior, repair, ACK validation, raw no-UDP operation, copied ownership and eight-link timing bounds. |

The final debug campaign executes 22 unit fixtures, 69 module fixtures/110 runs and 35 protocol fixtures/287 runs, including the two new generic IPv6 checks. Declared program-error cases remain separate from normal completions. Targeted release execution compares 393 unconditional observation rows; release assertions are disabled.

Feature-off and actual UDP-disabled checks are distinct from normal enabled tests. Required-feature closures are verified with their exact enabled IDs and local library/NED roots. [Release validation](release-validation.md) and its linked records own the actual selections, counts and limits.

This is a bounded simulation implementation. Native operation uses the selected unsecured same-PAN IEEE 802.15.4 profile at 250 kbit/s. See NED declarations, the user guide and [encoding contract](encoding-profile.md) for configuration and supported forms. Association, link security, AP-ND, backbone federation, RPL signaling and unimplemented external payload families are outside the selected profiles. Serialized external vectors do not establish live external MAC/radio interoperability; the fully elided-EUI mismatch remains a failed comparison.
