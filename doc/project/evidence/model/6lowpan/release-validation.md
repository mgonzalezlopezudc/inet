# First-release publication validation

> **Kind:** report · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [coverage](coverage.md), [provenance](provenance.md)

The [PR03 record](publication/pr03-verification.json) owns exact source/configuration/library identities, selections, counts, commands and external artifact paths. It distinguishes C06 executions from metadata-only C07 and from historical input-branch results.

C06 passes debug/release compilation, 11 unit fixtures, 65 module fixtures (87 runs: 70 normal and 17 declared errors), five native protocol fixtures (11 runs), and two generic IPv6 protocol fixtures. Native protocol behavior was executed in debug. Required-feature closure tests independently build the core without UDP/applications, run bare self-contained codec/native cases, and execute the two example leaves with receiving-application observations. Exact feature/NED bytes and both full-profile libraries are restored.

Six fresh C06 native captures pass the external TShark checker for PSDU bounds, FCS and observed ICMPv6/UDP checksums. Fresh route-over exports match the checked-in global-address vectors. The pinned external reference independently decodes and encodes these bytes, while INET's byte-only receive fixture proves production reconstruction. The separate fully elided-EUI comparison disagrees at IPv6 offsets 16 and 32; matching the stored incompatible output reproduces that failure rather than establishing agreement.

Feature-off regression uses five shared module fixtures and three CSMA NIC run-0 simulations, each with three stored fingerprint ingredients at 100 seconds. Fingerprints establish selected trajectory agreement, not protocol correctness. No baseline is updated. The first external comparison attempt failed before comparison because a relocated historical build referenced obsolete paths; the fresh isolated rebuild and second attempt are recorded separately.

This acceptance covers the bounded P0–P7 profile in [coverage](coverage.md). It does not establish live external MAC/radio operation, later completion features, or arbitrary configurations. Final combined acceptance is C41.
