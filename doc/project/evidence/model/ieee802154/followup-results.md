# Beacon, GTS and enhanced-frame execution record

> **Kind:** measurement · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [beacon-profile.md](beacon-profile.md), [enhanced-frame-profile.md](enhanced-frame-profile.md), [results.md](results.md)

Execution started 2026-11-16 in `/home/miguel/workspace/inet-ieee802154-standard`, from `efced47d1f952dc0094c09a4b17696c58365e4a8`. The user authorized executing the reviewed B1–B3/E1 plans through final publication to `origin/feat/inet-802154-standard`. The plans were recorded by `9b29b670fa`. The unrelated modified AGENTS.md is excluded from every commit; its SHA-256 remains `4405182693b0fa0b0b5c41b8d7c9ff59eb5014e433f3b133ccbed1512a939a82`.

This record distinguishes source-audit closure, compilation, executed fixtures and future acceptance gates. It does not replace the [M1/M2 results](results.md) or promote earlier results to evidence for new behavior.

## Execution status

| Units | Current state | Direct evidence |
| --- | --- | --- |
| B-00 | Source/service audit complete; committed `e06f55bf18` | Independent review confirmed the corrected catalog/English checks and beacon profile. First-PPDU timestamps require selected offset10, while offset0 denotes post-SFD. Local PIB/queued admission remains available during PHY-only down. |
| B-01 | Committed `29e98419cd`; independently reviewed; debug acceptance passes | Source audit and actual-radio regression reproduced the callback-retirement defect, then the repair passed all8 BeaconClock and all60 PassiveScan typed/raw runs plus existing DATA/lifecycle controls. Final scoped review:23 PASS,11 N/A,0 FLAG,0 QUESTION. |
| B1-02 | Debug acceptance and independent review complete | Fresh build,4 unit/5 module/12 protocol fixtures PASS; schedule62/62. Five reproduced defects are repaired. Canonical review23 PASS,11 N/A,0 FLAG,0 QUESTION. Commit/publication gates follow below. |
| B1-03 through B-99 | NOT_RUN | Dependent implementation, fixtures, review companions and release evidence remain open. |
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

Before proceeding to B1-02, corrected classification metadata for the initial four new commits without changing any of their trees, preserving authors and the working tree. The tree-equivalence map is `commit-metadata-equivalence.json` in the execution artifact directory: plans `9b29b670fa`→`8406180907`, B-00 `e06f55bf18`→`035c89dc62`, E1-00 `013ecb252d`→`fb0dff1f35`, B-01 `5a70ad837c`→`29e98419cd`. The earlier hashes identify the historical audit/run basis; their trees are identical to the corresponding current commits. Classification checks over the four-commit series pass. No source, test result, baseline or unrelated local edit was rewritten by this metadata repair.

## B1-02 intermediate diagnostics

The second diagnostic campaign compiled successfully and passed the PIB/beacon unit fixtures, then failed in the new clock-event cancellation path: canceling an event before its first schedule passed arrivalClockTime=-1 to ClockBase. Both the schedule and callback-RESET clock fixtures exposed the assertion. This was a production lifecycle defect, not a fixture expectation failure. Records are `/tmp/ieee802154-b102-diagnostic2-{build.log,unit.log,unit.out,protocol.log,protocol.out}`.

After guarding cancellation with the clock module's scheduled-event query, diagnostic3 rebuilt debug successfully and passed both `Ieee802154(MacPib|Beacons)_1[.]test$` unit fixtures, `Ieee802154SuperframeController_1[.]test$`, and both `Ieee802154(BeaconSchedule|BeaconClock)[.]test$` protocol fixtures. BeaconSchedule executed all32 typed/raw cases. Each runner used `--load . -p inet -m debug --no-build`; protocol selection also used `--concurrent -w '^tests/protocol/ieee802154$'`. Records are `/tmp/ieee802154-b102-diagnostic3-{build.log,unit.log,unit.out,module.log,module.out,protocol.log,protocol.out}`. The writer held production source fixed throughout this build and run.

These are intermediate checks, not B1-02 acceptance: primary-duty overlap, channel-change failure rollback and additional stop/crash cases are still being completed. No later implementation unit or final publication gate is closed by these results.

