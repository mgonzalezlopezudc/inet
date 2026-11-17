# 6LoWPAN publication validation

> **Kind:** report · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [coverage](coverage.md), [provenance](provenance.md), [publication verification](publication/manifest.json)

The publication manifest owns exact commit/source/configuration/library identities, commands, modes, fixture and simulation counts, expected diagnostics, raw artifact paths and inherited gate findings. Its per-PR records distinguish focused checks, combined checks and reused executable-equivalent evidence. Original runtime records remain historical.

The final enabled campaign passes both compilation modes, 22 debug unit fixtures, 69 debug module fixtures/110 runs and 35 debug protocol fixtures/287 runs. The two generic IPv6 protocol fixtures are included. Targeted release passes 18 protocol fixtures and the native completion module, with 393 matching unconditional observation rows. Assertions establish semantics in debug; release does not execute them.

Feature-off checks execute five shared module fixtures and three NIC run-0/seed-0 simulations with three stored ingredients at 100 seconds. Actual UDP-disabled tests cover codecs, native/completion behavior and the eight compatible advanced protocol selections, including raw recovery and original-MTU cases. Exact required-feature closures independently exercise the core and both application leaves. Local launchers and explicit NED/library roots identify the checkout actually under test. Profile-switching records identify exact feature-state restoration and matching rebuilt libraries; byte-identical library restoration is claimed only where recorded.

Six fresh first-release captures pass frame bounds, FCS and observed ICMPv6/UDP checksum checks. Serialized global-address vectors agree with the isolated pinned external reference and INET's byte-only production receiver. The fully elided-EUI comparison fails at IPv6 offsets 16 and 32. This is not live external-radio evidence. An initial relocated reference-build error and the superseded wrong-checkout direct-example attempt remain recorded separately.

Mechanical series/include/parameter/source-seal checks and scoped semantic reviews accompany the reconstructed units. Global architecture/interface/naming findings and the canonical C++ warning exit remain explicitly disclosed and attributed; they are not converted into a clean global result. No baseline is changed.

All observations are finite deterministic selections. They do not establish arbitrary seeds, topologies, unsupported profiles, every allocator, or broad RFC conformance. C42/C43 archive and update links without changing tested executable inputs.

Nine [invalid-configuration controls](publication/negative-controls.json) separately require initialization exit 1 and the specific diagnostic for unsupported compression/mesh and invalid window, lifetime, path, guard, gap and fragment-size bounds. Their isolated local-model executions are separate from the normal completion counts above.
