# HE DL OFDMA credit-based round-robin scheduler implementation plan

Status: proposed.

This plan supersedes the earlier strict ns-3-fidelity direction. The goal is an
independently designed INET scheduler that uses the same broad scheduling
principles—service credits, round-robin service, and complete equal-sized RU
layouts—without reproducing ns-3 implementation details, state machines,
defaults, source structure, or exact decision traces.

## Goal and success criteria

Add a selectable `HeDlSchedulerCreditBasedRoundRobin` policy that:

- serves backlogged HE downlink recipients fairly using persistent integer
  service credits;
- assigns one equal-sized OFDMA RU to every selected recipient;
- uses a complete standard equal-RU layout, avoiding unused RUs of the chosen
  size;
- requires at least two selected recipients, leaving a one-recipient
  opportunity to the existing HE SU path;
- remains deterministic under candidate input reordering;
- uses INET's existing MCS selection, PHY calculations, immutable MU plan, and
  frame-exchange machinery; and
- is an original LGPL-licensed INET implementation with no copied ns-3 code,
  comments, fixtures, or derived source artifacts.

The scheduler is inspired by a general algorithmic family. It is not named or
documented as an ns-3-compatible scheduler, and matching ns-3 output is not an
acceptance criterion.

## Deliberate scope

### Included

- IEEE 802.11ax HE DL OFDMA.
- Unicast QoS data candidates already admitted by `HeHcf`.
- Independent credit state for each access category and recipient MAC address.
- Configurable maximum simultaneously scheduled stations.
- Complete equal-sized RU layouts over 20, 40, 80, and 160 MHz.
- Existing INET HE rate-control integration and scheduler telemetry.
- Deterministic unit, plan-validation, module, and legacy-regression coverage.

### Excluded

- One-recipient HE MU transmissions.
- DL MU-MIMO and mixed OFDMA/MU-MIMO allocation.
- EHT, MLO, and EMLSR behavior.
- UL OFDMA, BSRP, Basic Trigger, and DL/UL alternation.
- Central 26-tone RUs added to another equal-sized layout.
- Mixed-size RU allocation, backlog-proportional RU sizing, and HoL-delay
  optimization.
- Preamble-punctured equal-RU scheduling in the first implementation.
- Reproduction of ns-3 association-list order, TID search order, aggregation,
  protection, acknowledgment-manager, TXOP, or fallback details.
- Cross-simulator golden traces, bit-for-bit decisions, or matching throughput.

Unsupported punctured contexts return no MU allocation without changing
credits; `HeHcf` then follows its existing SU fallback. Puncturing can be added
later as a separately specified INET extension.

## Existing integration boundary

The implementation stays behind the existing
[`IIeee80211HeDlScheduler`](../src/inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeDlScheduler.h)
contract:

1. [`HeHcfDl.cc`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfDl.cc)
   collects associated, awake, Block-Ack-capable potential candidates for the
   winning access category. Because this collection may include a peer without
   a negotiated-capability snapshot, the scheduler applies the stricter
   compatibility gate defined below.
2. The new scheduler ranks those candidates, chooses a complete equal-RU
   layout, and returns one `RuAllocation` per selected recipient.
3. `HeDlMuPlan::create()` remains the authoritative immutable-plan validator.
4. `HeDlMuTxOpFs` remains responsible for packet selection, aggregation,
   sequence numbers, acknowledgment exchange, and retransmission state.
5. The HE PHY remains authoritative for TXVECTOR legality and duration.

No change is planned for `HeHcf`, `HeDlMuPlan`, the frame-sequence classes, the
PHY, association management, or queue ownership. In particular, the current
`scheduleContext.candidates.size() >= 2` guard is retained: the common MAC path
continues to reject one-recipient HE MU operation even though the standard
permits it.

## Required behavioral contract

### State and identity

Maintain a signed integer service credit for each
`(AccessCategory, MacAddress)` key.
MAC address is already present in `CandidateInfo`, is stable in the model, and
avoids extending the scheduler contract solely to obtain an AID.

- A newly observed key starts with zero credit.
- A candidate absent from an invocation neither gains nor loses credit; its
  stored credit is frozen until it becomes eligible again.
- `invalidatePeer(peer)` removes that peer's credits from every access category
  and then delegates to `HeDlSchedulerBase::invalidatePeer(peer)`.
- State for one access category never affects another.
- Credits are represented with signed 64-bit integers, not time or bytes.
- Credit addition and subtraction are checked against overflow; they must never
  silently wrap or fall back to floating point.