The subsequent behavior checkpoint adds actual retune/PHY-loss rollback, active-receive overlap and MAC stop/crash recovery. A fresh debug build and all explicitly selected checks pass: three unit fixtures (`MacPib|Beacons|SuperframeTiming`), five module fixtures (`SuperframeController|Discovery|PassiveScan|Start|Composition`) and twelve protocol fixtures (`BeaconSchedule|BeaconClock|Broadcast|Reassociation|PanConflict|ReceiverEnable|Association|Indirect|Orphan|Lifecycle|PhyLifecycle|DataAck`). Unit/module names have the `_1[.]test$` suffix; protocol names have `[.]test$` with the same suite/mode arguments as above. BeaconSchedule executes42/42 successful typed/raw runs. Raw records are `/tmp/ieee802154-b102-acceptance-{build.log,unit.log,unit.out,module.log,module.out,protocol.log,protocol.out}`; checkpoint source hashes and retained raw/generated artifacts are in the execution artifact directory as `b102-acceptance-checkpoint.json` and `b102-acceptance/`.

Scoped architecture/naming/interfaces/source-seal checks also pass (`/tmp/ieee802154-b102-{architecture,naming,interfaces,source-seals}.log`). The stage remains open for direct beacon/CAP signals and their callback-reentry observations, independent review and the BO=SO transmit-preparation boundary assessment. These passing controls do not predate or validate those pending signal additions.

The final signal checkpoint adds `periodicBeaconStart` (actual first-symbol timestamp, low24 bits in local symbols) and `outgoingCapActive` (Boolean outgoing receive-duty transitions), with guarded callback continuations. Rebuilt debug and repeated the same3 unit,5 module and12 protocol selections: all pass, with46/46 schedule runs including synchronous RESET from each signal. Records are `/tmp/ieee802154-b102-final-{build.log,unit.log,unit.out,module.log,module.out,protocol.log,protocol.out}`. Source identity and retained raw/generated records are `b102-final-checkpoint.json` and `b102-final/` in the execution artifact directory. Independent review remains pending; no subsequent stage is closed by this checkpoint.

Review then found an uncovered inline recovery path: the interrupted START's STOPPED completion occurred before the general retirement barrier. Its callback could restart PHY and submit a new START, then the old availability-down handler erased the new PHY operation. Added a test-only case requiring BUSY inside that retirement callback and successful fresh START after unwind. Against the unchanged matching library it reproduces a failure at t=0.0006 in scenario23/raw=false:48 runs selected,46 successful,1 error,1 skipped; the reentrant admission is not BUSY. Records are `/tmp/ieee802154-b102-reentry-red.{log,out}` and retained `b102-reentry-red/` artifacts, including source/library identity. The earlier46-run PASS does not cover this repair, which remains pending.

Two further reviewed failures were reproduced separately against that same unchanged library, with only their new scenario selected. In scenario24/raw=false, a real peer's late acknowledged DATA causes an ACK across the next mandatory beacon boundary; the sender unexpectedly receives an ACK instead of NO_ACK at t=0.016303364619. In scenario25/raw=false, SET of128 payload bytes returns a result other than the required INVALID_PARAMETER at t=0.005. Each campaign selects2 runs and ends with1 error/1 skipped. Records are `/tmp/ieee802154-b102-{priority,payload}-red.{log,out}`, with exact fixtures/generated outputs in `b102-{priority,payload}-red/`. The required fixes are beacon priority over conflicting ACK work and side-effect-free resulting-beacon size validation before payload SET. Dynamic pending-address growth must remain bounded without losing queued transactions; the pinned2024 edition supplies no obsolete fixed52-byte payload cap. These fixes are not yet validated.

The checkpoint review ended with those three defects and a test-listener context correction: the new long/Boolean signal handlers invoking RESET need Enter_Method. Its canonical result is incomplete, not approval (0 PASS,0 N/A,3 FLAG,0 QUESTION); the flagged items are QR-DESIGN-MINIMAL, AR-ORG-CONTRACTS and AR-COM-NOTIFY. No exception-ledger entry applies. BO=SO's final PHY-turnaround RX preemption is permitted by mandatory beacon priority; a nominal CAP timer must not be described as uninterrupted physical reception. The same reviewer will check the stable repaired diff, including nondefault BLE/backoff settings, rollback/replacement continuations and the remaining checklist items.

