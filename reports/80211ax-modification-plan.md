# IEEE 802.11ax architecture and packet-level PHY correctness modification plan

## 1. Purpose and decisions

This plan turns the findings in
[`80211ax-architecture-and-quality-review.md`](80211ax-architecture-and-quality-review.md)
into one coordinated refactor of the HE implementation.

The following scope decisions are fixed:

- The work will be developed as one integration effort. It must still be split
  into buildable, reviewable commits with explicit verification gates; it must
  not be delivered as one untestable commit.
- Backward compatibility is not required for HE-specific C++/MSG interfaces,
  scheduler extension APIs, serialized HE chunks, PCAP/radiotap output, or
  result fingerprints.
- Existing NED/INI names should be retained only where their semantics remain
  correct. `opMode = "ax"` will mean the standards-oriented HE profile rather
  than the current compact approximation.
- The target remains a packet-level PHY at INET's normal network-simulation
  abstraction. HE format identity, modeled PLCP field semantics, serialized
  field layouts, RU allocation, legality, length, symbol count, duration,
  per-user reception, and capture metadata must be standards-correct.
  BCC/LDPC, scrambling, interleaving, DCM, modulation, and decoding remain
  analytical inputs to timing and packet-error models; the simulator will not
  construct or decode their bit streams.
- IEEE Std 802.11-2024 is the normative oracle. The checked-in implementation
  and reproducible simulations are the oracle for current behavior.
- Model-only scheduling, transaction, channel, and diagnostic state may remain
  packet-level, but it must use types that cannot be serialized or exported as
  IEEE wire fields.

The intended end state is one canonical, validated TXVECTOR/RXVECTOR contract
from scheduler to receiver; standards-correct packet-level representations of
HE SU, HE ER SU, HE MU, and HE TB; transactional MAC planning and packet
ownership; and a small `HeHcf` coordinator over independently testable
services.

## 2. Non-goals

The refactor will not attempt to provide:

- FEC encoding/decoding or generation of scrambled, coded, interleaved, or
  modulated PHY Data bits;
- a complete serialized PPDU bitstream or constellation/pilot sequence;
- time-domain waveform generation or a conformance-test radio;
- synchronization, timing-offset, CFO, channel-estimation, or RF-impairment
  models;
- standard-prescribed behavior for Minstrel or other research policies;
- wire serialization of simulation-only objects such as scheduler plans,
  Trigger correlation handles, per-user SNIR/PER annotations, or queue handles;
- automatic acceptance of changed fingerprints. Fingerprint updates still
  require separate user approval after the first changed event is explained.

## 3. Delivery strategy

Use one feature branch with the gates below. Every gate must compile and pass
its focused tests before the next gate removes old representations. A final
merge is allowed only after all gates pass.

The refactor should be implemented by one production-code owner at a time.
Standards-vector preparation, test-fixture work, and read-only review may run
independently when they do not race with production changes. After the stable
diff exists, run an independent correctness review and regression assessment.

Generated `_m.*` sources are never edited directly. Changes to HE packet
schemas start in the corresponding `.msg` files.

## 4. Gate 0: freeze the fidelity contract and restore the baseline

### 4.1 Produce the supported-feature matrix

Create a reviewable matrix, referenced by the code and tests, covering:

- HE SU, HE ER SU, HE MU, and HE TB field presence and construction;
- 20, 40, 80, 160, and 80+80 MHz behavior;
- applicable 2.4, 5, and 6 GHz compatibility requirements;
- all standard RU sizes and placements, central 26-tone RUs, mixed layouts,
  MU-MIMO users per RU, and puncturing patterns;
- all HE GI/LTF combinations and legal HE-LTF counts;
- MCS 0-11, mandatory/optional classification, NSS limits, DCM constraints,
  BCC/LDPC rules, packet extension, and maximum PPDU duration;
- modeled PLCP fields and analytical Data-field calculations for each PPDU
  format;
- applicable legacy/HT/VHT reception modes required by the HE profile.

Each row must identify an IEEE clause/table and one of: supported, deliberately
unsupported, or model-only. “Deliberately unsupported” is not allowed for a
mandatory item in the standards-oriented `ax` profile.

### 4.2 Build independent golden-vector fixtures

Add separately reviewable fixture data with provenance for:

- RU layouts and HE-SIG-B allocation codes;
- HE-SIG-A/B logical fields, field widths, reserved values, bit ordering, and
  exact serializer bytes for fields represented by the model;
