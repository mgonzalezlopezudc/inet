# Implementation plan: RTS selection and ACK/CTS response modes

Checkout: `inet-ieee80211-control-response`.
Plan source revision: `b01a6a78b17dcb589b0b9509d9f7f5dd092d2fa4`.
Implementation base: `83b5a1178fd38cc0d21b78c74c4e57f6297622ad`.
Plan revision: 2026-10-05.
Status: implemented; focused verification passes.
Evidence: current source inspection and local retrieval of IEEE Std 802.11-2024.
Section 8 defines the required checks.
Section 10 records the completed checks and their limits.

## 1. Problem and intended result

INET must select a permitted physical mode for each RTS and each ACK or CTS response.
The current response policies omit the BSS basic-rate set.
They also accept configured response rates without all the required checks.
As a result, the originator can predict a response that the recipient cannot legally transmit.
An incorrect prediction can change the calculated exchange duration and the Duration/ID field.

This change gives both built-in rate policies one common calculation for ACK and CTS responses.
The originator uses that calculation to predict the response.
The recipient uses the same calculation to select its response.
Both sides check the applicable rates, physical format, preamble, channel width, and complete frame length.
An unsupported request stops before its dependent transmission.

RTS selection also needs the request's position within the whole TXOP.
An initial RTS uses the applicable basic-rate set.
A later RTS uses a bound from the latest transmission to the same peer.
Preparation must supply this history without a change to actual transmission history.

### Terms used in this plan

| Term | Meaning |
| --- | --- |
| MAC | Medium access control. The protocol layer that controls channel access and exchanges frames. |
| PHY | Physical layer. The protocol layer that sends and receives signals through the medium. |
| PHY mode | A defined combination of PHY format and transmission parameters. |
| mode set | The collection of PHY modes available to a radio or MAC configuration. |
| BSS | Basic service set. A set of stations that follows one coordination function. |
| BSSID | Basic service set identifier. The identifier of a BSS. |
| peer | The other station in a protocol exchange or agreement. |
| RTS | Request to send. A control frame that requests protection for a subsequent exchange. |
| CTS | Clear to send. A control frame that provides protection, usually as a response to RTS. |
| ACK | Acknowledgment. The control frame that confirms successful reception of a frame that requires this response. |
| SIFS | Short interframe space. The interval that separates specified immediate responses and frames within an exchange. |
| TXOP | Transmission opportunity. A time interval in which a QoS station has the right to start frame exchange sequences. |
| QoS | Quality of service. Traffic treatment that accounts for requirements such as priority, delay, and throughput. |
| DCF | Distributed coordination function. The basic IEEE 802.11 coordination function that uses contention for channel access. |
| HCF | Hybrid coordination function. The coordination function that includes enhanced distributed channel access and controlled channel access for QoS traffic. |
| HT / VHT | High throughput / very high throughput. The MAC and PHY features introduced by IEEE 802.11n / IEEE 802.11ac. |
| MCS | Modulation and coding scheme. The scheme that selects modulation and error correction parameters for a transmission. |
| PPDU | PHY protocol data unit. The unit that the PHY transmits, with its preamble, PHY header, and data. |
| FCS | Frame check sequence. The field that lets the recipient check a frame for transmission errors. |
| MIB | Management information base. The set of attributes that describes station configuration and state. |

A basic rate is a rate that the BSS requires its stations to support.
A mandatory rate is a rate that the attached PHY requires its implementations to support.
An operational rate is a rate that a station permits for its current operation.
These sets have different purposes; a mode's presence in the catalog does not establish permission.
The catalog is the configured collection of represented PHY modes.
Airtime is the duration of one PPDU transmission, with its physical overhead.

The originator transmits the request; the recipient transmits the response.
The primary mode is the response mode that the standard's primary-rate rule selects.
A permitted alternate is another mode that satisfies every alternate-response rule in section 4.4.
A prepared exchange records its frames and modes before the MAC decides whether the exchange fits in the available TXOP time.
A reserved successor is a prepared exchange that the MAC retains for the next transmission.

### Example: basic rates take precedence

Consider an OFDM request at 24 Mb/s in a 20 MHz channel.
OFDM means orthogonal frequency division multiplexing.
The BSS basic rates are 6 and 12 Mb/s.
Both basic rates meet the request's rate bound, so the primary ACK rate is 12 Mb/s.
The request rate's mandatory status does not make 24 Mb/s the primary ACK rate.

