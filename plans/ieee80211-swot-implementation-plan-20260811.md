# IEEE 802.11 SWOT implementation plan

This plan turns the useful findings in
[`ieee80211_swot_report-opus4_6_20260811.md`](../reports/ieee80211_swot_report-opus4_6_20260811.md)
into small, reviewable changes. It is based on the current source tree, which is
already ahead of parts of the report.

The goal is to improve correctness and maintainability without redesigning the
whole 802.11 stack.

## What the report gets right, and what has changed

- `Hcf` and `HeHcf` are still too large and own too many decisions. However,
  extraction has already started: `HcfExchangeCoordinator`,
  `HcfResponseService`, `HcfRetryService`, `HcfFramePreparation`, and
  `HcfAggregationService` already exist. Extend these boundaries; do not create
  a second set of competing helpers.
- Non-QoS data and some control-frame paths are genuinely incomplete. These are
  correctness gaps and should be addressed before broad cleanup.
- PCF, MCF, and HCCA are still stubs, but the normal MAC composition already
  documents PCF and HCCA as omissions and does not select PCF or MCF. HCCA is
  always present as an inert placeholder. Implementing any of them properly is
  a large standards project; the practical work is to clean up this placeholder
  API without creating a late runtime failure.
- The proposed `getType()` plus unchecked `static_cast` dispatch is unsafe.
  The frame-control type and the concrete chunk class can disagree in malformed
  input. Centralized dispatch is useful, but concrete casts must remain checked.
- PHY mode indexes, timing profiles, and RU catalogs already exist. Do not add
  JSON or CSV loading. If tables need cleanup, keep them as typed C++ data owned
  by the PHY.
- HE tests are already substantial. Add a few missing end-to-end exchanges and
  failure cases instead of building a new parallel test framework.
- Many statistics listed in `src/inet/linklayer/ieee80211/__TODO` already exist.
  Audit the gaps before adding signals.
- The report's performance claims are static guesses. Optimize only after a
  repeatable benchmark shows that a path matters.

## Rules for every patch

- Preserve existing NED types, parameters, defaults, signals, frame bytes,
  timing, event order, and random-number use unless the patch explicitly fixes a
  documented behavior bug.
- Keep `Hcf` as the NED-facing module and callback bridge. Helpers use direct C++
  calls; do not add zero-time messages.
- Keep one owner for exchange state, timers, retry state, Block Ack state, queue
  state, and PHY legality decisions.
- Keep scheduling in the MAC and final PPDU legality, rate, and duration in the
  PHY.
- A normative behavior change needs an IEEE revision and clause in the code or
  focused test.
- Recheck sealing before each source change. The currently listed sealed packet
  subtree is not part of this plan.
- Add the focused test before or with the behavior change. Fingerprints are a
  regression gate, not proof that the new behavior is correct.
- Do not update fingerprint CSV files without separate approval and an
  explanation of the first changed event.

The main architecture constraints are `AR-MOD-COMPOSITION`,
`AR-ORG-CONTRACTS`, `AR-QUAL-TESTS`, `AR-QUAL-LOGGING`,
`AR-WLAN-ARCH-BOUNDARIES`, `AR-WLAN-ARCH-OWNERSHIP`,
`AR-WLAN-FRAME-REPRESENTATION`, `AR-WLAN-MAC-EXCHANGE`,
`AR-WLAN-MAC-QOS`, `AR-WLAN-MAC-MULTIUSER`,
`AR-WLAN-PHY-AUTHORITY`, `AR-WLAN-OBS-EVENTS`, and
`AR-WLAN-QUAL-TESTS`.

## Phase 1: lock down current behavior

### 1.1 Add focused dispatch and failure tests

Cover these cases without changing production behavior first:

- QoS data, non-QoS data, management, and known control frames.
- Late ACK, CTS, BAR, the supported Block Ack variants, and an unknown control
  frame.
- A mismatch between `Ieee80211FrameType` and the concrete header chunk.
- Headerless NDP and A-MPDU indications.
- Normal ACK, no-ACK, Block Ack, timeout, retry, and retry-limit completion.

Prefer extending current HCF and frame-sequence tests. Add a new test file only
when no current test has the right fixture.

Exit condition: every branch that will change in Phases 2 and 3 has a focused
test that records its current result.

### 1.2 Record a small performance baseline

Use the existing HE planning speed scenario and one dense EDCA scenario. Measure:

- time spent in buffer-status calculation;
- mode and RU lookup counts;
- frame classification and tag lookup counts;
- total CPU time and peak memory.

This is diagnostic work only. Do not add timing assertions to correctness tests.

Exit condition: the team knows whether any proposed optimization is large enough
to matter.

## Phase 2: fix unsupported and fragile behavior

Keep each item below as a separate patch because each has different compatibility
and fingerprint risk.

### 2.1 Resolve the PCF, MCF, and HCCA placeholder API

- Confirm and test that the supported `Ieee80211Mac` composition cannot select
  PCF or MCF and that the always-present HCCA placeholder never owns the
  channel.