- L-SIG/RL-SIG and HE PPDU field ordering;
- Trigger, Multi-STA Block Ack, management elements, and A-MPDU delimiters;
- analytical BCC/LDPC/DCM boundaries, code-rate and symbol-count calculations,
  LDPC shortening/puncturing/repetition counts, padding, and packet-extension
  cases;
- PPDU symbol counts, packet extension, and durations.

Vectors must not be generated by the implementation under test. Round trips
are retained as symmetry tests, never as the sole correctness oracle.

### 4.3 Restore the current HE test gate

Repair the `IAckHandler::isRetransmission(...)` mocks in:

- [`Ieee80211HeMuSeqAck_1.test`](../tests/unit/Ieee80211HeMuSeqAck_1.test)
- [`Ieee80211HeDlMuTxOpFs_1.test`](../tests/unit/Ieee80211HeDlMuTxOpFs_1.test)
- [`Ieee80211HeMuAddbaValidation_1.test`](../tests/unit/Ieee80211HeMuAddbaValidation_1.test)

Replace the tautological HE-SIG-B assertion in
[`Ieee80211OnWireBitCompliance_1.test`](../tests/unit/Ieee80211OnWireBitCompliance_1.test)
with a serializer/codec invocation and independent expected field bits.

Run the complete HE companion slice in release and debug. A compile failure is
a gate failure.

```sh
CCACHE_DISABLE=1 inet_run_unit_tests -m release \
  -f '(Ieee80211He|HeDlScheduler|HeUlScheduler|Ieee80211OnWireBitCompliance|Ieee80211TriggerFrameSerializer|Ieee80211MultiTidBlockAck|Ieee80211TwtFrames|Ieee80211RbirErrorModel|Tgax.*|PcapRecorderRadiotapExtended).*\.test'

CCACHE_DISABLE=1 inet_run_unit_tests -m debug \
  -f '(Ieee80211He|HeDlScheduler|HeUlScheduler|Ieee80211OnWireBitCompliance|Ieee80211TriggerFrameSerializer|Ieee80211MultiTidBlockAck|Ieee80211TwtFrames|Ieee80211RbirErrorModel|Tgax.*|PcapRecorderRadiotapExtended).*\.test'
```

Exit criteria:

- all selected tests compile and execute;
- the standards matrix and fixture provenance are reviewed;
- known failures are reproduced by independent red tests or documented test
  vectors;
- no production behavior has changed.

## 5. Gate 1: replace the distributed HE contract

### 5.1 Canonical immutable value objects

Introduce canonical validated types, provisionally named:

- `HeCommonTxVector` and `HeUserTxVector`;
- `HeCommonRxVector` and `HeUserRxVector`;
- `HePpduLayout` for exact logical field offsets, lengths, and durations;
- `HeValidationResult` with stable error codes and diagnostic context;
- immutable `HeDlPlan` and `HeUlPlan` objects containing TxVectors and packet
  selection handles, but no packet ownership.

The exact names may change during design review. The contract must cover PPDU
format, bandwidth, GI/LTF, HE-LTF count, puncturing, BSS color, TXOP/PE,
HE-SIG-B mode and symbol count, Trigger identity, RU geometry, AID/STA ID,
MCS, NSS/space-time streams, coding, DCM, PSDU length, power, and common
duration.

Construction must be factory/validator based. Invalid values cannot produce a
usable TxVector. Public validation must be non-throwing and active in release.

Initial ownership is near
[`Ieee80211HePhyCalculator.h`](../src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.h),
but queue, scheduler-policy, and module-discovery concerns must not enter these
PHY contracts.

### 5.2 Separate wire data from model metadata

Replace the current ambiguous HE chunks with two explicit families:

- normative PLCP/PSDU structures that have exact serializers/parsers; and
- model-only descriptors/containers with no serializer registration.

`Ieee80211HeMuRuPayloadHeader` must become model-only transaction metadata or
be removed. Do not repair its custom 12-byte layout and call it an IEEE wire
format. Receiver delimiting must derive from normative PSDU/A-MPDU and PLCP
length information, eliminating the `mpduLength == 0` accidental-NDP path.

Primary schema and serializer surfaces are:

- [`Ieee80211PhyHeader.msg`](../src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader.msg)
- [`Ieee80211Tag.msg`](../src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Tag.msg)
- [`Ieee80211Transmission.msg`](../src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.msg)
- [`Ieee80211PhyHeaderSerializer.cc`](../src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeaderSerializer.cc)

### 5.3 Directional capabilities and complete `ax` profile

Replace symmetric capability intersections with explicitly directional
contracts: local-TX/peer-RX and local-RX/peer-TX. Retain raw local and peer
advertisements so derived contracts can be audited.

Replace the compact `ax` mode set with the standards-oriented band-aware mode
and capability set. HE MCS 0-7 and optional 8-11 must be classified correctly,
and applicable earlier PHY modes must be admitted. Ordinary HE SU must no
longer create an HT PHY header, VHT preamble, or HT protocol tag.

Primary surfaces include:

- [`Ieee80211HeCapabilities.h`](../src/inet/linklayer/ieee80211/mib/Ieee80211HeCapabilities.h)
- [`Ieee80211ModeSet.cc`](../src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211ModeSet.cc)
- [`Ieee80211HeMode.h`](../src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h)
- [`Ieee80211Radio.cc`](../src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.cc)

### 5.4 Narrow link/PHY context

Introduce a read-only HE link/PHY context interface over scalar and dimensional
radio implementations. It exposes only the scheduler inputs needed for legal
planning: channel/bandwidth, transmit-power constraints, receive sensitivity
or noise data, negotiated capabilities, puncturing/GI policy, and link
estimates. Scheduler context collection must stop discovering and casting
concrete radio submodules.

Exit criteria:

- value objects and model/wire boundaries are independently unit-tested;
- invalid objects cannot be constructed through public APIs;
- scheduler tests operate on a mock link/PHY context;
- the `ax` profile's band/mode matrix passes;
- old parallel HE fields remain only behind temporary in-branch conversion
  code and are not accepted as independent sources of truth.

## 6. Gate 2: correct the packet-level HE PHY path

### 6.1 Canonical PPDU description and parsing

Implement one packet-level HE PPDU description for HE SU, HE ER SU, HE MU,
and HE TB. It consumes canonical TxVectors and PSDU/A-MPDU packets and
produces:

- the correct PPDU format and ordered modeled PLCP fields;
- standards-correct logical values for every represented HE-SIG field;
- per-user RU and spatial-stream mappings;
- PSDU/A-MPDU lengths and model-container boundaries;
- exact analytical symbol counts, field durations, and total PPDU duration;
- one immutable description consumed by the transmitter, medium, receiver,
  packet printer, and capture exporter.

The receive path must reconstruct RXVECTOR and select the correct user's
PSDU/A-MPDU using that canonical description. It must reject invalid reserved
values, lengths, RU layouts, capabilities, and unsupported combinations. It
does not decode a physical bit stream.

### 6.2 Correct serialized signaling semantics

Correct L-SIG/RL-SIG and HE-SIG-A/B representation, including:

- format-dependent field meanings and presence;
- SIG-B compression and symbol count;
- HE-LTF count encoding;
- LDPC Extra Symbol Segment semantics;
- punctured bandwidth and RU allocation encoding;
- field widths, bit ordering, reserved values, and CRC/tail fields where the
  packet-level serializer represents them;
- invalid-pattern rejection.

Do not add BCC encoding, interleaving, or other physical coding stages to the
serializer. Golden octets validate the logical wire fields represented by the
model, not a complete over-the-air PPDU.

This replaces the incorrect field assignments in
[`Ieee80211PhyHeaderSerializer.cc`](../src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeaderSerializer.cc)
and the ignored puncturing input in
[`Ieee80211HeSigCodec.cc`](../src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeSigCodec.cc).

### 6.3 Analytical Data-field calculation

Keep PHY Data processing analytical. The calculator must account correctly
for, without constructing encoded bits:

- exact PSDU/A-MPDU byte lengths and MAC delimiters;
- SERVICE, tail, PHY padding, MAC padding, and packet-extension lengths;
- BCC encoder count, code rate, and puncturing effects on symbol counts;
- LDPC codeword size/count, shortening, puncturing, repetition, and extra
  symbol segment decisions;
- stream count, DCM, RU data/pilot subcarrier counts, MCS, and NSS effects on
  `N_DBPS`, `N_CBPS`, symbol count, rate, and duration.