If the BSS basic set contains only 24 Mb/s, a request at 6 Mb/s has no eligible basic rate.
The response calculation uses the highest attached-PHY mandatory rate at or below 6 Mb/s.
It does not apply this fallback to an initial RTS with a nonempty basic-rate set.

## 2. Current implementation and change scope

The relevant source paths start at `src/inet/linklayer/ieee80211/` unless a row gives another root.

| Component | Current behavior | Required change |
| --- | --- | --- |
| `mac/rateselection/RateSelection.*` and `QosRateSelection.*` | ACK/CTS queries use mandatory modes or configured overrides. Both contain basic-rate omissions. | Use one complete response calculation for prediction and recipient selection. |
| `mac/contract/IRateSelection.h` and `IQosRateSelection.h` | The contracts expose recipient queries. The QoS mode query takes a `TxopProcedure` pointer. | Add explicit rate snapshots and prepared inputs with complete built-in implementations. |
| `mac/framesequence/FrameSequencePlanningContext.*` | Preparation predicts responses through recipient queries. It selects originated modes through the ordinary QoS query. | Supply explicit request modes, TXOP position, and private same-peer history. |
| `mac/coordinationfunction/Hcf.*` and `Dcf.*` | Response dispatch obtains a mode from the policy and passes it to `Tx`. | Check the final mode before transmission and check received responses before success callbacks. |
| `mib/Ieee80211Mib.*`, `Ieee80211RateSet.h`, and `Ieee80211RateContext.h` | The MIB supplies local, BSS, peer, active, and target rate facts. It emits `rateStateChanged` with a bool value. | Reuse these facts and identities without a second rate-state owner. |
| `src/inet/physicallayer/wireless/ieee80211/mode/IIeee80211Mode.h` | Typed queries supply modulation class, preamble type, non-HT reference rate, and duration. | Use these queries without mode-name or catalog-order inference. |

This plan covers represented non-HT RTS, ACK, and CTS modes.
Non-HT means a physical format outside HT and VHT.
It also covers non-HT ACK/CTS responses to represented HT/VHT requests when the format and width rules permit them.

The scope excludes Block Ack, BAR, agreement setup, and A-MPDU construction.
Block Ack confirms reception status for multiple MAC frames; BAR requests that response.
An A-MPDU combines multiple MAC frames within one PHY data unit.
This plan adds no BAR integration declarations and changes no Block Ack dispatch.
It changes no PHY model, response-start timeout, or reception-indication contract.

Required HT/VHT responses remain unsupported in this non-HT implementation.
The same limit applies to unrepresented non-HT duplicate formats, sub-1 GHz formats, and space-time block coding procedures.
The calculation must report these limits; it must not select a narrower or different format as a substitute.

## 3. Rate facts and their lifetime

### 3.1 Copy the applicable facts

Management owns rate advertisements and relationship changes.
The MIB owns the validated rate records.
Rate selection queries the local MIB; it performs no remote module lookup.
Reuse `getLocalRateSet()`, `getBssRateSet()`, `findPeerRateSet()`, and `snapshotRateContext()`.

Each `Ieee80211RateSetState` contains supported, basic, and operational sets.
Each set contains a `known` flag, legacy rates in bits per second, and HT MCS indexes.
An unknown set differs from a known empty set.
An unknown required set returns `UNSUPPORTED`.
A known set with no represented legal mode also returns `UNSUPPORTED`.
It does not qualify for a fallback intended for unknown peer information.

The policy constructs a `ResponseRateContext` from copied MIB facts and request restrictions.
This value contains no mutable management pointer.

| Required content | Purpose |
| --- | --- |
| Local supported and operational rates | Establish which rates the local station can use. |
| Applicable BSS rates and basic flags | Establish the primary-rate rule and initial RTS permission. |
| Peer supported and operational rates | Apply known peer limits without another peer's facts. |
| Local address, peer address, and BSSID | Identify the relationship to which the facts apply. |
| Context kind, transaction identifier, and generation | Detect a changed or removed relationship before transmission. |
| Request format, modulation class, preamble, and channel width | Establish the physical restrictions for this response. |
| Known-state information | Distinguish no applicable BSS, a known empty set, and unknown required facts. |