This defines fairness among currently eligible flows, rather than among every
associated station. It fits the information already owned by `HeHcf` and does
not add association callbacks to the scheduler contract.

### Candidate order

For every scheduling invocation:

1. Before any credit-state insertion or mutation, validate that recipients are
   unique and all candidates belong to one access category. An invalid context
   is recorded, returns an empty result, and leaves credit state unchanged.
2. Materialize zero-credit state for newly seen valid candidates.
3. Sort candidates by descending credit.
4. Break equal-credit ties by ascending MAC address.

The `anchor` flag, backlog size, HoL delay, source-queue order, pointer value,
and input-vector order do not override the credit order. They remain available
to the surrounding MAC and to MCS/duration estimation where applicable.

### Equal-RU selection

Let `L = eligibleCandidateCount` when `maxMuStations` is negative; otherwise
let `L = min(eligibleCandidateCount, maxMuStations)`. The scheduler then chooses
the largest supported complete count `K <= L` that satisfies the remaining
rules.

1. Enumerate the standard equal-RU counts provided by
   `getHeEqualRuCounts(channelBandwidth)`.
2. Consider counts from largest to smallest, restricted to `2 <= K <= L`.
3. For each `K`, obtain the canonical complete layout from
   `getHeEqualRuLayout(centerFrequency, bandwidth, K)`.
4. Traverse candidates in credit order and retain only those with a negotiated
   HE capability snapshot whose `localTxPeerRx` direction is valid and supports
   DL OFDMA, the current channel width, the layout's RU tone size, the selected
   coding, at least one receive spatial stream, and at least MCS 0. Match INET's
   existing HE legality rule by rejecting BCC when `ru.toneSize >= 484`; LDPC
   additionally requires mutually negotiated LDPC support.
5. Select the first `K` compatible candidates. If fewer than `K` are
   compatible, try the next smaller complete count.
6. If no count of at least two is feasible, return no allocation and leave all
   credits unchanged.
7. Pair selected recipients, in credit order, with RUs in the canonical layout
   order.

Every RU in the chosen complete layout is assigned. The scheduler does not
create a larger layout and leave same-sized RUs empty merely to serve a
nonstandard number of users. Capability or coding restrictions may therefore
cause SU fallback even when two candidates exist—for example, when the only
complete two-user layout requires an RU size unsupported by a recipient.

### PHY parameters and duration

For each selected recipient:

- estimate SNR through `HeDlSchedulerBase::estimateSnrDb()`;
- select MCS through the existing `IIeee80211HeRateControl` integration, using
  the base-class fallback only when no provider is configured;
- use one spatial stream and no DL MU-MIMO grouping;
- respect negotiated RU-size, channel-width, coding, MCS, and NSS limits; and
- estimate duration from the eligible backlog using the existing HE duration
  helper.

The duration estimate is used for the normal INET PHY/MAC plan, but it does not
weight fairness. This scheduler provides equal scheduling opportunity/resource
share, not airtime-weighted or throughput-weighted fairness.

### Credit update

Update credits only after a complete, valid allocation with `K >= 2` has been
constructed. Let `N` be the number of candidates in the compatible roster and
`K` the number selected:

```text
for every compatible-roster candidate i:
    credit[i] = credit[i] + K

for every selected candidate j:
    credit[j] = credit[j] - N
```

This update is exactly zero-sum over the instantaneous compatible roster:
`N * K - K * N = 0`. For example, with five candidates and four RUs, every
candidate gains four credits and each selected candidate pays five. Selected
candidates therefore move down by one while the omitted candidate moves up by
four. No division, tolerance comparison, PPDU-duration estimate, or PHY result
can perturb the ordering.

Accounting measures returned scheduling opportunities, not successful packet
delivery. PHY loss, missing acknowledgment, retransmission, and an unexpected
downstream plan rejection do not refund credit. This keeps link reliability
and transaction callbacks out of scheduler policy. Focused tests must make a
downstream rejection exceptional, but the accounting rule remains explicit if
one occurs.

### SU fallback and state preservation

The scheduler returns an empty allocation and does not mutate credits when:

- fewer than two eligible candidates are supplied;
- `maxMuStations < 2`;
- the context uses preamble puncturing;
- no complete capability-compatible equal-RU layout can serve at least two
  candidates; or
- allocation construction cannot produce legal MCS/duration values.

`HeHcf` retains responsibility for staging and transmitting an SU frame.

## Configuration surface