The mode and error-model objects may continue to describe BCC/LDPC and
modulation choices. They must not invoke a bit-level FEC encoder or decoder for
HE packet-level simulations.

### 6.4 Correct legality and timing

Rework
[`computeHePpduParameters()`](../src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HePhyCalculator.cc)
around the canonical vectors and analytical packet-level calculations:

- group streams per RU;
- require no more than eight streams on one RU;
- derive common HE-LTF count from the maximum initial per-RU requirement and
  receiver capability;
- make nine independent one-stream 26-tone RUs at 20 MHz legal with one
  HE-LTF;
- validate all MCS/RU/NSS/FEC/GI/LTF/puncturing/duration inputs without
  throwing;
- derive duration from the validated analytical field and symbol layout.

### 6.5 Packet-level channel integration

Keep propagation, interference, SNIR, and error decisions packet-level and per
RU. The error model must produce explicit per-user and, when an A-MPDU is
modeled, per-MPDU outcomes using the applicable RU bandwidth, MCS, NSS, coding,
length, and SNIR. Replace randomized post-hoc selection of one failed MPDU with
a documented packet-level outcome policy. The receiver then applies those
outcomes to delimiter/FCS/Block-Ack bookkeeping without pretending that FEC
decoding occurred.

Exit criteria:

- every supported PPDU format passes independent logical-field serialization
  and parsing vectors;
- analytical BCC/LDPC/DCM/padding/PE calculations pass independent timing and
  boundary vectors;
- punctured/unpunctured headers produce the expected distinct serialized
  fields;
- ordinary HE SU has true HE identity in packets and captures;
- serialization cannot change data into NDP or lose delimiting state;
- all malformed public inputs return structured invalid results;
- scalar and dimensional radios consume the same canonical PPDU contract.

## 7. Gate 3: reconnect MAC scheduling transactionally

### 7.1 Validate extension output before mutation

DL and UL scheduler results must be converted immediately into immutable
validated plans. In release builds, reject duplicate/overlapping RUs, invalid
AID/MCS/NSS, null queues, zero/illegal duration, unsupported capability, and
inconsistent common parameters before touching queues, BA state, timers, or
in-progress frames.

Define one observable failure policy:

- invalid DL plan: emit a structured diagnostic and use SU fallback if a legal
  inherited HCF transmission is available;
- invalid UL plan: emit a structured diagnostic and do not transmit a Trigger;
- internal invariant violations after commit: fail loudly because rollback is
  no longer guaranteed.

### 7.2 Transactional queue and packet ownership

Refactor DL first, then UL, but keep both within the same integration effort.
The transaction lifecycle is:

1. plan without dequeuing;
2. validate the complete plan and PPDU;
3. select stable packet handles;
4. perform one explicit commit transferring packets to in-progress ownership;
5. on pre-transmission failure, roll back queue order, age, BA occupancy, and
   packet ownership completely;
6. on transmission completion, move each MPDU exactly once to acknowledged,
   retry-queued, or dropped state.

The packet ledger invariant is that every MPDU is in exactly one state:
queued, selected-uncommitted, in progress, acknowledged/delivered,
retry-queued, or dropped.

Primary surfaces include:

- [`HeDlMuPackingPlanner.cc`](../src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuPackingPlanner.cc)
- [`HeDlMuTxOpFs.cc`](../src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc)
- [`HeUlMuTxOpFs.cc`](../src/inet/linklayer/ieee80211/mac/framesequence/HeUlMuTxOpFs.cc)
- [`HeUlCoordinator.cc`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HeUlCoordinator.cc)
- [`StationQueueBankManager.cc`](../src/inet/linklayer/ieee80211/mac/queue/StationQueueBankManager.cc)

### 7.3 Rate-control and capability fixes

Store a fresh peer's latest SNIR and timestamp before its rate table exists.
Define `updateInterval` as the validity/update contract or remove it. First
selection must use fresh SNIR and remain constrained by directional negotiated
capabilities and the selected RU.

Update MU-BAR/HE-TB response gating to use explicit directional capability
contracts. Reassociation and peer removal must invalidate all derived state.

Exit criteria:

- malformed scheduler output changes no external state in release or debug;
- planning failure, SU fallback, partial BA, timeout, wraparound, and teardown
  pass identity-based packet-conservation tests;
- DL and UL plans match the canonical TxVector consumed by the PHY;
- capability asymmetry and fresh/stale Minstrel state pass deterministic tests.

