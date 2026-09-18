# IEEE 802.15.4 — M1 applicability audit

> **Kind:** report · **Status:** snapshot 2026-09-19 · **Seal:** none · **Owns:** — · **Stands on:** [coverage.md](coverage.md), [catalog.md](../../standard/ieee802154/catalog.md)

Latest audit baseline: `6cb03df7c8` (initial pass: `fd6f800222`). This is the continuing extraction for step 0 of the
[implementation plan](../../../../../plan/pending/802154-implementation-plan.md).
**The applicability gate is open, not passed.** The extracted statements and English procedures
are reviewable inputs to closure; they are not a complete inventory of the selected profile.
No simulation behavior, source implementation or fingerprints changed in this pass.

## Selected engineering behavior

| Dimension | M1 selection | Effect on the claim |
| --- | --- | --- |
| Device roles | Statically configured device and PAN-coordinator endpoints | PAN-coordinator receive predicates remain applicable even without start/association procedures. A forwarding coordinator role is not claimed. |
| PAN mode | No periodic beacons; no superframe scheduling; direct data exchanges | Unslotted access applies. No CAP/GTS scheduling claim. |
| PHY | 2450 MHz O-QPSK, page 0, channels 11–26, 250 kbit/s; CCA Mode 1 | The intended PHY selection needs complete channel/PIB/measurement extraction before closure. |
| Addressing | Native short and EUI-64 identities; source-only, destination-only and dual-address legacy layouts | Static assignment is a simulation setup procedure, not evidence of standard association/address allocation. |
| Data service | MCPS request/confirm/indication; acknowledged and unacknowledged direct data; opaque MSDU, optional single receiver-configured protocol | No 6LoWPAN or IPv6 interoperability claim. |
| Management | Reset/get/set sufficient for selected attributes | Start, scan, association and other managed services remain separately owed or conditional. |
| Security | `macSecurityEnabled=false`; outgoing level zero; explicit rejection outcomes | Incoming secured version 0 and version 1 have different statuses. ACK eligibility is independent of security success for valid legacy input. |
| Sequence policy | One device-wide DSN; modulo-256 fresh allocation; stable retry DSN | No default duplicate suppression or exactly-once delivery claim. |
| Optional modes | No periodic beacon, GTS, enhanced frame/IE, scheduled access, ranging or security transform | Conditional requirements may be excluded only after checking their predicates; absence from the current catalog is not exclusion evidence. |

## Frame support decision

This is the proposed replacement's support boundary, not the current model's capability.

| Input or output | Transmit | Structural interpretation | Operational processing |
| --- | --- | --- | --- |
| Unsecured Data version 1 | Yes, `LegacyTx=true` | Legacy presence rules | Direct data service |
| Unsecured Data version 0 | No new-data generation initially | Legacy presence rules | Receive legacy data |
| Immediate ACK | Generate version 0 with the applicable Frame Pending value; 7.3.3 continuation sets other FCF fields to zero | Address-free legacy layout; accept legal versions 0/1 | Match DSN and outstanding exchange context |
| Valid secured legacy Data | No | Retain enough validated framing to apply filters and ACK decision; never expose protected bytes as plaintext MSDU | Version 0: `UNSUPPORTED_LEGACY`; version 1: `UNSUPPORTED_SECURITY`; preserve required ACK behavior |
| Version 2 Data/ACK | No | Identify unsupported version before applying legacy offsets | No legacy ACK fallback; enhanced behavior belongs to E1 |
| Beacon and command | No in the static data subset | Type discrimination; full codecs follow managed services | Deferred selected-device obligations remain visible |
| Reserved version/type/address mode or truncated bytes | Never generate | Distinguish malformed from legal-but-unsupported; do not assert on external input | No ordinary data delivery; exact malformed-frame outcome needs the remaining format extraction |

Version 1 transmission follows the `LegacyTx` requirement in the continuation of Table 8-30.
Version 0 reception does not import the 2003 security procedure. The immediate-ACK continuation
is in corpus node `ieee802154-2024:figure:7-16`, physical PDF pp. 91–92; reading only the parent
7.3.3 node would miss it. The plan's “reject unsupported combinations” must mean controlled
policy rejection, not premature removal of framing needed for a required ACK.

## Receive decision table