A generation is a counter that identifies the version of the rate facts.
The context kind is `ACTIVE`, `TARGET`, or `NONE`.
`ACTIVE` identifies the current BSS relationship.
`TARGET` identifies one live management transaction and its validated target rates.
`NONE` identifies an explicit procedure with no applicable BSS.
An unknown target must not become `NONE`.

A procedure without an applicable BSS uses only the fallback that its control-frame rule permits.

For an originated request, the MAC supplies the receiver and any explicit `BssRateContextRef`.
An RTS copies the context reference from the frame that it protects.
For a received request, the MAC supplies the transmitter, request subtype, and available BSSID.
The MIB resolves the applicable active relationship or the incoming binding that management installs.
A reference conflict or ambiguous binding returns unknown required context.
The tuned channel alone does not identify the BSS.

For example, a station can retain its active relationship with AP A during association with AP B.
AP means access point.
A request for B uses B's target rates; a request for A uses A's active rates.
Response preparation must not make B the active BSS.
An old request cannot use a replacement relationship at the same address because its copied identity differs.

### 3.2 Invalidate future transmissions after a change

The MIB commits related state before it emits `rateStateChanged`.
HCF subscribes with the bool listener overload.
The MAC invalidates affected future prepared steps when rates or relationship identities change.
Mode-set changes also invalidate steps whose representations no longer apply.
Stop, crash, and restart invalidate retained transmission state.

A required immediate response retains the copied context from the accepted request through its SIFS delay and transmission.
Ordinary reconfiguration waits until that response finishes.
A response already required by an accepted request must not disappear because an unrelated future plan becomes invalid.
Lifecycle stop or crash still cancels pending transmission through the normal lifecycle path.

## 4. One calculation for ACK and CTS responses

Add `Ieee80211ResponseModeSelection.h/.cc` under `mac/rateselection/`.
This helper performs a pure calculation: it changes no MIB state, frame state, or transmission history.
Both `RateSelection` and `QosRateSelection` use it.
The policies retain ownership of configured choices and rate selection.
The helper avoids duplicate rules without a new shared module base class.

The input contains the response kind, explicit request mode, complete response length, copied rate facts, and available mode representations.
The result contains the primary mode, permitted modes, common response airtime, and either a supported result or `UNSUPPORTED` with a reason.
Mode objects remain borrowed and immutable; their owner must remain valid while a prepared result exists.

### 4.1 Determine the format and width first

The helper determines whether the request permits a non-HT response before it selects a rate.
An HT RTS requires an HT CTS, so this implementation returns `UNSUPPORTED` for that exchange.
A VHT RTS with the HT Control MRQ subfield equal to 1 requires a VHT response.
MRQ requests feedback about the modulation and coding scheme.
Requests that require HT/VHT responses through beamforming or space-time block coding conditions also return `UNSUPPORTED`.

For a non-HT response to an HT/VHT request, the response channel width must equal the received request width.
The response uses OFDM or ERP-OFDM modulation.
ERP-OFDM is the OFDM modulation class of the extended rate PHY.
A 20 MHz representation cannot replace a required 40 MHz response.

For a supported ordinary non-HT request, preserve its modulation class and preamble type.
Use its received width for the ordinary response; the response must never exceed that width.
This profile excludes duplicate-format and bandwidth-signaling procedures without the required representations.
The calculation returns `UNSUPPORTED` before it uses an unavailable format or width.

### 4.2 Obtain the request's rate bound

For a non-HT request, use its data rate as the bound.
For an HT/VHT request, use the typed `getNonHtReferenceRate()` query.
The reference rate depends on the request's modulation and coding rate, not its aggregate data rate or mode name.
The applicable mapping is:

| Modulation | Coding rate | Reference rate, Mb/s |
| --- | --- | ---: |
| BPSK | 1/2 | 6 |
| BPSK | 3/4 | 9 |
| QPSK | 1/2 | 12 |
| QPSK | 3/4 | 18 |
| 16-QAM | 1/2 | 24 |
| 16-QAM | 3/4 | 36 |
| 64-QAM | 1/2 or 2/3 | 48 |
| 64-QAM | 3/4 or 5/6 | 54 |
| 256-QAM | 3/4 or 5/6 | 54 |

