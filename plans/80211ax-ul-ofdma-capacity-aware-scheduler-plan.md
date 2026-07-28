# Capacity-aware HE UL OFDMA scheduler implementation plan

Status: implemented and executed on 2026-07-28; final verification evidence is
recorded below.

## Decisions

- Replace `HeUlSchedulerBacklogBased` in place. Existing NED typenames and
  example configurations keep working.
- Optimize estimated delivered service bytes subject to the existing
  least-recently-served policy.
- Permit every legal nonoverlapping HE RU layout, including mixed layouts such
  as `106+52+52`.
- Keep the configured Trigger duration as an input constraint. The optimizer
  does not increase duration merely to improve its score.
- Change `maxHeTbPpduDuration` from a 1 ms default to a configurable 2 ms
  default. The effective budget remains capped by the available TXOP and the
  5.484 ms HE PPDU limit.
- Preserve the existing UORA reservation as a hard constraint.
- Add `maxOptimizedStations = 8`: use the exact allocation-tree DP when the
  selected fairness prefix does not exceed this limit, and a deterministic
  capacity-aware fallback above it. `maxMuStations` remains the independent
  service limit and is not silently capped at eight.
- Use only protocol-visible BSR and link state. The AP must not inspect a
  station's live EDCA queue or know the exact PSDU that the station will build.
- Keep final STA-side frame selection and `HeUlMuPlan` validation authoritative.

## Pre-change problem statement

The replaced policy mapped reported backlog to an RU request using three literal
thresholds:

```text
> 12000 B -> 242 tones
>  6000 B -> 106 tones
>  2000 B ->  52 tones
otherwise -> 26 tones
```

It sizes each request independently, without first considering the number of
scheduled stations, MCS, common HE-TB duration, or the payload capacity of the
complete layout. If the requests do not fit, it greedily shrinks the largest
scheduled RU. Consequently three stations with small nonzero reports can
receive three 26-tone RUs even when three 52-tone RUs fit in a 20 MHz channel.

The pre-change 1 ms duration and default MCS 0 were separate inputs:

- `maxHeTbPpduDuration` currently defaults to 1 ms and is passed to the
  scheduler as `requestedDuration`.
- `defaultMcs` is 0 unless an `IIeee80211HeRateControl` provider selects another
  legal MCS.
- `wlan[*].bitrate` does not select the HE-TB MCS.

The replacement policy must combine these inputs when scoring a layout instead
of treating reported bytes as a direct RU-size lookup.

As part of this change, the default maximum HE-TB duration becomes 2 ms. This
is an INET policy default, not an IEEE requirement. Configurations may still
override it, and final timing remains subject to TXOP and PHY limits.

## Required behavioral contract

### Candidate eligibility and fairness

1. Admit only associated, UL-MU-enabled candidates with a fresh BSR-derived
   estimate. A known zero backlog is not a scheduled Basic-Trigger candidate;
   random access and BSRP remain the mechanisms for unreported new work.
2. Establish a total deterministic order:
   - anchor first;
   - ascending `lastService`;
   - descending conservative backlog estimate;
   - ascending AID;
   - ascending MAC address.
3. Consider only prefixes of that order, capped by `maxMuStations` and by the
   number of scheduled RUs that can coexist with the required RA-RUs.
4. Start with the largest prefix that can coexist with the required RA-RUs.
   If no legal layout can serve it, remove candidates only from the tail.
5. Within the selected prefix, optimize RU assignment. A later candidate may
   not displace an earlier candidate merely because it has a higher rate or
   larger backlog.

This retains the present least-recently-served rotation while removing the
current accidental dependence on input/container order.

### Backlog semantics

Carry BSR uncertainty to the scheduler instead of flattening every report to an
apparently exact byte count:

- Quantized BSR: retain lower and upper bounds and score the conservative lower
  bound.
- Overflow code 254: retain its strict lower-bound meaning.
- Unknown code 255: use a conservative estimate of zero known bytes, but retain
  explicit unknown state for deterministic low-priority probing rather than
  treating it as a multi-gigabyte backlog.