Add the simple module `HeDlSchedulerCreditBasedRoundRobin` with this
policy-specific parameter:

```ned
int maxMuStations = default(4);
```

Because `HeDlSchedulerBase` is a C++ base rather than a NED base, the module
must also declare every parameter that its initializer reads:

```ned
int smallBacklogThreshold @unit(B) = default(80B);
int mediumBacklogThreshold @unit(B) = default(500B);
int mtuBacklogThreshold @unit(B) = default(1500B);
int largeBacklogThreshold @unit(B) = default(6000B);
double lowDurationRatio = default(0.5);
double highDurationRatio = default(1.5);
int maxDurationAlignmentIterations = default(0);
double thermalNoisePsd = default(-174);
string heMcsSnrThresholds = default("4 7 10 13 16 19 21 24 27 30 33 36");
string heRateControlModule = default("");
```

These are existing INET rate-control, SNR, backlog-duration, and base-class
configuration mechanisms rather than credit-round-robin policy. Duration
alignment remains disabled because changing individual RU sizes would violate
the equal-sized-RU contract.

Do not add switches for one-user MU, central 26-tone RUs, MU-MIMO, ns-3
compatibility, association-order ties, or UL scheduling. Those options would
blur the deliberately narrow model.

Example selection remains declarative:

```ini
**.ap.wlan[*].mac.hcf.dlScheduler.typename = "HeDlSchedulerCreditBasedRoundRobin"
**.ap.wlan[*].mac.hcf.dlScheduler.maxMuStations = 4
```

## Change surface

The proposed production files are new and are currently outside every sealed
path. The only recursively sealed source subtree is
`src/inet/common/packet/`, which is not involved.

### New scheduler

- `src/inet/linklayer/ieee80211/mac/scheduler/HeDlSchedulerCreditBasedRoundRobin.h`
- `src/inet/linklayer/ieee80211/mac/scheduler/HeDlSchedulerCreditBasedRoundRobin.cc`
- `src/inet/linklayer/ieee80211/mac/scheduler/HeDlSchedulerCreditBasedRoundRobin.ned`

Implement the scheduler as a `HeDlSchedulerBase` subclass. Keep credit
selection, layout selection, allocation construction, accounting, and
invalidation as separately testable methods. Use an ordered container or an
explicit total sort; observable behavior must never depend on pointer/hash
iteration order.

Implement a small policy-local compatibility predicate from public
`CandidateInfo`, `ScheduleContext`, and canonical RU fields. Test its output
against `HeDlMuPlan::create()` for every supported and rejected capability
case. Do not modify `HeDlSchedulerBase` or refactor the existing fBW/fHoL
scheduler solely for this addition.

### Tests

- `tests/unit/Ieee80211HeDlCreditBasedRoundRobin_1.test`
- Extend `tests/unit/Ieee80211HeSchedulerValidation_1.test` only if a produced
  allocation exposes a validator gap.
- `tests/module/Ieee80211HeDlCreditBasedRoundRobin_1.test`

The unit test may define a test subclass exposing read-only credit state and a
test-only rate-control provider. Production code should expose normal WATCH
state and decision logging through existing scheduler conventions, but should
not add packet metadata or a test-only production API.

### Runnable example and documentation

- Add a `CreditBasedRoundRobin` configuration to
  `examples/ieee80211ax/dl_ofdma_sched/omnetpp.ini`.
- Update `examples/ieee80211ax/dl_ofdma_sched/walkthrough.md` with the policy
  contract, a short fairness experiment, and evidence generated by its scripts.
- Mention that the scheduler is an independent INET policy inspired by the
  general credit-based round-robin family; do not claim ns-3 equivalence.

The example update is made only after the focused tests pass so the walkthrough
describes observed behavior rather than intended behavior.

## Verification plan

### Decision and layout unit tests

Cover:

- zero and one candidate: empty result and no state change;
- two through ten candidates at 20 MHz;
- representative counts at 40, 80, and 160 MHz;
- `maxMuStations` below, equal to, and above a standard layout count;
- canonical RU indices, equal tone sizes, unique recipients, no overlaps, and
  complete-layout occupancy;
- BCC/LDPC and negotiated RU-size/MCS constraints, including a valid 40 MHz
  two-user 242-tone BCC layout, rejection of 484-tone-or-wider BCC layouts at
  80/160 MHz, and fallback to a narrower complete layout when enough compatible
  candidates exist;
- a missing, invalid, or OFDMA-disabled negotiated-capability snapshot;
- duplicate-recipient and mixed-access-category contexts, both rejected before
  credit insertion or mutation;