BPSK and QPSK identify binary and quadrature phase-shift keying.
QAM means quadrature amplitude modulation; its numeric prefix identifies the constellation size.
For an HT MCS with unequal modulation across streams, the reference calculation uses stream 1.
An unavailable or invalid reference mapping returns `UNSUPPORTED`.
This profile adds no high efficiency (HE) request support or operation in television white-space bands.

### 4.3 Select the primary mode

1. Find the highest applicable BSS basic rate at or below the request's rate bound.
2. Use that rate if one exists.
3. Otherwise, find the highest attached-PHY mandatory rate at or below the same bound.
4. Locate its representation with the required format, modulation class, preamble, and width.
5. Check the applicable local and known peer limits.
6. Return `UNSUPPORTED` if a required fact or the primary representation is absent.

Unknown required BSS facts do not authorize the mandatory-rate fallback.
A known empty basic set, or a known basic set with no rate below the bound, permits the response fallback.
A primary representation that is absent does not permit selection of an arbitrary lower represented rate.
Use `getNumModes()` and `getMode(index)` to locate representations.
Catalog order and mode names cannot establish permission.

### 4.4 Derive the permitted alternates

The helper includes the primary mode in the permitted set.
An alternate must satisfy all these conditions:

1. Its rate belongs to the BSS basic set or the mandatory set of the attached PHY.
2. Its modulation class equals the primary mode's modulation class.
3. Its format, preamble, and channel width satisfy the request restrictions.
4. Its representation satisfies the applicable local and known peer limits.
5. Its complete response airtime equals the primary airtime exactly at simulation-time resolution.

Use `IIeee80211Mode::getDuration()` with the complete ACK or CTS length and its FCS.
An ordinary ACK or CTS has a complete length of 14 bytes: a 10-byte MAC header and a 4-byte FCS.
Use the model's authoritative length constants rather than a second literal in the implementation.
Keep airtime values in `simtime_t`.
Do not estimate airtime as bytes divided by bitrate; that estimate omits physical overhead.
A faster rate or shorter airtime alone does not establish a permitted alternate.

The originator predicts the primary mode without its own recipient override.
The recipient selects the primary mode by default.
A configured recipient override can select a permitted alternate.
The originator accepts that alternate even when its mode pointer differs from the primary pointer.
Equal airtime preserves the response contribution to the prepared exchange duration.

## 5. Select RTS modes with explicit position and history

### 5.1 Initial RTS

An RTS that starts the whole TXOP uses a rate from the applicable BSS basic set.
Mandatory rates apply only when that set is known empty.
Unknown required BSS facts return `UNSUPPORTED`.
A nonempty basic set with no represented legal mode also returns `UNSUPPORTED`.
The default policy selects the highest represented legal control rate.

### 5.2 Later RTS

A later RTS follows an earlier transmission within the TXOP.
It needs the latest earlier transmission to its peer.
Its bound is the highest BSS basic rate at or below that earlier mode's rate or non-HT reference rate.
If no basic rate meets that condition, use the highest attached-PHY mandatory rate at or below the same reference rate.
The RTS uses a locally permitted, peer-compatible, represented rate no higher than the calculated bound.
The default policy selects the highest rate in that legal set.

The earlier transmission can belong to an earlier TXOP.
History for another peer cannot supply the bound.
Absent same-peer history or an unavailable bound returns `UNSUPPORTED`.
The policy must not invent history or substitute its fastest mandatory rate.

For example, BSS basic rates of 6 and 12 Mb/s give a 12 Mb/s bound after a 24 Mb/s transmission.
A subsequent 6 Mb/s transmission to the same peer reduces that bound to 6 Mb/s.
A transmission to a different peer does not change it.

### 5.3 Keep preparation separate from actual history

The preparation caller starts with a copy of actual per-peer history.
It advances a private history through each prepared transmission.
A reserved successor uses that private history for its earlier same-peer mode.
Actual history changes only after actual transmission through `frameTransmitted()`.
Preparation emits no `datarateSelected` notification and no rate feedback.
Later rate feedback can affect new plans, but it cannot change a reserved exchange's modes.

Position refers to the whole TXOP, not the first step of each repeated exchange.
The first request uses `TXOP_INITIAL`; a request after an earlier exchange uses `TXOP_CONTINUATION`.
The caller supplies that position explicitly instead of an inference from `isTxopInitiator()`.

