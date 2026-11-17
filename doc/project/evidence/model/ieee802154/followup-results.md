# Beacon, GTS and enhanced-frame execution record

> **Kind:** measurement · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [beacon-profile.md](beacon-profile.md), [enhanced-frame-profile.md](enhanced-frame-profile.md), [results.md](results.md)

Execution started 2026-11-16 in `/home/miguel/workspace/inet-ieee802154-standard`, from `efced47d1f952dc0094c09a4b17696c58365e4a8`. The user authorized executing the reviewed B1–B3/E1 plans through final publication to `origin/feat/inet-802154-standard`. The plans were recorded by `9b29b670fa`. The unrelated modified AGENTS.md is excluded from every commit; its SHA-256 remains `4405182693b0fa0b0b5c41b8d7c9ff59eb5014e433f3b133ccbed1512a939a82`.

This record distinguishes source-audit closure, compilation, executed fixtures and future acceptance gates. It does not replace the [M1/M2 results](results.md) or promote earlier results to evidence for new behavior.

## Execution status

| Units | Current state | Direct evidence |
| --- | --- | --- |
| B-00 | Source/service audit complete; committed `e06f55bf18` | Independent review confirmed the corrected catalog/English checks and beacon profile. First-PPDU timestamps require selected offset10, while offset0 denotes post-SFD. Local PIB/queued admission remains available during PHY-only down. |
| B-01 | Implementation in progress | No new build or test verdict yet. |
| B1-02 through B-99 | NOT_RUN | Dependent implementation, fixtures, review companions and release evidence remain open. |
| E1-00 | Source/layout/service audit complete; independent review confirmed closure | Enhanced catalog/checks/profile contain complete PAN-presence and termination matrices, enhanced PIB/service inputs, concrete consumers and selected service/filter boundaries. No new runtime result is implied. |
| E1-01 through E1-99 | NOT_RUN | All implementation and direct runtime acceptance gates remain open. |

## Baseline control run

Before new compiled-source edits, ran the existing debug library against two production protocol fixtures with the local project description:

```sh
source setenv -q
opp_run_protocol_tests --load . -p inet -m debug --no-build \
  -w '^tests/protocol/ieee802154$' \
  -f 'Ieee802154(DataAck|Lifecycle)\.test$' \
  --log-file /tmp/ieee802154-followup-baseline.log
```

Exit0; two selected fixtures PASS (`Ieee802154DataAck.test`, `Ieee802154Lifecycle.test`). The library and source belonged to the completed M1/M2 baseline; no new feature code was loaded. This control does not establish beacon/slotted/GTS/enhanced behavior.

## Source audit artifacts

The pinned PDF hash is recorded in both profiles. The current local standards corpus is ready and fresh; its linter retains four unresolved references and extraction warnings already described in the plans. Execution-time status/lint records are `/tmp/ieee802154-execution-corpus-{status,lint}.json`; new raw retrievals are under `/home/miguel/workspace/.codex-artifacts/ieee802154-followup-execution-20261116/`. The earlier reviewed source records remain under the companion `ieee802154-followup-plans-20261115/` directory. Corpus readiness proves retrieval availability, not protocol conformance.

## Publication gate

Final debug and release compilation, every planned direct fixture, independent source review, architecture/naming/interface/source-seal/series checks, required trajectory controls, feature/clock isolation and release documentation remain open. No new baseline is approved or refreshed by this record. Final push is authorized after those gates complete with their actual outcomes recorded.
