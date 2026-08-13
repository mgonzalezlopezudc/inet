The work should be split into four independently testable changes. The key correction is that the second candidate is not “remove a redundant duplicate check”: IEEE 802.11 permits multiple matching Per-AID/TID records, so the implementation must process them deterministically.

No files were changed while preparing this plan.

## Governing design decisions

| Area | Intended outcome |
|---|---|
| Receiver Trigger validation | Stop revalidating all AP construction rules. Retain only safe parsing, local feature/state gates, selected-user capability checks, and final PHY validation. |
| Multi-STA BlockAck records | Process every record relevant to the active single-TID exchange; do not classify multiplicity as malformed. |
| Unassociated UORA | Support AID12=2045 from Trigger generation through one management S-MPDU response and preassociation Multi-STA BlockAck. |
| Existing behavior | Associated UORA remains AID12=0 and is unchanged by default. Foreign scheduled Triggers, stale responses, wrong correlation IDs, and capability-incompatible allocations remain rejected. |

The normative anchors are IEEE Std 802.11-2024 clauses 26.5.2.2.4, 26.5.2.3.2, 26.4.2, 26.5.4.2, 26.5.4.5, 26.11.1, Table 9-52, and 9.3.1.8.6.

## Phase 0 — Baseline and architecture gate

1. Recheck the sealing status immediately before implementation. The currently identified files are unsealed, but that must be verified again.

2. Run the current focused unit and module tests before editing and retain their output as the behavioral baseline.

3. Apply these architecture requirements throughout:

   - `AR-WLAN-STD-TRACE` and `AR-WLAN-STD-GATING`
   - `AR-WLAN-ARCH-OWNERSHIP`
   - `AR-WLAN-FRAME-REPRESENTATION`
   - `AR-WLAN-PHY-AUTHORITY`
   - `AR-WLAN-MAC-EXCHANGE`, `MAC-SEQUENCE`, and `MAC-MULTIUSER`
   - `AR-WLAN-QUAL-TESTS`
   - `AR-PKT-TAGS`, `AR-QUAL-DETERMINISM`, and `AR-QUAL-TRACEABILITY`

## Phase 1 — Remove exhaustive receiver-side Trigger validation

The production receiver currently calls `validateIeee80211HeUlTrigger()` from `parseTrigger()` in [HeTriggeredUlExchangeService.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeTriggeredUlExchangeService.cc). Its definition is in [HeHcfUl.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfUl.cc), with a public declaration in [HeHcf.h](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h).

### Implementation

1. Remove the `validateIeee80211HeUlTrigger()` call from `parseTrigger()`.

2. Replace it with a small private receiver-envelope check that covers only model safety:

   - supported Trigger type;
   - decoded channel width matching the receiving link;
   - representable, positive HE-TB duration;
   - NFRP fields needed before calculating a response resource;
   - no indexing or arithmetic on an unselected User Info record.

3. Do not globally reject a Trigger because another User Info record has:

   - invalid or overlapping RU geometry;
   - a duplicate scheduled AID;
   - an invalid stream allocation;
   - a bad MCS/NSS/FEC combination;
   - an AP-side-invalid GI/LTF or power setting.

   Those are AP construction obligations under 26.5.2.2.4.

4. Continue validating the selected User Info record through:

   - the local feature and capability predicate in `processTrigger()`;
   - `createHeTbTxVector()`;
   - final PHY TXVECTOR construction before transmission.

5. Keep these existing gates:

   - local non-AP role;
   - HE UL/UORA feature enablement;
   - NDP feedback capability;
   - TWT sleeping state;
   - active exchange exclusion;
   - selected-user RU/MCS/NSS/LDPC/MU-MIMO support;
   - the standard’s mandatory “shall not respond” conditions where represented by the model.

6. Remove the exported validator declaration and definition after confirming no supported external API compatibility rule requires it. It has no in-repository production caller after step 1. Because this is source/ABI-visible, record the removal in the release notes. If API policy forbids immediate removal, retain a deprecated compatibility helper for one cycle, but keep it out of receiver processing.

### Tests

