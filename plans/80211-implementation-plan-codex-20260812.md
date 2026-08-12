# IEEE 802.11 implementation improvement plan

## Purpose

This plan turns the findings in
[the static SWOT report](reports/80211-swot-report-codex-20260812.md) into a
sequence of reviewable implementation changes. It prioritizes protocol correctness
and ownership before structural cleanup.

The plan covers the detailed IEEE 802.11 implementation under:

- `src/inet/linklayer/ieee80211/`
- `src/inet/physicallayer/wireless/ieee80211/`
- focused IEEE 802.11 tests under `tests/unit/`, `tests/module/`, and
  `tests/fingerprint/`

All proposed production paths are currently unsealed. The only recursively sealed
source area is `src/inet/common/packet/`; this plan does not require changes there.
If the change surface later expands into that subtree, implementation must stop and
obtain explicit permission for each sealed target.

## Important correction to the SWOT premise

IEEE Std 802.11-2024 does not make all AP management state transitions depend on an
ACK:

- AP-side successful Open System authentication sets the peer state after the
  successful Authentication frame is transmitted; clause 11.3.4.3(h) does not add
  an ACK precondition.
- AP-side successful association advances state only when the successful
  Association Response is acknowledged: clause 11.3.5.3(o).
- AP-side successful reassociation advances state only when the successful
  Reassociation Response is acknowledged: clause 11.3.5.5(m).

The implementation must therefore defer **association and reassociation**, while
keeping successful authentication independent of response-ACK receipt. The existing
authentication TODO should be replaced with the correct clause reference rather than
implemented as written.

The standards evidence came from the fresh generated corpus for IEEE Std
802.11-2024: `80211ax-2024:chunk:06134`, `:06140`, and `:06142`. No PDF inspection
was required.

## Goals

1. Make AP association and reassociation state match the acknowledged frame exchange.
2. Implement address-role interpretation once and use Address 1/RA and Address 2/TA
   correctly across To DS/From DS combinations.
3. Replace mode-name strings and concrete-type checks with explicit operation and
   capability contracts.
4. Reduce `Hcf` responsibility without changing observable behavior during the
   refactoring stages.
5. Make represented management fields serialize and deserialize faithfully, while
   rejecting or documenting unsupported content.
6. Make ownership and limitations easier to audit.

## Non-goals

- Implementing HCCA, every optional management information element, mesh addressing,
  security protocols, or missing PHY fidelity in one campaign.
- Redesigning the OMNeT++ packet API or editing generated `*_m.cc`/`*_m.h` files.
- Combining protocol corrections with broad formatting or naming cleanup.
- Updating fingerprint CSV files without a separately explained trajectory change
  and explicit user approval.

## Required architecture rules

The main requirements governing the work are:

- `AR-MOD-COMPOSITION`, `AR-ORG-CONTRACTS`: prefer focused services and typed
  contracts over a larger inheritance hierarchy.
- `AR-PKT-CHUNKS`, `AR-PKT-DUAL`, `AR-PKT-TAGS`: preserve typed immutable content,
  serializer symmetry, and the wire-content/metadata boundary.
- `AR-OBS-SIGNALS`: observation must remain external to protocol decisions.
- `AR-QUAL-TESTS`, `AR-QUAL-DETERMINISM`, `AR-QUAL-FINGERPRINT`,
  `AR-QUAL-TRACEABILITY`: focused tests establish correctness; fingerprints detect
  trajectory changes but do not prove correctness.
- `AR-WLAN-STD-TRACE`, `AR-WLAN-STD-GATING`: cite the applicable IEEE revision and
  gate amendment behavior on configured operation plus local and peer capability.
- `AR-WLAN-ARCH-BOUNDARIES`, `AR-WLAN-ARCH-OWNERSHIP`,
  `AR-WLAN-ARCH-VARIANTS`: MAC owns exchanges, management owns association state,
  PHY owns legality and duration, and substantial variants use typed policies.
- `AR-WLAN-FRAME-REPRESENTATION`: common address interpretation and represented
  on-air fields are defined once.