- Stale or missing report: exclude it from scheduled Basic-Trigger service and
  use the existing stale-report/BSRP path.

The optimizer computes a plan from this estimate. It must not query a station's
queue, and committing a plan must not be reported as actual delivery.

### Capacity and score

For a fixed common HE-TB boundary, calculate each candidate/RU edge as follows:

1. Select MCS through the existing HE rate-control contract.
2. Ask the PHY calculator for the maximum PSDU bytes that fit the exact
   `(RU, MCS, NSS, coding, GI, LTF, UL Length/common symbols, packet extension)`
   boundary.
3. Convert PSDU capacity to conservative MAC service-byte capacity using a
   shared estimate of the minimum HE-TB QoS/A-MPDU framing overhead. The
   estimate belongs to the MAC model and must be tested against the actual
   triggered-response builder; it must not be an unexplained scheduler literal.
4. Estimate delivered service:

```text
estimatedDelivered =
    min(conservativeReportedBacklog, estimatedServiceCapacity)
```

Compare complete layouts lexicographically:

1. maximum total estimated delivered bytes;
2. maximum number of users in the selected fairness prefix;
3. maximum per-user delivered-byte vector in fairness order;
4. minimum unused service capacity;
5. canonical ascending `(toneOffset, toneSize, AID)` signature.

Store the estimated delivered value in each scheduled allocation as
`plannedBytes`. `plannedBytes` is bounded by the conservative backlog estimate
and is not the actual PSDU or a promise of delivery.

## Search algorithm

Use dynamic programming over the standard RU allocation tree rather than
enumerating arbitrary catalog subsets.

For each tree node, the choices are:

- leave the node unused;
- assign that RU to one scheduled candidate;
- at a 26-tone leaf, reserve it for random access;
- split it into its standard child RUs and combine child solutions.

For a selected prefix no larger than `maxOptimizedStations`, the exact DP key
is:

```text
(tree node, scheduled-candidate mask, random-access RU count)
```

The mask has `2^N` values for `N` optimized candidates. With the proposed
default `maxOptimizedStations = 8`, the exact path has at most 256 candidate
masks per tree node. `maxMuStations` currently defaults to eight but is not
hard-capped; it may be configured above the exact-search limit.

When the selected prefix exceeds `maxOptimizedStations`, use this deterministic
capacity-aware fallback for the entire prefix:

1. Require the mandatory HE non-AP 26/52/106/242-tone baseline during typed
   candidate eligibility (IEEE 802.11-2024 Clauses 27.1.1 and 27.3.2.7).
2. Construct the canonical 26-tone layout that serves every conforming prefix
   member and preserves all required 26-tone RA leaves. If the full prefix is
   genuinely layout-infeasible, remove only its tail and retry.
3. Repeatedly consider allocation-tree promotions in which one assigned RU and
   only unassigned sibling nodes are replaced by their parent RU.
4. Apply the promotion with the greatest positive marginal increase in the
   documented delivered-byte score.
5. Break equal gains by fairness order and then canonical parent
   `(toneOffset, toneSize)`.
6. Stop when no legal positive-gain promotion remains.

The fallback never drops or bypasses a member of the chosen fairness prefix,
never consumes an RA leaf, and is deterministic, but it is not claimed to find
the global optimum. Record whether the exact or fallback path produced each
schedule.

Retain only the best deterministic exact-DP score for each key. Both paths find
or construct mixed layouts without reconstructing tone gaps in MAC policy
code, and both remain bounded across 20/40/80/160 MHz.

The PHY RU utility must expose typed parent/child relationships from its
existing canonical allocation tree. The scheduler may consume those
relationships but must not duplicate the IEEE RU split table.

## Common timing preparation

Exact capacity scoring requires the common PHY choices before scheduling.
Refactor preparation in this order:

1. In `HeHcf`, resolve the requested duration/TXOP cap, GI/LTF pair, packet
   extension, puncturing state, and the coding constraints relevant to the
   proposed OFDMA exchange.
