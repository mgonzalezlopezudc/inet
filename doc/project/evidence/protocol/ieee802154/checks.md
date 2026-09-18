# IEEE 802.15.4 — selected English checks

> **Kind:** procedure · **Status:** draft · **Seal:** none · **Owns:** IEEE802154-C-* · **Stands on:** [features.md](features.md)

These procedures specify observations, not executable tests or verdicts. Each check verifies its
stimulus at the observed boundary; absence alone cannot establish correct rejection. Timing
observations require a declared resolution and uncertainty appropriate to the tested interface.
The scope notes identify additional clauses needed before a complete feature claim.

Common topology (O is a passive observer unless injection is explicitly requested):

```text
upper client U -> device A <---- radio link ----> device B -> upper observer
                            O                 energy source J
```

## Index

- [IEEE802154-C-WIRE](#ieee802154-c-wire) — Native address and legacy data/ACK encoding.
- [IEEE802154-C-ACCESS](#ieee802154-c-access) — Unslotted channel access.
- [IEEE802154-C-SEQUENCE](#ieee802154-c-sequence) — Device-wide sequence allocation and wrap.
- [IEEE802154-C-RECEIVE](#ieee802154-c-receive) — Filtering, immediate ACK eligibility and data indication.
- [IEEE802154-C-ACK](#ieee802154-c-ack) — Acknowledgment and direct/indirect retry distinction.
- [IEEE802154-C-SECURITY](#ieee802154-c-security) — Unsecured-profile security outcomes.
- [IEEE802154-C-PHY](#ieee802154-c-phy) — O-QPSK timing, CCA, ED and LQI.
- [IEEE802154-C-SERVICE](#ieee802154-c-service) — PIB access, reset and request capacity.
- [IEEE802154-C-SCAN](#ieee802154-c-scan) — Passive scan obligation and channel order.

## IEEE802154-C-WIRE

**Native address and legacy data/ACK encoding.**

Checks: [IEEE802154-ADDRESS-1](../../standard/ieee802154/catalog.md#ieee802154-address-1), [IEEE802154-ADDRESS-2](../../standard/ieee802154/catalog.md#ieee802154-address-2), [IEEE802154-WIRE-1](../../standard/ieee802154/catalog.md#ieee802154-wire-1), [IEEE802154-WIRE-2](../../standard/ieee802154/catalog.md#ieee802154-wire-2), [IEEE802154-WIRE-3](../../standard/ieee802154/catalog.md#ieee802154-wire-3), [IEEE802154-WIRE-4](../../standard/ieee802154/catalog.md#ieee802154-wire-4), [IEEE802154-WIRE-5](../../standard/ieee802154/catalog.md#ieee802154-wire-5), [IEEE802154-WIRE-6](../../standard/ieee802154/catalog.md#ieee802154-wire-6); strengths are retained in those entries.

Mockup: Two devices A and B with native short and extended identities, an octet observer O, and an independent frame decoder. Configure one PAN and then two distinct PAN IDs.

Procedure:

1. Encode AC-DE-48-23-45-67-89-01 and AC-DE-48-23-45-67-89-02, then two addresses differing only in their high 16 bits.
2. For legacy versions, enumerate short/short, short/extended, extended/short and extended/extended pairs, with equal and unequal PAN IDs. Repeat with source-only and destination-only addressing.
3. Request a legacy data transfer containing 00 FF 01 02 80 as payload. Observe the frame and delivered bytes independently.
4. Supply an independently authored immediate-ACK frame and verify the address-free layout. Decode truncated input at each supported field boundary as a robustness companion, without treating that companion as a new normative status requirement.

Expected observations:

1. The observer sees the requested source/destination identities, including distinct high address bits; AC-DE-48-23-45-67-89-01 is encoded as 01 89 67 45 23 48 DE AC.
2. Equal-PAN dual addressing includes only the destination PAN; unequal-PAN dual addressing includes both PANs. Single addressing includes its own PAN with compression clear.
3. LegacyTx produces version 1 data and the indicated MSDU is exactly 00 FF 01 02 80.
4. The immediate ACK contains FCF and DSN followed by the selected FCS, without source or destination addresses.

Arithmetic and scope: For unsecured legacy frames, MHR length is 3 plus the present PAN fields (2 octets each) and address fields (2 or 8 each). Same-PAN short/short therefore has a 9-octet MHR; different-PAN extended/extended has a 23-octet MHR. With O-QPSK two-octet FCS the latter has at most 102 payload octets in a 127-octet PSDU. A symmetric codec round trip is insufficient; use independent octets. CRC vectors and reserved-field semantics require their own extraction before this becomes a complete codec test.

## IEEE802154-C-ACCESS

**Unslotted channel access.**

Checks: [IEEE802154-ACCESS-1](../../standard/ieee802154/catalog.md#ieee802154-access-1), [IEEE802154-ACCESS-2](../../standard/ieee802154/catalog.md#ieee802154-access-2); strengths are retained in those entries.

Mockup: A transmits to B in a nonperiodic-beacon PAN. An energy source J controls whether each CCA observes busy or idle; O observes CCA decisions, backoff state and actual transmissions.

Procedure:

1. Set macMinBe=0, a valid macMaxBe, and macMaxCsmaBackoffs=0. Keep J active across the CCA observation.
2. Repeat with macMaxCsmaBackoffs=1: make the first observation busy, then make the next idle. Repeat with both busy.
3. Use a nonzero initial exponent and force enough busy results to reach the exponent cap. Start a fresh attempt after termination.

Expected observations:

1. The first observed CCA in the limit-zero case is busy; NB becomes 1 and the attempt fails without a data transmission.
2. At limit one, NB=1 permits another iteration. The second idle CCA permits transmission; a second busy CCA makes NB=2 and fails.
3. Backoff values remain in the Figure 6-2 range, BE saturates at macMaxBe, and a new attempt restores NB=0 and BE=macMinBe.

Arithmetic and scope: Figure 6-2 uses NB > macMaxCsmaBackoffs, not equality. Thus persistently busy access permits limit+1 CCA observations. Measure backoff in macUnitBackoffPeriod, separately from CCA observation time. This is an algorithm check, not a claim about contention fairness.

## IEEE802154-C-SEQUENCE

**Device-wide sequence allocation and wrap.**

Checks: [IEEE802154-SEQUENCE-1](../../standard/ieee802154/catalog.md#ieee802154-sequence-1), [IEEE802154-SEQUENCE-2](../../standard/ieee802154/catalog.md#ieee802154-sequence-2); strengths are retained in those entries.

Mockup: A sends data alternately to B and C; O captures serialized data and immediate ACKs. All peers remain reachable.

Procedure:

1. Arrange or observe the DSN just before wrap; retain the actual initial value as stimulus evidence.
2. Send more than 256 fresh acknowledged MSDUs while alternating B and C. Include DSNs 254, 255, 0 and 1.
3. Lose an ACK at the wrap boundary and distinguish the resulting retry from the next fresh MSDU. Restart a peer and repeat fresh transfers.

Expected observations:

1. O confirms the intended DSN boundary on actual transmitted octets.
2. Fresh transmissions progress in one device-wide modulo-256 sequence, irrespective of destination.
3. The retry retains its original DSN; the next fresh frame advances once. Upper indications remain usable across wrap and expose the received DSN.

Arithmetic and scope: The DSN is not a universal duplicate identifier. These checks do not require a particular duplicate cache or promise exactly-once application delivery. The retry assertion is also covered by IEEE802154-C-ACK.

## IEEE802154-C-RECEIVE

**Filtering, immediate ACK eligibility and data indication.**

Checks: [IEEE802154-RECEIVE-1](../../standard/ieee802154/catalog.md#ieee802154-receive-1), [IEEE802154-RECEIVE-2](../../standard/ieee802154/catalog.md#ieee802154-receive-2), [IEEE802154-RECEIVE-3](../../standard/ieee802154/catalog.md#ieee802154-receive-3), [IEEE802154-RECEIVE-4](../../standard/ieee802154/catalog.md#ieee802154-receive-4); strengths are retained in those entries.

Mockup: A sends to B; O can inject exact frames and observe both the link and B's upper indications. Each injection is confirmed at the receive boundary.

Procedure:

1. Inject valid local-PAN unicast data with AR set, then change only destination PAN to foreign and broadcast values, repairing the FCS each time.
2. Corrupt a data bit without updating the FCS. Repeat with correct FCS as a positive control.
3. Inject a structurally valid version-1 secured frame, with security disabled at B, using the same local unicast addressing and AR.
4. Compare ACK production with payload delivery and security-error indication for every case.

Expected observations:

1. The original and broadcast-PAN/local-address controls reach B and produce the required ACK and unsecured payload indication.
2. The foreign-PAN frame is rejected. The incorrect-FCS frame is discarded and produces neither delivery nor immediate ACK.
3. The valid legacy secured frame remains ACK-eligible under 6.6.2, while its unsuccessful incoming security result prevents MSDU delivery.

Arithmetic and scope: A broadcast PAN ID alone does not make the destination address broadcast. Separate the two predicates. Complete source/destination omission, coordinator and group-address cases must be added from all branches of 6.6.2 before claiming exhaustive filtering coverage. Promiscuous capture is not automatically a normative MAC delivery mode.

## IEEE802154-C-ACK

**Acknowledgment and direct/indirect retry distinction.**

Checks: [IEEE802154-ACK-1](../../standard/ieee802154/catalog.md#ieee802154-ack-1), [IEEE802154-ACK-2](../../standard/ieee802154/catalog.md#ieee802154-ack-2), [IEEE802154-ACK-3](../../standard/ieee802154/catalog.md#ieee802154-ack-3), [IEEE802154-ACK-4](../../standard/ieee802154/catalog.md#ieee802154-ack-4), [IEEE802154-ACK-5](../../standard/ieee802154/catalog.md#ieee802154-ack-5); strengths are retained in those entries.

Mockup: A sends to B with observer O and selective ACK loss. For the indirect variant, coordinator A already holds a pending frame and B issues Data Requests.

Procedure:

1. Transmit broadcast and unicast AR-clear data. Confirm the outgoing AR bit and observe completion without ACK.
2. Transmit AR-set unicast outside CAP, observe the received data end and ACK start, and compare the ACK DSN with the data DSN.
3. Suppress every ACK for a direct transmission with macMaxFrameRetries=0 and then 2. Include wrong-DSN and late-ACK injections in separate runs.
4. For a pending indirect frame, suppress the data ACK after one poll. Observe silence before a new poll, then request again.

Expected observations:

1. Broadcast traffic has AR=0; AR-clear unicast is delivered without generating ACK or autonomous retransmissions.
2. For O-QPSK, ACK starts macSifsPeriod after the last received data symbol and carries the data DSN.
3. Direct transmission attempts number 1 and 3 respectively; every retry retains DSN, and exhaustion reports NO_ACK. Wrong/late ACKs do not establish success for an unrelated exchange.
4. Failed indirect delivery does not retry by itself. A new Data Request permits sending the retained frame with the same DSN.

Arithmetic and scope: At 250 kbit/s and 4 bits/symbol, one symbol is 16 microseconds; 12-symbol AIFS is 192 microseconds. Specify the sender's expected-time deadline and event ordering before implementing late-ACK tests: 6.6.3.4 does not itself provide a numeric timeout. Full poll/release timing belongs to a separate indirect-service extraction.

## IEEE802154-C-SECURITY

**Unsecured-profile security outcomes.**

Checks: [IEEE802154-SECURITY-1](../../standard/ieee802154/catalog.md#ieee802154-security-1), [IEEE802154-SECURITY-2](../../standard/ieee802154/catalog.md#ieee802154-security-2), [IEEE802154-SECURITY-3](../../standard/ieee802154/catalog.md#ieee802154-security-3), [IEEE802154-SECURITY-4](../../standard/ieee802154/catalog.md#ieee802154-security-4), [IEEE802154-SECURITY-5](../../standard/ieee802154/catalog.md#ieee802154-security-5); strengths are retained in those entries.

Mockup: A and B have macSecurityEnabled=false. O observes outgoing requests/results and injects structurally valid secured and unsecured legacy frames.

Procedure:

1. Submit SecurityLevel=0 and nonzero SecurityLevel requests at A.
2. Inject local addressed unsecured data at B.
3. Inject secured version-0 and secured version-1 frames at B, each otherwise valid and AR-set.

Expected observations:

1. SecurityLevel=0 returns the unchanged frame with SUCCESS; a nonzero request reports UNSUPPORTED_SECURITY and does not transmit.
2. Unsecured reception returns SUCCESS without requiring key lookup.
3. Secured version 0 reports UNSUPPORTED_LEGACY; secured version 1 reports UNSUPPORTED_SECURITY. Neither supplies a valid plaintext MSDU. Required legacy ACK behavior is checked independently by IEEE802154-C-RECEIVE.

Arithmetic and scope: Do not read output security parameters that the procedure has not initialized. Malformed/truncated security representations need structural safety checks separately; these well-formed-input checks do not assign them invented standardized statuses.

## IEEE802154-C-PHY

**O-QPSK timing, CCA, ED and LQI.**

Checks: [IEEE802154-PHY-1](../../standard/ieee802154/catalog.md#ieee802154-phy-1), [IEEE802154-PHY-2](../../standard/ieee802154/catalog.md#ieee802154-phy-2), [IEEE802154-PHY-3](../../standard/ieee802154/catalog.md#ieee802154-phy-3), [IEEE802154-PHY-4](../../standard/ieee802154/catalog.md#ieee802154-phy-4), [IEEE802154-PHY-5](../../standard/ieee802154/catalog.md#ieee802154-phy-5), [IEEE802154-PHY-6](../../standard/ieee802154/catalog.md#ieee802154-phy-6), [IEEE802154-PHY-7](../../standard/ieee802154/catalog.md#ieee802154-phy-7), [IEEE802154-PHY-8](../../standard/ieee802154/catalog.md#ieee802154-phy-8); strengths are retained in those entries.

Mockup: PHY A receives from B and energy source J on the selected 2450 MHz channel. O observes timed PHY operations and PPDU boundaries.

Procedure:

1. Generate a PPDU with a known PSDU length L, including FCS; inspect SHR, PHR and total duration.
2. With no decodable frame, apply J below and above the Mode-1 threshold throughout CCA. Repeat CCA during a confirmed PPDU reception.
3. Configure two CCA durations, including a value not divisible by 16 microseconds, and measure the observation interval.
4. Apply stepped power during an ED observation and then sweep packet quality over a range for LQI.

Expected observations:

1. O confirms the PSDU bytes and length L; preamble is 4 zero octets, PHR counts L, and total PPDU duration is (6+L)*32 microseconds for the selected PHY.
2. Energy above threshold makes Mode 1 busy even without a decodable frame; a request during the defined PPDU reception interval reports busy.
3. CCA duration equals the configured microseconds without silently rounding the observation itself to whole symbols.
4. ED uses the specified averaging interval and LQI is present for received packets with at least eight distinct values.

Arithmetic and scope: The 6-octet PHY overhead is preamble 4, SFD 1 and PHR 1. ED measurement time and CSMA backoff rounding are different rules. Define the complete ED power mapping and saturation from 11.2.6 before asserting numeric ED results. Waveform, sensitivity and PER conformance require separate validation.

## IEEE802154-C-SERVICE

**PIB access, reset and request capacity.**

Checks: [IEEE802154-SERVICE-1](../../standard/ieee802154/catalog.md#ieee802154-service-1), [IEEE802154-PIB-1](../../standard/ieee802154/catalog.md#ieee802154-pib-1), [IEEE802154-PIB-2](../../standard/ieee802154/catalog.md#ieee802154-pib-2), [IEEE802154-PIB-3](../../standard/ieee802154/catalog.md#ieee802154-pib-3); strengths are retained in those entries.

Mockup: An upper client U issues requests to MAC A; O observes returned statuses, PIB values and transmissions. Select a known read-only attribute and a writable attribute with documented bounds/default.

Procedure:

1. GET an absent attribute and SET a read-only attribute. Read the latter again.
2. Set a writable attribute to a nondefault valid value, reset with SetDefaultPib=false, read back, then reset with true and read back.
3. Send an MSDU at the selected addressing-dependent capacity and another one octet larger. Observe exact frame size and terminal statuses.

Expected observations:

1. Absent GET reports UNSUPPORTED_ATTRIBUTE with no valid value; read-only SET reports READ_ONLY without changing the attribute.
2. The false reset preserves MAC PIB values; true reset restores specified defaults; successful reset completes with SUCCESS.
3. The boundary frame fits the selected PSDU limit. The oversized request reports FRAME_TOO_LONG and produces no frame transmission.

Arithmetic and scope: For same-PAN short/short unsecured O-QPSK data, capacity is 127-9-2=116 octets. For different-PAN extended/extended it is 127-23-2=102. Full status precedence and validation need the service tables, not one generic failure result. Reset PHY details are implementation-dependent; preserve this distinction from MAC PIB defaults.

## IEEE802154-C-SCAN

**Passive scan obligation and channel order.**

Checks: [IEEE802154-SCAN-1](../../standard/ieee802154/catalog.md#ieee802154-scan-1), [IEEE802154-SCAN-2](../../standard/ieee802154/catalog.md#ieee802154-scan-2); strengths are retained in those entries.

Mockup: Device A scans channels containing beacon sources B and C. O observes A's channel visits, transmissions and scan result.

Procedure:

1. Request passive scan with an unordered channel set containing both sources and an empty channel.
2. Observe the actual channel order and relevant received beacons, then inspect the terminal scan result.

Expected observations:

1. A performs passive scanning and does not replace it with active solicitation.
2. Visits proceed from lowest to highest selected channel; discovered descriptors are returned on completion.

Arithmetic and scope: This check covers the universal capability and order only. Duration, result limits, cancellation, invalid-channel handling and state restoration need the passive-scan and MLME-SCAN child clauses before full scan acceptance.

## Statements without a procedure

None in the current bounded catalog. This does not cover statements not yet extracted; see the
[catalog extraction boundary](../../standard/ieee802154/catalog.md#extraction-boundary).