- no feasible complete layout;
- punctured context rejection; and
- every nonempty result accepted by `HeDlMuPlan::create()`.

### Credit and fairness unit tests

Assert exact pre/post signed integer credits for:

- equal initial credits and deterministic MAC tie-breaking;
- permuted candidate input producing the same result;
- unequal credits selecting the highest-credit prefix;
- five saturated candidates sharing a four-RU layout over repeated calls;
- candidate disappearance and re-entry with frozen credit;
- a newly appearing zero-credit candidate;
- independent AC_BE and AC_VO histories;
- `invalidatePeer()` across all access categories;
- checked large positive and negative credit arithmetic; and
- a failed/empty scheduling opportunity leaving state unchanged.

For identical continuously eligible candidates, the saturated test must prove:

- no starvation;
- selected-opportunity counts differ by at most one, excluding explicitly
  tested join/leave transients; and
- the sum of credits remains zero for a fixed roster initialized at zero.

### Module behavior test

Use one AP and five fixed HE stations, reliable links, one AC/TID, established
Block Ack agreements, fixed seed, and saturated equal traffic. Configure a
four-user maximum so the omitted recipient rotates.

Prove in one deterministic configuration/run:

- each MU opportunity contains at least two recipients;
- decoded PPDU recipients and RU indices match the independently specified
  sequence obtained from the fixture's fixed MAC addresses and integer-credit
  rule;
- all five recipients appear over a complete expected rotation;
- the MU-BAR/Block-Ack exchange completes; and
- reducing eligibility from two candidates to one takes the SU path. Exact
  no-mutation behavior remains a scheduler unit assertion because protected
  WATCH state is not a black-box module-test interface.

This is scheduler-to-MAC integration evidence, not a throughput comparison
with another simulator.

### Non-regression

Run unchanged coverage for:

- fBW/fHoL equal-RU scheduling in
  `tests/unit/HeDlScheduler_1.test`;
- queue-aware schedulers in
  `tests/unit/HeDlSchedulerQueueAware_1.test`;
- immutable plan validation in
  `tests/unit/Ieee80211HeSchedulerValidation_1.test`;
- DL MU exchange and acknowledgment behavior in
  `tests/module/Ieee80211HeDlMuExchange_1.test`;
- existing MU-MIMO fairness; and
- the `SuEdcaBaseline` configuration in the DL OFDMA example.

Fingerprints are supplemental change detectors, not correctness evidence. Do
not update any fingerprint CSV without separate explicit user approval.

## Implementation sequence and gates

### Phase 1 — Freeze the contract in tests

1. Add the scheduler-specific unit fixture and encode the selection, layout,
   credit, join/leave, invalidation, and `<2`-recipient behavior above.
2. Use deterministic MAC addresses, durations, capabilities, and candidate
   permutations.
3. Avoid any ns-3 fixture, generated output, source-derived constant table, or
   differential oracle.

Gate: the tests clearly distinguish the new policy from existing fBW/fHoL
behavior and contain no assumption of ns-3 identity.

### Phase 2 — Implement the policy

1. Add the NED module and C++ class.
2. Reuse INET RU catalogs and PHY/rate-control helpers.
3. Implement state cleanup through `invalidatePeer()`.
4. Record the last candidate order, selected allocations, scheduling reason,
   and exact credit transitions for deterministic inspection.

Gate: all scheduler unit and immutable-plan tests pass.

### Phase 3 — Integrate and demonstrate

1. Add the fixed module test and run one configuration/run/seed at a time.
2. Add the example configuration.
3. Generate the walkthrough's scalar/vector/PCAP evidence using the repository
   scripts and document the observed fairness rotation.

Gate: the module test proves the independently specified credit rotation at the
PPDU boundary and the SU/MU boundary; the example is reproducible.

### Phase 4 — Independent regression and architecture review

1. Run the focused unit and module suites plus the existing scheduler and SU
   baselines.
2. Run the architecture fitness function for the scheduler subtree.
3. Apply the complete general and IEEE 802.11 semantic review checklists.
4. Reconcile any existing allowlisted findings; do not expand an exception
   ledger or alter a seal without explicit approval.
5. Review the diff for copied wording, source structure, constants, comments,
   fixtures, or identifiers traceable to ns-3 implementation files.

Gate: tests pass, the architecture review has no new unresolved finding, and
the provenance review confirms an original INET implementation.

## Architecture requirement map

The design is governed primarily by:

- `R-SCOPE-WIRELESS`, `R-SCOPE-FIDELITY`, `R-COMPOSE-NOCODE`,
  `R-COMPOSE-DEFAULTS`, `R-RUN-REPRO`, `R-DOC-RUNNABLE`, and
  `R-DIST-LICENSE`;
- `AR-ORG-DOMAINS`, `AR-MOD-PLUGGABLE`, `AR-COM-DIRECT`,
  `AR-CFG-PARAMS`, `AR-EXT-NOCORE`, `AR-QUAL-TESTS`,
  `AR-QUAL-DETERMINISM`, `AR-QUAL-NAMING`, and
  `AR-QUAL-TRACEABILITY`; and
- `AR-WLAN-STD-TRACE`, `AR-WLAN-STD-GATING`,
  `AR-WLAN-ARCH-BOUNDARIES`, `AR-WLAN-ARCH-OWNERSHIP`,
  `AR-WLAN-ARCH-VARIANTS`, `AR-WLAN-PHY-AUTHORITY`,
  `AR-WLAN-PHY-TIMING`, `AR-WLAN-MAC-EXCHANGE`,
  `AR-WLAN-MAC-QOS`, `AR-WLAN-MAC-MULTIUSER`, and
  `AR-WLAN-QUAL-TESTS`.

For IEEE 802.11-2024 traceability:

- Clause 26.5.1.1 governs HE DL MU operation and common PPDU completion/padding
  behavior;
- Clause 26.15.3 governs the recipient capability constraints on bandwidth,
  MCS, and NSS;
- Clause 27.3.2.2 and its referenced RU allocation figures/tables govern legal
  HE RU geometry; and
- Clause 27.3.11.13 governs the common HE MU data-symbol duration.

Credit calculation, MAC-address tie-breaking, the choice to use only complete
equal-RU layouts, the user cap, and the two-recipient minimum are explicitly
INET scheduling choices, not IEEE requirements.

## Clean-room and licensing boundary

The implementation workflow must maintain these boundaries:

- Write code solely from this INET behavioral specification, IEEE requirements,
  and existing INET interfaces/helpers.
- Do not copy, translate, mechanically transform, or closely paraphrase ns-3
  scheduler source, comments, class layout, control flow, tests, or fixtures.
- Do not add ns-3 source or generated artifacts to the repository and do not
  link ns-3 into INET tests or builds.
- Do not use an ns-3 exact-output fixture as the implementation oracle.
- Use INET naming and file structure, including the existing
  `LGPL-3.0-or-later` source header.
- Preserve a short design note identifying the general inspiration and the
  deliberate differences listed in this plan.

These controls substantially reduce copyright and license-contamination risk,
but they are an engineering provenance process, not a legal opinion. Final
license clearance remains a project-maintainer or legal-review decision.

## Commands to run during implementation

From the repository root, preserving exact exit statuses and artifact paths:

```sh
export CCACHE_DISABLE=1
make MODE=release -j$(nproc)
inet_run_unit_tests -m release \
  -f '(Ieee80211HeDlCreditBasedRoundRobin|Ieee80211HeSchedulerValidation|HeDlScheduler|Ieee80211HeDlMuMimoFairness).*\.test'
bash .agents/skills/inet-architectural-requirements/references/enforcement/check-architecture.sh \
  src/inet/linklayer/ieee80211/mac/scheduler
```

Then run only the new fixed module configuration/run/seed before expanding to
any additional scenarios. Do not accept a changed fingerprint as a substitute
for the focused behavioral assertions.

## Definition of done

The work is complete when:

1. The new scheduler is selectable through the existing NED interface without
   modifying core MAC or PHY control flow.
2. It never returns a one-recipient MU allocation.
3. Every nonempty result contains a complete legal equal-sized RU layout with
   unique compatible recipients.
4. Exact integer credit transitions, deterministic ties, per-AC isolation, peer
   invalidation, and join/leave behavior pass focused unit tests.
5. Saturated eligible recipients satisfy the documented no-starvation,
   bounded-service-count, and zero-sum-credit invariants.
6. A fixed module run proves the independently specified credit rotation in
   decoded HE MU PPDUs, Block-Ack completion, and the one-candidate SU fallback.
7. Existing DL schedulers, MU exchange behavior, MU-MIMO coverage, and legacy
   SU operation pass unchanged.
8. Architecture and IEEE 802.11 review gates report no new unresolved finding.
9. The final diff contains no ns-3 code, copied text, fixtures, build
   dependency, or claim of behavioral equivalence.