The combined repair checkpoint builds debug and passes all4 selected units, including the new pending-transaction FIFO/byte-budget assertions. The module campaign passes4/5, failing Discovery at t=0.012 because its old control expected a200-byte payload SET to succeed and a later beacon to fail FRAME_TOO_LONG. That expectation conflicts with the corrected8.2.5.4 rejection at SET; it is being changed to require INVALID_PARAMETER and preservation of the previous valid payload. Records are `/tmp/ieee802154-b102-repaired-{build.log,unit.log,unit.out,module.log,module.out}` and retained `b102-discovery-expectation/`. The chained protocol command was NOT_RUN after that module failure. Source review and all three protocol repair confirmations remain open.

After that fixture correction, all5 modules pass and11/12 protocols pass (`b102-repaired2-*`). The new ACK-priority fixture then fails while inverse-converting a historical clock target; it also omitted module context before deleting delivered DATA. Retained setup evidence is `b102-priority-fixture-setup/`. Fixing only that fixture to save its future target conversion and enter its own context produces56/56 successful schedule runs (`/tmp/ieee802154-b102-repaired3-protocol.{log,out}`). Thus all three reproduced production repairs pass their actual-radio cases, along with the prior4 unit/5 module/11 protocol controls; retained records are `b102-repaired56-pass/`. This is not yet independent acceptance: review additionally found the BLE duration's hardcoded20-symbol period and an unguarded replacement-START rollback continuation after CAP-close notification. Those repairs remain open.

The additional failures were also reproduced separately: scenario28/raw=false observes the radio already off at t=0.004 after setting macUnitBackoffPeriod40; scenario29/raw=false observes the wrong SUCCESS/RESET/SUCCESS START terminal sequence after a real in-progress PPDU makes retune/restoration BUSY. Each selects2 runs, ending1 error/1 skipped. Exact records are `/tmp/ieee802154-b102-{ble,rollback}-red.{log,out}` and retained `b102-{ble,rollback}-red/`.

The repaired checkpoint consumes authoritative BLE periods in64-bit arithmetic and guards the original START identity across CAP-close callbacks in both rollback continuations. An additional max41/unit40 BO3/SO0 case distinguishes extended receive duty from unchanged CAP geometry/BI. A fresh matching debug build and the complete4 unit/5 module/12 protocol selection pass, including62/62 schedule runs and no stderr diagnostics (`/tmp/ieee802154-b102-repaired4-{build.log,unit.log,unit.out,module.log,module.out,protocol.log,protocol.out}`). The unit selector adds `PendingTransactions` to the previously recorded three; other selectors are unchanged. Source/library SHA-256 identity is `b102-repaired62-checkpoint.json`, and retained raw/generated records are `b102-repaired62-pass/`. Scoped architecture/naming/interfaces/source-seal/release-fragment checks pass with the same `repaired4` prefix. The independent static closure found no additional defect; final canonical acknowledgment follows this matching execution evidence.

The same independent reviewer confirms final B1-02 acceptance:23 PASS,11 N/A,0 FLAG,0 QUESTION. All five source findings and the test module-context correction are closed; no exception-ledger change applies. The restore-error confirmation branch is statically reviewed, while the runtime causal case exercises native BUSY rollback; no invented error provider is claimed. BO=SO preparation legitimately preempts the final192 physical microseconds of receive duty. Diagram rendering remains unverified where the renderer is unavailable. Later B1-03/B2/B3 behavior, final combined campaigns and release/feature isolation remain open.

## Forward B1-03 source contract

The independent primary-source audit resolved local SYNC installation versus ongoing tracking, public inactive-device BO configuration, FALSE's next-success termination, four failed searches, inclusive fourth-deadline completion, elapsed-time drift growth and PHY-only outage behavior. The resulting contract is in [beacon-profile.md](beacon-profile.md#acquisition-and-tracking-contract), with matching B1-03 plan/test assignments. It explicitly preserves the pinned edition's nonmatching-source/PAN notification wording while separating parent-anchor eligibility. The81ppm relative drift bound and numerical window/deadline convention are selected model policies. This is a source/design audit, not B1-03 code review or runtime evidence.

## Publication gate

Final debug and release compilation, every planned direct fixture, independent source review, architecture/naming/interface/source-seal/series checks, required trajectory controls, feature/clock isolation and release documentation remain open. No new baseline is approved or refreshed by this record. Final push is authorized after those gates complete with their actual outcomes recorded.