Update [Ieee80211HeUlControlFrames_1.test](tests/unit/Ieee80211HeUlControlFrames_1.test):

- remove direct receiver-validator tests for duplicate AIDs, global RU overlap, and MU-MIMO stream geometry;
- preserve equivalent rejection tests at `HeUlMuPlan`/AP finalization or TXVECTOR construction;
- retain one receiver-envelope test for a genuinely unsafe decoded core field.

Update [Ieee80211HeUlMuTransaction_1.test](tests/unit/Ieee80211HeUlMuTransaction_1.test):

- a Trigger containing an irrelevant unsupported User Info record must not globally fail;
- an unsupported selected allocation must produce no response;
- NAV processing must remain independent of response eligibility.

## Phase 2 — Correct and simplify Multi-STA BlockAck processing

The affected code is `processMultiStaBlockAck()` in [HeTriggeredUlExchangeService.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeTriggeredUlExchangeService.cc).

### Implementation

1. Preserve all pre-record correlation checks:

   - Trigger correlation tag and ID;
   - response deadline;
   - transmitter/exchange peer;
   - association identity and epoch for associated exchanges.

2. Replace `matchingRecords` cardinality rejection with collection of all relevant records.

3. For an ordinary single-TID data exchange:

   - select every record matching the exchange AID and TID;
   - for each outstanding MPDU, consider it acknowledged if any matching record contains its sequence number in its bitmap window;
   - retire each acknowledged packet once;
   - retry each unacknowledged packet once;
   - ignore unrelated AID/TID records.

4. For compressed-BAR recovery:

   - select records matching AID, requested TID, and requested starting sequence number;
   - combine bitmaps from records with that exact session identity;
   - ignore records belonging to another starting sequence/window;
   - report failure only when no applicable response record exists.

5. Do not label multiple records as malformed. Add a comment citing 26.4.2, which requires the originator to examine each Per-AID/TID field.

6. Keep the current modeled 64-bit bitmap restriction explicit. Variable negotiated bitmap lengths are a separate feature.

### Tests

Change the duplicate-record case in [Ieee80211HeUlMuTransaction_1.test](tests/unit/Ieee80211HeUlMuTransaction_1.test):

- two matching records must terminate the exchange deterministically;
- the union of their bitmap results must be applied;
- packets must never be retired or retried twice;
- unrelated packets must remain untouched.

Keep the existing wrong-trigger, late-response, foreign-transmitter, and reassociation-epoch tests unchanged.

## Phase 3 — Implement unassociated UORA end to end

Simply relaxing the BSSID and AID checks would be incorrect. The current path assumes associated data, QoS Null responses, negotiated peer capabilities, association-scoped queues, and associated BlockAck records.

### 3.1 Add an explicit RA-RU target

In [IIeee80211HeUlScheduler.h](src/inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeUlScheduler.h):

1. Add a typed random-access target, for example:

   - `ASSOCIATED_STAS`
   - `UNASSOCIATED_STAS`

2. Store this target in each random-access `RuAllocation`. Do not represent an unassociated allocation by pretending `associationId == 2045`; 2045 is a wire identifier, not a real association ID.

3. Have Trigger construction map:

   - associated target → AID12=0;
   - unassociated target → AID12=2045.

Update [HeUlMuPlan.h](src/inet/linklayer/ieee80211/mac/framesequence/HeUlMuPlan.h) to validate both forms separately.

Add a scheduler parameter such as `randomAccessTarget`, defaulting to `"associated"`, to preserve all existing configurations. Update the two UL scheduler implementations to populate the typed target.

### 3.2 Separate association state from Trigger-peer state

Refactor snapshots in [HeTriggeredUlExchangeService.h](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeTriggeredUlExchangeService.h):

- associated BSSID;
- Trigger transmitter/AP address;
- explicit associated/unassociated state;
- optional local association ID rather than a zero or wrapped sentinel;
- association epoch only for associated exchanges;
- response STA-ID;
- BSS color obtained from the received HE PPDU;
- local HE transmit capabilities;
- negotiated peer capabilities only when associated.

In [HeHcfUl.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfUl.cc):