Rows describe the selected legacy data path. Apply structural bounds and FCS validation before
ordinary filtering; apply 6.6.2 validity predicates before ACK and security decisions. A separate
capture observer is not an MCPS data indication. Normative promiscuous mode is defined by 10.23.1
and Table 10-114 (physical PDF pp. 379–380). It enables reception and passes MHR+MAC payload,
with only Msdu, MpduLinkQuality, Timestamp and Rssi valid; disabling it restores macRxOnWhenIdle.

The selected interpretation separates promiscuous monitor acceptance from ordinary 6.6.2 validity:
correctly received, structurally supported foreign-address frames can be monitored but do not gain
ACK eligibility. Valid local legacy AR frames retain their ordinary ACK obligations. Protected
bytes may be monitored as raw MHR+payload without asserting successful security or plaintext
MSDU delivery. This reconciles the table's receive-all wording with the clause's 6.6.2 reference;
the source does not explicitly prescribe this two-decision architecture or blanket ACK suppression.
Malformed/unsupported parsing boundaries remain explicit; incorrect FCS is discarded in both modes.

| Cause or accepted case | Capture visibility | Immediate ACK | Duplicate state | DSN in MCPS indication | MSDU delivery | Outcome |
| --- | --- | --- | --- | --- | --- | --- |
| Truncated/structurally invalid frame | Raw observation only if safe | No (structural safety policy; not a standardized status claim) | No suppression cache in M1 | No | No | Malformed classification; no invented IEEE status |
| Incorrect FCS | May remain visible to forensic capture | No | None | No | No | FCS discard |
| Reserved type/version or failing PAN/address filter | May remain visible to capture | No | None | No | No | Filter discard |
| Valid local legacy unsecured unicast, AR=1 | Yes | Yes, received DSN | None | Yes | Yes | Successful indication |
| Same, AR=0 | Yes | No | None | Yes | Yes | Successful indication |
| Valid broadcast-addressed unsecured data | Yes | No | None | Yes | Yes | Successful indication; transmitted AR must be zero |
| Broadcast PAN but local unicast address, AR=1 | Yes | Yes | None | Yes | Yes | PAN broadcast does not imply address broadcast |
| Valid legacy secured unicast, AR=1 | Protected bytes may be observed | Yes under 6.6.2 | None | No data indication | No | Version 0 `UNSUPPORTED_LEGACY`; version 1 `UNSUPPORTED_SECURITY` |
| Valid ACK, matching outstanding exchange | Yes | No | None | Not an MSDU indication | No | Transaction success only within its expected interval |
| Wrong/late ACK | Yes | No | None | No | No | Must not complete an unrelated transaction |
| Unsupported version-2 traffic | Raw observation | No immediate-ACK fallback | None | No | No | Unsupported capability; no enhanced-processing claim |

Additional selected receive decisions:

| Predicate or mode | Normal MAC result | Promiscuous monitor result |
| --- | --- | --- |
| Source-only legacy Data/MAC command, source PAN matches, receiver is PAN coordinator, implicit broadcast false | Accept through 6.6.2(d)(4); apply ordinary AR/nonbroadcast rule | MHR+payload, only the four specified fields valid |
| Same frame at ordinary device with implicit broadcast false | Filter discard; no ACK | Monitor if structurally supported and FCS-correct |
| No destination address or destination PAN, implicit broadcast true | Accept through 6.6.2(d)(3); Table 8-36 describes broadcast treatment, so no ACK | Monitor with the same four-field validity rule |
| Extended group destination, group reception enabled, conforming AR=0 | Accept through 6.6.2(d)(2); no ACK requested | Monitor under the same field-validity rule |
| Foreign PAN/address frame, otherwise correctly received | No normal indication or ACK | Monitor MHR+payload; no ACK gained merely by monitoring |
| Correct local secured legacy AR frame | ACK eligible, security error and no plaintext MSDU delivery | Raw protected MHR+payload under the documented interpretation; AckSent is not a valid monitor parameter |

For nonbroadcast group frames received with AR=1, retain an open interpretation item: 6.6.3.1
requires senders to clear AR, whereas the immediate-ACK receiver predicate in 6.6.2 excludes
broadcast specifically. Do not infer a blanket receiver ACK prohibition from a sender rule.

