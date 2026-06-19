# Complete IEEE 802.11ax DL MU-OFDMA Compliance

## Summary

Complete the current packet-level IEEE 802.11ax downlink multi-user OFDMA
implementation in five related areas:

1. Correct Block Ack solicitation and response sequencing.
2. Exact HE-MU PHY symbol, preamble, and PPDU duration calculation.
3. HE capability and operation advertisement, negotiation, and enforcement.
4. Per-MPDU receive outcomes with partial Block Ack and selective retry.
5. Standards-shaped A-MPDU framing and HE-SIG-A/HE-SIG-B representation.

The implementation must retain one logical HE-MU PPDU, explicit per-user RU
geometry, and packet-level radio propagation. It is not required to generate
waveform samples, but every standard field represented by the model must have
correct encoding semantics and must affect observable MAC or PHY behavior.

The work is divided into ordered phases because later features depend on
earlier ones:

```text
Exact HE PHY timing and signaling
             |
             +--> HE capability negotiation --> capability-aware scheduling
             |
             +--> standards-shaped A-MPDU --> per-MPDU receive outcomes
                                                  |
                                                  +--> partial BA and retry
             |
             +------------------------------------+--> correct DL MU BA exchange
```

## Current Baseline

The current implementation already provides:

- Standard HE RU tone sizes and valid mixed-size, non-overlapping layouts.
- Per-associated-STA and per-access-category queues.
- Equal-sized, backlog-based, and HoL-delay DL schedulers.
- Active ADDBA and occupied Block Ack window gating.
- Transactional MU planning with ordinary SU fallback.
- Per-user RU, MCS, NSS, GI, and DCM duration and error evaluation.
- RU-specific receive power, noise, interference, and payload extraction.
- Per-user A-MPDU packing and retry state.
- A 5.484 ms HE PPDU duration limit and TXOP-aware packing.

The focused baseline command is:

```sh
export CCACHE_DISABLE=1
source /home/user/omnetpp-6.4.0/setenv -f
source setenv -q
bin/inet_run_unit_tests -m release -f "(Ieee80211He|HeDlScheduler).*\\.test"
```

At the time this plan was written, all 18 selected tests passed.

## Scope and Compliance Target

### In scope

- HE-MU downlink PPDUs for 20, 40, 80, and 160 MHz channels.
- BCC and LDPC coding semantics needed for correct duration and packet-level
  error evaluation.
- HE Capabilities and HE Operation information used by DL OFDMA.
- Per-peer MCS/NSS, channel-width, RU, DCM, coding, and aggregation limits.
- Single-TID and multi-TID per-user A-MPDUs where permitted by negotiated
  capability.
- Per-MPDU FCS outcomes and partial Block Ack processing.
- DL MU acknowledgment sequences using standard Trigger/MU-BAR and HE-TB
  responses, plus any explicitly supported legacy-compatible alternative.
- Standards-shaped HE-SIG-A and HE-SIG-B fields and RU allocation encodings.

### Out of scope

- Waveform or sample-level conformance.
- Beamforming, channel sounding, and MU-MIMO spatial separation.
- BSS coloring, dual NAV, and OBSS/PD spatial reuse beyond carrying validated
  signaling fields.
- Target Wake Time.
- Doppler, midambles, STBC, preamble puncturing, and extended-range SU unless
  required as a dependency of a negotiated field.
- Certification against commercial hardware.

The final result should be described as a standards-compliant packet-level
model for the implemented DL MU-OFDMA feature set, not as a waveform
conformance implementation.

## Design Principles

1. **One authoritative calculation path.** Scheduling, packing, serialization,
   transmission duration, NAV protection, and error evaluation must consume the
   same resolved HE PHY parameter object.
2. **Negotiate before scheduling.** The AP may allocate only features supported
   by both the AP and the destination STA.
3. **No metadata-only fields.** A field must either affect behavior or be
   explicitly unsupported and rejected.
4. **Plan before mutation.** Queue removal, sequence assignment, and ACK-state
   transitions occur only after the complete PPDU and response exchange have
   been validated.
