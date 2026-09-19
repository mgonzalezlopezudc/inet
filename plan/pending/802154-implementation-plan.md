# Implement IEEE 802.15.4 in INET

Status: **in progress — step 0 complete; package 1a next**. The expanded applicability extraction (87 statements, including two imported address definitions) and nine English checks are
recorded in the [model coverage ledger](../../doc/project/evidence/model/ieee802154/coverage.md).
Independent review passed the bounded M1 applicability gate on 2026-09-19. No implementation package or executable conformance check is complete; deferred device-profile obligations remain owed.
Prepared against INET `c913a63a8335ca39a319cb2089daf835ea5e95cd` and the local
IEEE 802.15.4 survey on 2026-09-19.

Starting point: [initial gap analysis](802154-initial-plan.md). This document is the execution
plan; the initial analysis remains background. Implement the bounded profiles below before
extending coverage. “Correct” means satisfying the selected profile's applicable requirements,
with specification-derived tests and declared modeling limitations. It does not mean that every
PHY and optional mechanism in IEEE 802.15.4 has been implemented.

## 1. Scope and release milestones

Pin **IEEE Std 802.15.4-2024**, without subsequent amendments, using the existing
[source identity](../../doc/project/evidence/standard/ieee802154/source.md) and
[standards map](../../doc/project/evidence/protocol/ieee802154/standards.md).
The [IEEE register](https://standards.ieee.org/ieee/802.15.4/11041/) was checked on
2026-09-19; amendment implementation is a separate scope decision.

The following are project delivery profiles, not IEEE-defined conformance classes.

| Milestone | Selected behavior | Explicit limits | Release gate |
| --- | --- | --- | --- |
| M1: static non-beacon data transfer | 2.4 GHz O-QPSK, channel page 0/channels 11–26, 250 kbit/s; static PAN and native short/extended addressing; legacy frame versions 0/1 as applicable; data and immediate ACK; unslotted CSMA-CA; retries, filtering and DSN handling; PHY CCA, ED and LQI services | Unsecured operation; no dynamic association, periodic beacons, GTS, enhanced frames or ranging; packet-level PHY fidelity; an engineering milestone, not complete device conformance | Steps 0–6 and 10; every obligation in the M1 engineering subset has passing direct evidence; deferred profile obligations remain visible |
| M2: managed non-beacon PAN | M1 plus coordinator start, scan, association/disassociation, indirect transactions, polling, receiver control and selected orphan recovery/PAN conflict procedures | No claim of beacon-enabled operation or security | Steps 7–10; join, exchange, sleep/poll, failure and recovery scenarios pass |
| M3: beacon and secured operation | Beacon synchronization/superframes, slotted CSMA-CA, GTS/CFP, and a separately selectable base-standard security capability | Security and beacon mode have independent coverage; neither is implied merely by landing shared fields | Follow-on work packages B1–B3 and S1 |
| Later profiles | Enhanced frames/IEs, TSCH, DSME, additional PHYs and modern UWB/ranging | Each requires its own applicability matrix and validation | Follow-on packages E1, T1, D1, P1 and A1 |

Before implementing each profile, extend step 0 to reconcile its mandatory and conditional
requirements for the selected roles and modes; the first PR closes the bounded M1 audit. A necessary dependency discovered there joins the profile;
it cannot be excluded merely to meet this schedule. If needed for an M1 behavior, it blocks that
behavior until implemented; otherwise record its later delivery milestone without claiming profile
completion at M1. Until that closure is complete, use
“implementation of the selected subset,” not a whole-standard conformance claim.

In particular, 6.4.1.1 requires passive scanning for all devices. Its implementation is scheduled
in step 8, so it remains **owed** at M1 rather than being declared inapplicable to static PANs.
M2 is the first candidate for a complete selected-device-profile claim, subject to step 0's
mandatory-service inventory and the final evidence gate.

Maintain two explicit sets in the existing model coverage ledger: the **M1 engineering subset**
and **deferred selected-profile obligations**, including passive scan. Distinguish known deferred
implementation from unresolved applicability. M1 completion closes the first set only; a complete
selected-device-profile claim requires every applicable obligation to have passing direct evidence
and no unresolved applicability questions. M2 is a candidate, not a guarantee of that closure.

6LoWPAN, IPv6 neighbor discovery, routing and CoAP are separate projects. This plan supplies their
MAC address, payload and service boundaries; it does not promise an operational IPv6 stack merely
because MAC frames are correct. Native MAC test applications are sufficient for M1/M2 acceptance.

## 2. Corrections to the starting plan

1. Introduce PIB ownership and PHY service contracts **before** production MAC behavior uses them.
2. Ship each new wire chunk with its serializer, dissector/printer support and tests. Do not merge
   a packet model that has no byte representation; see
   [adding a protocol](../../doc/project/guide/add-a-protocol.md).
3. Implement the indirect-transaction and Data Request mechanism **before association**.
   In the selected association procedure, the coordinator delivers its response indirectly
   (2024, 10.21.2). An ACK to an Association Request is not successful association.
4. Distinguish representability, codec support and operational support. A parsed security header
   or IE is not an implemented security or TSCH feature.
5. Scope the two-octet FCS and 127-octet PSDU limit to M1/M2's PHY. Do not hard-code them as
   universal properties of all 2024 PHYs. Payload capacity depends on the actual MAC overhead.
6. Verify the sequence-wrap defect through a byte boundary. The current in-memory sequence
   field is wider than its serialized value; simulation-only exchanges may hide the mismatch.
   Do not prescribe a generic modulo ordering comparison as the entire duplicate algorithm.
7. Keep metadata local. Removing the Source PAN ID protocol-number workaround also requires
   a receive-side payload dispatch design; a sender's tag cannot act as an on-wire discriminator.
8. Retain the existing UWB composition as a separately documented legacy model. Replacing the
   narrowband MAC must not silently replace `AckingMac` on the UWB interface.
9. Use a device-wide DSN, not a counter per destination (6.6.1). Indirect delivery has a different
   retry rule from direct transmission: a failed attempt waits for another Data Request (6.6.3.4).

## 3. Evidence and current change surface

The existing [claim survey](../../doc/project/evidence/model/ieee802154/conformance.md) and
[coverage ledger](../../doc/project/evidence/model/ieee802154/coverage.md) establish Level 1
survey evidence only. Extend them rather than creating a competing status inventory.

The following observations were checked in this checkout; they are implementation facts, not
normative expectations for the new tests.

| Surface | Observed limitation | Planned owner/change |
| --- | --- | --- |
| [MAC header](../../src/inet/linklayer/ieee802154/Ieee802154MacHeader.msg) and [serializer](../../src/inet/linklayer/ieee802154/Ieee802154MacHeaderSerializer.cc) | Fixed `0xCC01`, padded 48-bit addresses, Source PAN ID carries `networkProtocol`; emitted header is 23 octets | Address value type in 1a; version-aware frame codec, steps 2–3 |
| [Narrowband MAC NED](../../src/inet/linklayer/ieee802154/Ieee802154NarrowbandMac.ned) | Fixed 72-bit header and 118-byte MTU derived from minimum overhead | Derive actual frame sizes; validate per-request payload capacity, steps 3–6 |
| [MAC implementation](../../src/inet/linklayer/ieee802154/Ieee802154Mac.cc) | ACK detection uses `CSMA-Ack`; reception-state CCA; host-integer sequence maps | Transaction, access and receive owners, steps 4–6 |
| [Dissector](../../src/inet/linklayer/ieee802154/Ieee802154ProtocolDissector.cc) | Payload decoding uses the nonstandard protocol field | Opaque payload or explicit receiver-side adaptation, step 3 |
| [Narrowband PHY](../../src/inet/physicallayer/wireless/ieee802154/packetlevel) | Existing transmitter/receiver/error model needs scoped timing and reception validation | PHY service adapter and O-QPSK profile, step 4 |
| [UWB interface](../../src/inet/linklayer/ieee802154/Ieee802154UwbIrInterface.ned) | Uses `AckingMac` | Preserve explicit legacy scope; modern UWB is P1 |
| [Serializer gaps](../../tests/serializer/REMAINING_GAPS.md) and [capture](../../tests/serializer/pcap/ieee802154.pcap) | Known frame-model blocker; capture exists but is not proof of codec support | Inspect provenance/link type and restore bounded capture coverage, step 3 |

Reuse `Protocol::ieee802154` and existing registrations. The PCAP reader already recognizes
link types 195 and 230 and attaches FCS-presence metadata; recognition alone does not prove
correct stripping, writing or replay. Test those paths explicitly.

### Normative lookup record

The local corpus was built for `802154-2024.pdf` and linted with the tracked
`inet-skills/bin/inet_process_standards` launcher. The PDF hash is in the source identity above.
These are normative clause entry points; a parent heading's page is **not** its complete scope.
Retrieve its children and referenced tables when deriving tests.

| Topic | Canonical node in `ieee802154-2024` | Physical PDF page(s) retrieved |
| --- | --- | --- |
| Unslotted access | `ieee802154-2024:clause:6.3.2.1` | 63–64 |
| Reception/rejection | `ieee802154-2024:clause:6.6.2` | 70–72 |
| ACK/retry entry | `ieee802154-2024:clause:6.6.3` | 72 |
| General format entry | `ieee802154-2024:clause:7.2` | 78 |
| Individual frame types entry | `ieee802154-2024:clause:7.3` | 86 |
| Constants/PIB entry | `ieee802154-2024:clause:8.4` | 151 |
| Association procedure | `ieee802154-2024:clause:10.21.2` | 356–358 |
| PHY services entry | `ieee802154-2024:clause:12` | 600 |
| O-QPSK entry | `ieee802154-2024:clause:13` | 614 |

The three extracted outgoing references from 6.3.2.1 resolved: 10.22.4.1 (Data Request),
10.11 (slotted access), and Figure 6-2. Corpus lint exited successfully but reported 169 rejected
false-heading candidates, one tiny node and 18 unresolved references across the document, plus
informational duplicate candidates. This is not a clean-reference or full-standard audit.
Figure 6-2 was visually inspected on physical page 64, and Table 7-2, including its continuation
and footnote, on physical pages 80–81. The flowchart's failure branch tests `NB >
macMaxCsmaBackoffs`, not equality. Step 0 still owes the remaining applicable figures/tables;
extracted flowchart text alone is insufficient. Outgoing references retrieved for 6.6.3.4,
10.21.2 and 11.2.8 all resolved. Resolution identifies a target; it does not mean the target's
requirements have all been implemented or audited.

### Source-derived constraints that must shape implementation

All clause/table IDs below belong to `ieee802154-2024`; their canonical form is
`ieee802154-2024:clause:<label>` or `ieee802154-2024:table:<label>`. These are bounded planning
findings from normative text, not the complete catalog to be produced in step 0.

| Source and physical PDF pages | Implementation consequence and required check |
| --- | --- |
| 6.4.1.1, pp. 64–65 | Passive scan is mandatory; scan channel order is ascending. Keep this obligation visible through the M1/M2 split and test restoration after scan. |
| 6.6.1, pp. 69–70 | Initialize sequence state through the simulation RNG; allocate a device-wide DSN, wrap at its representable limit, and keep beacon sequence spaces separate. Interleave two destinations in a test to expose the current per-destination counter model. |
| 6.6.3.3, pp. 72–73; 6.3.1, pp. 62–63 | ACK start timing and interframe spacing have separate rules. For this PHY the ACK timing uses `macSifsPeriod`; derive waits from the selected PIB/PHY values instead of retaining an unexplained fixed delay. Test long/short frames and acknowledged/unacknowledged exchanges. |
| 6.6.3.4, p. 73 | Direct retransmissions retain DSN and obey their retry limit. Failed indirect transmissions remain pending for another poll, retaining DSN; apply this to association responses too. |
| 7.2.2.6, p. 80; Table 7-2, pp. 80–81 | Legacy PAN compression rules and version-2 rules differ. In version 2, extended/extended with compression clear includes destination PAN only; with compression set it includes neither PAN. Test both and the table's short-address footnote when E1 lands. |
| 7.2.2.7–7.2.2.8, p. 81 | Legacy versions cannot enable sequence suppression or IEs. Reject unsupported combinations deliberately instead of silently changing offsets. |
| 7.3.3, p. 91; 6.6.2, pp. 70–72 | Immediate ACK layout lacks source/destination addresses; matching uses DSN and the outstanding exchange context. Version-2 traffic requires enhanced-ACK behavior rather than reusing immediate ACK blindly. |
| 7.2.11, pp. 84–85 | FCS covers MHR plus MAC payload. Use the standard's ACK CRC example as one independent vector, respecting transmission bit order; do not apply an Ethernet CRC preset. |
| 9.2.5, pp. 165–166 | An unsecured profile still defines security-policy behavior: with `macSecurityEnabled=false`, the unsecured incoming procedure succeeds. Secure inputs and attempts to enable unavailable security need explicit outcomes. |
| 11.2.8, pp. 596–597; Table 12-2, pp. 601–603 | CCA uses `phyCcaDuration` and `phyCcaEdThreshold`; a request during PPDU reception reports busy. M1 selects Mode 1 (energy detection); other modes are unsupported until separately implemented. |
| Table 8-36, pp. 152–156, especially p. 156 | Derive the default `macUnitBackoffPeriod` from turnaround plus CCA duration converted to whole symbols with upward rounding. A fixed 320 microseconds is not valid for every configured CCA duration. Test a nonintegral symbol conversion. |
| Table 12-1, p. 601; 13.1.2.2, p. 614; 13.1.3.2, p. 615 | O-QPSK turnaround is 12 symbols, preamble is 8 symbols, and PHR length counts the entire PSDU. Assert the duration calculation against bytes including MAC FCS. |
| 10.21.2, pp. 356–358 | Association ACK is not admission; response delivery uses indirect transactions. Keep higher-layer admission/address-allocation policy separate from the MAC exchange. |

The retrieved transmission/ACK clauses do not specify a general-purpose duplicate-cache algorithm.
6.6.1 explicitly limits the usefulness of the 8-bit DSN; 8.3.6 exposes it in the data indication.
Step 0 must establish any further applicable duplicate obligations. Until then, treat suppression
as a separately documented model/adaptation policy, not an invented standard requirement.
DSN alone cannot guarantee exactly-once application delivery across wraparound or peer restart.

## 4. Architecture and ownership decisions

Follow the current [project map](../../doc/project/README.md),
[protocol anatomy](../../doc/project/design/protocol-anatomy.md),
[packet anatomy](../../doc/project/design/packet-anatomy.md) and
[architecture rules](../../doc/project/rule/architecture.md). Proposed subpackages below are
implementation destinations, not existing APIs. Settle concrete names and paired C++/NED
contracts in step 1 using the [naming rules](../../doc/project/rule/naming.md).

| Owner | State and responsibility | Boundary |
| --- | --- | --- |
| Protocol-local address/frame values | None/short/extended address, FCF fields, PAN fields, command payloads, optional wire structures and FCS | `linklayer/ieee802154`; independent of MAC timers and radio implementations |
| MAC PIB | Validated MAC attributes, defaults and access restrictions | One authoritative store; no duplicate NED/management/runtime copies |
| PHY service provider | Channel/page, transceiver operation, CCA/ED, transmission timing, receive metadata | Paired contracts in PHY-owned `contract`; implementation under `physicallayer/wireless/ieee802154`; no dependency back to concrete MAC |
| MAC transaction controller | Explicit operation type, current request, DSN, ACK matching, retry count, exactly one terminal result | Calls access and PHY contracts; owns packet lifetime through completion |
| Unslotted/slotted access provider | Per-attempt backoff state and timers | Returns access success/failure; does not own retries, association or upper-layer policy |
| Receive processing | Length/FCS/filter checks, ACK eligibility and delivery; optional duplicate policy has explicit ownership | Single receive decision path for typed and byte-backed packets |
| Management | Scan, start, association/disassociation and recovery procedures | Uses shared transmission services; PAN choice/address-allocation policy is supplied by a higher-layer client |
| Pending transaction store | Per-device queued data/commands, expiration and purge | Shared by association and indirect data; no second association-only queue |
| Security provider, later | Key/device state, counters and security transformation | Defined hooks and ownership early; implemented only with S1 evidence |

Queueing uses existing queue contracts. Do not create a submodule for every value type; use
replaceable modules for independently selectable behavior and plain values for packet/state data.
Keep management and channel access out of one expanding MAC FSM. Specify separate direct,
indirect-delivery, poll and association operation lifetimes and completion events. Direct retry
exhaustion, indirect retention for another poll, poll response waiting and association completion
must not share an implicit success/failure rule. Decide whether to share an exchange engine only
after these contracts are explicit; no particular policy-class hierarchy is required.

Every asynchronous operation has a request identity, result/status, packet ownership rules,
cancellation behavior and lifecycle semantics. PHY busy, unsupported operation, invalid request,
channel-access failure, no ACK and successful completion must remain distinguishable. All timers
have one owner; late callbacks after cancellation or stop cannot complete a replacement request.

**Address integration:** native 16/64-bit identity lives in the protocol. Use protocol-specific
request/indication tags and attached interface data where existing extension points permit it.
Do not widen `MacAddress`, truncate EUI-64, or add protocol switches to the core. In step 1d,
before codec and operational MAC work, prove that two extended addresses sharing their low 48 bits remain distinct through the actual
interface. Any framework contract gap is a separate design decision with its own change surface.
Produce a compatibility matrix for interface identity, upper destination requests, lower source
indications, queue classification, neighbor/forwarding lookups, filtering, display/configuration,
and generic consumers expecting `MacAddress`. Mark each as supported, adapted or outside M1;
name the actual extension point and required evidence for supported paths. M1 need not make every
generic upper layer compatible, but its native MAC application path must work end to end.

**Payload integration:** M1 accepts opaque MAC service payloads and optionally one configured
upper protocol per interface. The receiver supplies that protocol tag for delivered data payloads;
without configuration, payloads remain opaque and use the native MAC service boundary. Unknown
capture payloads stay opaque. Mixed upper protocols on one interface are unsupported until an
adaptation-layer discriminator exists. The MAC and dissector must use the same context rule.

**PIB mutation:** step 1b defines each attribute's access, startup/runtime mutability, validation,
cross-attribute constraints, reset default and retention, effective-change event and notifications.
NED parameters initialize the authoritative store; they are not a competing runtime value. Define
how MLME-SET interacts with active operations and reject unsupported changes with explicit status.
The owning procedure applies operational changes after validation; the store must not reach into
unrelated modules. Preserve consistent state across failed changes. Add atomic multi-attribute
operations only if a supported procedure requires them, with explicit commit/failure semantics.

**Frame order:** follow the selected version's layout: MAC header (including applicable auxiliary
security/header IEs), MAC payload (including applicable payload IEs in their specified position),
then FCS. Do not append payload IEs after application data merely because the initial sketch did.

**Protected surface:** `src/inet/common/packet/` is sealed in the
[registry](../../doc/project/audit/seal-list.md). Protocol serializers and dissectors can register
outside it. If capture integration exposes a core defect, produce the reproduction and a bounded
repair proposal, then follow the current seal procedure before editing that path. This plan does
not grant permission to edit sealed code.

## 5. Ordered implementation work

Numbered steps are roadmap stages, not promises of one PR each. Split them at the gates below,
with a bounded change surface and direct checks for each PR. Land dependencies first; every PR
must build and keep its selected operational mode coherent. The owner column in section 4 names
responsibility, not staffing assignments.

### Step 0 — Close the profile and write the checks

Dependencies: none. Source baseline and local survey already exist; refresh them if HEAD moves.

Execution record: [applicability audit](../../doc/project/evidence/model/ieee802154/applicability.md)
and [inspection evidence](../../doc/project/evidence/model/ieee802154/results.md). The bounded
M1 audit passed independent review; the [reference dispositions](../../doc/project/evidence/model/ieee802154/dependencies.md)
record its closure boundary. This does not close the later M2 audit or any runtime gate.

- [x] Extract the selected normative statements into
  `doc/project/evidence/standard/ieee802154/catalog.md`, including conditions, revision, clause,
  physical page/source span and cross-references. Inspect ambiguous tables/figures in the PDF.
- [x] Create protocol-only `features.md`, `checks.md` and focused check descriptions under
  `doc/project/evidence/protocol/ieee802154/`, following
  [standard-derived tests](../../doc/project/guide/derive-tests-from-a-standard.md).
- [x] Pin the first audit to M1's static non-beacon O-QPSK data service and its necessary
  dependencies. Declare which device, coordinator and PAN-coordinator roles are exercised;
  distinguish configured roles from support for their management procedures. Record exposed
  services, selected CCA mode, conditional predicates and unsupported PIB requests/statuses.
- [x] Record the M1 engineering subset and known deferred profile obligations in the model ledger.
  Follow applicable cross-references to closure for the bounded M1 claim. Extend the audit to M2
  roles/services before steps 7–9; the first PR need not close unrelated M2 procedures. Keep
  implementation status and outcomes out of the standard catalog and protocol feature map.
- [x] Produce a version matrix separating transmitted, structurally decoded and operationally
  processed formats, legal-but-unsupported combinations and malformed/reserved combinations.
  Resolve version 0/1 selection before codec work. Version-2 recognition must never imply
  legacy-layout parsing or operational support; E1 remains the implementation gate.
- [x] Derive a receive decision table from the applicable clauses: for each rejection or acceptance
  cause, record ordering, promiscuous visibility, ACK eligibility, DSN indication, delivery,
  duplicate-state effects and status/statistic. Include malformed security fields, valid secured
  but unsupported traffic and unsecured traffic; add companion outcomes for local requests
  asking for unavailable security. Resolve the table before step 6; do not reduce ACK eligibility to one generic accepted flag.
- [x] Record apparent source contradictions with exact clauses/figures and resolve their
  interpretation before turning them into timing oracles. Do not silently substitute an older
  edition, an extracted table fragment or another simulator's algorithm.
- [x] Audit existing examples, callers, address tags, PCAP paths, `.oppfeatures` and fingerprints;
  identify migration consumers and capture provenance/admission. Suspected defects remain assigned
  to separate production-path reproductions; source arithmetic is not reported as a reproduced run.

**Exit:** a reviewer can determine the profile's obligations without reading implementation code.
Every applicable obligation in the bounded audit has a planned observable check and delivery
milestone; exclusions have explicit predicates and source justification. The bounded M1 audit has
zero unresolved applicability questions or unclassified necessary references. Known implementation
debt remains visible and does not count as an applicability exclusion. Unresolved questions for
later procedures block those procedures and any complete-profile claim, not unrelated M1 work.
A machine-readable coverage export, if useful, is derived from the ledger rather than maintained
as another source of truth.

### Step 1 — Fix contracts, state ownership and migration boundaries

Dependencies: step 0. Deliver as separate bounded PRs; each builds with the existing default.

| Package | Deliverable and exit gate | Dependencies |
| --- | --- | --- |
| 1a: protocol values and MAC services | Native none/short/extended address type, parsing/formatting, equality/hash and reserved values; MCPS and minimal MLME primitives, statuses, request identity, ownership and cancellation. Value/contract tests cover refusal and distinct identities. | Step 0 |
| 1b: MAC PIB | Authoritative store plus the attribute mutation table specified in section 4; initialization, validation, reset and failed-change tests. | 1a |
| 1c: PHY contracts | Paired substitutable contracts, units, status/lifecycle rules and event/timer ownership table below. Contract tests distinguish acceptance from completion and stale callbacks. | 1a |
| 1d: address and payload integration | Compatibility matrix and minimal production interface fixture, including two EUI-64 identities with identical low 48 bits, request/indication identity and configured/opaque delivery. Resolve required adapters and framework gaps. | 1a–1c |
| 1e: selectable composition | Public type names, paired C++/NED contracts and explicit replacement selection. Prove existing default initialization and replacement contract wiring; isolate old wire assumptions. | 1d |

Record these decisions before dependent behavior, without requiring all contracts to land in one
PR. No concrete PHY-to-MAC dependency or protocol switch in core is permitted.

The PHY event/timer table must define request acceptance, PPDU start, PSDU/FCS completion, PPDU end,
turnaround completion and receive indication, including timestamp reference points and result
availability. Assign one owner to CCA/ED observation, turnaround, backoff, immediate-ACK scheduling,
interframe spacing, ACK timeout and transmit completion. Specify which events start each wait,
which intervals overlap, units/rounding and cancellation/reset behavior. Resolve exact assignments
against the selected clauses before step 4; no timing implementation may rely on an unassigned row.

**Exit:** all five package gates pass; validation, refusal ownership, reset/stop and distinct
operation results are explicit. Step 2 consumes the address type already proven by 1d.

### Step 2 — Basic frame codec using native addresses

Dependencies: step 1.

- [ ] Use the step 1a address type and step 0 version matrix; carry broadcast and
  unallocated-address semantics through the actual wire boundary.
- [ ] Add the M1 FCF/address/sequence representation and registered serializers together.
  Derive the version-dependent address/PAN presence table from the specification.
- [ ] Support data and immediate ACK layout, variable lengths and exact octet order; distinguish
  unsupported legal formats from malformed inputs. Do not use `ASSERT` for untrusted wire input.
- [ ] Establish the extension boundary for security and IEs. Until their codec work lands,
  reject unsupported combinations explicitly rather than consuming them as ordinary data.

**Exit:** independent golden bytes for every supported addressing/PAN-presence combination,
high EUI-64 bits, sequence boundaries and ACK format; truncated/reserved inputs terminate safely.
Typed-to-bytes and bytes-to-typed checks agree on consumed length and field presence.

### Step 3 — Complete MPDU, FCS and capture tooling

Dependencies: step 2.

- [ ] Add the selected PHY's MAC FCS trailer and CRC with independent known-answer vectors;
  derive total MPDU length from actual chunks. Reject oversized PSDUs before transmission.
- [ ] Remove `networkProtocol` from the new wire format; finish opaque/configured payload
  handling in the MAC and dissector. Add printer/filter visibility and capture registration.
- [ ] Exercise both with-FCS and no-FCS capture input and output through production tooling.
  Define absent, computed, invalid and simulation-declared FCS handling. Never invent captured
  bytes to make a truncated packet round-trip; preserve supplied invalid FCS in forensic decoding.
- [ ] Restore supported captures to the serializer suite, recording provenance, hashes, frame
  selection and external decoder version. Compare MPDU bytes, separately from PCAP timestamps
  and container metadata. Use independent decoded fields as well as round trips.

**Exit:** supported MPDUs round-trip byte-for-byte; generated captures decode independently;
packet length, FCS coverage, PHY PSDU length and observed bytes agree. No sender metadata is
required to decode captured payload boundaries. Core capture defects are isolated if encountered.

### Step 4 — O-QPSK PHY services before MAC timing

Dependencies: steps 1 and 3. A read-only feasibility investigation or isolated fixture may run
once 1c is defined, before complete MPDU integration.

- [ ] Establish a CCA/ED feasibility gate: identify the existing medium/radio energy observation
  API and demonstrate energy detection without a decodable frame. If insufficient, define and
  validate a bounded PHY-local listening/energy contract before promising Mode 1 behavior.
  Treat insufficiency as an investigated result, not an assumption about the current medium.
- [ ] Implement the PHY provider over the existing narrowband radio infrastructure: timed CCA,
  ED, state changes, channel/page selection, PD-DATA completion and receive metadata/LQI.
- [ ] Derive symbol timing, SHR/PHR/PSDU duration, turnaround and size checks from the selected
  O-QPSK mode. Account for each PHY/MAC overhead exactly once.
- [ ] Implement and advertise CCA Mode 1, observation duration and thresholds;
  unsupported modes fail explicitly. Test energy/interference without a decodable frame, not
  just the radio's current reception-state enum.
- [ ] Define ED/LQI mapping and saturation with documented model limits. Validate timing and
  channel selection independently of the existing generic BER/PER formula.

**Exit:** module tests cover successful/refused/interrupted operations, channel switching,
CCA threshold/time boundaries, ED bounds and LQI delivery. A PHY timing oracle matches
generated transmissions. PER claims await the separate validation gate in step 10.

### Step 5 — Unslotted access and transmission lifecycle

Dependencies: steps 3–4.

- [ ] Implement specification-derived NB/BE initialization, random backoff range, busy-CCA
  updates, exponent cap and access-failure boundary, using a named simulation RNG stream.
- [ ] Wire access success to real PHY transmission and completion. Separate access attempts
  from frame retransmission attempts; handle zero backoff and configured limit boundaries.
- [ ] Define queue admission, refusal, cancellation and one terminal MCPS result per request.
  Handle stop/crash/reset during backoff, CCA and transmission; cancel owned timers.

**Exit:** deterministic production-path traces match the clause-derived state transitions and
symbol times for idle, persistently busy and busy-then-idle channels. No transmission follows
an access failure, and no request is completed twice or stranded.

### Step 6 — Receive filtering, ACKs, retries and DSN handling

Dependencies: step 5.

- [ ] Apply length/FCS, version, frame type, PAN/address and receive-mode checks in the required
  order from step 0's decision table, including promiscuous behavior. Test ACK eligibility
  independently of upper delivery and optional suppression.
- [ ] Implement unsecured-profile security-policy handling through the applicable branches
  (9.2.2 and 9.2.5) with `macSecurityEnabled=false`; reject unsupported configuration and
  secured traffic with defined outcomes. This does not implement cryptographic security.
- [ ] Generate real immediate ACKs with specified timing and transaction matching. Cover
  ACK-disabled traffic and broadcasts; a packet name never determines frame type.
- [ ] Implement ACK timeout/retry exhaustion and interframe timing. Keep DSN stable across a
  retransmission; allocate new DSNs device-wide with the specified width and ownership (6.6.1).
- [ ] Deliver DSN and source identity through the data indication. Resolve duplicate policy from
  step 0's evidence. Unless an applicable requirement is found, suppression is disabled by
  default and remains outside the conformance claim. If retained as an opt-in policy, document
  its cache key, lifetime, reset behavior and ambiguity limits. It must not prevent required ACKs.
  Test wraparound, peer restart, interleaved destinations, late ACKs and unexpected ACK sequences.

**Exit / M1 candidate:** two real interfaces exchange addressed data through the real radio;
loss injection proves retries and the declared indication/suppression policy; a serialized 254→255→0→1 sequence remains
usable. Run the same cases on typed and byte-backed frames. Complete step 10 before releasing M1.

### Step 7 — Command frames and indirect transaction engine

Dependencies: step 6 and closure of the M2 applicability extension from step 0.

- [ ] Add each selected command payload with codec and independent vectors; include Data
  Request and association/disassociation commands required by M2.
- [ ] Implement pending transactions keyed by native device identity, expiration, bounded
  queue capacity, purge and ownership. Association responses and data use the same store.
- [ ] Implement polling, Frame Pending behavior, empty response/no-data behavior and the
  specified data-release timing/access rules. Implement receiver-enable/sleep coordination.
- [ ] Keep a failed indirect frame pending for the next Data Request, retaining its DSN;
  do not route it through the direct-transmission retry loop (6.6.3.4).

**Exit:** poll with zero/one/multiple pending transactions; expiration at a poll boundary;
lost poll ACK/data ACK; queue refusal; two sleeping devices. No duplicate dequeue, delivery
to the wrong device, or success confirmation before its defining event.

### Step 8 — PAN start, scanning and recovery primitives

Dependencies: steps 4, 6 and 7.

- [ ] Implement non-beacon coordinator start and selected beacon/command generation for scan
  responses. A scan-response beacon does not imply periodic beacon support.
- [ ] Add ED, active and passive scan contracts, channel iteration, durations, PAN descriptor
  collection/deduplication, cancellation and restoration of radio/PIB state.
- [ ] Add the selected orphan scan/realignment and PAN conflict procedures with their own
  command fields, timeout outcomes and scope. Keep beacon-loss synchronization for B1.
- [ ] Keep PAN selection policy outside the MAC; expose sufficient descriptors/status to a
  small management client used by examples and tests.

**Exit:** discovery on multiple configured channels, no coordinator, duplicate responses,
partial/invalid channel sets, scan interruption, and state restoration all have direct evidence.

### Step 9 — Association, disassociation and managed operation

Dependencies: steps 7–8.

- [ ] Implement reset/scan/select/associate orchestration and device/coordinator procedure
  states. Higher-layer policy decides admission and allocates unique permitted addresses.
- [ ] Deliver Association Response through the pending store and device polling procedure.
  Apply the specified response wait and failure semantics; do not infer success from request ACK.
- [ ] Support associated devices without an allocated short address, rejection, timeout,
  repeated association and stale responses; keep identities distinct across PANs.
- [ ] Implement both selected disassociation directions and cleanup of pending transactions,
  duplicate state and timers. Test reassociation and recovery after interruption.

**Exit / M2 candidate:** an initially unassociated device discovers a PAN, joins, exchanges
data, sleeps, polls, leaves and rejoins. Failure cases yield the specified status and consistent
PIB/state. Complete step 10 again for the expanded M2 claim.

### Step 10 — Validate, migrate and publish each milestone

Dependencies: step 6 for M1; step 9 for M2. Prepare fixtures and evidence throughout the series.

- [ ] Close every selected obligation in the coverage ledger with test identity, exact command,
  result, revision and artifacts; leave unimplemented obligations visible.
- [ ] Add external validation for timing and the chosen PHY reception/error abstraction.
  Specify modulation, channel/noise assumptions, frame length, reference source and tolerance
  before comparing results. Correct or narrow unsupported PER claims.
- [ ] Inventory old consumers and migrate examples deliberately. At M1 cutover, replace the
  default narrowband path only after migration tests pass; document removed/changed parameters,
  addresses, MTU and wire incompatibility. Retire the temporary old narrowband path on a stated
  schedule: retain an explicitly named legacy narrowband type for the first release shipping
  the replacement as default, deprecate it in that release, and remove it no earlier than the
  following release after inventoried consumers are migrated and regression gates pass.
  Set concrete type names in 1e and release identifiers at cutover. Do not add silent address,
  wire-format or parameter conversions; document any explicit adapter's limits. This retirement
  policy excludes the separately retained legacy UWB composition. Do not reinterpret old
  captures as standard frames.
- [ ] Update `.oppfeatures`, examples, user/migration documentation and `WHATSNEW`; distinguish
  packet-level abstraction from waveform fidelity and M1/M2 from whole-standard support.
- [ ] Run relevant regression gates and feature-disabled builds. Attribute every changed
  fingerprint to the behavior that moved it; retain the known UWB baseline or investigate changes.

**Exit:** reproducible evidence bundle, migration instructions and a profile-specific support
statement. Missing checks, setup failures and unsupported obligations remain explicit blockers
to the corresponding claim, even if the broad regression suite passes.

## 6. Later feature series and their gates

These are scoped follow-on packages, not promises hidden inside M1/M2. Before starting one,
repeat step 0 for its requirements and expand it into PR-sized tasks.

| Package | Prerequisites | Work and acceptance gate |
| --- | --- | --- |
| B1: beacon scheduler | M2 | Periodic beacons, BO/SO, active/inactive intervals, synchronization and loss; assert symbol boundaries, drift/reacquisition and pending-address advertisement |
| B2: slotted access | B1 | Slotted algorithm, contention window and CAP fit/defer rules; prove boundary behavior when a complete exchange cannot fit and across inactive intervals |
| B3: GTS/CFP | B2 | Request/allocation/deallocation, descriptors, direction, expiration and scheduling; prove exclusive scheduled use and return to contention after removal |
| E1: modern frame machinery | M1 codec/contracts | Version-2 address/PAN rules, sequence suppression, bounded Header/Payload IE parsing, termination rules and unknown IE preservation; enhanced ACK/beacon behavior requires separate production exchanges |
| S1: base MAC security | M1; E1 for enhanced-frame claims | Auxiliary header, key/device/security-level state, AES-based operations, authenticated fields, nonce/counter rules, replay protection and status handling; independent crypto vectors plus secured exchanges, tampering, counter exhaustion and reset/persistence tests |
| T1: TSCH | E1, PHY switching/timing, selected S1 joining policy | ASN, slotframes/links, templates, hopping, shared/dedicated links, enhanced beacons, synchronization/drift and keepalive; fixed schedule first, pluggable scheduling policy later; test slot/channel oracles and synchronization loss |
| D1: DSME | E1 and relevant beacon/security services | Separate scheduler, multi-superframes, allocation commands and conflict recovery; test simultaneous allocation, expiration and channel use |
| P1: additional PHYs/UWB | Stable PHY service contract and selected common MAC | One PHY/mode per profile; independent PPDU/timing/reception validation. Modern ranging adds timestamps, ranging exchanges and a ranging-error model; legacy UWB constants are not proof |
| A1: amendments | Applicable base capability and locally retrieved amendment | Pin edition and override map before work; Ascon and privacy remain independent additions with their own evidence |

For S1, specify the incoming/outgoing processing order from clause 9, including when authentication
precedes state mutation/delivery, when ACKs are permitted, how encrypted bytes are represented,
and how FCS covers the transmitted frame. The initial plan's short security pipeline is not a
complete procedure. A simulated “security enabled” flag cannot establish cryptographic support.

## 7. Minimum test matrix

Use [test categories](../../doc/project/design/test-anatomy.md) and the
[protocol authoring guide](../../tests/protocol/lib/AUTHORING.md). Proposed case names below
are work items, not existing tests or runnable commands. Allocate actual paths in each PR.

| Case family | Category | Required observable evidence |
| --- | --- | --- |
| `Ieee802154Address`, `Ieee802154FrameWire` | Unit/serializer | Native identity; independent octets; supported presence matrix; malformed/truncated inputs; exact lengths |
| `Ieee802154FcsCapture` | Serializer plus production recorder/replay fixture | CRC oracle; link types with/without FCS; unchanged supported MPDU bytes; external decoded fields |
| `Ieee802154PhyServices` | Module | Timed CCA/ED/state results, channel changes, LQI, cancellation and refusal |
| `Ieee802154UnslottedAccess` | Module and protocol | Backoff/CCA trace, attempt limits, RNG reproducibility and actual transmit/no-transmit outcome |
| `Ieee802154DataAck` | Protocol | Fields/times, ACK loss, retry exhaustion, broadcast, wrong/late ACK, filtering, declared delivery policy and one terminal request result |
| `Ieee802154SequenceWrap` | Protocol with byte boundary | More than 256 transfers; retries near wrap; peer restart and two peers without false duplicate suppression |
| `Ieee802154Lifecycle` | Module and protocol | Reset/stop/crash at each asynchronous state; no leaked packet, stale timer or double completion |
| `Ieee802154Indirect`, `Ieee802154Association` | Protocol | Poll/response/ACK ordering, expiration, admission failure, no-short-address join and leave/rejoin |
| `Ieee802154Scan` | Protocol | Selected-channel coverage, durations, descriptors, cancellation and restored configuration |
| O-QPSK timing/PER | Validation | Independent duration calculations and a documented external reception reference with uncertainty |
| Feature isolation and migrated examples | Features/networks plus focused fingerprints | Build without optional siblings; examples initialize; explained trajectory changes |

Use fixed seeds for deterministic exchange checks. For contention robustness use an explicitly
bounded campaign, initially 20 seeds × 2 and 5 contenders, and require the same safety/termination
invariants in every run; this is not a throughput/fairness claim. For PER validation select SNRs
and packet lengths from the reference, use independent repetitions and predeclare sample size,
confidence interval and acceptance tolerance in step 10. Do not rerun failing seeds until green.

All protocol checks enter production gates/services. Helper tests alone cannot establish that the
MAC calls the helper with correct units, addresses or timing. Capture round trips alone cannot
detect a symmetric encoder/decoder mistake. Cross-model comparisons can supplement the standard
and independent vectors, but another simulator's behavior is not a normative oracle.

## 8. Per-PR verification and completion record

Follow [run the gates](../../doc/project/guide/run-the-gates.md). For each implementation PR:

1. Record changed paths/contracts, normative check IDs and explicitly selected test cases.
2. Build a fresh debug library before behavioral tests; compile release before push. Use the
   relevant runner's documented selector, verified with its current help, and record selected
   versus executed counts. Zero selected cases are `NOT_RUN`.
3. Run scoped architecture/naming/interface checks and the semantic review checklist; then the
   prescribed pre-push gates. Verify feature isolation when feature dependencies change.
4. Record command, working directory, build mode, HEAD/worktree state, configuration, seed,
   expected invariant, verdict and artifact path. Update model results/coverage, not normative text.
5. Explain baseline changes under the
   [baseline procedure](../../doc/project/guide/change-a-baseline.md); ship evidence and release
   notes with the behavior. Keep mechanical renames separate from behavioral commits.

Commands already documented in this checkout include:

```sh
make -j$(nproc) MODE=debug
doc/project/enforcement/check-architecture.sh src/inet/linklayer/ieee802154
doc/project/enforcement/check-architecture.sh src/inet/physicallayer/wireless/ieee802154
doc/project/enforcement/check-naming.sh src/inet/linklayer/ieee802154
doc/project/enforcement/check-interfaces.sh
# Run the explicit unit/module/protocol selectors recorded by the implementing PR.
make -j$(nproc) MODE=release
```

These are future implementation gates, not tests executed while writing this plan.

## 9. First implementation PR

The initial **step 0** package closes bounded M1 applicability, authors its normative catalog and
English checks, and maps them to the model ledger. That audit gate has passed; executable checks
remain NOT_RUN. The documented source findings on wire format/length and sequence truncation
remain candidates for targeted production reproductions. This documentation package changes no
simulation behavior or fingerprints.

The next PR is 1a; packages 1b–1e establish the remaining contracts and prove native-address
integration before the codec. Steps 2–6 then deliver M1, with step 10 as its release gate.
Keep each new wire chunk paired with its serializer and vectors. Within step 3, separate complete
MPDU/FCS proof from capture integration where independently buildable. Within step 6, separate
receive decisions/unsecured policy, immediate ACKs, and direct retry/DSN lifecycle into coherent
PRs with explicit dependencies and production-path evidence. This does not postpone integrated
exchange checks until release. This is the critical path; association, security and scheduled
MAC modes must not delay establishing a correct, independently observable basic frame exchange.