- normalize `getLocalAssociationId() <= 0` to “not associated” before converting to an unsigned type; currently `-1` can become `65535`;
- capture the received BSS color from `Ieee80211HeRxVectorInd` for a foreign AP rather than using the local MIB’s BSS color;
- do not look up a foreign AP using the local association epoch.

### 3.3 Refine Trigger eligibility

In `parseTrigger()`:

1. Continue rejecting foreign scheduled allocations.

2. Permit a foreign Trigger only when all of these hold:

   - local role is non-AP;
   - UORA/HE UL support is enabled;
   - the Trigger was received in an HE PPDU;
   - the Trigger is a supported Basic/BSRP form;
   - it contains an RA-RU with AID12=2045;
   - the station is not associated with the Triggering BSS;
   - the selected parameters are supported by local transmit capabilities;
   - there is a pending management frame addressed to that AP.

3. An associated station may contend only for AID12=0 RA-RUs from its associated BSSID.

4. A scheduled recipient must not also contend or decrement OBO for an RA-RU in the same Trigger.

### 3.4 Build only the standard-permitted response

For the initial unassociated implementation:

- select at most one queued `Ieee80211MgmtHeader` packet addressed to the Trigger transmitter;
- do not select QoS data, QoS Null, BSR, or BAR traffic;
- do not expose backlog belonging to another BSS;
- prepare one tagged management S-MPDU;
- preserve its management address fields;
- use STA-ID 2045 in the HE-TB TXVECTOR;
- use the BSS color obtained from the triggering HE PPDU;
- bind queue reservation and rollback to the exact management packet.

Add an exchange kind such as `PREASSOCIATION_MANAGEMENT_ACK`. Store the Trigger AP, local STA address, queue token, packet identity, and deadline directly in the exchange ledger.

The UORA state owner remains [HeUlCoordinator.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeUlCoordinator.cc), but its preparation/commit context must include the AP address. Switching to a different unassociated AP must reinitialize OCW/OBO using default values as required by 26.5.4.5.

### 3.5 Represent preassociation Multi-STA BlockAck correctly

The current `Ieee80211MultiStaBlockAckRecord` cannot represent the preassociation format.

Update [Ieee80211Frame.msg](src/inet/linklayer/ieee80211/mac/Ieee80211Frame.msg) with typed record information sufficient for:

- Ack Type;
- AID;
- TID;
- block-ack SSN/bitmap context;
- preassociation receiver address.

Update [Ieee80211MacHeaderSerializer.cc](src/inet/linklayer/ieee80211/mac/Ieee80211MacHeaderSerializer.cc):

- ordinary block-ack context: Ack Type 0, TID 0–7, SSN plus bitmap;
- preassociation context: Ack Type 0, AID 2045, TID 15, four reserved octets plus the unassociated STA’s six-octet address;
- reject unsupported record variants at serialization rather than silently projecting them into the ordinary bitmap form;
- round-trip both record formats.

Generated `_m.*` files should be regenerated through the normal build, not edited manually.

### 3.6 Extend AP collection and acknowledgment

In [HeUlMuTxOpFs.cc](src/inet/linklayer/ieee80211/mac/framesequence/HeUlMuTxOpFs.cc):

1. Recognize a management S-MPDU received on an AID12=2045 RA-RU.

2. Require:

   - exactly one management MPDU in that unassociated response;
   - receiver address equal to the AP;
   - successful MPDU decoding;
   - no conflict with another response collected for that RU.

3. Extend [IHeUlMuExchangeCallback.h](src/inet/linklayer/ieee80211/mac/contract/IHeUlMuExchangeCallback.h) with a management-frame delivery operation.

4. Deliver the management frame through the normal recipient management path without scheduling a legacy ACK.

5. Generate a preassociation Multi-STA BlockAck record:

   - Ack Type 0;
   - AID 2045;
   - TID 15;
   - record RA equal to the unassociated STA address.

6. On the station, match that record using Trigger ID, AP transmitter address, AID 2045, TID 15, and the local MAC address. On success, retire the management packet and publish the management exchange result; on timeout, restore/retry it.

