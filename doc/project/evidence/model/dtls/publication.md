# DTLS publication evidence

> **Kind:** report · **Status:** snapshot 2026-11-17 · **Seal:** none · **Owns:** — · **Stands on:** [scope](scope.md), [publication plan](../../../../../plan/done/dtls-publication-plan.md), [verification record](publication-verification.json), [history identities](publication-history.json)

The complete modeled DTLS implementation is reconstructed as 53 commits in 18
sequential INET review units. The 50 commits through the final code change have
focused build/runtime evidence. The remaining steps publish this acceptance
record, archive the plan without changing its contents, and update incoming links.

| Final scope | Debug | Release |
| --- | ---: | ---: |
| DTLS protocol fixtures | 352 PASS | 62 PASS |
| DTLS/TLS unit fixtures | 15 PASS | 15 PASS |
| DTLS module fixtures | 26 PASS | 26 PASS |
| Generic packet-tap, UDP and IPv6 controls | 7 PASS | 7 PASS |
| Asserting examples | 16 PASS | 16 PASS |

The release interaction set is the historical 60 fixtures plus the PMTU boundary
and inflated ClientHello cases. All 160 selected endpoint measurements agree
between modes. Six original statistical configurations pass against their
unchanged stored values. Three debug profiles also pass: TLS without DTLS,
DTLS/TLS disabled, and DTLS without examples. Each restores exact feature state.
The scenario commit additionally passes ten asserting examples, five exact
invalid-action refusals and two real-stack controls.

Clean historical checks cover standalone TLS serialization in both modes and the
algorithm-catalog and legacy-session-storage safe points in debug. Generated
message files without an owning message source are quarantined during these
checks and omitted from regenerated Makefiles. Earlier contaminated spot checks
are explicitly excluded. The exact tested final library bytes are restored.

## Input identity and approved corrections

The original branches remain `feat/dtls` at
`cb1691184444afa64b1819348d446d4cad610474` and `feat/dtls-extensions` at
`5284847df92eea8451bf056d8722fabb0ddf65bd`. Reconstruction starts at pinned
upstream `476ab0f2fa019c0bcf36d23d9f99ec884ee9393c`. The final approved input is
`89285a8cdbc9777f9531a2ea354d6a30ea154654`.

Two explicitly approved revisions qualify the original source projection:

- TLS/DTLS encoders publish cached bytes only after validation. Their two existing
  unit fixtures require repeated rejection of inconsistent native lengths. The
  original input reproduces the inflated ClientHello release failure; corrected
  fixtures pass in both modes.
- The C19 statistics pin is `411462a19e67dda7438213bfa3dec4d1257dd05e`.
  Its complete tree is byte-identical to the original
  `3d3a8995a00d75f2f21ccd42a648db591974611c`: only the provenance message changes.
  The canonical list, first-record attribution and six new comparisons pass.
  The [paired statistics branch](https://github.com/mgonzalezlopezudc/statistics/tree/review/dtls-06-closure-and-core-examples)
  retains the original CI-produced files and environment record. No expectation
  is regenerated.

Every other executable source, fixture, example and feature declaration equals
the immutable input. Message-only rewrites preserve whole trees. Baseline repinning
changes only `inet.opp` in C19 and its descendants; model executions are reused
with explicit source/configuration/fixture/library and baseline-tree identities.

## Gates and limits

The five series gates, source/document protection and canonical baseline provenance
pass through the code prefix. The archive and link commits run their own mechanical
checks. Scoped architecture, NED/MSG naming and interface checks pass for DTLS,
shared TLS, the local contract and adapter. Their global findings exactly match
the pinned upstream target and remain reported as inherited failures.

The complete C++ gate reports findings: 4,631 occurrences, 2,198 distinct location/
message tuples, and zero compiler errors. Of the distinct findings, 578 are in
generated headers, 1,571 in other framework/library files and 49 in hand-authored
DTLS/TLS/adapter sources. The latter comprise 24 parameter-shape candidates, 16
nodiscard suggestions, four guarded optional-access candidates, two string-compare
and two return-initializer suggestions, and one Protocol-instance naming candidate.
The verification record retains their assessment, including the local-identity/
shared-catalog policy question. No suppression, new naming exception or seal is
introduced. This is not a warning-free C++ result.

Exact commands, selectors, seeds, fixture/library hashes, normalized results and
excluded attempts are in the verification record. Raw local artifacts remain in
`/home/miguel/workspace/.codex-artifacts/dtls-publication-20261117`. Historical input
reports keep their original revisions; they are not fresh checks of this series.

The checks establish finite, fixed-seed simulation outcomes for retained modeled
content, overhead, processing costs and resource bounds. They establish neither
real cryptographic security, unrestricted interoperability, exhaustive conformance
nor statistical confidence. Unexecuted exact-parent comparisons remain explicitly
source-based; missing APIs are not treated as observed runtime defects.

## Delivery

Review branches are `review/dtls-01-shared-prerequisites` through
`review/dtls-18-examples-and-acceptance`, using the names in the plan. Upstream unit
01 targets master. Each later incremental PR follows its predecessor landing or
maintainer-provided upstream base refs. All unit descriptions and exact fork
comparisons are prepared together. Original INET #1459/#1465 and statistics #1
remain open and unchanged pending separate supersession authorization.
