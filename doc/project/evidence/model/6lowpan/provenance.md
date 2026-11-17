# 6LoWPAN publication provenance

> **Kind:** report · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [standards map](../../protocol/6lowpan/standards.md), [publication plan](../../../../../plan/pending/6lowpan-publication-plan.md)

The publication plan maps original P0–P15 packages and original commit IDs to C00–C43 and PR01–PR17. Its frozen input is `9908c007bdb1eda9d74f6ca1f99fe1cae33fbc61`; the pinned upstream base is `91219b0fa7d2794ee67ac8a3dd45c3a09cfc45b0`. The prepared source oracle `1853c49ba0c29edeb2cfed5378c01305beb519ba` excludes the specified Wi-Fi-only changes and adds the explicitly approved feature requirements and generic IPv6 test fixtures.

This prefix's new executed evidence belongs to C06, `1758fe1ddfa145df918e26358e7bbd30619cecea`, identified in [PR03 verification](publication/pr03-verification.json). C07 introduces the acceptance procedures and metadata with unchanged executable inputs. Original runtime records are historical evidence for their original source/library cohorts and are retained separately at C41.

The RFC corpus and catalogs came from the original standards inputs. [Errata snapshot](errata-20261115.json) retains its 2026-11-15 retrieval metadata; it is not a fresh network retrieval. The standards map identifies applicable RFC and IEEE profiles. No broader conformance claim follows from a successful selected simulation.

The optional [external adapter](../../protocol/6lowpan/checks/peer/README.md) exchanges serialized vectors with a separately built pinned ns-3 reference. INET production code and simulations have no ns-3 runtime dependency. The adapter copies no external production implementation into INET. Its known fully elided-EUI disagreement is retained as a failed comparison.