7. Change BlockAck transmitter validation to compare against the AP stored in the exchange ledger. The current unconditional comparison with the local MIB BSSID would discard every valid foreign-AP acknowledgment.

## File-level change map

| Files | Responsibility |
|---|---|
| [HeHcf.h](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.h), [HeHcfUl.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcfUl.cc) | Remove exported exhaustive validator, construct correct snapshots, normalize association state |
| [HeTriggeredUlExchangeService.h](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeTriggeredUlExchangeService.h), [HeTriggeredUlExchangeService.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeTriggeredUlExchangeService.cc) | Eligibility, response construction, exchange ownership, BA record processing |
| [Ieee80211Frame.msg](src/inet/linklayer/ieee80211/mac/Ieee80211Frame.msg), [Ieee80211MacHeaderSerializer.cc](src/inet/linklayer/ieee80211/mac/Ieee80211MacHeaderSerializer.cc) | Preassociation Multi-STA BA wire representation |
| [IIeee80211HeUlScheduler.h](src/inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeUlScheduler.h), [HeUlMuPlan.h](src/inet/linklayer/ieee80211/mac/framesequence/HeUlMuPlan.h), UL scheduler files | Associated versus unassociated RA-RU policy |
| [HeUlMuTxOpFs.cc](src/inet/linklayer/ieee80211/mac/framesequence/HeUlMuTxOpFs.cc), [IHeUlMuExchangeCallback.h](src/inet/linklayer/ieee80211/mac/contract/IHeUlMuExchangeCallback.h) | AP collection, management delivery, preassociation acknowledgment |
| [HeUlCoordinator.cc](src/inet/linklayer/ieee80211/mac/coordinationfunction/HeUlCoordinator.cc) | Per-AP UORA context and reset semantics |
| [ch-80211.rst](doc/src/developers-guide/ch-80211.rst) | Document both AID12 forms and the one-management-S-MPDU limitation |

## Verification plan

Build:

```sh
make MODE=release -j$(nproc)
```

First narrow test:

```sh
inet_run_unit_tests -m release -f '(Ieee80211HeUlMuTransaction).*\.test'
```

Expanded focused set:

```sh
inet_run_unit_tests -m release -f '(Ieee80211HeUlControlFrames|Ieee80211HeUlMuTransaction|Ieee80211HeUora|Ieee80211HeSchedulerValidation|HeUlScheduler|Ieee80211HeMultiStaBlockAckFs|Ieee80211HeTxRxInterception).*\.test'
```

Module baseline:

```sh
inet_run_module_tests -f '(Ieee80211HeUlTriggerExchange_1).*\.test'
```

Add a deterministic module scenario with:

- one AP configured to advertise unassociated RA-RUs;
- one unassociated HE STA with a queued management request;
- fixed UORA contention parameters;
- AID12=2045 Trigger;
- one HE-TB management S-MPDU;
- one preassociation Multi-STA BlockAck;
- successful management delivery and queue retirement.

Also test:

- no pending management frame → no OBO decrement and no response;
- AID12=0 from foreign AP → no response;
- AID12=2045 from associated AP does not enter associated UORA;
- unsupported selected RU → no response but NAV remains updated;
- wrong record RA, wrong AP TA, late BA, and wrong Trigger ID → no retirement;
- feature-disabled and non-HE paths remain unchanged.

Finally run:

```sh
bash .agents/skills/inet-architectural-requirements/references/enforcement/check-architecture.sh src/inet/linklayer/ieee80211
```

No fingerprint CSV should be updated without separate approval.

## Recommended commit sequence

1. Remove receiver-global Trigger validation and relocate tests.
2. Implement multi-record BlockAck processing.
3. Add the preassociation BlockAck record representation and serializer tests.
4. Add typed RA-RU targeting and unassociated STA response construction.
5. Add AP management collection, acknowledgment, module coverage, and documentation.
6. Run independent regression and architecture reviews.

The change is complete only when the AID12=2045 path works end to end. Merely accepting the Trigger in `parseTrigger()` would leave the response addressed incorrectly, discard its BlockAck, and risk selecting traffic belonging to another BSS.