Add these value types and the pure virtual `computePreparedMode()` declaration to the QoS policy contract:

```cpp
enum class PreparedControlPosition { TXOP_INITIAL, TXOP_CONTINUATION };

struct PreviousPeerTransmission {
    MacAddress peer;
    const physicallayer::IIeee80211Mode *mode;
    int64_t packetId;
    uint64_t txopGeneration;
};

struct PreparedModeInput {
    const Packet *frame;
    Ptr<const Ieee80211MacHeader> header;
    PreparedControlPosition position;
    std::optional<PreviousPeerTransmission> previousToSamePeer;
    ResponseRateContext rates;
    const physicallayer::Ieee80211ModeSet *transmitModes;
};

enum class ModePreparationStatus { READY, UNSUPPORTED };

struct PreparedModeResult {
    ModePreparationStatus status;
    const physicallayer::IIeee80211Mode *mode;
    std::string reason;
};

virtual PreparedModeResult computePreparedMode(
        const PreparedModeInput& input) const = 0;
```

`packetId` identifies the frame that supplies the history.
`txopGeneration` records its TXOP identity; it does not require history from the current TXOP.
`transmitModes` supplies the current transmit catalog.
The policy checks format and physical configuration before it selects a mode.
The caller retains copied rate facts and validates their identity before transmission.
The ordinary control path uses the same RTS rules with actual position and history.

## 6. Policy contracts and transmission checks

### 6.1 Supply the same facts to both policies

Add the snapshot query below to `IRateSelection` and `IQosRateSelection`:

```cpp
enum class ResponseRequestRole { ORIGINATED, RECEIVED };

virtual ResponseRateContext snapshotResponseRateContext(
        const Ptr<const Ieee80211MacHeader>& requestHeader,
        ResponseRequestRole role,
        const std::optional<BssRateContextRef>& explicitContext) const = 0;
```

The role determines whether the response peer is the request's receiver or transmitter.
The policy delegates the snapshot lookup to the MIB without protocol progress.
Both built-in policies implement every new pure virtual declaration.
Keep the current recipient queries as consumers of the common calculation.
Extend the prediction path to consume its primary mode, permitted modes, copied context, and airtime.
Do not force an originator prediction through a configured recipient override.

The current prepared-exchange caller consumes `computePreparedMode()` and the common response result.
It records the selected request mode once before TXOP admission.
It uses the same response airtime for admission and Duration/ID.
Execution retains those values unless a rate or relationship change invalidates the future step.
This plan does not redefine the TXOP budget or response timeout.

### 6.2 Reject illegal configured choices

Keep `controlFrameBitrate`, `responseAckFrameBitrate`, and `responseCtsFrameBitrate` as requests for permitted choices.
Initialization checks that a requested choice has a local representation.
Per-request validation checks its complete legal set.
Validate `controlFrameBitrate` before admission of its dependent exchange.
Validate ACK/CTS overrides during response calculation and again at actual HCF/DCF response dispatch.

An illegal override raises `cRuntimeError` before `Tx::transmitFrame()`.
The MAC must not transmit a substitute mode or add the override to the legal set.
If a compatibility helper substitutes a mode, the MAC revalidates that final mode.
The error reports the response kind, request mode, primary mode, requested override, and failed rule.
Unsupported required facts or representations produce `UNSUPPORTED` with a specific reason.
The originator rejects the dependent request before transmission; the recipient sends no response to an unsupported external request.

### 6.3 Check received responses before success

The originator checks the actual ACK or CTS before any normal completion callback.
Use `Ieee80211ModeInd` for the actual received mode.
Use the typed response header and complete length for the response-kind and length checks.
Compare the mode's format, modulation class, preamble, width, and permission against the retained request context.

A legal equal-airtime alternate completes the exchange normally.
A wrong format, wrong length, absent mode indication, or out-of-set mode fails validation.
The failure path reports the violated rule before acknowledgment progress or successor execution.
It cancels a reserved successor through the normal failure or abort path.
A shorter illegal response remains illegal.
The current response-start and timeout procedures retain their duties.

## 7. Implementation steps

Each step includes its production consumer and verification.
No interface addition remains without a caller.