## 8. Gate 4: split HE orchestration into services

Only begin this gate after packet ownership and TxVector contracts are stable.
Extract the following services while keeping `HeHcf` as the EDCA/TXOP ordering
and fallback facade:

- queue-bank lifecycle and query service;
- DL MU exchange service;
- UL Trigger/UORA exchange service;
- directional capability service;
- TWT gating service;
- sounding/CSI service.

Preserve NED parameter paths and typenames when their meanings are still
valid. Update or replace misleading configuration names in one migration
change rather than retaining compatibility aliases.

The coordinator's only policy is the documented order after an EDCA win:
eligible pending UL exchange, eligible DL MU exchange, then inherited SU HCF
fallback, subject to TWT/capability/sounding gates.

Primary surfaces are
[`HeHcf.h`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h),
[`HeHcf.cc`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc),
[`HeHcfDl.cc`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfDl.cc),
[`HeHcfUl.cc`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfUl.cc),
[`HeHcfTxRx.cc`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfTxRx.cc),
and [`HeHcf.ned`](../src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.ned).

Exit criteria:

- each service has focused tests with fake dependencies;
- coordinator priority, gating, and SU fallback are deterministic;
- association/disassociation and dynamically created queue-bank lifecycles
  leak no packets or modules;
- external behavior is unchanged except for intentional corrections already
  established by earlier gates.

## 9. Gate 5: end-to-end and cross-mode proof

### 9.1 Required unit-test additions

Add or extend focused tests for:

- `Ieee80211HePhyCalculatorGolden_1`: RU/LTF, timing, PE, legality, boundaries;
- `Ieee80211HeSigAGolden_1` and existing SIG-B tests: exact fields and bits;
- HE BCC/LDPC/Data-pipeline golden vectors;
- `Ieee80211HeSuProtocolIdentity_1`: HE header, preamble, protocol, capture;
- `Ieee80211HeTxVector_1` and `Ieee80211HeTxVectorCrossLayer_1`;
- `Ieee80211HeSchedulerValidation_1` in release and debug;
- `Ieee80211HeDlMuTransaction_1` and `Ieee80211HeUlMuTransaction_1`;
- `Ieee80211HeDirectionalCapabilities_1`;
- actual analytic RU attenuation/noise/interference and per-user decoding;
- payload extraction with distinct sentinel bytes and positive/negative AIDs;
- new queue, DL, UL, and coordinator service boundaries.

### 9.2 Required deterministic module tests

Add:

- `Ieee80211HeConfigurationContract_1`: effective `ax`/QoS/`HeHcf` topology;
- `Ieee80211HeDlMuExchange_1`: association + ADDBA + DL MU + MU-BAR/HE-TB
  BA, followed by partial loss and timeout cases;
- `Ieee80211HeUlTriggerExchange_1`: BSR/Trigger + parallel same-Trigger-ID HE
  TB + Multi-STA BA, followed by late/missing/colliding responders;
- `Ieee80211SharedMacModes_1`: DCF, HCF, HT, VHT, HE SU, rejected HE MU/SU
  fallback, and EHT reuse where shared HE interfaces changed.

Use one configuration/run/seed first. Assert event, frame, timing, and packet
ledger invariants; PCAP, logs, and result files are supporting evidence rather
than the sole oracle.

### 9.3 Fingerprint and example checks

Run only the relevant existing entries from
[`ieee80211-he.csv`](../tests/fingerprint/ieee80211-he.csv) after deterministic
tests pass:

- DL contract/PHY: `dl_ofdma/EqualSizedRUs_fBW`;
- UL contract: `ul_ofdma/MixedUora` and `he_bsr/FullBsrAccounting`;
- puncturing/LDPC: `he_features/CombinedHeFeatures`;
- rate control: `he_rate_adaptation/HeMinstrel`;
- service-specific checks only when those services changed.

For every mismatch, identify the first changed event and classify it as an
intended correction, secondary timing effect, or regression. Generate
`.UPDATED` files only for inspection; do not update committed fingerprint CSVs
without explicit approval.

Exit criteria:

- all focused unit tests pass in release and debug;
- all deterministic module tests pass at their fixed seed;
- golden serialized-field and analytical timing vectors pass for the full
  supported-feature matrix;
- cross-layer TxVector equality and packet conservation hold;
- DCF/HCF/HT/VHT/HE/EHT shared-path regressions are clean;
- every fingerprint change is explained, with no expected files modified.