- Keep the existing omission clear in NED documentation.
- For the standalone PCF and MCF module types, make a compatibility decision:
  either remove them in a dedicated breaking-change patch, or retain them as
  explicitly unsupported types that fail immediately during initialization.
- For HCCA, either keep a small inert placeholder or make the HCF submodule
  optional. Do not reject its mere presence, because current HCF instances
  contain it by default.
- Remove deep `"Hcca is unimplemented"` branches only after tests prove the
  unsupported path cannot be entered.
- Do not attempt a partial PCF, MCF, or HCCA implementation in this program.

Exit condition: the public model surface states what is unsupported and no
supported configuration reaches a late `"Unimplemented"` error.

### 2.2 Define error handling by situation

Use three simple rules:

- Invalid configuration: fail during initialization with `cRuntimeError`.
- Broken internal invariant or impossible exchange state: keep `ASSERT`,
  `check_and_cast`, or `cRuntimeError`.
- Malformed, late, foreign, or unsupported received frame: handle according to
  the protocol path, normally by dropping or ignoring it with the existing drop
  signal and a useful log message.

Replace empty `else ;` branches only when the expected action is known. Do not
turn all exceptions into warnings or all ignored frames into exceptions.

Exit condition: the tested HCF receive and recovery paths follow these rules and
no valid frame reaches an `"Unknown frame"` error accidentally.

### 2.3 Complete non-QoS data handling

- Trace non-QoS data through classification, queue selection, sequence-number
  assignment, ACK/retry handling, and delivery.
- Use the existing management/non-QoS recovery procedure rather than adding a
  new retry owner.
- Keep QoS mapping centralized in EDCA; do not infer an access category later
  from unrelated packet fields.
- Verify unicast, multicast, and broadcast behavior separately.

Exit condition: supported non-QoS data either completes normally or fails at a
documented capability boundary, never at a generic cast fallback.

## Phase 3: finish the HCF split

The target is clearer ownership, not an arbitrary line-count goal.

### 3.1 Make the existing exchange coordinator authoritative

- Route all exchange start, transmit, expected-response, timeout, recovery,
  completion, and abort transitions through `HcfExchangeCoordinator`.
- Keep frame-sequence timers and completion state in one place.
- Remove duplicate booleans or derived state from `Hcf` only after every reader
  uses the coordinator.
- Verify that no callback can complete or abort an exchange twice.

### 3.2 Finish the existing service boundaries one at a time

Use the helpers already present:

1. Move remaining response classification and timeout plumbing into
   `HcfResponseService` while leaving exchange state in the coordinator.
2. Move pure frame construction and mode-independent preparation into
   `HcfFramePreparation`.
3. Move remaining aggregate construction and constituent bookkeeping into
   `HcfAggregationService`.
4. Move only stateless retry calculations into `HcfRetryService`; the existing
   procedure and exchange owners keep mutable retry state.
5. Keep Block Ack agreement lifecycle in the existing originator and recipient
   handlers. HCF should call their contracts, not duplicate their windows.

Each move should be behavior-preserving and have its own unit test or existing
test extension.

### 3.3 Centralize safe frame classification

- Add one small classification helper for broad frame categories and supported
  subtypes.
- A `switch` on `getType()` may choose a route, but use `dynamicPtrCast`,
  `check_and_cast`, or an equivalent checked conversion before accessing a
  concrete header.
- Keep malformed-type detection at the boundary.
- Keep Block Ack variant decoding explicit because the variants have different
  concrete fields.
- Do not add a generic visitor framework unless at least three independent
  consumers need the same operation.

Exit condition: HCF's top-level paths read as dispatch plus delegation, while
the existing exchange, ACK, retry, and capability behavior is unchanged.

### 3.4 Reduce `Hcf.h` coupling

Do this after the ownership moves settle:

- Forward-declare pointer-only types.
- Move their concrete includes to the relevant `.cc` files.
- Keep full definitions for value members such as the existing coordinator and
  services.
- Pass the full build and record a before/after dependency or incremental-build
  measurement. Do not claim a compile-time improvement without measuring it.

Exit condition: `Hcf.h` exposes only the types required by its declaration and
does not depend on avoidable concrete implementations.

## Phase 4: strengthen HE/EHT integration coverage

Extend current unit tests with a small number of deterministic module tests:

1. UL: Trigger to HE TB response to Multi-STA Block Ack, including one timeout
   and retry case.
2. DL: multi-user data to sequential Block Ack or MU-BAR response, including a
   partial-response case.
3. Capability gating: reject an illegal RU, bandwidth, GI/LTF, MCS/NSS, or peer
   capability combination without mutating queues, sequence numbers, retry
   counters, or Block Ack state.
4. Compatibility: run one legacy/HT case and one VHT case beside each HE change.
5. EHT: add one smoke exchange that proves the EHT path still uses the common
   exchange and PHY legality boundaries.

Use Cmdenv. For the two end-to-end exchanges, keep a short PCAP or result-based
assertion that checks the actual frame order and key fields; do not rely only on
the final fingerprint.