- `AR-WLAN-MAC-EXCHANGE`: transmission, expected response, retry, timeout, and
  completion have one explicit owner.
- `AR-WLAN-QUAL-TESTS`: cover boundaries, roles, capability combinations, legacy
  modes, group addressing, and applicable aggregation paths.

## Delivery order

| Phase | Outcome | Depends on | Behavioral change |
| --- | --- | --- | --- |
| 0 | Baselines and explicit contracts | — | None |
| 1 | Correct AP association/reassociation commit point | Phase 0 | Yes |
| 2 | Central address-role interpretation | Phase 0 | Yes for affected edge cases |
| 3 | Explicit HE/EHT/legacy capability gates | Phase 0 | Intended gating corrections |
| 4 | Smaller HCF preparation and result services | Phases 1–3 | No, split mechanically |
| 5 | Faithful management serialization | Phase 1 | Yes, packet bytes/content |
| 6 | Ownership and limitation cleanup | Phases 4–5 | Normally none |

Phases 1, 2, 3, and 5 may be developed as separate branches after Phase 0, but they
should be merged in the order above. Phase 4 must not overlap a functional HCF change.
Use one production-code writer at a time. Every nontrivial phase receives independent
regression and architecture review after its diff stabilizes.

## Phase 0 — establish baselines and contracts

### Work

1. Record the current release and debug unit-test results for the focused areas.
2. Select one deterministic module configuration and run/seed for each exchange that
   later phases change: association, reassociation, DS address handling, legacy SU,
   HE SU, HE DL MU, and HE UL Trigger.
3. Record current fingerprints without changing expected CSV files.
4. Write a short design note for the management-to-MAC result contract before adding
   code. It must answer:
   - Which component owns a pending association transaction?
   - How is a response correlated without putting local state on the wire?
   - Which callback reports ACK success and final retry failure?
   - How is an allocated AID reserved without publishing ASSOCIATED state?
   - How are stop, crash, deauthentication, and a replacement request handled?
5. Inventory every literal `"ax"`/`"be"` operation check and every runtime downcast
   used as a capability decision. Classify startup compatibility checks separately;
   they do not all need replacement.

### Acceptance gate

- Baselines name the exact command, mode, filter/configuration, run, seed, exit status,
  and artifact path.
- The design assigns one owner to pending association state and one owner to the MAC
  exchange outcome.
- The proposed feedback path is typed and direct; observational signals are not used
  to control association state.
- The change surface and seal status are rechecked immediately before implementation.

## Phase 1 — commit AP association only after response acknowledgment

### Current issue

[Ieee80211MgmtAp.cc](src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtAp.cc:405)
installs peer capabilities and commits association before sending the response. The
same pattern appears in the reassociation path. The MAC coordination function already
knows when a management frame receives a valid ACK and when its retry limit is reached.
The HCF paths are visible in
[Hcf.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc:1827) and
[Hcf.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc:1635), but that
outcome is not returned to the management owner. The result contract must also cover
DCF when that coordination function owns the exchange.

### Design

1. Add an immutable, local-only correlation record to successful Association and
   Reassociation Response packets. Use a request/tag or typed transaction identifier;
   do not encode it in `Ieee80211MgmtFrame` wire content.
2. Add a typed MAC-management exchange-result contract with outcomes equivalent to:
   `ACKNOWLEDGED` and `RETRY_LIMIT_REACHED`. Do not use a general packet signal as the
   control path.
3. Keep the pending transaction in `Ieee80211MgmtAp`. It contains the peer, response
   subtype, prior association snapshot, proposed capability snapshot, and reserved AID.
4. Extend `Ieee80211Mib` with an explicit reservation/commit/abort API if necessary:
   - reserve an AID for the response without exposing ASSOCIATED state;
   - atomically publish peer capabilities, member status, AID, and association epoch
     when the ACK result arrives;
   - release the reservation and staged data after final failure or cancellation.
5. Emit the typed association transition and existing observational signals only
   after the MIB commit. Synchronous listeners must see the committed snapshot.
