# Beacon, GTS and enhanced-frame execution record

> **Kind:** measurement · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [beacon-profile.md](beacon-profile.md), [enhanced-frame-profile.md](enhanced-frame-profile.md), [results.md](results.md)

Execution started 2026-11-16 in `/home/miguel/workspace/inet-ieee802154-standard`, from `efced47d1f952dc0094c09a4b17696c58365e4a8`. The user authorized executing the reviewed B1–B3/E1 plans through final publication to `origin/feat/inet-802154-standard`. The plans were recorded by `9b29b670fa`. The unrelated modified AGENTS.md is excluded from every commit; its SHA-256 remains `4405182693b0fa0b0b5c41b8d7c9ff59eb5014e433f3b133ccbed1512a939a82`.

This record distinguishes source-audit closure, compilation, executed fixtures and future acceptance gates. It does not replace the [M1/M2 results](results.md) or promote earlier results to evidence for new behavior.

## Execution status

| Units | Current state | Direct evidence |
| --- | --- | --- |
| B-00 | Source/service audit complete; committed `e06f55bf18` | Independent review confirmed the corrected catalog/English checks and beacon profile. First-PPDU timestamps require selected offset10, while offset0 denotes post-SFD. Local PIB/queued admission remains available during PHY-only down. |
| B-01 | Implemented and independently reviewed; debug acceptance passes | Source audit and actual-radio regression reproduced the callback-retirement defect, then the repair passed all8 BeaconClock and all60 PassiveScan typed/raw runs plus existing DATA/lifecycle controls. Final scoped review:23 PASS,11 N/A,0 FLAG,0 QUESTION. |
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

## B-01 first checkpoint and regression reproduction

The first B-01 source checkpoint built with `source setenv -q; make MODE=debug -j$(nproc)` (exit0), log `/tmp/ieee802154-b01-debug-build.log`. Against that matching library, the following explicit debug selections all passed with `--load . -p inet -m debug --no-build --no-concurrent`:

| Runner / selector | Result | Raw record |
| --- | --- | --- |
| `opp_run_unit_tests -f 'Ieee802154(SuperframeTiming|MacPib)_1\.test$'` | 2 fixtures PASS | `/tmp/ieee802154-b01-unit.{log,out}` |
| `opp_run_module_tests -f 'Ieee802154(SuperframeController|PhyServices|PhyServiceContract|Measurements|ExternalPhy|PassiveScan|Composition)_1\.test$'` | 7 fixtures PASS; PassiveScan executed all52 typed/raw runs | `/tmp/ieee802154-b01-module.{log,out}` |
| `opp_run_protocol_tests -w '^tests/protocol/ieee802154$' -f 'Ieee802154(DataAck|Lifecycle|PhyLifecycle)\.test$'` | 3 fixtures PASS | `/tmp/ieee802154-b01-protocol.{log,out}` |

The source audit then identified a supported synchronous callback path: a promiscuous client submits nondefault RESET, native PHY confirms inline, and ordinary started/available flags return true before the old receiving call resumes. Its already consumed local clock anchor is no longer in controller storage for reset to clear. The new `Ieee802154BeaconClock.test` is a narrow actual-radio/native-service regression, accompanied by `beacon-clock.md` in the same planned behavior commit.

The first narrow run had a copied module-depth NED path and failed in setup, before behavior; it is not protocol failure evidence. After removing that override, ran `opp_run_protocol_tests --load . -p inet -m debug --no-build --no-concurrent -w '^tests/protocol/ieee802154$' -f 'Ieee802154BeaconClock[.]test$' --log-file /tmp/ieee802154-b01-beaconclock-red2.log`. Exit1: normal typed/raw runs passed, then scenario1/raw=false failed at `t=0.002120003336` because `handleBeaconNotifyIndication` ran while only the first beacon had started, after its callback RESET. The six-run campaign stopped with two successful, one error and three skipped runs. This is a reproduced pre-retirement-fix failure at the intermediate B-01 checkpoint, not a replay of the fixture on the incompatible pre-controller M2 parent.