2. Canonicalize the duration through the existing Trigger UL Length calculator.
3. Put the immutable finalized timing boundary in `ScheduleContext`.
4. Run the scheduler against that boundary.
5. Run the existing finalization and `HeUlMuPlan` validation again before
   transmitting. Reject the Trigger if the repeated result differs.

Do not use `estimateHeMuUserDuration()` as an inverse capacity oracle: that
helper constructs a simplified single-user estimate with defaults and does not
represent the complete finalized HE-TB boundary.

Post-scheduler transformations must not silently invalidate the optimum:

- apply puncturing and BCC/LDPC restrictions before scoring;
- do not convert the result to UL MU-MIMO after OFDMA scoring;
- when UL MU-MIMO is selected by policy, bypass this OFDMA optimizer and use
  the existing MU-MIMO path.

## Change surface

All listed production paths are currently unsealed. The recursively sealed
`src/inet/common/packet/` subtree is outside the change.

### PHY geometry

`src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h`

`src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.cc`

- Add a read-only allocation-tree API returning canonical root/child RUs.
- Reuse the existing catalog, `validateHeRuLayout()`, and tone tables.
- Add unit coverage for every bandwidth and central 26-tone branches.

### PHY capacity

`src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h`

`src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.cc`

- Add an inverse capacity helper for a finalized HE-TB timing boundary.
- Implement it using the existing symbol, coding, padding, and duration
  calculation path; do not copy PHY formulas into the scheduler.
- Return a typed validation result, not a sentinel byte count.
- Cover BCC/LDPC, MCS, RU size, exact-fit, one-byte-over, and duration-limit
  boundaries.

### Scheduler contract and timing handoff

`src/inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeUlScheduler.h`

`src/inet/linklayer/ieee80211/mac/coordinationfunction/HeUlCoordinator.h`

`src/inet/linklayer/ieee80211/mac/coordinationfunction/HeUlCoordinator.cc`

`src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.ned`

`src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfUl.cc`

- Add typed BSR estimate semantics to `CandidateInfo`.
- Add finalized common PHY/timing inputs to `ScheduleContext`.
- Add `plannedBytes` to `RuAllocation`.
- Change the NED default of `maxHeTbPpduDuration` to 2 ms while retaining the
  parameter as the configurable maximum.
- Resolve common timing/coding before scheduling and preserve the existing
  independent final validation.
- On commit, emit `plannedBytes` as AP-planned service. Do not set scheduled
  bytes equal to the whole reported backlog.
- Keep station response telemetry as the source of actual selected PSDU bytes.

### Policy

`src/inet/linklayer/ieee80211/mac/scheduler/HeUlSchedulerBacklogBased.h`

`src/inet/linklayer/ieee80211/mac/scheduler/HeUlSchedulerBacklogBased.cc`

`src/inet/linklayer/ieee80211/mac/scheduler/HeUlSchedulerBacklogBased.ned`

- Remove the 12,000/6,000/2,000-byte mapping and greedy shrink loop.
- Implement the deterministic fairness-prefix/allocation-tree DP.
- Add `maxOptimizedStations = default(8)` with validation that it is positive.
- Preserve `selectMcs()`, target RSSI selection, `maxMuStations`, and RA policy;
  do not reinterpret `maxMuStations` as an eight-station hard cap.
- Implement and label the deterministic promotion fallback for prefixes larger
  than `maxOptimizedStations`.
- Correct the NED/class documentation: the policy is capacity-aware and may
  produce mixed RUs; it is not an equal-RU or descending-backlog scheduler.
- Record a concise decision reason and final score for inspection.

No new scheduler typename or compatibility alias is needed.

### BSR handoff

`src/inet/linklayer/ieee80211/mac/Ieee80211HeBsr.h`

`src/inet/linklayer/ieee80211/mac/Ieee80211MacHeaderSerializer.cc`

- Preserve queue-size kind and bounds through deserialization instead of
  exposing only the legacy flattened byte value.
- Keep wire encoding unchanged.
- Thread the typed estimate through the coordinator to `CandidateInfo`.

### Example and documentation

`examples/ieee80211ax/ul_ofdma/omnetpp.ini`

`examples/ieee80211ax/ul_ofdma/walkthrough.md`