6. Keep successful authentication at its existing non-ACK-gated transition point and
   replace the misleading TODO with IEEE 802.11-2024 clause 11.3.4.3(h).
7. Reassociation must preserve or replace prior state according to clause 11.3.5.5;
   do not clear a working association merely because a new response was not ACKed.

### Candidate files

- `src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtAp.h`
- `src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtAp.cc`
- `src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtBase.h` if the result contract is
  shared by detailed management roles
- `src/inet/linklayer/ieee80211/mib/Ieee80211Mib.h`
- `src/inet/linklayer/ieee80211/mib/Ieee80211Mib.cc`
- a narrowly named contract/tag under `mac/contract/` or `mac/`, chosen during the
  Phase 0 design review
- `src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.h`
- `src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc`
- `src/inet/linklayer/ieee80211/mac/coordinationfunction/Dcf.h/.cc` if the selected
  configuration sends management responses through DCF
- existing association unit tests, especially
  `tests/unit/Ieee80211AssociationSignals_1.test`

Do not change `Ieee80211MgmtStaSimplified` merely to make this design easier; simplified
management has a different abstraction and should receive only the minimum compatibility
adaptation proven necessary.

### Focused tests

- Successful Authentication transmission changes AP peer state without requiring an
  ACK callback; failed authentication leaves it unchanged.
- Successful Association Response prepared and transmitted, but not ACKed: no
  ASSOCIATED state, association epoch, or associated signal.
- Valid ACK: exactly one atomic transition and one signal; duplicate/stale result is
  ignored or rejected deterministically.
- Retry limit: reservation and staged capabilities are discarded and no associated
  state leaks.
- Reassociation ACK success, failure, timeout, same-AP replacement, and cancellation
  during shutdown.
- Two peers with overlapping pending responses cannot consume or commit each other's
  transaction or AID.

### Completion criteria

- The state timeline matches clauses 11.3.4.3(h), 11.3.5.3(o), and 11.3.5.5(m).
- Management remains the sole owner of association intent and MIB state; the active
  MAC coordination function reports only exchange results.
- No control decision depends on an observational signal.
- Legacy association tests are updated to assert the new commit point, not weakened.

## Phase 2 — centralize address-role interpretation

### Current issue

`Hcf::isSentByUs()` uses Address 3 even though its role changes with To DS and From DS.
The same shortcut is duplicated in DCF and in
[FrameSequenceContext.cc](src/inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.cc:44).
IEEE 802.11-2024 clause 9.3.2.1.2 and Table 9-60 establish that Address 1 is RA and
Address 2 is TA for all nonmesh Data-frame combinations.

### Design

1. Introduce one stateless address interpretation helper in the IEEE 802.11 MAC common
   package. A suitable shape is a value result containing available RA, TA, DA, SA,
   and BSSID roles, rather than several unrelated boolean shortcuts.
2. For the first patch, support nonmesh data and ordinary management/control formats
   already represented by INET. Reject or return an explicit unsupported result for
   mesh/GLK cases rather than guessing.
3. Make `isForUs()` compare the local address with RA/Address 1. Make loopback
   suppression compare the local address with TA/Address 2.
4. Replace the duplicated HCF, DCF, and frame-sequence predicates with the helper.
5. In a separate follow-up diff, use the same helper to simplify MAC
   encapsulation/decapsulation and MSDU aggregation/deaggregation only if byte and
   delivery equivalence is demonstrated.

### Candidate files

- new `Ieee80211Addressing.h` and, if needed, `.cc` under
  `src/inet/linklayer/ieee80211/mac/common/`
- `src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc`
- `src/inet/linklayer/ieee80211/mac/coordinationfunction/Dcf.cc`
- `src/inet/linklayer/ieee80211/mac/framesequence/FrameSequenceContext.cc`
- later follow-up only: `Ieee80211Mac.cc`, `MsduAggregation.cc`, and
  `MsduDeaggregation.cc`

### Focused tests

- Table-driven test for all four To DS/From DS combinations from Table 9-60.
- Individual and group RA; local and foreign TA.
- A-MSDU cases where DA/SA live in subframe headers but RA/TA remain in the outer
  header.
