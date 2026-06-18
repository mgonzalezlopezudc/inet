# IEEE 802.11ax Compliance Gap Review

## Executive summary

The current implementation is a strong packet-level IEEE 802.11ax model for
downlink and uplink OFDMA. It includes standard RU layouts, per-station
queueing, multiple schedulers, per-user A-MPDU packing, trigger-based uplink,
UORA, RU-aware interference, and Multi-STA Block Ack.

It is not yet a fully standards-complete 802.11ax MAC/PHY model. The highest
priority gap is the disconnect between per-user HE metadata and PHY reception:
each user carries an RU, MCS, NSS, and DCM value, but reception success still
largely follows one PPDU-wide `IIeee80211Mode`. Other major missing areas are
HE capability negotiation, spatial reuse, Target Wake Time, MU-MIMO, and
detailed HE PHY signaling.

The recommended development order is:

1. Complete per-user HE PHY duration and error evaluation.
2. Add HE capability and operation negotiation and use the active radio
   configuration.
3. Implement BSS coloring, dual NAV, and OBSS/PD spatial reuse.
4. Complete the trigger-based uplink aggregation, acknowledgment, and timing
   model.
5. Add Target Wake Time.
6. Add MU-MIMO and per-user spatial streams.
7. Add the remaining optional HE PHY features and conformance-style tests.

Even after these additions, a packet-level simulation should be described as a
standards-shaped or standards-aware 802.11ax model, rather than a
waveform-conformance implementation.

## 1. Review scope

This review considers the current implementation under:

- `src/inet/linklayer/ieee80211/`
- `src/inet/physicallayer/wireless/ieee80211/`
- `tests/unit/`
- `examples/ieee80211/`

It also takes into account:

- `reports/80211ax-dl-mu-ofdma-implementation-report.md`
- `reports/80211ax-dl-mu-ofdma-walkthrough.md`
- `reports/80211ax-sched-queue-improvements.md`
- `reports/ul-mu-ofdma-synchronization-and-decoding-notes.md`

The earlier reports are valuable design history, but some findings have been
superseded by newer code. In particular, the current implementation now
contains trigger-based uplink OFDMA and uses a common HE MU duration derived
from the longest user allocation.

## 2. Capabilities already implemented

### 2.1 HE mode foundations

The HE mode implementation provides:

- 20, 40, 80, and 160 MHz channel widths.
- MCS 0 through 11, including 1024-QAM.
- Mode definitions for multiple spatial-stream counts.
- HE OFDM timing and subcarrier counts.
- HE SU and MU preamble abstractions.

The implementation currently exposes 0.8 us and 3.2 us guard intervals, but
not the standard 1.6 us interval.

Relevant files:

- `src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.h`
- `src/inet/physicallayer/wireless/ieee80211/mode/Ieee80211HeMode.cc`

### 2.2 Standard RU model

The RU implementation supports the standard tone sizes and valid layouts for
20, 40, 80, and 160 MHz channels. It carries RU index, tone size, tone offset,
bandwidth, center frequency, data subcarriers, and pilot subcarriers.

The scheduler and radio-medium paths consume explicit RU geometry instead of
merely dividing channel bandwidth by the number of users.

Relevant file:

- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h`

### 2.3 Downlink MU-OFDMA

The downlink implementation provides:

- Per-associated-STA, per-access-category queues.
- Queue-aware schedule contexts.
- Equal-sized, backlog-based, and HoL-delay schedulers.
- Mandatory anchor preservation.
- Active Block Ack agreement gating.
- Per-user MCS selection metadata.
- Per-user A-MPDU packing.
- One logical HE-MU PPDU.
- RU-specific receive power, noise, and interference.
- Receiver extraction of only the assigned PSDU.
- Per-user recovery through sequential Block Ack and BAR exchanges.
- Fallback to ordinary HCF operation when MU scheduling is unavailable.

Relevant files:

- `src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc`
- `src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc`
- `src/inet/linklayer/ieee80211/mac/scheduler/`
- `src/inet/linklayer/ieee80211/mac/queue/StationQueueBank.cc`

### 2.4 Uplink MU-OFDMA

Contrary to an older limitation recorded in the DL implementation report, the
current tree includes a substantial uplink implementation:

- Basic Trigger and BSRP Trigger exchanges.
- Scheduled RU allocation.
- Buffer-status reporting.
- Uplink OFDMA random access and OFDMA contention windows.
- Target-RSSI-based transmit-power requests.
- HE trigger-based PPDU metadata.
- Parallel reception correlation through Trigger IDs.
- RU-specific uplink transmission and reception.
- Frequency-overlap-based interference filtering.
- Multi-STA Block Ack responses.

Relevant files:

- `src/inet/linklayer/ieee80211/mac/coordinationfunction/HeUlCoordinator.cc`
- `src/inet/linklayer/ieee80211/mac/coordinationfunction/HeUlDefaultTriggerPolicy.cc`
- `src/inet/linklayer/ieee80211/mac/framesequence/HeUlMuTxOpFs.cc`
- `src/inet/linklayer/ieee80211/mac/scheduler/HeUlSchedulerBase.cc`
- `src/inet/linklayer/ieee80211/mac/scheduler/HeUlSchedulerEqualSizedRUs.cc`
- `src/inet/linklayer/ieee80211/mac/scheduler/HeUlSchedulerBacklogBased.cc`

### 2.5 Corrected common MU duration

The older DL report identified aggregate-container-length airtime as the
largest PHY limitation. The current implementation has partially corrected
this:

- Each user receives an estimated duration based on PSDU length, RU size, and
  MCS.
- The HE MU PHY header stores the maximum user duration as `commonDuration`.
- The transmitter uses `commonDuration` as the PPDU duration when present.

Relevant files:

- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.cc`
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.cc`

The duration calculation is still approximate and is not yet tied to a
complete per-user HE PHY mode.

## 3. Highest-priority missing features

## 3.1 Per-user HE PHY processing

This is the most important remaining correctness gap.

The HE MU user metadata contains:

- RU index, size, and offset.
- STA ID.
- MCS.
- Number of spatial streams.
- DCM flag.
- PSDU length.
- User duration.

However, the underlying `Ieee80211Transmission` still carries one global
`IIeee80211Mode`. Reception decisions and error models therefore do not
necessarily use each user's selected MCS, NSS, DCM, and RU-specific coding
parameters.

Consequences include:

- Scheduler-selected MCS may affect metadata and packing but not packet error
  probability.
- All users can effectively inherit the PPDU-wide mode.
- NSS and DCM fields can appear supported while having no behavioral effect.
- HE-TB uplink users can similarly be evaluated using the wrong PHY mode.

Required work:

1. Add a per-user HE PHY mode or reception-parameter object.
2. Resolve the receiving user's RU, MCS, NSS, DCM, coding, and guard interval
   before error evaluation.
3. Compute user duration with symbol rounding, service bits, tail bits,
   coding, and packet-extension rules.
4. Evaluate each user's payload with the resolved mode and RU bandwidth.
5. Ensure scheduler estimates, generated PPDU duration, and error evaluation
   share one calculation path.
6. Add the missing 1.6 us HE guard interval.

Relevant files:

- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211PhyHeader.msg`
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmission.h`
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.cc`
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.cc`
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeMuUtil.h`

## 3.2 HE capability and operation negotiation

The implementation largely enables HE behavior from configuration and local
mode selection. It does not model the full management-plane negotiation that
determines whether an associated peer supports a feature.

Required additions include:

- HE Capabilities element.
- HE Operation element.
- Supported HE MCS/NSS maps.
- Supported channel widths.
- LDPC, DCM, STBC, TWT, OFDMA, and MU-MIMO capability bits.
- BSS color and spatial-reuse information.
- PPE thresholds where relevant to packet-level sensitivity.
- Per-peer negotiated capability state in the MIB.
- Capability-aware scheduling and frame construction.

The scheduler must not allocate an RU, MCS, NSS count, DCM mode, channel width,
or trigger behavior unsupported by the selected STA.

## 3.3 BSS coloring and spatial reuse

The HE PHY header includes a `bssColor` field, and the serializer preserves it,
but the field does not currently influence medium access or reception.

Spatial reuse is a central 802.11ax feature for dense deployments. A meaningful
packet-level implementation should include:

- BSS color assignment and advertisement.
- BSS color collision detection and color-change procedures.
- Intra-BSS versus inter-BSS frame classification.
- Separate intra-BSS and basic NAV state.
- OBSS/PD threshold configuration.
- Transmit-power restrictions coupled to the selected OBSS/PD threshold.
- Spatial Reuse Parameter Set support.
- Optional SRP-based behavior for trigger exchanges.
- Tests with multiple overlapping BSSs.

Without these mechanisms, the model covers OFDMA efficiency inside one BSS but
omits a major part of 802.11ax's dense-network behavior.