| Step and purpose | Responsible components and files | Proposed change | Expected result and verification |
| --- | --- | --- | --- |
| 1. Establish one response calculation. | `mac/rateselection/Ieee80211ResponseModeSelection.h/.cc`; typed mode queries. | Implement the format, width, primary-rate, alternate, and airtime rules in section 4. | Unit cases distinguish basic precedence, mandatory fallback, unsupported facts, and absent primary representations. |
| 2. Connect prediction and recipient selection. | `IRateSelection`, `IQosRateSelection`, `RateSelection`, `QosRateSelection`, and `FrameSequencePlanningContext`. | Add copied response contexts. Route both policies and preparation through the helper. | HCF and DCF predict and transmit responses under the same rules. Module cases compare the primary prediction with actual permitted responses. |
| 3. Supply RTS position and same-peer history. | QoS policy contract, built-in policies, preparation caller, and actual transmission feedback. | Add section 5's prepared input and result. Advance private history only during preparation. | Initial and later RTS use different legal sets. Tests check prior-TXOP history, peer isolation, and no false actual history updates. |
| 4. Enforce permission at transmission and reception. | `Hcf`, `Dcf`, frame-sequence validation, and pending response state. | Reject overrides before transmission. Validate actual responses before success. Invalidate affected future plans after context changes. | Module cases observe no illegal transmission, no false acknowledgment progress, and no invalid successor execution. Required immediate responses survive ordinary reconfiguration. |
| 5. Explain compatibility changes. | Maintainer of the rate-policy contracts. | Add migration entries for required methods, copied inputs, changed defaults, and override rejection. | Each entry explains how an external policy preserves these rules and handles unsupported inputs. |

The MIB facts and typed PHY queries already exist at the stated source revision.
Reuse them; this change adds no second advertisement mechanism or PHY model.
External implementations of the policy contracts must implement the new pure virtual methods.
Current configurations can select different default control or response rates after the basic-rate rules apply.
An override that previously bypassed a rule now produces an explicit error.

## 8. Verification and completion criteria

Add `tests/unit/Ieee80211ResponseModeSelection_1.test` for the common calculation.
Add `tests/module/Ieee80211ControlResponseModes_1.test` for the production paths.
Use consistent management advertisements at both stations for alternate-response cases.
Do not mark an optional catalog rate mandatory to manufacture an alternate.

| Case | Required observation |
| --- | --- |
| Known nonempty, known empty, and unknown rate facts | The result follows the frame-specific fallback rule. Unknown required facts never become an empty set. |
| Basic-rate precedence and primary representation | Both policies select the highest eligible basic rate. An absent required primary mode produces `UNSUPPORTED`. |
| Permitted alternates | Primary and alternate complete-frame airtimes match exactly. The originator accepts a different permitted mode pointer. |
| Illegal ACK/CTS and RTS overrides | Validation reports the failed rule before the response transmission or dependent request admission. |
| Initial and later RTS | Position covers the whole TXOP. Different earlier data modes produce the correct bounds. |
| Same-peer history | Another peer's transmission cannot change the bound. Valid prior-TXOP history remains usable. Absent history is unsupported. |
| Pure preparation | Actual history, real frame tags, protocol progress, and transmission notifications remain unchanged until actual transmission. |
| HCF and DCF response paths | Originator prediction and actual recipient callbacks satisfy the same legal set. |
| HT/VHT requests | Typed reference mapping and required response width apply. Required HT/VHT formats or absent representations are unsupported. |
| Illegal external responses | Wrong format, wrong complete length, absent mode indication, and out-of-set modes fail before success callbacks. |
| Catalog independence | Extra or reordered illegal slow modes cannot change permitted airtime or the prepared duration. |
| Rate or relationship changes during SIFS | Affected future steps become invalid. An accepted required recipient response retains its copied context. |
| Prepared exchange integration | Corrected responses preserve the nominal exchange duration, Duration/ID, and successor checks. |

Build the current implementation in debug mode before the behavioral tests:

```sh
make -j$(nproc) MODE=debug
inet_run_unit_tests -m debug -f '(Ieee80211ResponseModeSelection_1|Ieee80211NonHtReferenceRate_1|Ieee80211PeerModeSelection_1)\.test'
inet_run_module_tests -m debug -f '(Ieee80211ControlResponseModes_1|Ieee80211TxopExchange_1)\.test'
inet_run_protocol_tests -m debug -w '^tests/protocol/wifi$' -f '/(Legacy_DataAck|Legacy_RtsCts)\.test$'
make -j$(nproc) MODE=release
```