- AP, non-AP, independent, and four-address nonmesh paths currently supported by INET.
- An explicit unsupported-format test for any excluded mesh/GLK interpretation.

### Completion criteria

- No HCF, DCF, or frame-sequence decision uses Address 3 to decide who transmitted
  the frame.
- The helper is the single implementation of represented address-role mapping.
- Ordinary infrastructure, ad hoc, multicast, aggregation, ACK, and retry regressions
  remain green.

## Phase 3 — replace literal mode-name gating with capabilities

### Current issue

HE behavior is selected with literal names in
[HeHcf.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc:616),
[Ieee80211Mac.h](src/inet/linklayer/ieee80211/mac/Ieee80211Mac.h:153), management,
TWT, and HE UL/DL paths. This ties protocol behavior to profile spelling and does not
express local, BSS, and peer capability separately.

### Design

1. Add a mode-set query for configured PHY-family availability instead of testing the
   profile name. Reuse the existing `Ieee80211PhyFamily` classification.
2. Add MAC/MIB queries that distinguish:
   - HE or EHT family available in the configured mode set;
   - local HE option/capability enabled;
   - AP versus non-AP role;
   - peer HE capability and negotiated operation constraints;
   - whether a particular HE feature such as MU, TWT, sounding, or puncturing is
     allowed.
3. Use the narrowest query at each call site. Do not replace `isAxMode()` with one
   equally broad `isHeMode()` boolean that hides peer requirements.
4. Decide and document EHT interaction before changing behavior. EHT may use HE
   fallback capabilities, but compiling or selecting `be` must not automatically
   enable every HE procedure.
5. Keep concrete `dynamic_cast` checks that validate a configured implementation at
   initialization. Replace runtime downcasts used to choose protocol behavior with a
   typed capability or result contract.

### Candidate files

- `src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.h/.cc`
- `src/inet/linklayer/ieee80211/mib/Ieee80211Mib.h/.cc`
- `src/inet/linklayer/ieee80211/mac/Ieee80211Mac.h`
- `src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtBase.cc`
- `src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc`
- `HeHcfDl.cc`, `HeHcfUl.cc`, and `Ieee80211TwtManager.cc` where the inventory proves
  a behavioral name check

### Focused tests

- Configured legacy profile with HE code compiled: no HE procedure or HE-only IE.
- HE profile with local HE disabled: fail configuration early or use the documented
  fallback.
- HE-capable local STA with non-HE peer: legacy exchange only.
- HE peers with incompatible Basic HE MCS/NSS: association refused or HE operation
  rejected at the correct boundary.
- `ax`, `ax-catalog`, and any supported alias/profile expose equivalent capabilities.
- Explicit EHT cases proving which HE fallback procedures are and are not allowed.

### Completion criteria

- No operational decision in the scoped files depends on the strings `"ax"` or
  `"be"`.
- Gating tests cover operation mode, local capability, peer capability, AP/non-AP
  role, and a legacy peer.
- HE-specific behavior remains absent from legacy trajectories.

## Phase 4 — decompose HCF without changing behavior

This phase is a series of small, behavior-preserving patches. It starts only after the
functional changes above are stable.

### 4A. Immutable transmission-preparation result

Extend the existing
[HcfFramePreparation](src/inet/linklayer/ieee80211/mac/coordinationfunction/HcfFramePreparation.h:18)
direction. Introduce an immutable preparation result containing the selected mode,
ACK policy, protection decision, duration/NAV result, aggregation membership, and
required packet mutations. The plan may hold references or immutable chunk pointers;
it must not become a second owner of retry, queue, TXOP, or Block Ack state.

`Hcf` remains the exchange coordinator and applies the result once. PHY mode objects
remain authoritative for legality, rate, and duration.

### 4B. Aggregation planning and materialization

Separate candidate selection and PPDU-duration validation from packet
materialization. Reuse `HcfAggregationService`; do not copy PHY duration formulas into
MAC code. The selected member order must remain deterministic.

### 4C. Transmission-result service