IEEE Std 802 group-address definitions remain a cross-document dependency; the simulator's
native-EUI-64 representation must not silently substitute MacAddress group tests. Ordinary
MCPS-DATA.indication AckSent is true only after an ACK has been sent (Table 8-32), not on AR
inspection or ACK scheduling. For PAN compression, distinguish the effective source PAN inferred
by 6.6.2 from the wire-presence validity of the SrcPanId indication field. Lifecycle and packet
ownership outcomes are implementation-contract obligations in step 1, not extra IEEE statements.

## Interpretations and prerequisites

| Item | Evidence and disposition | Gate affected |
| --- | --- | --- |
| IFS wording and figure | 6.3.1 (PDF p. 62) gives an “at least AIFS” lower bound after ACK; Figure 6-1 (p. 63) shows size-dependent max(SIFS/LIFS, turnaround). A size-dependent interval satisfies the textual lower bound too. Use both constraints; do not incorrectly replace the longer interval with AIFS. Record exact owner and start event in 1c. | 1c, 4–6 |
| ACK expected time | 6.6.3.4 says “within the expected time”. Searches for `macAckWaitDuration` and `AckWait` returned no corpus matches. A numeric timeout, propagation allowance and simultaneous-event rule remain to be justified from timing clauses/model assumptions; do not call a copied older-edition constant a 2024 requirement. | 1c, 6 |
| Promiscuous indication | 10.23.1 and Table 10-114 supply normative receive-all/format/receiver-control rules. Monitor acceptance and ordinary ACK eligibility are separated by the documented interpretation above; blanket ACK suppression is not a sourced 2024 rule. | 1a, 1b, 6 |
| Incoming secured legacy ACK | 6.6.2 ACK eligibility precedes the security outcome controlling delivery. 9.2.4(a)/(b) returns before key lookup when legacy/disabled. Preserve framing and ACK decision even when security is unsupported. | 2, 6 |
| Invalid security outputs | 9.2.4 declares outputs invalid until explicitly set; early failure cannot justify reading uninitialized security descriptors. Define indication validity explicitly. | 1a, 6 |
| Legacy encoding rules | 7.2.2.6 selects compression based on equal/different PAN IDs; Table 7-3 includes source-only coordinator semantics. Version-2 Table 7-2 is not a legacy oracle. | 2 |
| Native-address framework | `NetworkInterface` keeps generic `MacAddress`, but has attached `InterfaceProtocolData`. This establishes a candidate extension point, not a proven compatible interface. | 1d |
| Energy observations | Mode 1 requires energy detection without a decoded frame. Public medium listening/noise APIs can observe undecodable energy. Timed observation, averaging and cache retention still require the production fixture described in [results.md](results.md#phy-feasibility-inspection). | 4 |

## PIB selection and service boundaries

The complete source rows of Tables 8-36 and 12-2 are now recorded in the
[catalog field domains](../../standard/ieee802154/catalog.md#pib-field-domains), including
all 31 MAC and 13 generic PHY attributes. The following is the M1 implementation selection;
it does not change source access markers or turn deferred capability into a normative exclusion.

| Attribute group | M1 responsibility / later dependency |
| --- | --- |
| macExtendedAddress, macShortAddress, macPanId, macCoordExtendedAddress, macCoordShortAddress | Native identity/configured PAN context. Honor read-only extended identity and distinguish unknown coordinator context from a fabricated zero address. Static setup is outside association-service evidence. |
| macDsn, macMinBe, macMaxBe, macMaxCsmaBackoffs, macMaxFrameRetries | Device-wide sequence and direct-access/retry state; validate cross-attribute BE domains. |
| macSifsPeriod, macLifsPeriod, macUnitBackoffPeriod | PHY-derived timing and default backoff calculation. SIFS/LIFS are upper-layer read-only; backoff period is not dagger-marked. Automatic recomputation after PHY mutation is a step-1b decision, not implied by the default expression. |
| macRxOnWhenIdle, macImplicitBroadcast | Idle-receiver and receive-filter behavior. TRUE/FALSE transitions need direct tests, including ACK wait while idle reception is false. |
| macGroupRxMode | Default false is usable; enabling group reception requires the IEEE Std 802 group-address definition. This cross-document prerequisite remains open, not silently replaced by the generic 48-bit address helper. |
| macSecurityEnabled | Disabled profile only; enabling unsupported security must not silently succeed. Status mapping for model capability restrictions is a step-1a/1b contract decision, separate from out-of-range source rules. |
| macSyncSymbolOffset, macTimestampSupported | Decide optional timestamp support explicitly. A false capability cannot be presented as valid per-frame time metadata; enabled support needs the 6.5.3 boundary and width rules. |
| macBeaconOrder | BO=15 context for M1; periodic values require later beacon-mode support. Optional attribute in the source does not erase the non-beacon receive-policy predicate. |
| macAutoRequest, macBeaconPayload, macBsn, macNotifyAllBeacons | Deferred scan/beacon services. Passive scan remains owed; BeaconNotify support and auto-request effects must be audited with its procedure. |
| macResponseWaitTime, macTransactionPersistenceTime | Deferred managed/indirect exchanges. These are not direct ACK-timeout substitutes. |
| macBattLifeExt, macBattLifeExtPeriods, macGtsPermit | Deferred beacon/GTS mechanisms; their absence must remain explicit in capability reporting. |
| macAoaEnable | Unsupported ranging/angle-of-arrival feature, outside selected O-QPSK behavior. |
| macFcsType | Not applicable to ordinary O-QPSK per the attribute description. Do not use its generic default 0 to select a four-octet O-QPSK FCS. |
| phyCcaDuration, phyCcaMode, phyCcaEdThreshold, phyCurrentChannelInfo | PHY-owned timed CCA/channel configuration, Mode 1 selected. Duration is in microseconds; thresholds are implementation-dependent. |
| phyMaxPacketSize | O-QPSK fixes the applicable value to 127, but the table does not mark the attribute read-only. Distinguish invalid PHY-specific values from READ_ONLY access. |
| phyMaxTxPower, phyTxPower, phyBroadcastTxPower, phyUnicastTxPower, phyPeersTxPower | PHY-owned power constraints. Table 12-3 supplies SHORT/EXTENDED deviceAddrMode, corresponding deviceAddress and signed TxPower; no default is given. Validate peer values against the applicable power constraints; specify lookup/mutation semantics in 1b/1c. |
| phyRanging, phyRxRmarkerOffset, phyTxRmarkerOffset | Ranging unsupported in M1. Preserve the read-only capability marker; offsets do not establish ranging support. |
| macPromiscuousMode (Table 10-114) | Optional feature selected for M1; use the documented monitor/ordinary-acceptance separation above. Default false; exit restores the stored idle policy. |

Source MLME-SET outcomes are READ_ONLY, UNSUPPORTED_ATTRIBUTE, INVALID_INDEX and
INVALID_PARAMETER, with SUCCESS indicating the value was written. Validate these separately
from implementation-only cancellation/busy/refusal results. A local service API may distinguish
such results without falsely claiming that every one is a standard MLME status.

MCPS-DATA confirm fields also have validity predicates: Timestamp is valid only on SUCCESS;
NumBackoffs is undefined on failure; ranging results are invalid if unsupported/disabled.
The indication's SrcPanId is valid only when a source PAN is included, whereas 6.6.2 supplies an
effective source PAN for internal processing when compression omitted it. Preserve both facts
with presence/validity metadata. DataRate=0 selects ordinary O-QPSK; it is not a bitrate value.

The full service continuation contains a source spelling discrepancy outside the selected ranging
behavior: Table 8-31 lists UNSUPPORTED_RANGING, while its prose says RANGING_NOT_SUPPORTED.
Record both for a future ranging contract; do not invent an alias and call it an exact standard
status. Likewise, its Rssi type is printed Boolean; numeric ACK-RSSI representation needs an
explicit contract interpretation rather than a silent claim that the table says Integer.

## Mandatory-service and PICS reconciliation

Clause 6.1 (physical PDF p. 62, `ieee802154-2024@302462:302610`) declares clause 6
features mandatory. Its channel-access, PAN maintenance, synchronization and exchange descendants
must be classified individually; a heading-only extraction does not close their requirements.
Tables 8-1 (pp. 111–112, `@484842:489336`) and 8-26 (p. 136, `@583267:583941`)
provide the service inventory. Asterisks mark optional primitives; operation predicates still come
from the referenced procedures.

| Service boundary | M1 disposition / remaining debt |
| --- | --- |
| MCPS-DATA request/confirm/indication | Selected; direct data contracts and field-validity checks in 1a/6. |
| MLME-GET, SET and RESET request/confirm | Selected; attribute domains and status rules extracted, mutation semantics owed in 1b. |
| MLME-COMM-STATUS indication | Unstarred; classify each generation predicate, including rejected incoming security, before closing the 1a service surface. |
| MLME-START request/confirm | Unstarred; static configured endpoints do not establish this service. Managed start remains later debt; classify non-beacon prerequisites before M2. |
| MLME-BEACON request/confirm/indication and BEACON-REQUEST indication | Unstarred; audit procedure predicates with passive scan and coordinator response. Neither a blanket M1 obligation nor a justified exclusion follows from the table alone. |
| MLME-SCAN request/confirm | Starred interface; passive-scan capability remains mandatory under 6.4.1.1. Retain M2 debt. |
| Other starred services | Their optional interface marker is not a substitute for checking dependencies of selected behaviors. Association, indirect transfer and optional mode procedures remain separately gated. |

Annex E is explicitly **informative** (`ieee802154-2024:clause:E`, p. 941,
`@3502812:3502940`). Its PICS tables are a completeness cross-check, not normative overrides.
E.7.3 (p. 945, `@3511511:3512657`) marks extended addressing mandatory and short/enhanced
support optional. E.7.5.1 (pp. 951–954, `@3534457:3546133`) marks data service, access,
validation, acknowledged delivery, unsecured mode and passive scan mandatory. E.7.5.2
(pp. 954–957, `@3546133:3555819`) marks data, ACK and command format mandatory, while
individual commands have their own predicates. Command-format debt needs explicit treatment;
marking every command optional would miss this distinction.

E.7.4.2 (pp. 945–946, `@3512820:3515260`) labels generic ED/LQI optional, but selected
O-QPSK clauses 13.3.12/13.3.13 require them. Preserve those PHY-specific requirements.
The PICS M/O labels do not alter the normative catalog or establish a full-device claim.

## PHY measurement boundary

The selected 2450 MHz O-QPSK profile brings all applicable clause 13.3 RF requirements, not
just packet timing. Clauses 13.3.1–13.3.13 occupy physical PDF pp. 618–621,
`ieee802154-2024@2462791:2469312`; Table 11-30 (p. 594, `@2365464:2366669`)
provides sensitivity-test conditions. Sensitivity is −85 dBm or better at PER below 1%, with
20-octet random PSDUs and no interference. This is not established by setting a receiver threshold.
PSD mask, interference rejection, EVM, frequency/symbol tolerance, actual transmit power and
maximum input level need independent PHY validation. They remain outside the M1 packet-level
engineering evidence claim and owed for any corresponding PHY-conformance claim.

Table 12-3 (p. 603, `@2396656:2397655`) closes the peer-power field extraction.
ED averaging is eight symbol periods or the shorter PHY-specified CCA duration (11.2.6);
CCA Mode 1's energy decision and ED's quantitative average are distinct observations.
The [feasibility inspection](results.md#phy-feasibility-inspection) identifies suitable extension
points and the missing timed contract. A Boolean listening decision does not prove ED calibration.

## Remaining closure work

These are **open audit work**, not justified exclusions. No zero-unresolved claim is made.

1. Pin the IEEE Std 802 normative group-address definition. Clause 2's reference is undated;
   no companion normative text is available in the local corpus. Clause 6.2 independently defines
   extended broadcast as all ones, but 6.6.2(d)(2) still gates extended-group acceptance on
   macGroupRxMode. Do not give extended broadcast the unconditional short-broadcast predicate.
2. Close remaining structural/security rejection and service-generation predicates, particularly
   COMM-STATUS and the boundary between malformed secured input and valid unsupported security.
   Record explicit unsupported-capability outcomes before the 1a contract is ready.
3. Classify the remaining clause-6 descendants and transitive references for the selected roles.
   Fifty catalog source nodes now yield 122 resolved extracted edges; this establishes target
   resolution, not semantic closure. Table-cell references require manual inventory as well.
   Reconcile beacon/command/start dependencies with the bounded M1 claim and retain M2 debt.
4. Finish selected channel-descriptor and timing dependencies. Concrete PIB mutation/notification,
   reset/configuration and timing ownership belong to 1b/1c, after their normative inputs close.
   A general propagation bound is absent from the current medium API; 1c must specify supported
   propagation assumptions and an explicit ACK allowance before timing behavior is implemented.
5. Complete consumer/adapter classification and admit independent capture vectors for their
   intended claims. The existing capture has incomplete provenance; it cannot serve as the sole
   codec oracle. Production integration proof remains the deliverable of 1d, not this audit.

Until these are closed, step 0 remains in progress. No executable standard-derived check has run.
