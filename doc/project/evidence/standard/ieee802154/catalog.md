# IEEE 802.15.4 — selected base-standard statements

> **Kind:** what · **Status:** draft · **Seal:** none · **Owns:** IEEE802154-ADDRESS-*, IEEE802154-WIRE-*, IEEE802154-ACCESS-*, IEEE802154-SEQUENCE-*, IEEE802154-RECEIVE-*, IEEE802154-ACK-*, IEEE802154-SECURITY-*, IEEE802154-PHY-*, IEEE802154-SERVICE-*, IEEE802154-PIB-*, IEEE802154-SCAN-* · **Stands on:** [source.md](source.md)

Edition: **IEEE Std 802.15.4-2024**, without amendments. Entries below are a bounded extraction,
not an exhaustive mandatory-statement inventory. Source excerpts retain normative wording;
conditions prevent an excerpt from being applied outside its full clause. All cited clauses,
tables and figures are normative; explanatory notes are not promoted to requirements.
Physical PDF pages are one-based. Canonical locators refer to the pinned local corpus.
Model applicability and execution evidence live in the [model ledger](../../model/ieee802154/coverage.md).

## Index

| ID | Statement |
| --- | --- |
| [IEEE802154-ADDRESS-1](#ieee802154-address-1) | Device extended identity uses EUI-64. |
| [IEEE802154-ADDRESS-2](#ieee802154-address-2) | Address octets are transmitted from rightmost to leftmost. |
| [IEEE802154-WIRE-1](#ieee802154-wire-1) | Legacy same-PAN dual addressing omits the source PAN. |
| [IEEE802154-WIRE-2](#ieee802154-wire-2) | Legacy different-PAN dual addressing includes both PAN IDs. |
| [IEEE802154-WIRE-3](#ieee802154-wire-3) | Legacy single addressing includes its PAN without compression. |
| [IEEE802154-WIRE-4](#ieee802154-wire-4) | Immediate ACKs have the Figure 7-15 layout. |
| [IEEE802154-WIRE-5](#ieee802154-wire-5) | A data payload preserves the supplied octets. |
| [IEEE802154-WIRE-6](#ieee802154-wire-6) | LegacyTx selects version 1 data. |
| [IEEE802154-ACCESS-1](#ieee802154-access-1) | Initialize attempt-local backoff state. |
| [IEEE802154-ACCESS-2](#ieee802154-access-2) | Nonperiodic-beacon transmission follows successful unslotted access. |
| [IEEE802154-SEQUENCE-1](#ieee802154-sequence-1) | Sequence allocation is device-wide. |
| [IEEE802154-SEQUENCE-2](#ieee802154-sequence-2) | Sequence counters wrap at their representable maximum. |
| [IEEE802154-RECEIVE-1](#ieee802154-receive-1) | An incorrect FCS causes discard. |
| [IEEE802154-RECEIVE-2](#ieee802154-receive-2) | A present destination PAN must match or be broadcast. |
| [IEEE802154-RECEIVE-3](#ieee802154-receive-3) | Valid legacy unicast AR frames require immediate ACK. |
| [IEEE802154-RECEIVE-4](#ieee802154-receive-4) | Data delivery requires successful incoming security processing. |
| [IEEE802154-ACK-1](#ieee802154-ack-1) | Broadcast and group-addressed frames do not request ACK. |
| [IEEE802154-ACK-2](#ieee802154-ack-2) | AR-clear frames receive no ACK. |
| [IEEE802154-ACK-3](#ieee802154-ack-3) | Outside-CAP ACK start is measured from the received last symbol. |
| [IEEE802154-ACK-4](#ieee802154-ack-4) | Direct retransmissions retain the DSN. |
| [IEEE802154-ACK-5](#ieee802154-ack-5) | Failed indirect transmission waits for a new poll. |
| [IEEE802154-SECURITY-1](#ieee802154-security-1) | Security level zero returns the outgoing frame unchanged. |
| [IEEE802154-SECURITY-2](#ieee802154-security-2) | A nonzero outgoing security request fails when security is disabled. |
| [IEEE802154-SECURITY-3](#ieee802154-security-3) | Incoming secured version zero reports unsupported legacy security. |
| [IEEE802154-SECURITY-4](#ieee802154-security-4) | Incoming nonlegacy secured frames fail when security is disabled. |
| [IEEE802154-SECURITY-5](#ieee802154-security-5) | Unsecured input succeeds when security is disabled. |
| [IEEE802154-PHY-1](#ieee802154-phy-1) | CCA Mode 1 detects energy above its threshold. |
| [IEEE802154-PHY-2](#ieee802154-phy-2) | A CCA request during PPDU reception reports busy. |
| [IEEE802154-PHY-3](#ieee802154-phy-3) | CCA observes for phyCcaDuration. |
| [IEEE802154-PHY-4](#ieee802154-phy-4) | ED averages over the selected measurement interval. |
| [IEEE802154-PHY-5](#ieee802154-phy-5) | LQI uses at least eight values. |
| [IEEE802154-PHY-6](#ieee802154-phy-6) | O-QPSK preamble is eight zero symbols. |
| [IEEE802154-PHY-7](#ieee802154-phy-7) | PHR length counts the complete PSDU. |
| [IEEE802154-PHY-8](#ieee802154-phy-8) | 2450 MHz O-QPSK operates at 250 kbit/s. |
| [IEEE802154-SERVICE-1](#ieee802154-service-1) | Oversize MPDUs fail before transmission. |
| [IEEE802154-PIB-1](#ieee802154-pib-1) | Dagger-marked MAC attributes are read-only to the upper layer. |
| [IEEE802154-PIB-2](#ieee802154-pib-2) | Unknown PIB reads return unsupported attribute. |
| [IEEE802154-PIB-3](#ieee802154-pib-3) | Reset can preserve or restore MAC PIB values. |
| [IEEE802154-SCAN-1](#ieee802154-scan-1) | All devices support passive scan. |
| [IEEE802154-SCAN-2](#ieee802154-scan-2) | Scan visits channels in ascending order. |

## IEEE802154-ADDRESS-1

**Device extended identity uses EUI-64.**

- Source: `ieee802154-2024:clause:7.1`, physical PDF pp. 78–78; `ieee802154-2024@359535:359817`.
- Source excerpt: “A device’s extended address shall be an extended unique identifier (EUI-64), as defined by IEEE Std 802”
- Strength: `shall`; observation class: `encoding`.
- Condition: Device extended address.
- Check idea: Encode identities that differ only in the high 16 bits and verify they remain distinct.

## IEEE802154-ADDRESS-2

**Address octets are transmitted from rightmost to leftmost.**

- Source: `ieee802154-2024:clause:4.5.1`, physical PDF pp. 50–51; `ieee802154-2024@263734:264671`.
- Source excerpt: “For every address field, the bit transmission order shall be performed from the right most octet (RMO) to the left most octet (LMO) in the field, and inside an octet from the LSB to the MSB.”
- Strength: `shall`; observation class: `encoding`.
- Condition: Every address field.
- Check idea: Check AC-DE-48-23-45-67-89-01 against octets 01 89 67 45 23 48 DE AC.

## IEEE802154-WIRE-1

**Legacy same-PAN dual addressing omits the source PAN.**

- Source: `ieee802154-2024:clause:7.2.2.6`, physical PDF pp. 80–80; `ieee802154-2024@367029:368560`.
- Source excerpt: “If the PAN IDs are identical, the PAN ID Compression field shall be set to one, and the Source PAN ID field shall be omitted from the transmitted frame.”
- Strength: `shall`; observation class: `encoding`.
- Condition: Frame version 0 or 1; both addresses present and PAN IDs equal.
- Check idea: Enumerate short/extended address pairs and verify compression and exact offsets.

## IEEE802154-WIRE-2

**Legacy different-PAN dual addressing includes both PAN IDs.**

- Source: `ieee802154-2024:clause:7.2.2.6`, physical PDF pp. 80–80; `ieee802154-2024@367029:368560`.
- Source excerpt: “If the PAN IDs are different, the PAN ID Compression field shall be set to zero, and both Destination PAN ID field and Source PAN ID fields shall be included in the transmitted frame.”
- Strength: `shall`; observation class: `encoding`.
- Condition: Frame version 0 or 1; both addresses present and PAN IDs different.
- Check idea: Use distinct PAN values and independently verify both two-octet fields.

## IEEE802154-WIRE-3

**Legacy single addressing includes its PAN without compression.**

- Source: `ieee802154-2024:clause:7.2.2.6`, physical PDF pp. 80–80; `ieee802154-2024@367029:368560`.
- Source excerpt: “If only either the destination or the source addressing information is present, the PAN ID Compression field shall be set to zero, and the PAN ID field of the single address shall be included in the transmitted frame.”
- Strength: `shall`; observation class: `encoding`.
- Condition: Frame version 0 or 1; exactly one address present.
- Check idea: Exercise each direction with short and extended addresses.

## IEEE802154-WIRE-4

**Immediate ACKs have the Figure 7-15 layout.**

- Source: `ieee802154-2024:clause:7.3.3`, physical PDF pp. 91–91; `ieee802154-2024@403772:404594`.
- Source excerpt: “The Imm-Ack frame shall be formatted as illustrated in Figure 7-15.”
- Strength: `shall`; observation class: `encoding`.
- Condition: Immediate ACK.
- Check idea: Verify FCF, DSN and the PHY-selected FCS with no address fields; inspect Figure 7-15 and continuation after Figure 7-16.

## IEEE802154-WIRE-5

**A data payload preserves the supplied octets.**

- Source: `ieee802154-2024:clause:7.3.2.3`, physical PDF pp. 91–91; `ieee802154-2024@403577:403772`.
- Source excerpt: “The payload of a Data frame shall contain the sequence of octets that the next higher layer has requested the MAC sublayer to transmit.”
- Strength: `shall`; observation class: `end-to-end`.
- Condition: Data frame.
- Check idea: Send octets containing zeros and apparent header patterns and compare the received MSDU byte-for-byte.

## IEEE802154-WIRE-6

**LegacyTx selects version 1 data.**

- Source: `ieee802154-2024:table:8-30`, physical PDF pp. 144–146; `ieee802154-2024@625153:628609`, `ieee802154-2024@628609:632178`.
- Source excerpt: “If LegacyTx of TxOptions is TRUE, the Data frame sent shall use the Frame Version field set to 0b01 and use the IEEE Std 802.15.4-2006 format.”
- Strength: `shall`; observation class: `wire`.
- Condition: LegacyTx true; data transmission.
- Check idea: Observe the transmitted FCF version independently of the request representation.

## IEEE802154-ACCESS-1

**Initialize attempt-local backoff state.**

- Source: `ieee802154-2024:clause:6.3.2.1`, physical PDF pp. 63–64; `ieee802154-2024@306402:310168`.
- Source excerpt: “BE shall be initialized to the value of macMinBe.”
- Strength: `shall`; observation class: `internal`.
- Condition: New CSMA-CA attempt, including a direct retransmission.
- Check idea: Verify NB=0 and BE=macMinBe at each attempt, then follow Figure 6-2 on idle and busy CCA results.

## IEEE802154-ACCESS-2

**Nonperiodic-beacon transmission follows successful unslotted access.**

- Source: `ieee802154-2024:clause:6.6.1`, physical PDF pp. 69–70; `ieee802154-2024@325413:329415`.
- Source excerpt: “If the frame is to be transmitted on a PAN not using periodic beacons, the frame shall be transmitted following the successful application of the unslotted version of the CSMA-CA algorithm, as described in 6.3.2.”
- Strength: `shall`; observation class: `wire`.
- Condition: Data transmission in a non-beacon PAN.
- Check idea: Keep the medium busy through the limit and verify no data transmission; release it before exhaustion and observe transmission.

## IEEE802154-SEQUENCE-1

**Sequence allocation is device-wide.**

- Source: `ieee802154-2024:clause:6.6.1`, physical PDF pp. 69–70; `ieee802154-2024@325413:329415`.
- Source excerpt: “Each device shall generate exactly one Sequence Number regardless of the number of unique devices with which it wishes to communicate.”
- Strength: `shall`; observation class: `wire`.
- Condition: Generated frames using the same sequence space.
- Check idea: Alternate destinations and verify a single progressing DSN sequence.

## IEEE802154-SEQUENCE-2

**Sequence counters wrap at their representable maximum.**

- Source: `ieee802154-2024:clause:6.6.1`, physical PDF pp. 69–70; `ieee802154-2024@325413:329415`.
- Source excerpt: “The sequence numbers stored in the MAC PIB attributes shall roll over to zero after reaching the maximum value representable.”
- Strength: `shall`; observation class: `wire`.
- Condition: Sequence counter reaches its maximum.
- Check idea: Drive DSN through 254, 255, 0, 1 over a byte boundary and observe continued delivery.

## IEEE802154-RECEIVE-1

**An incorrect FCS causes discard.**

- Source: `ieee802154-2024:clause:6.6.2`, physical PDF pp. 70–72; `ieee802154-2024@329415:338486`.
- Source excerpt: “The MAC sublayer shall discard all received frames that do not contain a correct value in their frame check sequence (FCS) field in the MFR, as described in 7.2.11.”
- Strength: `shall`; observation class: `end-to-end`.
- Condition: Received frames with an MFR.
- Check idea: Inject a one-bit corruption without repairing FCS; confirm the bytes arrived and that no MSDU or ACK is emitted.

## IEEE802154-RECEIVE-2

**A present destination PAN must match or be broadcast.**

- Source: `ieee802154-2024:clause:6.6.2`, physical PDF pp. 70–72; `ieee802154-2024@329415:338486`.
- Source excerpt: “If a destination PAN ID is included in the frame, it shall match macPanId or shall be the broadcast PAN ID.”
- Strength: `shall`; observation class: `end-to-end`.
- Condition: Not scanning; destination PAN present.
- Check idea: Compare local, broadcast and foreign PAN frames with valid FCS and otherwise identical fields.

## IEEE802154-RECEIVE-3

**Valid legacy unicast AR frames require immediate ACK.**

- Source: `ieee802154-2024:clause:6.6.2`, physical PDF pp. 70–72; `ieee802154-2024@329415:338486`.
- Source excerpt: “the MAC sublayer shall send an Imm-Ack frame.”
- Strength: `shall`; observation class: `wire`.
- Condition: Valid nonbroadcast Data/MAC command, version 0 or 1, AR set; full predicate in 6.6.2.
- Check idea: Compare normal delivery and a well-formed secured frame rejected by incoming security; check ACK independently of delivery.

## IEEE802154-RECEIVE-4

**Data delivery requires successful incoming security processing.**

- Source: `ieee802154-2024:clause:6.6.2`, physical PDF pp. 70–72; `ieee802154-2024@329415:338486`.
- Source excerpt: “If the valid frame is a Data frame or Multipurpose frame and the status from the incoming frame security procedure is SUCCESS, the MAC sublayer shall pass the MAC service data unit (MSDU) to the next higher layer.”
- Strength: `shall`; observation class: `end-to-end`.
- Condition: Valid data or multipurpose frame.
- Check idea: Verify successful unsecured delivery and suppression of payload delivery on security failure.

## IEEE802154-ACK-1

**Broadcast and group-addressed frames do not request ACK.**

- Source: `ieee802154-2024:clause:6.6.3.1`, physical PDF pp. 72–72; `ieee802154-2024@338546:339023`.
- Source excerpt: “any frame that is broadcast or has a group address as the extended destination address, as defined in IEEE Std 802, shall be sent with its AR field set to indicate no acknowledgment requested.”
- Strength: `shall`; observation class: `wire`.
- Condition: Broadcast/group destination.
- Check idea: Observe AR=0 and no resulting ACK for a broadcast data request.

## IEEE802154-ACK-2

**AR-clear frames receive no ACK.**

- Source: `ieee802154-2024:clause:6.6.3.2`, physical PDF pp. 72–72; `ieee802154-2024@339023:340044`.
- Source excerpt: “shall not be acknowledged by its target.”
- Strength: `shall`; observation class: `wire`.
- Condition: Frame AR=0; full predicate in 6.6.3.2.
- Check idea: Send addressed AR-clear data, observe delivery and successful transmission completion without an ACK.

## IEEE802154-ACK-3

**Outside-CAP ACK start is measured from the received last symbol.**

- Source: `ieee802154-2024:clause:6.6.3.3`, physical PDF pp. 72–73; `ieee802154-2024@340121:342816`.
- Source excerpt: “The transmission of an Ack frame outside the CAP shall commence AIFS after the reception of the last symbol of the Data frame or MAC command.”
- Strength: `shall`; observation class: `wire`.
- Condition: ACK outside CAP.
- Check idea: For O-QPSK use AIFS=macSifsPeriod and measure from received PPDU end to ACK PPDU start.

## IEEE802154-ACK-4

**Direct retransmissions retain the DSN.**

- Source: `ieee802154-2024:clause:6.6.3.4`, physical PDF pp. 73–73; `ieee802154-2024@343219:345787`.
- Source excerpt: “The retransmitted frame shall contain the same DSN as was used in the original transmission.”
- Strength: `shall`; observation class: `wire`.
- Condition: Failed direct acknowledged transmission.
- Check idea: Suppress ACKs, inspect every retry, and verify at most macMaxFrameRetries additional transmissions.

## IEEE802154-ACK-5

**Failed indirect transmission waits for a new poll.**

- Source: `ieee802154-2024:clause:6.6.3.4`, physical PDF pp. 73–73; `ieee802154-2024@343219:345787`.
- Source excerpt: “If a single transmission attempt has failed and the transmission was indirect, the coordinator shall not retransmit the frame.”
- Strength: `shall`; observation class: `wire`.
- Condition: Indirect delivery attempt fails.
- Check idea: Verify no autonomous retry, then request again and verify the same pending frame/DSN.

## IEEE802154-SECURITY-1

**Security level zero returns the outgoing frame unchanged.**

- Source: `ieee802154-2024:clause:9.2.2`, physical PDF pp. 160–162; `ieee802154-2024@697713:703495`.
- Source excerpt: “If the SecurityLevel parameter is zero, the procedure shall set the secured frame to be the frame to be secured and return with a Status of SUCCESS.”
- Strength: `shall`; observation class: `error-signal`.
- Condition: Outgoing SecurityLevel=0.
- Check idea: Observe unchanged unsecured frame and successful policy result.

## IEEE802154-SECURITY-2

**A nonzero outgoing security request fails when security is disabled.**

- Source: `ieee802154-2024:clause:9.2.2`, physical PDF pp. 160–162; `ieee802154-2024@697713:703495`.
- Source excerpt: “If macSecurityEnabled is set to FALSE, the procedure shall return with a Status of UNSUPPORTED_SECURITY.”
- Strength: `shall`; observation class: `error-signal`.
- Condition: Outgoing SecurityLevel is nonzero; step (a) did not return.
- Check idea: Request unavailable security, observe UNSUPPORTED_SECURITY and no frame transmission.

## IEEE802154-SECURITY-3

**Incoming secured version zero reports unsupported legacy security.**

- Source: `ieee802154-2024:clause:9.2.4`, physical PDF pp. 163–163; `ieee802154-2024@707731:710227`.
- Source excerpt: “If the Frame Version field of the frame to be unsecured is set to zero, the procedure shall return with a Status of UNSUPPORTED_LEGACY.”
- Strength: `shall`; observation class: `error-signal`.
- Condition: Security Enabled=1, Frame Version=0.
- Check idea: Inject a valid addressed legacy secured frame; distinguish status from version-1 UNSUPPORTED_SECURITY.

## IEEE802154-SECURITY-4

**Incoming nonlegacy secured frames fail when security is disabled.**

- Source: `ieee802154-2024:clause:9.2.4`, physical PDF pp. 163–163; `ieee802154-2024@707731:710227`.
- Source excerpt: “If macSecurityEnabled is set to FALSE, the procedure shall return with a Status of UNSUPPORTED_SECURITY.”
- Strength: `shall`; observation class: `error-signal`.
- Condition: Security Enabled=1, version nonzero, security disabled.
- Check idea: Check the status before auxiliary-header/key lookup and prevent payload delivery.

## IEEE802154-SECURITY-5

**Unsecured input succeeds when security is disabled.**

- Source: `ieee802154-2024:clause:9.2.5`, physical PDF pp. 165–166; `ieee802154-2024@718207:722316`.
- Source excerpt: “If macSecurityEnabled is set to FALSE, the procedure shall set the validated frame to be the frame to be validated and return with a Status of SUCCESS.”
- Strength: `shall`; observation class: `end-to-end`.
- Condition: Security Enabled=0 and macSecurityEnabled=false.
- Check idea: Verify that no key or device table is needed for accepted unsecured data.

## IEEE802154-PHY-1

**CCA Mode 1 detects energy above its threshold.**

- Source: `ieee802154-2024:clause:11.2.8`, physical PDF pp. 596–597; `ieee802154-2024@2373831:2378773`.
- Source excerpt: “CCA shall report a busy medium upon detecting any energy above the ED threshold.”
- Strength: `shall`; observation class: `internal`.
- Condition: CCA Mode 1.
- Check idea: Inject energy without a decodable frame; sweep power on both sides of the configured threshold.

## IEEE802154-PHY-2

**A CCA request during PPDU reception reports busy.**

- Source: `ieee802154-2024:clause:11.2.8`, physical PDF pp. 596–597; `ieee802154-2024@2373831:2378773`.
- Source excerpt: “For any of the CCA modes, if a request to perform CCA is received by the PHY during reception of a PPDU, CCA shall report a busy medium.”
- Strength: `shall`; observation class: `internal`.
- Condition: Reception interval from detected SFD through decoded PHR octet count.
- Check idea: Request CCA inside that interval and immediately outside it; distinguish reception from generic carrier state.

## IEEE802154-PHY-3

**CCA observes for phyCcaDuration.**

- Source: `ieee802154-2024:clause:11.2.8`, physical PDF pp. 596–597; `ieee802154-2024@2373831:2378773`.
- Source excerpt: “The CCA detection time shall be equal to phyCcaDuration, as defined in Table 12-2.”
- Strength: `shall`; observation class: `internal`.
- Condition: CCA operation.
- Check idea: Measure accepted request to observation completion and test a nonintegral number of symbols.

## IEEE802154-PHY-4

**ED averages over the selected measurement interval.**

- Source: `ieee802154-2024:clause:11.2.6`, physical PDF pp. 596–596; `ieee802154-2024@2372030:2373030`.
- Source excerpt: “The ED measurement time, to average over, shall be equal to eight symbol periods, unless the PHY specifies shorter phyCcaDuration time, in which case it is used instead.”
- Strength: `shall`; observation class: `internal`.
- Condition: Receiver ED.
- Check idea: Apply changing power within the interval and verify an average, not an instantaneous reception-state result.

## IEEE802154-PHY-5

**LQI uses at least eight values.**

- Source: `ieee802154-2024:clause:11.2.7`, physical PDF pp. 596–596; `ieee802154-2024@2373030:2373831`.
- Source excerpt: “At least eight unique values of LQI shall be used.”
- Strength: `shall`; observation class: `internal`.
- Condition: Received packet LQI.
- Check idea: Vary received signal quality and verify per-packet LQI with at least eight representable output levels.

## IEEE802154-PHY-6

**O-QPSK preamble is eight zero symbols.**

- Source: `ieee802154-2024:clause:13.1.2.2`, physical PDF pp. 614–614; `ieee802154-2024@2450789:2450993`.
- Source excerpt: “The length of the Preamble field for the O-QPSK PHYs shall be 8 symbols (i.e., 4 octets), and the bits in the Preamble field shall be binary zeros.”
- Strength: `shall`; observation class: `encoding`.
- Condition: O-QPSK PPDU.
- Check idea: Check SHR duration and preamble bytes independently of the MAC length.

## IEEE802154-PHY-7

**PHR length counts the complete PSDU.**

- Source: `ieee802154-2024:clause:13.1.3.2`, physical PDF pp. 615–615; `ieee802154-2024@2452100:2452253`.
- Source excerpt: “The Frame Length field specifies the total number of octets contained in the PSDU (i.e., PHY payload).”
- Strength: `description`; observation class: `encoding`.
- Condition: O-QPSK PHR.
- Check idea: Compare PHR length to MAC header plus payload plus FCS at minimum and maximum sizes.

## IEEE802154-PHY-8

**2450 MHz O-QPSK operates at 250 kbit/s.**

- Source: `ieee802154-2024:clause:13.2.2`, physical PDF pp. 615–615; `ieee802154-2024@2452853:2453503`.
- Source excerpt: “The data rate of the O-QPSK PHY shall be 250 kb/s when operating in the 2450 MHz, 915 MHz, 780 MHz or 2380 MHz bands”
- Strength: `shall`; observation class: `wire`.
- Condition: 2450 MHz O-QPSK.
- Check idea: Use 4 bits/symbol from 13.2.4 to derive 16 microseconds/symbol and compare PPDU duration.

## IEEE802154-SERVICE-1

**Oversize MPDUs fail before transmission.**

- Source: `ieee802154-2024:table:8-31`, physical PDF pp. 146–148; `ieee802154-2024@633380:637006`, `ieee802154-2024@637006:643870`.
- Source excerpt: “If the length of the frame exceeds phyMaxPacketSize (e.g., due to the additional overhead required for security processing or additional IEs), the MAC sublayer shall discard the frame and issue the MCPS- DATA.confirm primitive with a Status of FRAME_TOO_LONG.”
- Strength: `shall`; observation class: `error-signal`.
- Condition: Generated MPDU exceeds the selected PHY maximum.
- Check idea: Compare requests at the exact payload capacity and one octet above it; inspect status and absence of transmission.

## IEEE802154-PIB-1

**Dagger-marked MAC attributes are read-only to the upper layer.**

- Source: `ieee802154-2024:clause:8.4.3.1`, physical PDF pp. 152–152; `ieee802154-2024@659416:660132`.
- Source excerpt: “Attributes marked with a dagger (†) are read-only attributes (i.e., attribute can only be set by the MAC sublayer)”
- Strength: `description`; observation class: `internal`.
- Condition: MAC PIB attribute marked with dagger.
- Check idea: Attempt MLME-SET and verify READ_ONLY without mutation; GET still reports the value.

## IEEE802154-PIB-2

**Unknown PIB reads return unsupported attribute.**

- Source: `ieee802154-2024:table:8-9`, physical PDF pp. 120–121; `ieee802154-2024@522591:524416`.
- Source excerpt: “If the identifier of the PIB attribute is not found, the primitive returns with a Status of UNSUPPORTED_ATTRIBUTE.”
- Strength: `description`; observation class: `error-signal`.
- Condition: MLME-GET for an absent attribute.
- Check idea: Verify UNSUPPORTED_ATTRIBUTE and do not consume the invalid returned value.

## IEEE802154-PIB-3

**Reset can preserve or restore MAC PIB values.**

- Source: `ieee802154-2024:table:8-12`, physical PDF pp. 123–123; `ieee802154-2024@530664:531472`.
- Source excerpt: “If FALSE, the MAC sublayer is reset, but all MAC PIB attributes retain their values prior to the generation of the MLME-RESET.request primitive.”
- Strength: `description`; observation class: `internal`.
- Condition: SetDefaultPib false; compare with true as defined in the same table.
- Check idea: Set nondefault attributes, reset in both modes, then read them back; check completion and PHY reset.

## IEEE802154-SCAN-1

**All devices support passive scan.**

- Source: `ieee802154-2024:clause:6.4.1.1`, physical PDF pp. 64–65; `ieee802154-2024@310297:311546`.
- Source excerpt: “All devices shall be capable of performing passive scan across a specified set of channels.”
- Strength: `shall`; observation class: `wire`.
- Condition: All devices, including static non-beacon operation.
- Check idea: Request a passive scan across multiple channels and observe discovery without active scan requests.

## IEEE802154-SCAN-2

**Scan visits channels in ascending order.**

- Source: `ieee802154-2024:clause:6.4.1.1`, physical PDF pp. 64–65; `ieee802154-2024@310297:311546`.
- Source excerpt: “Channels are scanned in order from the lowest channel number to the highest.”
- Strength: `description`; observation class: `internal`.
- Condition: Channel scan.
- Check idea: Submit an unordered channel set and observe ascending visits and terminal results.

## Extraction boundary

This catalog extracts selected legacy data/ACK, unslotted access, unsecured-policy, O-QPSK,
PIB and passive-scan statements. It does not exhaust these clauses: complete receive-address
predicates, reserved-bit rules, CRC arithmetic, PIB tables, channel definitions, service error
precedence, reset interactions and references from the extracted nodes require further extraction.
Beacon scheduling, indirect delivery beyond the retry distinction, association, security transforms,
enhanced formats, scheduled access, other PHYs and amendment requirements are outside this
extraction. Their absence does not assert that they are optional for a particular device role.