Extract ACK/Block Ack/retry result bookkeeping behind a typed service. Keep the
exchange transition in `HcfExchangeCoordinator`, EDCA retry state in its current
owner, and Block Ack window state in its agreement handlers.

### 4D. HE/VHT typed contexts

Replace repeated runtime discovery of concrete frame-sequence and rate-control types
with narrow immutable contexts or capability interfaces where an actual extension
boundary exists. Do not introduce an interface for a one-off startup validation.

### Candidate files

- `Hcf.h/.cc`
- `HcfFramePreparation.h/.cc`
- `HcfAggregationService.h/.cc`
- `HcfRetryService.h/.cc`
- a narrowly scoped new result/service pair if existing helpers cannot own it cleanly
- `VhtHcf.cc` and `HeHcfTxRx.cc` only when adapting to the new typed result

### Completion criteria for every subphase

- The focused unit suite and selected fingerprint use the same build mode, config,
  run, and seed before and after.
- No unexplained fingerprint, packet-byte, signal, timer, or event-order change.
- `Hcf` retains authoritative exchange state; extracted helpers are stateless or have
  one explicitly documented state responsibility.
- The diff does not mix extraction with protocol corrections or unrelated changes.

## Phase 5 — make management serialization faithful

### Design

1. Define the intentionally modeled fields before editing the serializer. Start with
   the fixed fields already represented by the message types:
   - Authentication algorithm, transaction sequence, and status;
   - Association/Reassociation Capability Information, Listen Interval, Status, AID,
     Current AP, SSID, and rates;
   - Beacon/Probe Response Timestamp, Beacon Interval, Capability Information, SSID,
     and rates.
2. Add missing typed fields to `Ieee80211MgmtFrame.msg` only when INET has an
   authoritative producer and consumer. Never edit generated `_m` files.
3. Replace dummy zero values and discarded decoded values in
   `Ieee80211MgmtFrameSerializer` with symmetric encode/decode logic.
4. Decide timestamp authority explicitly. Do not serialize `simTime().raw()` as TSF
   merely because both are integers; either model a defined timestamp source or mark
   timestamp fidelity unsupported.
5. For optional elements, implement the supported subset with correct presence rules.
   Reject malformed or prohibited encodings and document omitted model scope.
6. Keep serializers free of association decisions and other protocol state.

### Standards anchors

- Authentication: clause 9.3.3.11, Tables 9-70 and 9-71.
- Association Request/Response: Tables 9-64 and 9-65.
- Reassociation Request/Response: Tables 9-66 and 9-67.
- Beacon and Probe Response: Tables 9-62 and 9-69.

### Candidate files

- `src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame.msg`
- `src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrameSerializer.h/.cc`
- management frame builders in `Ieee80211MgmtAp.cc`, `Ieee80211MgmtSta.cc`, and
  shared management helpers only for fields newly made authoritative
- new focused serializer test, suggested name
  `tests/unit/Ieee80211MgmtFrameSerializer_1.test`

### Focused tests

- Byte-exact known fixtures for each fixed frame-body format in scope.
- Encode/decode/encode stability and semantic field equality.
- Baseline legacy and HE-capability-bearing variants.
- Boundary lengths, invalid element lengths, invalid AID/status values where the
  serializer is responsible for validation, and unsupported conditional content.
- PCAP/dissector readability for at least one association and one beacon exchange.

### Completion criteria

- No represented field is silently serialized as a dummy value or discarded on
  decode.
- Unsupported fields are explicitly absent or rejected, not silently fabricated.
- Generated sources are regenerated by the normal build and never hand-edited.

## Phase 6 — ownership and limitation cleanup

### Work

1. Convert plain C++ helper ownership in HCF and HE coordination to `std::unique_ptr`
   where OMNeT++ does not own the object. Do not wrap module-owned or message-owned
   objects blindly.
2. Keep raw non-owning pointers visibly non-owning and document unusual lifetime
   dependencies.
3. Classify TODO/FIXME/KLUDGE entries in the touched area as:
   - correctness defect with a focused test;
   - supported-scope limitation with a clear error or documentation;
   - out-of-scope issue to record separately;
   - obsolete note to remove.