- Keep `CapacityFit` as the proved positive small-payload boundary.
- Use `AsymmetricBacklog` as the mixed-layout policy discriminator.
- Add the selected RU layout, MCS, common duration, planned bytes, and actual
  station-selected bytes to the evidence table.
- State that the scheduler is INET policy; the standard defines legal fields
  and behavior, not this optimization objective.
- Reconcile the effective 1 s configuration and retained repaired evidence in
  the walkthrough.

The existing user modification to `omnetpp.ini` must be preserved and reviewed
as input; implementation must not overwrite it.

## Implementation sequence

1. **Freeze policy tests and terminology**
   - Add score, fairness, and deterministic tie-break helpers to the unit-test
     oracle first.
   - Rename telemetry semantics in assertions from “selected” to “planned”
     where the value is AP-side.

2. **Preserve BSR uncertainty**
   - Carry quantized/overflow/unknown status and bounds into the coordinator.
   - Add serialization/integration tests before changing scheduling behavior.

3. **Expose PHY-owned allocation-tree and capacity APIs**
   - Add geometry traversal and inverse HE-TB capacity helpers.
   - Verify exact-fit/one-byte-over results independently of the scheduler.

4. **Move common PHY resolution before scheduling**
   - Extend `ScheduleContext`.
   - Change the configured default maximum HE-TB duration to 2 ms.
   - Ensure scheduling and finalization consume the same canonical timing
     boundary.
   - Remove or convert any post-scheduler filter that can change the scored
     layout.

5. **Replace the backlog threshold policy**
   - Implement eligibility, total order, prefix selection, DP, and
     lexicographic score.
   - Add the bounded exact-search parameter and the deterministic promotion
     fallback without reducing the configured `maxMuStations`.
   - Return canonical allocations with MCS and `plannedBytes`.
   - Assert `validateHeRuLayout()` on the selected physical RUs.

6. **Correct commit and observability semantics**
   - Emit AP-planned bytes from `allocation.plannedBytes`.
   - Keep actual station-selected bytes in `HeTbResponseEvent`.
   - Never debit or claim the full BSR merely because a Trigger was sent.

7. **Run focused regression gates**
   - Unit tests, deterministic module exchange, then one configuration/run/seed
     at a time.
   - Inspect fingerprints but do not update their CSV without explicit
     approval.

8. **Refresh the walkthrough only from retained evidence**
   - Update claims, commands, configuration duration, result paths, and
     limitations together.

## Verification matrix

### Unit tests

Extend `tests/unit/HeUlScheduler_1.test` with an independent small 20 MHz
brute-force oracle. It may use public RU/capacity helpers, but it must not reuse
the optimizer's DP or comparison implementation.

Required cases:

- three symmetric positive reports select at least three 52-tone RUs when
  26-tone capacity cannot serve the reported payload estimate;
- asymmetric demand selects a legal mixed layout and matches the oracle score;
- a `106+52+52` optimum is accepted and survives final validation;
- the anchor and oldest users cannot be skipped for a newer larger backlog;
- input permutation produces the same AID-to-RU result;
- `maxMuStations <= maxOptimizedStations` uses the exact path and matches the
  independent oracle;
- a prefix larger than `maxOptimizedStations` uses the fallback, retains the
  complete feasible fairness prefix, preserves RA-RUs, and returns the same
  layout under input permutation;
- every fallback promotion has positive marginal score and the final layout
  has no remaining legal positive-gain promotion;
- exact-fit and one-byte-over capacity boundaries differ;
- the default configuration supplies a 2 ms maximum duration, while an
  explicit 1 ms override remains valid and produces the expected lower
  capacity;
- TXOP values below 2 ms reduce the effective duration, and the PHY ceiling
  prevents values above 5.484 ms from producing an overlong HE PPDU;
- `plannedBytes` never exceeds the conservative backlog estimate or PHY service
  capacity;
- required RA-RUs survive scheduled-user pressure;
- RA-only, no-eligible-user, `ulMuDisabled`, and invalid-capability paths are
  deterministic;
- existing 20/40/80/160 MHz validity coverage remains.