5. **Preserve partial success.** A failed MPDU must not force correctly received
   MPDUs in the same PSDU to be retried.
6. **Keep compatibility explicit.** Legacy, SU, and non-HE paths must retain
   their existing behavior unless a shared bug fix is required.

## Public Data Model

### Resolved HE PHY parameters

Replace the current fixed-duration assumptions with immutable common and
per-user parameter objects:

```cpp
struct Ieee80211HeCommonPhyParameters {
    Ieee80211HePpduFormat ppduFormat;
    Hz channelBandwidth;
    Ieee80211HeGuardInterval guardInterval;
    Ieee80211HeLtfType ltfType;
    int numberOfHeLtfSymbols;
    bool ldpcExtraSymbol;
    int packetExtensionDurationUs;
    Ieee80211HeSigAFields sigA;
    Ieee80211HeSigBFields sigB;
    simtime_t legacyPreambleDuration;
    simtime_t rlSigDuration;
    simtime_t heSigADuration;
    simtime_t heSigBDuration;
    simtime_t heStfDuration;
    simtime_t heLtfDuration;
    simtime_t commonPreambleDuration;
};

struct Ieee80211HeUserPhyParameters {
    Ieee80211HeRu ru;
    int mcs;
    int numberOfSpatialStreams;
    bool dcm;
    Ieee80211HeCoding coding;
    B psduLength;
    int numberOfEncoders;
    int codedBitsPerSymbol;
    int dataBitsPerSymbol;
    int serviceBits;
    int tailBits;
    int preFecPaddingFactor;
    int postFecPaddingBits;
    int numberOfDataSymbols;
    simtime_t dataDuration;
};

struct Ieee80211HePpduParameters {
    Ieee80211HeCommonPhyParameters common;
    std::vector<Ieee80211HeUserPhyParameters> users;
    int commonNumberOfDataSymbols;
    simtime_t duration;
};
```

The calculator must return either a valid parameter set or a structured
validation error. Ordinary traffic conditions must not produce fatal runtime
errors after queue mutation.

### Negotiated HE capabilities

Add local and negotiated capability types:

```cpp
struct Ieee80211HeMcsNssMap {
    int maxMcsPerNss[8]; // -1 means unsupported
};

struct Ieee80211HeCapabilities {
    std::set<Hz> supportedChannelWidths;
    Ieee80211HeMcsNssMap rxMcsNss;
    Ieee80211HeMcsNssMap txMcsNss;
    bool dlOfdma;
    bool ulOfdma;
    bool dcm;
    int maxDcmConstellation;
    int maxDcmNss;
    bool ldpc;
    bool multiTidAggregationRx;
    bool multiTidAggregationTx;
    int maxAmpduLengthExponent;
    int maxMpduLength;
    int maxBlockAckBufferSize;
    std::set<int> supportedRuToneSizes;
};

struct Ieee80211HeOperation {
    uint8_t bssColor;
    Hz operatingChannelWidth;
    int basicHeMcsNss;
    bool defaultPeDurationPresent;
    int defaultPeDurationUs;
};

struct Ieee80211NegotiatedHeCapabilities {
    Ieee80211HeCapabilities intersection;
    Ieee80211HeOperation operation;
    bool valid;
};
```

Store negotiated capabilities in the MIB per peer. Scheduler candidates must
carry a read-only view or identifier for the negotiated state.

### Per-MPDU receive result

Add explicit MPDU-level outcomes:

```cpp
enum class Ieee80211MpduReceiveStatus {
    SUCCESS,
    DELIMITER_ERROR,
    HEADER_ERROR,
    PAYLOAD_ERROR,
    FCS_ERROR,
    NOT_EVALUATED
};

struct Ieee80211MpduReceiveResult {
    SequenceNumberCyclic sequenceNumber;
    FragmentNumber fragmentNumber;
    Tid tid;
    B offset;
    B length;
    Ieee80211MpduReceiveStatus status;
};
```

Attach the result vector to the extracted per-user packet with a new receive
indication tag. The MAC must use this vector to update reordering and Block Ack
state.