The two new test targets now exist.
The reference-rate, peer-mode, TXOP exchange, and legacy protocol targets exist at the stated revision.
Unit tests establish the calculation rules; module tests must establish the HCF/DCF dispatch and completion behavior.
The legacy protocol cases check the data/ACK and RTS/CTS exchanges for regressions.
Release compilation checks the changed contracts without debug-only assumptions.
These focused checks do not establish Block Ack support, aggregate support, or complete fingerprint and statistical coverage.

The implementation is complete when every case above has direct evidence and both built-in policies use the common rules.
Record the tested revision, build mode, selected cases, results, and verification limits.
The user authorized implementation on 2026-10-05 with the request to execute this plan.

## 9. Standard rules and evidence

The protocol rules in sections 4 and 5 follow IEEE Std 802.11-2024.
The table below records normative sources; the required rules appear in full within this plan's supported scope.
The selected non-HT profile and explicit unsupported outcomes are implementation limits, not additional standard permissions.

| Clause or table | Rule covered | Canonical corpus node | Physical PDF pages |
| --- | --- | --- | --- |
| 10.6.6.1 | Select the required control-response format. | `ieee80211-2024:clause:10.6.6.1` | 1942–1943 |
| 10.6.6.2 | Select an initial RTS rate. | `ieee80211-2024:clause:10.6.6.2` | 1943–1944 |
| 10.6.6.4 | Bound a later RTS by the earlier same-peer rate. | `ieee80211-2024:clause:10.6.6.4` | 1944–1945 |
| 10.6.6.5.2 | Select the primary rate, modulation class, and preamble. | `ieee80211-2024:clause:10.6.6.5.2` | 1945–1947 |
| 10.6.6.5.4 | Permit only equal-airtime alternates with the required rate membership and modulation class. | `ieee80211-2024:clause:10.6.6.5.4` | 1950 |
| 10.6.6.6 | Select the response channel width. | `ieee80211-2024:clause:10.6.6.6` | 1951–1953 |
| 10.6.11 and Table 10-10 | Map represented HT/VHT request modes to non-HT reference rates. | `ieee80211-2024:clause:10.6.11`, `ieee80211-2024:table:10-10` | 1962–1963 |

The local corpus reports the base standard as fresh and ready.
The retrieved references connect the primary-rate procedure to the alternate, width, and reference-rate procedures.
The corpus resolves the reference to Table 10-10 and the HT/VHT MCS clauses.
The width clause also references RTS protection and bandwidth-signaling procedures outside this supported profile.
No source PDF inspection was necessary for these text rules.

## 10. Implementation result

Both built-in policies now use the common response calculation.
Preparation supplies explicit RTS position and private same-peer history.
HCF and DCF reject illegal responses before transmission or success callbacks.
HCF invalidates affected future plans after a rate or relationship change.
An accepted recipient response retains its copied context through SIFS.
The migration guide explains the changed contracts and unsupported outcomes.

The debug and release builds pass on the implementation base plus the local implementation changes.
Five unit targets, six module targets, and two legacy protocol targets pass.
The new module target covers 52 configurations with seed 0.
The architecture and source-protection checks pass.
The naming and interface checks retain existing findings in files that match the implementation base exactly.

The local report at `audit/control-response-modes/implementation.md` records commands, artifacts, case coverage, and the complete changed-path list.
The report also records the AP forwarding correction and the required recipient-duration consumers.
The protected RTS fixture verifies projected continuation modes; existing TXNAV admission requires a new grant for each actual protected exchange.
The focused tests establish no new Block Ack or aggregate support.
The initial verification did not run the fingerprint or statistical suites.
The later audit passed five unit targets, five module targets, and six scoped fingerprint cases.
The control-response module target includes 56 configurations after the HT permission repair.
That repair stops future prepared transmissions after relevant peer HT capabilities change.
The complete statistical suite and graphical fingerprints remain unverified.

The plan precedes the implementation commits in the branch history.
The RTS selection commit carries the two hidden-node fingerprint updates that its behavior causes.
The HT permission repair remains a separate commit.