4. Replace the informal `src/inet/linklayer/ieee80211/__TODO` scratchpad with tracked,
   scoped issues or maintained documentation. Do not delete useful knowledge without
   preserving its actionable content.
5. Make HCCA's unsupported status unmistakable at configuration/initialization time.
   Do not present it as working merely because the submodule exists.
6. Correct misleading NED defaults such as the generic BPSK modulation only after
   tracing whether the value is authoritative or overridden by the mode pipeline.

### Completion criteria

- Every newly introduced owner uses an ownership-revealing type.
- No change transfers ownership across the OMNeT++ boundary without an explicit test.
- Remaining limitations say which modes/configurations are affected and how failure
  is reported.
- Cleanup commits remain separate from behavioral fixes and broad renames.

## Verification commands for implementation phases

Run from the repository root. Adjust the regex only to the files actually changed;
use one combined `-f` expression.

```sh
make MODE=release -j$(nproc)
inet_run_unit_tests -m release -f '(Ieee80211.*(Association|Mgmt|Address|Mode|Hcf)|Hcf.*|Dcf.*).*\.test'

make MODE=debug -j$(nproc)
inet_run_unit_tests -m debug -f '(Ieee80211.*(Association|Mgmt|Address|Mode|Hcf)|Hcf.*|Dcf.*).*\.test'
```

For each selected module or simulation regression, preserve the exact working
directory, configuration, run, seed-set, command-line overrides, exit status, and
artifact directory. Start with one run and expand seeds only after the narrow behavior
is understood.

For architecture-sensitive changes:

```sh
bash .agents/skills/inet-architectural-requirements/references/enforcement/check-architecture.sh src/inet/linklayer/ieee80211
bash .agents/skills/inet-architectural-requirements/references/enforcement/check-architecture.sh src/inet/physicallayer/wireless/ieee80211
```

Reconcile nonzero output with the exception ledger; do not label all existing output
as newly introduced. Review each stabilized 802.11 diff with both the complete general
and WLAN semantic checklists. Fingerprint updates require an explained first divergence
and explicit user approval.

## Cross-phase regression matrix

| Behavior | Focused proof | Legacy proof |
| --- | --- | --- |
| AP association/reassociation | ACK, lost ACK, retry limit, stale result, two peers | existing STA association and retransmission tests |
| Address roles | four To DS/From DS combinations, group RA, A-MSDU | infrastructure and ad hoc delivery |
| HE gates | local/peer capability matrix, AP/non-AP, aliases | non-HE peer and legacy SU |
| HCF extraction | state/timer/result assertions | unchanged focused fingerprint and retransmission suite |
| Management serialization | byte fixture and round trip | legacy frame decode and PCAP/dissector |
| Ownership cleanup | debug assertions and lifecycle stop/crash | same release/debug behavior |

## Review and merge policy

Each phase should be independently reviewable and revertible:

1. Establish the exact behavior and standards clause before implementation.
2. Recheck target seals and architecture identifiers.
3. Use exactly one production-code writer.
4. Build and run the focused release tests before broadening coverage.
5. Run the matching debug tests for ownership, assertions, and generated-code changes.
6. Use an independent regression lane for the claimed behavior.
7. Use an independent reviewer applying both required semantic checklists.
8. Explain every fingerprint change; never update its CSV as part of hiding a failure.
9. Merge structural extraction separately from semantic changes.

## Final definition of done

The campaign is complete when:

- association and reassociation commit exactly once after the acknowledged successful
  response, while successful authentication retains its correct non-ACK-gated timing;
- represented address roles follow IEEE 802.11-2024 Table 9-60 and have one shared
  implementation;
- scoped HE/EHT decisions use explicit configured/local/peer capability contracts;
- HCF coordinates a smaller set of single-purpose preparation and result services;
- management serialization preserves every field the model claims to represent;
- ownership is explicit and remaining limitations are actionable;
- focused, legacy, release, debug, architecture, and independent review gates pass;
- no fingerprint baseline is changed without explanation and explicit approval.