### DL MU acknowledgment plan

Represent the response sequence before transmission:

```cpp
enum class HeDlMuAckMethod {
    MU_BAR_TRIGGER,
    EXPLICIT_SEQUENTIAL_BAR
};

struct HeDlMuAckUserPlan {
    MacAddress staAddress;
    uint16_t associationId;
    Tid tid;
    Ieee80211HeRu responseRu;
    SequenceNumberCyclic startingSequenceNumber;
};

struct HeDlMuAckPlan {
    HeDlMuAckMethod method;
    std::vector<HeDlMuAckUserPlan> users;
    simtime_t triggerDuration;
    simtime_t responseDuration;
    simtime_t timeout;
};
```

The default for HE-capable peers should be `MU_BAR_TRIGGER`.

## Phase 1: Exact HE-MU PHY Timing

### Objective

Replace fixed 40 us preamble and 8 us header durations and the simplified
`16 + PSDU bits + 6` calculation with the standard HE PHY procedure.

### Calculation requirements

Implement a shared HE PPDU calculator that accounts for:

- L-STF, L-LTF, L-SIG, RL-SIG, HE-SIG-A, HE-SIG-B, HE-STF, and HE-LTF.
- HE-SIG-B common and user field sizes based on channel width, RU allocation,
  compression mode, and user count.
- 0.8, 1.6, and 3.2 us guard intervals.
- 1x, 2x, and 4x HE-LTF symbol durations.
- Number of HE-LTF symbols derived from total space-time streams.
- RU data and pilot subcarrier counts.
- MCS modulation and coding rate.
- NSS and number of BCC encoders.
- DCM restrictions and data-bit reduction.
- BCC service and per-encoder tail bits.
- LDPC codeword sizing, shortening, puncturing, repetition, and extra symbol.
- Pre-FEC and post-FEC padding.
- Common data-symbol alignment across all users in one HE-MU PPDU.
- Packet extension duration.
- The 5.484 ms PPDU duration limit.

Shorter users must be padded to the common number of HE data symbols; their
individual encoded data duration must not define a separate PPDU end time.

### Integration

- Replace fixed timing fields in
  `Ieee80211HeUserPhyParameters`.
- Replace `estimateHeMuUserDuration()` with the common calculator or retain it
  only as a thin compatibility wrapper.
- Make schedulers request duration estimates through this calculator.
- During packing, recompute the complete multi-user PPDU whenever a user gains
  or loses an MPDU because HE-SIG-B and common symbol count may change.
- Make `Ieee80211Radio::encapsulate()` validate, not independently recalculate,
  the planned PPDU parameters.
- Store the resolved common and per-user parameters in
  `Ieee80211Transmission`.
- Make `Ieee80211Transmitter::createTransmission()` use the resolved preamble,
  header, data, packet-extension, and complete durations.
- Evaluate HE-SIG-A and HE-SIG-B error probability separately from user data.
- Ensure the Duration/ID field and TXOP admission include the exact selected
  acknowledgment sequence.

### Primary files

- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h`
- New `Ieee80211HePhyCalculator.h/.cc` beside the HE packet-level files.
- `src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h/.cc`
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h/.cc`
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.cc`
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.cc`
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/`
- `src/inet/linklayer/ieee80211/mac/scheduler/HeDlSchedulerBase.cc`
- `src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc`

### Acceptance criteria

- No fixed HE-MU preamble or header duration remains in production code.
- Scheduler estimate, packing plan, serialized signaling, and transmission
  duration are identical.
- All users share one common data-symbol count and PPDU end time.
- BCC tail bits scale with the number of encoders.
- LDPC and packet extension either calculate correctly or are rejected during
  capability negotiation before scheduling.
- Boundary tests cover one-symbol transitions and the 5.484 ms limit.

## Phase 2: Standards-Shaped HE Signaling

### Objective

Replace the custom serialized user list with explicit, validated HE-SIG-A and
HE-SIG-B semantics while retaining convenient derived fields internally.

### HE-SIG-A

Represent fields required by the packet-level model, including:

- PPDU format.
- BSS color.
- Bandwidth.
- UL/DL indication.
- TXOP duration.
- GI and HE-LTF size.
- Number of HE-LTF symbols.
- Doppler and STBC flags, rejected when unsupported.
- Coding and LDPC extra-symbol indication where applicable.
- Packet extension information.
- CRC and tail validation.

### HE-SIG-B

Represent:

- Common field.
- Standard RU allocation encoding per 20 MHz content channel.
- Center 26-tone RU indications where applicable.
- Compression mode.
- User-specific fields containing STA-ID, MCS, DCM, coding, and NSS.
- Correct content-channel assignment for 40, 80, and 160 MHz operation.
- HE-SIG-B symbol count and MCS.
- CRC and tail validation.

Do not serialize center frequency, tone offset, PSDU duration, or other derived
simulation values as if they were standard transmitted fields. Derive them
from channel configuration and RU allocation codes after decoding.

### Internal representation

- Add `Ieee80211HeSigA` and `Ieee80211HeSigB` chunks to
  `Ieee80211PhyHeader.msg`, or model them as nested structures in an HE PHY
  header if separate chunks would conflict with INET's packet layout.
- Keep `Ieee80211HeRu` as the authoritative runtime geometry.
- Add bidirectional conversion between RU layout and standard HE-SIG-B RU
  allocation encoding.
- Validate that decoded user fields reference valid allocated RUs.
- Keep a debug-only derived view for Qtenv visualization and tests.

### Serializer behavior

- Serialize fields at their standard bit widths and bit order.
- Calculate and verify HE-SIG CRCs.
- Mark malformed or reserved combinations incorrect on deserialization.
- Do not silently clamp invalid RU, STA-ID, NSS, MCS, GI, or coding values.
- Keep old custom-format deserialization only if required for recorded-test
  compatibility, behind an explicit compatibility option.

### Primary files

- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader.msg`
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeaderSerializer.h/.cc`
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h`
- New `Ieee80211HeSigCodec.h/.cc`
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.cc`
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.cc`
- HE visualization code that currently reads derived HE MU fields.

### Acceptance criteria

- Every scheduler-generated valid RU layout round-trips through HE-SIG-B
  encoding without geometry loss.
- Reserved or overlapping RU allocations fail validation.
- HE-SIG-B size changes with user count and allocation structure and therefore
  changes PPDU duration.
- Receivers derive RU geometry from decoded standard-shaped signaling.
- Existing Qtenv visualization still displays per-user RU/MCS/NSS details.

## Phase 3: HE Capability and Operation Negotiation

### Objective

Advertise HE support in management frames, compute a negotiated feature
intersection during association, and enforce it in scheduling and PPDU
construction.

### Management elements

Add packet representations and serializers for:

- HE Capabilities element.
- HE Operation element.
- HE 6 GHz Band Capabilities when the active band requires it.
- HE MCS/NSS maps for each supported channel width.
- Extended capabilities needed to advertise OFDMA-related behavior.

At minimum, model fields governing:

- DL and UL OFDMA support.
- Supported 20/40/80/160 MHz operation.
- RX and TX MCS/NSS combinations.
- DCM support and limits.
- LDPC support.
- Maximum MPDU length.
- Maximum A-MPDU length exponent.
- Multi-TID aggregation support.
- Block Ack buffer size.
- Triggered response and Multi-STA Block Ack support.
- BSS color and default packet-extension duration.

### Management procedures

- AP beacons and probe responses advertise HE Capabilities and HE Operation.
- STA probe and association requests advertise local HE Capabilities.
- AP association responses return the AP capabilities and current operation.
- Simplified management must perform the same capability intersection without
  exchanging full frames.
- Reassociation refreshes negotiated state.
- Disassociation and deauthentication remove negotiated capability state.
- A change in channel width or HE operation invalidates or recomputes affected
  negotiated state.

### MIB state

Add:

- Local HE capabilities for AP and STA.
- AP HE operation state.
- Per-peer advertised capabilities.
- Per-peer negotiated capability intersections.
- Lookup helpers that return `nullptr` or an invalid result for non-HE peers.

### Scheduling enforcement

Extend `CandidateInfo` and `RuAllocation` validation so that:

- Non-HE or non-DL-OFDMA peers are excluded.
- Channel width is supported by both peers.
- MCS does not exceed the peer's RX MCS/NSS map.
- NSS does not exceed the peer map or local antenna count.
- DCM is selected only when negotiated.
- LDPC is selected only when negotiated and implemented.
- RU size is supported by the peer and current operation.
- PSDU, MPDU, A-MPDU, and BA-window limits use negotiated values.
- Ack method is supported by every selected STA.

If capability filtering leaves fewer than two users, use ordinary SU
transmission without modifying queues.

### Configuration

Expose local capability defaults in NED. Defaults should preserve current
scenarios:

- HE and DL OFDMA enabled when `opMode = "ax"`.
- All channel widths supported by the configured radio up to its active width.
- MCS 0-11 for NSS 1 unless configured otherwise.
- BCC enabled.
- LDPC disabled until Phase 1 support is complete.
- DCM enabled only for combinations already supported by the model.

### Primary files

- `src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrame.msg`
- `src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtFrameSerializer.h/.cc`
- `src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtAp*.cc`
- `src/inet/linklayer/ieee80211/mgmt/Ieee80211MgmtSta*.cc`
- `src/inet/linklayer/ieee80211/mib/Ieee80211Mib.h/.cc`
- `src/inet/linklayer/ieee80211/mac/scheduler/IIeee80211HeDlScheduler.h`
- `src/inet/linklayer/ieee80211/mac/scheduler/HeDlSchedulerBase.cc`
- `src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc`
- `src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc`

### Acceptance criteria

- HE management elements round-trip at field boundaries.
- Association stores the exact local/peer intersection.
- A peer lacking DL OFDMA is never placed in a DL HE-MU PPDU.
- Unsupported MCS, NSS, DCM, LDPC, RU, width, or aggregation choices are
  rejected before queue mutation.
- Reassociation and disassociation update MIB state correctly.
- Mixed HE and legacy stations continue to receive traffic through the
  appropriate MU or SU path.

## Phase 4: Standards-Shaped A-MPDU and Per-MPDU Reception

### Objective

Represent every per-user PSDU as a standards-shaped A-MPDU, including the
single-MPDU case, and preserve independent MPDU receive outcomes.

### A-MPDU construction

- Add an MPDU delimiter before every MPDU, including singleton A-MPDUs.
- Represent delimiter signature, MPDU length, EOF, reserved bits, and delimiter
  CRC.
- Calculate padding from delimiter plus MPDU length.
- Respect negotiated maximum MPDU and A-MPDU lengths.
- Ensure each MPDU carries its own MAC FCS mode and result.
- Keep one TID per A-MPDU unless multi-TID aggregation is negotiated and fully
  represented.
- Build the PSDU through the normal aggregation component instead of manually
  duplicating delimiter logic in `HeDlMuTxOpFs`.

### Per-MPDU error evaluation

Add a packet-level model that:

1. Evaluates the common HE preamble and signaling.
2. Resolves the selected user's RU and PHY parameters.
3. Evaluates each delimiter.
4. Evaluates each MPDU over its exact coded-bit region.
5. Produces independent or correlated MPDU outcomes according to a configured
   model.

The default model should derive all MPDU outcomes from one deterministic random
draw over cumulative coded-bit success probabilities. This preserves realistic
correlation without incorrectly treating every MPDU as an independent PPDU.
Provide an optional independent-MPDU mode for sensitivity studies.

If common HE signaling fails, no MPDU is delivered. If a delimiter fails, the
corresponding MPDU is not parsed. A failed MPDU must not invalidate later
delimiters unless the selected delimiter-error model says synchronization is
lost.

### Receive and reordering path