Source/library SHA-256 identity and the changed-source patch are retained in the execution artifact directory as `b01-pre-retirement-fix-{manifest.json,patch}`. The causal red fixture, generated source and raw simulation stdout/stderr are retained in `b01-beaconclock-prefixed-red/`. The post-repair result follows below; no earlier pass is relabelled as evidence for that repair.

## B-01 repaired checkpoint

The MAC now retires the receiving call's continuity identity on accepted RESET, stop/crash and PHY-down, independently of the optional anchor store. It checks the identity after actual callback-bearing continuations; ordinary SET leaves reception valid. The source review also removed duplicate PIB configuration state and deferred optional dynamic attributes until their real consumers land: macBeaconTxTime in B1-02, macGtsPermit in B3-02. The added pure PHY callback has migration guidance and a source-interface release note; existing concrete clients were updated explicitly.

The intermediate repair run exposed two fixture defects, distinct from production failures: BeaconClock's six simulations all succeeded but its wrapper expected an unreachable compile marker; the new non-scan PassiveScan branch had not retuned the receiver to the injected beacon's channel. Both expectations/setup defects were corrected without weakening the behavioral assertions. The final protocol fixture also adds the controller-disabled RESET case, preserving offset0/post-SFD timestamps while proving the same MAC retirement barrier.

After a fresh debug rebuild, exit0 (`/tmp/ieee802154-b01-crossprofile-build.log`), executed:

```sh
source setenv -q
opp_run_module_tests --load . -p inet -m debug --no-build \
  -f 'Ieee802154PassiveScan_1[.]test$' \
  --log-file /tmp/ieee802154-b01-crossprofile-module.log
opp_run_protocol_tests --load . -p inet -m debug --no-build --concurrent \
  -w '^tests/protocol/ieee802154$' \
  -f 'Ieee802154(BeaconClock|DataAck|Lifecycle|PhyLifecycle)[.]test$' \
  --log-file /tmp/ieee802154-b01-crossprofile-protocol.log
```

Both commands exit0. PassiveScan passes all60 typed/raw runs, including first-symbol timestamps with both fixed drift signs, actual RX reset/PHY-stop recovery, malformed FCS, callback/signal RESET and ordinary SET controls. BeaconClock passes all8 typed/raw runs; all four selected protocol fixtures pass. The unchanged timing/PIB owners passed both focused units at the intermediate repaired checkpoint (`b01-unit-fixed`), and the six unchanged controller/PHY/composition module controls passed (`b01-module-fixed`). These prior unchanged-owner controls are retained as such, rather than misreported as newly executed by the final two commands.

Scoped MAC/PHY architecture, MAC naming/interfaces, source-seal guard, links, release fragments and document seals pass. Raw scoped gate records are `/tmp/ieee802154-b01-{architecture-mac,architecture-phy,naming,interfaces,source-seals}.log`. Diagram source is reviewed; rendering remains unverified because the CLI is unavailable. Release compilation and the complete combined final campaign remain publication gates after the remaining implementation units.

The same independent reviewer confirmed the repaired working tree against `013ecb252dd5d2aa3624c73e7422e85efe54df82`:23 PASS,11 N/A,0 FLAG,0 QUESTION in the canonical general checklist. No source seal or exception-ledger change applies. Concurrent copied keys have direct-role coverage; simultaneous live concurrent-radio reception was not separately exercised. Commit-message/series gates and final publication gates are not folded into this source verdict.

## Publication gate

Final debug and release compilation, every planned direct fixture, independent source review, architecture/naming/interface/source-seal/series checks, required trajectory controls, feature/clock isolation and release documentation remain open. No new baseline is approved or refreshed by this record. Final push is authorized after those gates complete with their actual outcomes recorded.
