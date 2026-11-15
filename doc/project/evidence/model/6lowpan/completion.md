# 6LoWPAN completion evidence

> **Kind:** ledger · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [6lowpan-completion.md](../../../../../plan/pending/6lowpan-completion.md), [release-validation.md](release-validation.md)

This ledger tracks P8–P15 separately from the archived P0–P7 acceptance. The user requested completion of the remaining packages on 2026-11-15. Passing one package does not establish later functionality. Existing first-release fixtures remain regression controls.

## P8 — Stateful contexts

Production owner: per-interface `LowpanLayer` and its bounded `LowpanContextTable`; codec: `LowpanIphcCodec`. Prefix lengths 0–128 are merged with inline/link-derived IID bits with context precedence. Source and destination IDs are independent; context 0 can omit CID. Prefix-based multicast uses a context prefix of at most 64 bits. Compression permission and decompression validity are distinct. Static XML configuration is optional and defaults empty.

Direct evidence: `LowpanStatefulContexts` unit vectors and the [native protocol fixture](../../../../../tests/protocol/6lowpan/stateful-contexts.md). The native fixture requires ten literal input frames, six exact IPv6 deliveries, two invalid-context drops, one replacement invalidation, two reassembly completions, five fragments and one real radio data transmission. It proves exact expiry rejection, stable first-fragment snapshots, FRAGN-first invalidation on prefix replacement, multicast reconstruction and receive-only source permission in emitted bytes.

The [P8 verification record](p8-verification.json) retains exact changed-source hashes, commands, exit statuses and log paths. All selected checks pass: 12 units, 62 modules and six protocol fixtures/twelve parameter runs, matching debug/release builds, feature-off build/five shared modules/three IEEE 802.15.4 fingerprint configurations, and core-without-UDP build/three units/one stateful protocol fixture. Original feature-state bytes and generated NED exclusions are restored. Raw logs remain under `/tmp/lowpan-p8/`. The initial unit enum spelling error was a fixture compilation error, not a protocol failure; its correction is retained in the raw attempt logs. The complete parent protocol fixture cannot execute unchanged against the parent API, so no runtime failing-parent claim is made.

## Remaining packages

P9–P15 remain in execution order under the completion plan. Extension-header NHC, optimized/extended ND, mesh-under, paging, virtual fragment forwarding and selective recovery are not inferred from context support.