- Extract the selected user's complete A-MPDU.
- Deaggregate it into MPDUs while preserving receive-result tags.
- Deliver only MPDUs with a valid delimiter, MAC header, payload result, and
  FCS.
- Record successful sequence/fragment numbers in the recipient BA record.
- Leave failed MPDUs unacknowledged.
- Pass successful MPDUs through duplicate removal, reordering, A-MSDU
  deaggregation, and normal delivery.

### Partial Block Ack and selective retry

- Generate Block Ack bitmaps from actual MPDU receive results.
- On the AP, mark acknowledged MPDUs complete and remove them from in-progress
  state.
- Requeue or retain only unacknowledged MPDUs.
- Preserve retry flags and sequence numbers for retransmission.
- Advance the BA transmit window only according to the received bitmap and
  agreement rules.
- Support sequence-number wrap and fragmented MPDUs.
- Ensure retry limits apply per MPDU, not per user PSDU.

### Primary files

- `src/inet/linklayer/ieee80211/mac/aggregation/MpduAggregation.cc`
- `src/inet/linklayer/ieee80211/mac/aggregation/MpduDeaggregation.cc`
- `src/inet/linklayer/ieee80211/mac/Ieee80211Frame.msg`
- `src/inet/linklayer/ieee80211/mac/Ieee80211MacHeaderSerializer.cc`
- `src/inet/linklayer/ieee80211/mac/recipient/RecipientQosMacDataService.cc`
- `src/inet/linklayer/ieee80211/mac/blockack/BlockAckRecord.cc`
- `src/inet/linklayer/ieee80211/mac/originator/QosAckHandler.cc`
- `src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc`
- `src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc`
- `src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc`
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.cc`
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/`

### Acceptance criteria

- A singleton HE user PSDU contains one valid MPDU delimiter.
- Delimiter and MPDU FCS errors are represented separately.
- One failed MPDU and one successful MPDU in the same PSDU produce a partial BA
  bitmap.
- Only the failed MPDU is retried, with the original sequence number and retry
  flag.
- Successful MPDUs are delivered once and are not retransmitted.
- BA-window occupancy and advancement remain correct across partial success and
  sequence wrap.

## Phase 5: Correct DL MU Block Ack Exchange

### Objective

Replace the current special-case first-STA implicit Block Ack behavior with a
standards-shaped acknowledgment procedure.

### Default exchange

Use the following default sequence for an HE-MU downlink transmission requiring
Block Ack:

```text
AP:  HE-MU DL PPDU
     SIFS
AP:  MU-BAR Trigger
     SIFS
STAs: simultaneous HE-TB Block Ack responses on assigned RUs
```

The MU-BAR Trigger must identify each responding STA, response RU, TID, starting
sequence number, MCS, and response duration. Each selected STA sends its BA in
the commanded HE-TB RU. The AP collects responses belonging to the same trigger
exchange and processes them independently.

### Alternative exchange

Retain explicit sequential BAR/BA only as a selectable compatibility method:

```text
AP:  HE-MU DL PPDU
     SIFS
AP:  BAR to STA 1
     SIFS
STA1: BA
     SIFS
AP:  BAR to STA 2
     ...
```

Every STA, including the first, must be explicitly solicited in this method.
Do not infer an immediate Block Ack solely from the QoS Ack Policy value
`BLOCK_ACK`.

### Ack policy semantics

- Separate QoS Ack Policy encoding from internal “BA requested” state.
- Add explicit representation for any HE implicit Block Ack request semantics
  used by the selected exchange.
- Remove the special `myAllocationIndex == 0` immediate-response rule.
- Make recipient behavior depend on the decoded Trigger/BAR and negotiated BA
  agreement.
- Reject unsolicited, stale, duplicate, wrong-TID, wrong-AID, wrong-RU, or
  wrong-trigger responses.

### MU-BAR Trigger support

Extend Trigger representation with the fields required for MU-BAR:

- Trigger type.
- UL length/common duration.
- GI and HE-LTF information.
- Per-user AID, RU allocation, coding/MCS/NSS, and target RSSI if applicable.
- Per-user BAR control and BAR information.