## 10. Gate 6: repository migration and documentation

Update all 802.11ax examples, walkthroughs, packet printers, radiotap/PCAP
exporters, configuration presets, and result queries to the new standard HE
identity and field names. Explicitly document:

- the exact packet-level PHY fidelity boundary;
- the continuing packet-level channel/error abstraction;
- which types are normative wire/PLCP data and which are model-only;
- the `ax` mode/capability matrix;
- removal or replacement of old scheduler and MSG interfaces;
- renamed/removed NED/INI parameters and result signals;
- expected PCAP and fingerprint trajectory changes.

No compatibility shim is required, but a concise migration table must map old
HE types, parameters, fields, and result names to their replacements.

Exit criteria:

- every repository example selects valid standards-oriented HE settings;
- walkthrough claims match the new fidelity boundary;
- no printer/exporter reports HE traffic as HT/VHT;
- old wire-like model containers have no serializer registration;
- the final independent implementation and regression reviews have no open P0
  or P1 findings.

## 11. Suggested reviewable commit sequence

The branch remains one refactor, but the following checkpoints preserve
bisectability and evidence:

1. Repair dead fixtures and establish the expanded HE test gate.
2. Add standards matrix and independent vector fixtures.
3. Add canonical TXVECTOR/RXVECTOR, validation, and model/wire type split.
4. Add directional capabilities, standard `ax` mode set, and HE identities.
5. Implement exact RU/timing/HE-LTF calculation.
6. Correct L-SIG/RL-SIG and HE-SIG-A/B logical field serialization.
7. Correct analytical BCC/LDPC/DCM/padding/PE calculations and error-model
   inputs.
8. Integrate explicit packet-level per-user/per-MPDU outcomes and remove
   randomized MPDU failure placement.
9. Convert DL scheduling/packing to immutable transactional plans.
10. Convert UL Trigger/UORA scheduling to immutable transactional plans.
11. Fix directional gating and HE-Minstrel temporal state.
12. Introduce the link/PHY context and split `HeHcf` services.
13. Add deterministic DL/UL/cross-mode module tests and reconcile examples.
14. Run final regression, explain fingerprints, update documentation, and
    complete independent review.

## 12. Main risks and controls

| Risk | Control |
| --- | --- |
| Serializer and parser share the same wrong field mapping | Independent IEEE-derived field/octet vectors; round trips are supplementary. |
| Broad refactor hides packet loss or duplication | Identity-based MPDU ledger and transactional commit/rollback tests before service extraction. |
| PHY and MAC changes fail simultaneously | Canonical vectors and the corrected packet-level PHY pass their gates before MAC reconnection. |
| `ax` changes regress shared modes | Deterministic DCF/HCF/HT/VHT/HE/EHT matrix plus existing retransmission coverage. |
| Analytical FEC/error behavior is mistaken for bit-level decoding | Document the packet-level boundary in code, captures, examples, and results; do not expose encoded-bit artifacts. |
| NED/INI or capture migration is missed | Repository-wide reference inventory and migration table before the final gate. |
| Fingerprints are normalized without explanation | First-event analysis and explicit approval before any expected fingerprint update. |

## 13. Definition of done

The refactor is complete only when:

1. HE SU, HE ER SU, HE MU, and HE TB produce and parse independently verified
   packet-level headers and logical HE signaling fields for the supported
   matrix.
2. No HT/VHT header, preamble, or protocol identity is used for HE traffic.
3. Scheduler plan, packed PSDUs, TXVECTOR, PPDU, transmission, RXVECTOR, and
   receiver indication agree by construction.
4. All external extension data is runtime-validated before state mutation.
5. DL and UL planning are transactional and all packet-conservation fault cases
   pass.
6. Model-only metadata cannot enter wire serialization or capture output.
7. Per-user RU interference/error decisions produce explicit packet-level
   PSDU/A-MPDU outcomes using the correct PHY parameters rather than random
   post-processing or claims of FEC decoding.
8. Directional capabilities, association lifecycle, TWT, sounding, rate
   control, and SU fallback pass focused tests.
9. Release/debug HE gates and shared-mode module tests pass.
10. Every changed fingerprint is explained and any update is separately
    approved.
11. The final documentation describes the model as a standards-correct
    packet-level PHY, not a bit-level, waveform, or certification
    implementation.
