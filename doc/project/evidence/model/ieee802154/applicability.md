# IEEE 802.15.4 — M1 applicability audit

> **Kind:** report · **Status:** snapshot 2026-09-19 · **Seal:** none · **Owns:** — · **Stands on:** [coverage.md](coverage.md), [catalog.md](../../standard/ieee802154/catalog.md)

Baseline: `fd6f800222`. This is the first extraction pass for step 0 of the
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
capture observer is not an MCPS data indication. Promiscuous capture semantics are a model policy
until an applicable normative predicate is established.

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

The remaining full-address predicate audit must expand this table for PAN-coordinator source-only
traffic, implicit broadcast, group reception and every selected receive mode. Lifecycle and packet
ownership outcomes are implementation-contract obligations in step 1, not extra IEEE statements.

## Interpretations and prerequisites

| Item | Evidence and disposition | Gate affected |
| --- | --- | --- |
| IFS wording and figure | 6.3.1 (PDF p. 62) gives an “at least AIFS” lower bound after ACK; Figure 6-1 (p. 63) shows size-dependent max(SIFS/LIFS, turnaround). A size-dependent interval satisfies the textual lower bound too. Use both constraints; do not incorrectly replace the longer interval with AIFS. Record exact owner and start event in 1c. | 1c, 4–6 |
| ACK expected time | 6.6.3.4 says “within the expected time”. Searches for `macAckWaitDuration` and `AckWait` returned no corpus matches. A numeric timeout, propagation allowance and simultaneous-event rule remain to be justified from timing clauses/model assumptions; do not call a copied older-edition constant a 2024 requirement. | 1c, 6 |
| Incoming secured legacy ACK | 6.6.2 ACK eligibility precedes the security outcome controlling delivery. 9.2.4(a)/(b) returns before key lookup when legacy/disabled. Preserve framing and ACK decision even when security is unsupported. | 2, 6 |
| Invalid security outputs | 9.2.4 declares outputs invalid until explicitly set; early failure cannot justify reading uninitialized security descriptors. Define indication validity explicitly. | 1a, 6 |
| Legacy encoding rules | 7.2.2.6 selects compression based on equal/different PAN IDs; Table 7-3 includes source-only coordinator semantics. Version-2 Table 7-2 is not a legacy oracle. | 2 |
| Native-address framework | `NetworkInterface` keeps generic `MacAddress`, but has attached `InterfaceProtocolData`. This establishes a candidate extension point, not a proven compatible interface. | 1d |
| Energy observations | Mode 1 requires energy detection without a decoded frame. Current medium feasibility has not been established by this documentary pass. | 4 |

## Remaining closure work

The following are **open audit work**, not justified exclusions. No zero-unresolved claim is made.

1. Finish clause/statement extraction for complete 6.6.2 address predicates, 7.2 reserved fields,
   sequence/IE restrictions, full immediate-ACK continuation and 7.2.11 CRC/bit-order vectors.
2. Extract all selected rows and access rules of MAC/PHY PIB tables, service parameter/status
   tables and reset semantics, including cross-attribute ranges and unsupported-value outcomes.
3. Close channel/page, turnaround, ED mapping/saturation, LQI and PPDU/FCS size dependencies;
   separate packet-level behavior from waveform/sensitivity validation claims.
4. Classify the transitive normative references for the selected roles. The 66 resolved edges
   below only prove target resolution, not semantic dependency closure; corpus extraction also
   misses some prose references, so manually inspect each complete clause and its continuations.
5. Finish the selected-role mandatory-service inventory beyond the known passive-scan obligation.
   Catalog the full deferred scan procedure before claiming M2 coverage; retain all discovered debt.
6. Finish capture provenance and native consumer/adapter classification, then assign the concrete
   1a service surface and 1c timing ownership. Current source inspection is not a production fixture.

Until these are closed, step 0 is in progress and its exit criterion does not authorize reporting
step 1 as ready. No executable standard-derived check has run.