Extend:

- `tests/unit/Ieee80211HeBsrBsrpIntegration_1.test` for quantized, overflow,
  unknown, stale, and reused-AID behavior plus planned-byte telemetry;
- the HE RU unit for typed allocation-tree traversal;
- the HE PHY calculator unit for inverse capacity boundaries;
- `tests/unit/Ieee80211HeSchedulerValidation_1.test` for optimizer output
  capability/timing rejection.

Keep the existing UL control-frame and UL transaction tests as the wire and
common-timing regression gates.

### Focused commands

From the repository root:

```sh
CCACHE_DISABLE=1 inet_run_unit_tests -m release --no-concurrent \
  -f '(HeUlScheduler|Ieee80211HeBsrBsrpIntegration|Ieee80211HeSchedulerValidation|Ieee80211HeUlMuTransaction|Ieee80211HeUlControlFrames).*\.test'

CCACHE_DISABLE=1 inet_run_module_tests -m release --no-build --no-concurrent \
  -f 'Ieee80211HeUlTriggerExchange_1.*'
```

Run the focused simulation first:

```sh
bin/inet -u Cmdenv \
  -f examples/ieee80211ax/ul_ofdma/omnetpp.ini \
  -c AsymmetricBacklog -r 0 --seed-set=1 \
  --result-dir=/tmp/ul-capacity-repaired-asymmetric-r0-s1 \
  --cmdenv-express-mode=true \
  '--*.ap.wlan[*].recordPcap=true' \
  '--*.ap.wlan[*].pcapRecorder[*].timePrecision=9' \
  '--*.ap.wlan[*].pcapRecorder[*].alwaysFlush=true' \
  '--*.ap.wlan[*].pcapRecorder[*].verbose=false' \
  '--**.checksumMode="computed"' \
  '--**.fcsMode="computed"'
```

Correlate:

- decoded Trigger AID/RU/MCS/common-duration fields;
- AP reported and planned-byte vectors;
- station `HeTbResponseEvent` selected bytes and response reason;
- HE-TB QoS Data versus QoS Null;
- server received bytes and the terminal Multi-STA Block Ack.

Then run, individually:

- `CapacityFit`, run 0, seed set 1: positive data-fitting boundary;
- `EdcaBaseline`, run 0, seed set 1: no-Trigger feature-gate control;
- `examples/ieee80211ax/ul_uora`, `MixedUora`, run 0, seed set 1:
  scheduled/RA coexistence.

The retained repaired regression evidence uses run 0 / seed set 1 for each
focused configuration. Broader seed campaigns remain optional statistical
follow-up and are not claimed by this implementation record.

Inspect the existing mixed-UORA fingerprint:

```sh
cd tests/fingerprint
./fingerprinttest -s -t 1 -m 'ul_uora/.*MixedUora' ieee80211-he.csv
```

A changed fingerprint is diagnostic evidence, not permission to update
`tests/fingerprint/ieee80211-he.csv`.

## Execution record

- Release build completed successfully.
- Nine focused unit tests and the deterministic
  `Ieee80211HeUlTriggerExchange_1` module test passed.
- `AsymmetricBacklog` run 0 / seed 1 under
  `/tmp/ul-capacity-repaired-asymmetric-r0-s1/` recorded 282 Basic Triggers,
  511 nonzero planned-user events totaling 292,475 B, and a representative
  `106+26+106` three-user Trigger at frame 300.
- `CapacityFit` under `/tmp/ul-capacity-repaired-capacityfit-r0-s1/` recorded
  129 Basic Triggers and 2,074 length-10 HE-TB QoS Data MPDUs.
- `EdcaBaseline` under `/tmp/ul-capacity-repaired-edca-r0-s1/` recorded no HE
  UL Trigger, scheduled-user, or RA event.
- `MixedUora` under `/tmp/ul-capacity-repaired-mixeduora-r0-s1/` recorded 765
  Basic Triggers, 1,468 scheduled-user assignments, and 765 RA assignments.