Use the common Trigger serialization and HE-TB receive collection introduced by
the UL OFDMA implementation, but do not couple DL acknowledgment correctness to
UL data scheduling policy.

### AP response collection

- Create a receive-collection frame-sequence step keyed by trigger ID and
  deadline.
- Accept simultaneous non-overlapping HE-TB BA responses.
- Allow same-RU interference to produce loss through the radio model.
- At the deadline, classify each expected response as received, malformed, or
  missing.
- Process each valid BA bitmap independently.
- Fail or retry only unacknowledged MPDUs for that STA.
- Continue processing other users after one timeout or malformed response.

### Duration and NAV

- Compute the complete data, SIFS, Trigger/BAR, HE-TB BA, and timeout durations
  before committing the MU plan.
- Set Duration/ID fields from the exact remaining exchange duration.
- Keep the TXOP within the live TXOP limit.
- For legacy observers, expose the correct total protected duration rather than
  only the HE-MU PPDU duration.

### Primary files

- `src/inet/linklayer/ieee80211/mac/Ieee80211Frame.msg`
- `src/inet/linklayer/ieee80211/mac/Ieee80211MacHeaderSerializer.cc`
- `src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.h/.cc`
- `src/inet/linklayer/ieee80211/mac/framesequence/HeUlMuTxOpFs.h/.cc`
- Frame-sequence receive-collection infrastructure.
- `src/inet/linklayer/ieee80211/mac/coordinationfunction/Hcf.cc`
- `src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc`
- `src/inet/linklayer/ieee80211/mac/originator/QosAckHandler.cc`
- `src/inet/linklayer/ieee80211/mac/blockack/`
- `src/inet/linklayer/ieee80211/mac/recipient/RecipientQosAckPolicy.cc`

### Acceptance criteria

- The first STA never sends an unsolicited BA merely because it is allocation
  index zero.
- MU-BAR Trigger causes all addressed STAs to respond after SIFS on assigned
  HE-TB RUs.
- Missing one response does not discard successful responses from other STAs.
- Partial BA bitmaps drive per-MPDU selective retry.
- Sequential compatibility mode explicitly sends a BAR to every STA.
- Duration/ID and TXOP calculations cover the complete chosen response
  exchange.

## Cross-Phase Refactoring

### Transactional plan object

Expand the existing read-only DL MU plan so it contains:

- Selected peers and negotiated capabilities.
- Per-user packet and A-MPDU layouts.
- Standard RU and HE-SIG-B allocations.
- Resolved common and per-user PHY parameters.
- Ack method and complete response plan.
- Complete TXOP and Duration/ID calculations.
- Queue, BA-window, and sequence-number reservations.

Commit only after all parts validate. If planning fails, stage the oldest
eligible packet for ordinary HCF SU transmission.

### Error reporting

Use structured validation results for expected failures:

- Unsupported peer capability.
- No legal common PHY combination.
- Illegal RU allocation.
- HE-SIG-B overflow or invalid user mapping.
- A-MPDU or MPDU size limit.
- Full BA window.
- TXOP or PPDU duration limit.
- Unsupported acknowledgment method.

Reserve `cRuntimeError` for internal invariants and malformed externally
constructed test packets.

### Statistics

Add signals/statistics for:

- HE-MU PPDU common and signaling duration.
- Data-symbol padding per user.
- HE-SIG-A and HE-SIG-B failures.
- MPDUs attempted, acknowledged, partially failed, and retried.
- Delimiter, payload, and FCS failures.
- MU-BAR responses expected, received, malformed, and timed out.
- Capability-based scheduler exclusions.
- SU fallback reason.

## Verification Plan

### Unit tests

Add or extend tests for:

1. HE preamble and HE-SIG duration across PPDU formats, widths, user counts,
   GI/LTF choices, NSS, BCC encoders, LDPC, DCM, and packet extension.