## 3.4 Complete trigger-based uplink behavior

The current uplink implementation is functional but intentionally simplified.

Important missing or incomplete behavior includes:

- Per-user UL A-MPDU transmission.
- Multiple MPDUs represented in Multi-STA Block Ack bitmaps.
- Full retry and partial-acknowledgment behavior.
- Additional relevant Trigger variants such as MU-BAR and MU-RTS.
- Standards-based response arrival-time tolerance.
- More accurate HE-TB common preamble and user-field treatment.
- Trigger-dependent coding and packet-extension parameters.
- Stronger handling of unscheduled, stale, or mismatched responses.

The current Multi-STA Block Ack path records a one-bit bitmap for a received
non-null response. It should acknowledge individual MPDUs and retry only the
unacknowledged subset.

The UL synchronization report also identifies an important testing gap. Add
end-to-end regression cases for:

1. Same-Trigger transmissions on different RUs, both successfully decoded.
2. Same-RU collision.
3. Same-RU capture.
4. Overlapping transmissions with different Trigger IDs.
5. Arrival skew around the selected synchronization tolerance.

## 3.5 Target Wake Time

There is no meaningful TWT implementation in the reviewed HE paths.

Add:

- Individual TWT setup, modification, and teardown.
- Broadcast TWT.
- Wake interval, wake duration, and service-period state.
- Triggered and non-triggered TWT service periods.
- Radio doze and awake transitions.
- Scheduler exclusion of sleeping stations.
- Interaction with EDCA, OFDMA, buffer reports, and retransmissions.
- Power-consumption and service-period statistics.

TWT is both a major power-saving feature and a useful deterministic-access
mechanism. It should be implemented before claiming broad 802.11ax MAC
coverage.

## 3.6 MU-MIMO and multiple per-user spatial streams

HE mode tables contain multi-stream entries and the HE MU user record contains
an NSS field, but HE scheduling and reception do not implement MU-MIMO.

Required work includes:

- Per-user NSS selection.
- Several users sharing a frequency resource through spatial separation.
- Channel-state or packet-level spatial-isolation abstractions.
- Beamforming, precoding, and sounding state.
- Combined OFDMA and MU-MIMO allocation.
- Stream-aware power allocation.
- Inter-user spatial interference.
- Error evaluation per stream or per user.

This is a substantial architectural project and should follow completion of
the per-user PHY pipeline.

## 4. Secondary PHY completeness gaps

After the per-user PHY architecture is corrected, the following features can
be added without creating metadata-only behavior:

- LDPC coding.
- Functional DCM.
- STBC.
- HE extended-range SU operation.
- Packet extension.
- Doppler indication and midambles.
- Preamble puncturing.
- Exact HE-LTF selection and duration.
- More accurate HE-SIG-A and HE-SIG-B behavior.
- Standard PHY field validation.

The present HE MU headers are useful internal simulation formats, but they are
not bit-accurate standard HE-SIG encodings.

## 5. Correctness and robustness improvements

### 5.1 Read the active radio channel

`HeDlMuTxOpFs` still infers channel center frequency from the mode set and uses
fixed defaults for the 2.4 and 5 GHz bands.

The schedule context should instead obtain:

- Actual center frequency.
- Actual channel bandwidth.
- Actual configured transmit power.
- Receiver noise figure and sensitivity.
- Current channel number and puncturing state.

This prevents scheduler, container, and radio-medium geometry from diverging.

### 5.2 Remove the MAC-derived STA-ID fallback

Most current paths can resolve the actual association ID. The fallback that
uses the low 11 bits of a MAC address can still produce collisions.

HE MU and Trigger frame construction should require an association ID whenever
the standard STA-ID field represents an associated STA. Special STA-ID values
should be modeled explicitly.

### 5.3 Replace fatal assembly failures with SU fallback

The DL MU builder throws runtime errors when final validation or packing leaves
fewer than two users.

MU assembly should be transactional:

1. Build and validate an immutable plan.
2. Verify that at least two users remain.
3. Mutate queues and in-progress state only after successful planning.
4. Restore state and invoke the ordinary SU path if late validation fails.

### 5.4 Tighten Block Ack window accounting

Packing currently limits the number of MPDUs using the agreement's configured
buffer size. It should account for the currently occupied transmit window and
available sequence-number slots.

### 5.5 Ensure queue and backlog metrics are eligibility-specific

Scheduler backlog should count only packets that can actually be packed under
the selected destination, TID, access category, and active Block Ack
agreement. Destination-wide backlog can overstate usable data.