Exit condition: the main DL and UL exchanges and their failure paths are covered
at both focused and module level.

## Phase 5: fill observability gaps

### 5.1 Audit before adding signals

Compare the current NED signals and statistics with the `__TODO` list. Mark each
item as present, missing, or no longer useful. Do not implement the list blindly.

### 5.2 Add only the high-value per-AC gaps

Prioritize:

- transmission attempts, retries, and retry-limit drops;
- internal collisions;
- TXOP duration and frame count;
- aggregation count and MPDU count;
- packet drops by reason.

Emit each semantic event once from its owner. Prefer recording at the existing
per-AC EDCAF/module path over adding an access-category field everywhere. Use NED
filters to derive statistics where the current signal already carries enough
information.

Keep log cleanup local to code touched by these patches. Follow the existing log
levels: `EV_INFO` for externally meaningful actions, `EV_DETAIL` for internal
decisions, `EV_DEBUG`/`EV_TRACE` for diagnosis, and `EV_WARN` for recoverable
unexpected input. Structured event IDs are unnecessary unless a real analysis
tool requires them.

Exit condition: the selected metrics can be queried per AC without reading
private state, and enabling recording does not change simulation behavior.

## Phase 6: simplify PHY tables without external data loading

This is lower priority than the MAC correctness work.

- Keep mode, timing, and RU data in the PHY subtree.
- Preserve the existing mode indexes, timing profiles, RU keys, and RU catalogs.
- Replace difficult macros with typed `constexpr` rows only where this makes a
  touched table easier to review.
- Split family-specific table construction into focused C++ files if that
  reduces `Ieee80211ModeSet.cc`; keep the public mode-set API unchanged.
- Validate every table row at construction and fail on duplicates or illegal
  combinations.
- Prove identical mode membership, rates, durations, and serialized PHY headers
  before removing old definitions.

Do not load CSV or JSON at runtime. It adds packaging, validation, and
reproducibility problems without solving a current user need.

Exit condition: table definitions are easier to inspect, with byte-, rate-, and
duration-equivalent behavior.

## Phase 7: optimize only confirmed hotspots

Authorize each optimization separately using the Phase 1 measurements.

- If buffer-status calculation is significant, put any cached byte total in the
  queue/state owner and test enqueue, dequeue, fragmentation, aggregation,
  retry, drop, and rollback. Preserve MAC-SAP service-unit deduplication.
- If repeated tag lookup is significant, build a small local receive context at
  the boundary; do not copy mutable protocol state into it.
- If checked casts are significant, optimize only the proven hot dispatch path
  while retaining validation at the packet boundary.
- Reuse the existing mode and RU indexes instead of adding more caches.

Accept an optimization only when it improves an end-to-end scenario beyond
measurement noise and leaves frame bytes, decisions, events, results, and
fingerprints unchanged.

## Work deliberately left separate

- A complete PCF, MCF, or HCCA implementation needs its own standards-backed
  project and use cases.
- Repairing the layered bit-level PHY is a separate fidelity project. First add
  one failing reproduction and define which bit/symbol-domain scenarios must be
  supported; do not mix it into HCF cleanup.
- Broad TODO cleanup is not a deliverable. Convert a TODO into work only when it
  describes a reproducible bug, an accepted feature, or obsolete text.
- Do not introduce a new plugin interface for an internal helper unless users
  need to replace it through NED configuration.

## Verification for each production patch

Run the narrow unit test first, then:

```sh
make MODE=release -j$(nproc)

inet_run_unit_tests \
  -m release \
  -f '(Hcf|Ieee80211(He|Ht|Vht|Eht|.*BlockAck|.*ModeSet|.*Ru)).*\.test'

inet_run_module_tests \
  -m release \
  --no-build \
  --no-concurrent \
  -f 'Ieee80211(SharedMacModes|HeUlTriggerExchange|HeDlMuExchange|FailedBarRecovery|VhtDlMuNegative)_1.*'
```

Use a narrower filter during development. For changes under either 802.11 source
root, also run the focused architecture checks and apply both the general and
WLAN semantic review checklists. Run relevant fingerprint rows after the focused
tests pass.

## Completion criteria

The SWOT work is complete when:

- placeholder coordination functions are either removed through a deliberate
  compatibility change or fail clearly before simulation behavior begins;
- supported non-QoS, management, control, QoS, and Block Ack paths have explicit
  tested outcomes;
- one coordinator owns each active HCF exchange and the existing helpers own
  their narrow responsibilities;
- `Hcf.h` no longer includes avoidable concrete implementation headers;
- HE DL and UL success and failure exchanges have deterministic module-level
  evidence, with legacy and VHT coverage beside them;
- the chosen per-AC metrics are available from owner-emitted signals;
- PHY table cleanup preserves current rates, durations, and serialized bytes;
- every performance change is backed by a repeatable end-to-end measurement;
- focused tests, relevant module tests, architecture checks, semantic reviews,
  and fingerprints have no unexplained failures.