- The narrow fingerprint diagnostic intentionally remains a mismatch:
  expected `8705-fe54/tplx`, calculated `3953-411d/tplx`. The changed
  `MixedUora` workload and legacy-EDCA suppression alter the trajectory; its
  first Basic Trigger is now at 0.203988 s where the prior run had none. No
  fingerprint CSV or generated baseline artifact is retained.

## Acceptance criteria

- No 12,000/6,000/2,000-byte RU thresholds remain.
- The selected scheduled users are a prefix of the deterministic
  least-recently-served order.
- For the bounded modeled search space, the chosen layout equals the
  independent oracle's maximum score when the selected prefix is no larger
  than `maxOptimizedStations`.
- Above `maxOptimizedStations`, the documented fallback serves the complete
  feasible fairness prefix, is deterministic, preserves RA-RUs, and makes no
  claim of global optimality.
- Configuring `maxMuStations` above eight does not overflow a fixed-width mask,
  trigger an unbounded exact search, or silently cap service at eight users.
- Three 20 MHz stations receive at least `3x52` when that layout carries more
  estimated service than `3x26`.
- Mixed layouts are canonical, nonoverlapping, and accepted by
  `HeUlMuPlan`.
- Required RA-RUs are never consumed by scheduled service.
- Identical modeled state produces byte-identical schedules.
- The scheduler uses BSR-derived estimates only; no STA queue pointer or exact
  pending PSDU enters its contract.
- The default maximum HE-TB duration is 2 ms, remains configurable, and is
  bounded by TXOP and the 5.484 ms HE PPDU limit.
- AP-planned bytes and actual STA-selected bytes remain distinct observables.
- `CapacityFit` carries HE-TB QoS Data; the insufficient-capacity control may
  still produce QoS Null for the documented reason.
- No fingerprint expectation is changed without explicit approval.

## Standards and architecture traceability

Normative IEEE 802.11-2024 anchors:

- Clause 4.3.16: Trigger-based UL MU operation.
- Tables 9-52 and 9-53: Trigger User Info RU allocation encoding.
- Clause 26.5.2.2.4: Trigger fields and valid HE-TB response requirements.
- Clause 26.5.5 and Clause 9.2.4.7.4/Table 9-33: BSR purpose and queue-size
  representation.
- Table 27-7: legal RU counts and mixed-RU operation.
- Table 27-8: canonical 20 MHz RU positions.
- Clause 27.3.2.6: HE-TB duration, RU, target RSSI, and MCS inputs.
- Clause 27.3.12.5.5: LDPC extra-symbol and pre-FEC padding behavior.
- Clause 27.3.13 and Clause 27.4.3: common HE-TB duration and padding.
- Table 27-61: HE-TB pre-FEC padding-factor interpretation.

The standard does not define the backlog thresholds, least-recently-served
order, RA reservation heuristic, optimization score, or tie-breaks. Those are
explicit INET policy.

Applicable repository requirements:

- `R-COMPOSE-DEFAULTS`, `R-COMPOSE-NOCODE`, `R-RUN-REPRO`,
  `R-RESULT-BUILTIN`, and `R-DOC-RUNNABLE`;
- `AR-MOD-PLUGGABLE`, `AR-CFG-INFER`, `AR-CFG-PARAMS`,
  `AR-OBS-SIGNALS`, `AR-OBS-NED-TRUTH`, `AR-QUAL-TESTS`,
  `AR-QUAL-DETERMINISM`, and `AR-QUAL-FINGERPRINT`;
- `AR-WLAN-STD-TRACE`, `AR-WLAN-STD-GATING`,
  `AR-WLAN-ARCH-BOUNDARIES`, `AR-WLAN-ARCH-OWNERSHIP`,
  `AR-WLAN-PHY-AUTHORITY`, `AR-WLAN-PHY-TIMING`,
  `AR-WLAN-MAC-MULTIUSER`, `AR-WLAN-OBS-EVENTS`, and
  `AR-WLAN-QUAL-TESTS`.

Before implementation, rerun the sealing check because sealing status can
change. After implementation, run the architecture checker for both modified
IEEE 802.11 MAC and PHY subtrees and obtain an independent review of the final
patch and regression evidence.