2. RU layout to HE-SIG-B encoding and decoding for all standard layouts.
3. HE-SIG field widths, CRCs, reserved values, and malformed combinations.
4. HE Capabilities and HE Operation serialization.
5. Capability intersection and MIB lifecycle.
6. Scheduler rejection of unsupported MCS/NSS/RU/coding/width/ack methods.
7. Singleton and multi-MPDU A-MPDU delimiter framing.
8. Per-MPDU receive outcomes, delimiter errors, FCS errors, and partial success.
9. Partial BA bitmap generation and selective retry.
10. MU-BAR Trigger construction, simultaneous HE-TB BA collection, timeout,
    wrong-RU, wrong-trigger, and malformed-response handling.
11. Exact Duration/ID and TXOP calculations for MU-BAR and sequential modes.

### Integration tests

Create end-to-end AP/STA scenarios covering:

- Two users with different RU/MCS/NSS values and different A-MPDU lengths.
- One MPDU failing inside a multi-MPDU user PSDU.
- One STA missing its BA while another succeeds.
- Mixed HE-capable and legacy stations.
- A STA supporting only 20 MHz and MCS 0-7 in a wider-channel AP.
- Reassociation after an HE operation change.
- 20, 40, 80, and 160 MHz operation.
- Sequence-number wrap with a partially acknowledged A-MPDU.
- MU-BAR and sequential compatibility acknowledgment methods.

### Scenario-level validation

Validate observable trends:

- Increasing user count increases HE-SIG-B airtime.
- Shorter users incur symbol padding but do not shorten the common PPDU.
- Partial MPDU failure retransmits fewer bytes than whole-PSDU failure.
- MU-BAR reduces response airtime relative to sequential BAR/BA as STA count
  grows.
- A constrained peer receives only supported MCS/NSS/RU allocations.
- TXOP and PPDU duration limits are never exceeded.

### Required commands

Run from the repository root:

```sh
export CCACHE_DISABLE=1
source /home/user/omnetpp-6.4.0/setenv -f
source setenv -q
make -j$(nproc)
bin/inet_run_unit_tests -m release -f "(Ieee80211He|HeDlScheduler).*\\.test"
```

Also run the ordinary IEEE 802.11 unit and module tests affected by shared
aggregation, Block Ack, management, and serialization changes.

## Delivery Order

Implement and merge in this order:

1. Exact HE PHY calculator and tests.
2. HE-SIG-A/HE-SIG-B representation and RU allocation codec.
3. HE management capability/operation elements and negotiated MIB state.
4. Standards-shaped A-MPDU construction and per-MPDU receive outcomes.
5. Partial BA bookkeeping and selective retry.
6. MU-BAR Trigger and HE-TB BA response collection.
7. Sequential BAR/BA compatibility cleanup.
8. End-to-end scenarios, statistics, documentation, and performance checks.

Each step must leave the focused HE suite passing. Temporary adapters are
allowed between steps, but they must be deleted before the final compliance
milestone.

## Compatibility and Migration

- Keep DL MU OFDMA opt-in through `HeHcf`.
- Preserve existing scheduler module names and configuration where possible.
- Add capability defaults so current `opMode = "ax"` examples continue to run.
- Keep old parameters as deprecated aliases for one release when practical.
- Version or explicitly gate any incompatible PHY-header serialization change.
- Preserve legacy and SU aggregation behavior while moving common delimiter and
  FCS logic into shared components.
- Update existing reports that still describe per-user PHY processing as
  missing; that issue has already been corrected in the current branch.

## Definition of Done

The work is complete when:

- All five feature areas meet their acceptance criteria.
- No production DL MU path uses fixed HE preamble/header constants.
- No AP schedules a PHY or MAC feature unsupported by the destination.
- Every HE user PSDU has standards-shaped A-MPDU framing.
- Per-MPDU success is reflected in BA bitmaps and selective retry.
- DL MU Block Ack responses are explicitly solicited through MU-BAR or BAR.
- Standard-shaped HE-SIG fields determine RU placement and PPDU timing.
- The focused HE suite, affected general IEEE 802.11 tests, and new end-to-end
  scenarios all pass.
- Documentation clearly states the remaining waveform-level and out-of-scope
  limitations.
