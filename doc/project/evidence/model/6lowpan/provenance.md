# 6LoWPAN source and evidence provenance

> **Kind:** report · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [standards map](../../protocol/6lowpan/standards.md), [publication plan](../../../../../plan/pending/6lowpan-publication-plan.md)

The original implementation input is `9908c007bdb1eda9d74f6ca1f99fe1cae33fbc61`. The pinned upstream base is `91219b0fa7d2794ee67ac8a3dd45c3a09cfc45b0`. The source oracle `1853c49ba0c29edeb2cfed5378c01305beb519ba` retains the selected implementation, excludes the specified unrelated Wi-Fi changes and adds only the approved feature requirements and two generic protocol fixtures. The publication model tree equals that oracle exactly.

[Publication verification](publication/manifest.json) maps all 95 original dispositions to C00–C43 and PR01–PR17. Actual test records identify their tested revisions. A checkpointed metadata repair changes documentation and message attribution while preserving every source, executable fixture, configuration and example input. The manifest links the old tested cuts to their executable-equivalent review cuts rather than pretending they were rerun.

RFC texts and catalogs retain their standards-source provenance. [Errata snapshot](errata-20261115.json) remains the dated 2026-11-15 retrieval. Historical source-hash files and stage JSON are preserved with their original paths and identities; [input snapshots](publication/input-snapshots/README.md) explain their scope. They are separate from new runtime observations and do not imply a fresh standards retrieval.

The optional [external vector adapter](../../protocol/6lowpan/checks/peer/README.md) uses a separately built pinned ns-3 reference. No external production implementation is copied into INET, and INET simulations have no ns-3 runtime dependency. Six C06 native captures and two fresh global-address exports are checked independently. The separate fully elided-EUI disagreement remains a failed external comparison, with the RFC 6282 U/L-bit expectation retained.