## 6. Test and validation roadmap

The focused HE test suite currently passes:

```text
14 tests passed
```

The passing tests cover RU layouts, schedulers, HE mode calculations,
serialization, RU attenuation and isolation, receive filtering, ADDBA gating,
sequential acknowledgment, and HE control frames.

This demonstrates internal consistency of the implemented slice, but it does
not establish broad 802.11ax compliance.

Add the following test groups.

### 6.1 Per-user PHY tests

- Different users in one DL PPDU use different MCS values and obtain different
  error probabilities.
- User duration matches the generated number of HE symbols.
- NSS and DCM change the expected rate or error result.
- All three guard intervals are exercised.
- Scheduler duration and generated PPDU duration agree within one-symbol
  rounding.

### 6.2 Management capability tests

- HE capability and operation elements serialize and round-trip.
- Association produces the correct negotiated feature intersection.
- Unsupported RUs, MCS values, channel widths, NSS counts, or trigger modes are
  rejected.

### 6.3 Spatial-reuse tests

- Same-BSS frames update the intra-BSS NAV.
- Other-BSS frames update the basic NAV.
- OBSS/PD permits or prevents concurrent transmission at threshold
  boundaries.
- Transmit-power restrictions are applied.
- BSS color collision and color-change behavior work.

### 6.4 Uplink tests

- Different-RU simultaneous decoding.
- Same-RU collision and capture.
- Trigger-ID isolation.
- Timing-tolerance boundaries.
- Multi-MPDU partial Multi-STA Block Ack.
- UORA contention-window update after success and collision.

### 6.5 TWT tests

- Setup, modification, and teardown.
- Doze/awake timing.
- Scheduler behavior for sleeping stations.
- Triggered service periods.
- Energy-consumption reduction.

### 6.6 Performance validation

Add scenario-level expectations for:

- OFDMA throughput scaling.
- Airtime utilization.
- Queueing delay.
- Fairness and starvation prevention.
- Spatial-reuse gain in overlapping BSSs.
- TWT energy and latency tradeoffs.
- MU-MIMO scaling.

Correct metadata and successful serialization are not enough; each advertised
feature must affect observable simulation behavior.

## 7. Recommended implementation phases

### Phase 1: PHY correctness

- Introduce per-user HE PHY parameters.
- Use per-user RU/MCS/NSS/DCM for duration and error evaluation.
- Add the 1.6 us guard interval.
- Use one authoritative symbol-duration calculation.
- Obtain active channel and power parameters from the radio.

### Phase 2: HE management

- Add HE capability and operation elements.
- Store negotiated per-peer capabilities.
- Make schedulers capability-aware.
- Eliminate association-ID fallbacks for associated users.

### Phase 3: Dense-network MAC behavior

- Implement BSS coloring.
- Add dual NAV.
- Add OBSS/PD and transmit-power coupling.
- Add overlapping-BSS validation scenarios.

### Phase 4: Complete uplink OFDMA

- Add per-user UL A-MPDU.
- Add full Multi-STA Block Ack bitmaps and partial retry.
- Add timing-tolerance policy.
- Add the dedicated UL interference regression tests.

### Phase 5: Power saving

- Add individual and broadcast TWT.
- Integrate TWT with scheduling and radio state.

### Phase 6: Spatial multiplexing

- Add MU-MIMO, sounding, beamforming abstractions, and per-user NSS.
- Support combined OFDMA and MU-MIMO scheduling.

### Phase 7: Remaining HE PHY features

- LDPC, DCM, STBC, ER-SU, packet extension, Doppler/midambles, puncturing, and
  detailed HE signaling.

## 8. Overall assessment

The implementation is well structured and already solves several difficult
packet-level integration problems:

- Standard-shaped RU allocation.
- Destination-aware queueing.
- DL and UL OFDMA scheduling.
- One logical DL MU PPDU.
- Correlated parallel UL responses.
- RU-specific propagation and interference.
- Per-user payload extraction.
- Per-user aggregation and recovery.
- Transparent coexistence with legacy HCF paths.

The main limitation is no longer the absence of OFDMA procedures. It is PHY
semantic completeness: several per-user HE fields exist, but not all of them
control duration, decoding, or error probability.

Completing the per-user PHY path first will provide a sound base for every
other feature. HE management and spatial reuse should follow because they are
fundamental to standards-shaped operation in real deployments. TWT and
MU-MIMO are then the largest remaining feature areas needed for broad
802.11ax coverage.

