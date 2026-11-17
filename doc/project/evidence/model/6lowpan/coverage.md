# First-release 6LoWPAN coverage

> **Kind:** ledger · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [publication plan](../../../../../plan/pending/6lowpan-publication-plan.md), [features](../../protocol/6lowpan/features.md)

This publication prefix implements original P0–P7. The standards capability map also lists later packages; those rows do not imply implementation here. [PR03 verification](publication/pr03-verification.json) identifies the tested C06 model, source/configuration/library hashes, exact commands and retained outputs.

| Contract | Direct evidence at C06 |
| --- | --- |
| P0/P1 identity, lower boundaries and legacy indications | Four legacy module fixtures, native address/frame vectors and native receive matrix. |
| P2/P3 bounded dispatch, fragmentation and reassembly | Native fragmentation/reassembly units; production receive, sender-boundary, identity, restart and tag-guard protocol fixtures. |
| P4/P5 stateless IPHC and carried-checksum UDP NHC | Codec units and receive matrices; computed-checksum native UDP and route-over module runs. |
| P6 native link service and 1280-byte IPv6 composition | Real native queue/MAC/radio sender tests, six independently decoded captures, native identity and FCS checks. |
| P7 selected-profile acceptance | 11 unit fixtures, 65 module fixtures/87 runs, five native protocol fixtures/11 runs, two generic IPv6 protocol fixtures; exact minimal core and both application leaves. |

Both compilation modes pass; native protocol executions above use debug. Expected errors in 17 module runs are recorded separately from 70 normal completions. The core required-feature closure excludes UDP and both application leaves. Feature-off shared controls and stored NIC fingerprints are separate regression evidence.

The native profile is unsecured same-PAN IEEE 802.15.4 at 250 kbit/s, extended unicast and short broadcast, stationary radios and a dedicated medium, static IPv6 neighbors, and MTU 1280. See [encoding choices](encoding-profile.md) and [link contract](link-contract.md). Contexts, extension NHC, optimized ND, mesh, paging and recovery belong to later review units.

Capture decoding and serialized global-address vectors are bounded evidence. They do not establish live external-radio interoperability or every standardized encoding. The external fully elided-EUI comparison fails at the two IID U/L-bit positions; the normative INET mapping is retained.

## Publication through C29

At the PR10/C29 checkpoint, P8 stateful contexts, P9 extension/nested-IPv6 NHC and the selected P10 legacy registration/discovery/authority/relay/context-distribution profile are implemented. Their fixtures and companion explanations ship with their owning commits C08–C29. The publication verification manifest identifies each exact review tip and its executed scope; no later updated-host, mesh, paging or recovery behavior is inferred for this checkpoint. The first-release tables above remain the bounded C06/P0–P7 acceptance account.
