# 802.11ax Downlink MU-OFDMA Implementation Report

## Executive summary

The current implementation provides a coherent packet-level model of IEEE
802.11ax downlink multi-user OFDMA. It covers:

- AP-side per-station queuing.
- Pluggable resource-unit scheduling.
- Standard RU allocation layouts for 20, 40, 80, and 160 MHz channels.
- Per-user A-MPDU packing.
- One logical HE-MU PPDU carrying multiple users.
- RU-specific received power, attenuation, noise, and SNIR.
- Receiver-side extraction of only the assigned user's payload.
- Sequential Block Ack/BAR recovery.
- Per-user retry handling.
- Transparent fallback to ordinary HCF single-user transmission.

Its strongest design choice is preserving the HE-MU PPDU as one radio
transmission while evaluating each selected station over its assigned frequency
range. Its main limitation is that per-user MCS and parallel PHY timing are not
fully reflected by the underlying transmission and error model: the MU
container still has one global transmission mode and a duration derived from
the aggregate serialized packet length.

The implementation should therefore be viewed as a strong MAC-level and
packet-level OFDMA model, but not yet as a waveform-accurate or fully
standards-complete 802.11ax PHY.

## 1. Architecture

| Layer | Main component | Responsibility |
|---|---|---|
| Queueing | `StationQueueBank` | Four FIFO queues per associated STA, one for each access category |
| Coordination | `HeHcf` | Detects MU opportunities and selects MU or SU operation |
| Scheduling | `IIeee80211HeDlScheduler` | Defines candidate information and returns STA/RU/MCS assignments |
| Frame sequence | `HeDlMuTxOpFs` | Builds the MU container and executes sequential acknowledgments |
| RU model | `Ieee80211HeRu` | Defines standard RU sizes, positions, and valid layouts |
| PHY framing | `Ieee80211PhyHeader.msg` | Carries per-user RU, STA ID, and MCS metadata |
| Radio medium | `Ieee80211RadioMedium` | Creates RU-specific receptions without splitting the PPDU |
| Receiver | `Ieee80211Receiver` | Extracts the assigned PSDU or exposes only the legacy preamble |

The primary implementation files are:

- `src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc`
- `src/inet/linklayer/ieee80211/mac/framesequence/HeDlMuTxOpFs.cc`
- `src/inet/linklayer/ieee80211/mac/scheduler/`
- `src/inet/linklayer/ieee80211/mac/queue/StationQueueBank.cc`
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211HeRu.h`
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.cc`
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211RadioMedium.cc`
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.cc`

## 2. End-to-end operation

The following sequence summarizes the complete downlink MU-OFDMA path:

1. Upper-layer traffic enters the AP MAC.
2. HCF classifies the packet into an EDCA access category.
3. Unicast traffic at an 802.11ax AP is routed to a per-STA, per-AC queue.
4. The corresponding EDCAF performs ordinary channel contention.
5. After winning access, `HeHcf` scans the queues for eligible destinations.
6. If at least two eligible STAs are present, `HeDlMuTxOpFs` is selected.
7. The configured DL scheduler chooses STAs, RUs, and estimated MCS values.
8. The frame sequence packs one PSDU or A-MPDU for each selected STA into one
   HE-MU container.
9. The radio translates the container metadata into an HE-MU PHY header.
10. The radio medium stores the PPDU as one transmission but computes each
    assigned STA's reception using its RU bandwidth and center frequency.
11. Each selected receiver extracts only its own PSDU.
12. The first STA sends a Block Ack after SIFS; subsequent STAs wait for BARs.
13. Missing responses trigger per-STA recovery and requeueing.
14. The EDCAF releases the channel or starts another contention/TXOP cycle.

## 3. Queueing and MU admission

### 3.1 Per-station queue banks

At an 802.11ax AP, unicast QoS packets are routed into per-STA, per-AC queues
instead of solely using the shared EDCAF queue. The routing hook is implemented
in `Hcf::processUpperFrame()`.

Each `StationQueueBank` contains:

- `AC_BK`
- `AC_BE`
- `AC_VI`
- `AC_VO`

Queue banks are dynamically created and destroyed as stations associate or
leave. This keeps the scheduling state aligned with the AP's station table.

The original enqueue time is stored in `OrigEnqueueTimeTag`. This is important
because packets may move between a station queue, the shared EDCAF queue, and
the in-progress set. The scheduler can therefore use the actual waiting time
rather than the most recent queue insertion time.

### 3.2 Schedule-context collection

When an EDCAF obtains the channel, `HeHcf::collectScheduleContext()` collects
one candidate per eligible destination. Candidate data includes:

- Destination MAC address.
- Access category.
- Total destination backlog in bytes.
- Head-of-line packet size.
- Original head-of-line enqueue time and delay.
- Source queue.
- Path-loss estimate and freshness.
- Anchor status.

The schedule context also includes:

- Channel center frequency and bandwidth.
- Remaining TXOP duration.
- Total transmit power.
- Receiver noise figure.

This is a deliberately rich scheduler interface. A scheduler can make
decisions based on queue state, delay, channel estimates, PHY resources, and
remaining airtime without depending directly on HCF internals.

### 3.3 Strict MU eligibility

A destination is eligible for the HE-MU container only when its packet is:

1. Unicast.
2. QoS data.
3. Covered by an active originator Block Ack agreement.

An agreement is not treated as active merely because it exists. The ADDBA
response must have been received.

Eligibility is checked at multiple points:

- During candidate collection.
- When matching scheduler allocations to queued packets.
- Immediately before queue removal.
- Before adding the final RU payload.

This repeated validation is intentional defensive programming. It prevents
queue mutation, sequence-number assignment, or in-progress state changes when
the Block Ack state has changed between scheduling and PPDU construction.

### 3.4 Anchor selection

Candidates are stably sorted by their original enqueue time. The oldest
eligible destination becomes the anchor.

Schedulers preserve this station preferentially when:

- Candidate count exceeds the configured station limit.
- Requested RU sizes do not fit the channel.
- An RU allocation must be downgraded.

The anchor is effectively a starvation-prevention invariant. Efficiency
optimizations cannot repeatedly exclude the oldest eligible destination.

### 3.5 Single-user fallback

`HeHcf` uses ordinary HCF operation when:

- The mode set is not `ax`.
- Fewer than two eligible destinations are present.
- The earliest SU-transmittable shared-queue packet is MU-ineligible.

When traffic exists only in per-STA queues and an MU transmission is not
possible, the oldest per-STA packet is staged into the shared queue for the
normal single-user sequence.

This preserves compatibility with existing EDCA and HCF behavior instead of
creating a second independent channel-access mechanism.

## 4. Scheduler interface

`IIeee80211HeDlScheduler` defines three central structures.

### 4.1 `CandidateInfo`

This describes a possible scheduled station:

- `staAddress`
- `accessCategory`
- `anchor`
- `backlogBytes`
- `holPacketBytes`
- `holEnqueueTime`
- `holDelay`
- `pathLossDb`
- `hasFreshPathLoss`
- `sourceQueue`

### 4.2 `ScheduleContext`

This describes the transmission opportunity:

- Candidate list.
- Anchor address.
- Channel center frequency and bandwidth.
- Remaining TXOP limit.
- Total transmit power.
- Receiver noise figure.

### 4.3 `RuAllocation`

The scheduler returns:

- Selected station.
- Concrete `Ieee80211HeRu`.
- MCS index.
- Estimated SNR.
- Estimated user duration.

The interface also retains a simpler legacy scheduling overload that receives
only destination addresses and channel parameters. It creates a minimal
schedule context and delegates to the context-aware scheduler.

## 5. Available schedulers

### 5.1 Equal-sized RU scheduler

`HeDlSchedulerEqualSizedRUs` assigns one equal-sized RU to each selected STA.
It supports two policies.

#### `fBW`

`fBW` selects the largest standard RU count that does not exceed the number of
candidates.

For example, with three candidates on a 20 MHz channel, valid equal-sized
counts are 1, 2, 4, and 9. The scheduler chooses the two-RU layout and serves
two stations with 106-tone RUs.

This policy prioritizes larger bandwidth per selected user.

#### `fHoL`

`fHoL` selects the smallest standard RU count that can accommodate the
candidates.

With three candidates on 20 MHz, it selects the four-RU layout and serves all
three candidates, leaving one RU unused.

This policy prioritizes serving more queued destinations.

#### Candidate ordering

Before selection, candidates are sorted by:

1. Anchor first.
2. Larger HoL delay.
3. Larger backlog.
4. MAC address.

The scheduler is deterministic and is the default scheduler configured by
`HeHcf`.

### 5.2 Backlog-based scheduler

`HeDlSchedulerBacklogBased`:

- Always starts with the anchor.
- Includes stations whose path loss differs from the anchor by at most
  `deltaPlMax`.
- Does not exclude candidates when either path-loss estimate is missing or
  stale.
- Sorts non-anchor candidates by path-loss similarity, backlog, HoL delay, and
  MAC address.
- Requests RU size based on the destination's total queued bytes.

Path-loss grouping attempts to avoid combining stations with very different
channel conditions and consequently very different PPDU completion times.

### 5.3 HoL minimum-delay scheduler

`HeDlSchedulerHoLMinDelay` sorts candidates by:

1. Anchor.
2. Largest HoL delay.
3. Largest HoL packet.
4. Largest backlog.
5. MAC address.

Unlike the backlog scheduler, it requests RU size from the HoL packet size.
This favors reducing delay for the oldest immediately transmittable packets
rather than draining complete destination queues.

## 6. Common RU sizing and fitting

Queue-aware schedulers use `HeDlSchedulerBase::fitRequestedRus()`.

### 6.1 Backlog-to-RU mapping

Default requested sizes are:

| Queue data | Requested RU |
|---|---:|
| Up to 80 B | 26 tones |
| Up to 500 B | 52 tones |
| Up to 1500 B | 106 tones |
| Up to 6000 B | 242 tones, or 484 on sufficiently wide channels |
| Above 6000 B | Full channel |

All thresholds are configurable NED parameters.

### 6.2 Initial fitting

Requests are ordered largest-first and passed to the canonical RU allocator.
The allocator chooses the first non-overlapping standard RU matching each
requested size.

If the requests do not fit:

- Candidates with RUs larger than 26 tones are considered for downgrade.
- Non-anchor candidates are downgraded before the anchor.
- Larger requests are generally downgraded before smaller requests.
- Fitting repeats until the layout succeeds or all requests reach 26 tones.

### 6.3 Duration alignment

The scheduler estimates each user's duration from:

- Payload bytes.
- RU tone count.
- Selected MCS.
- A fixed 48 microsecond overhead.

It then optionally performs iterative duration alignment. A candidate is
considered:

- Slow if its duration exceeds `highDurationRatio` times the mean.
- Fast if its duration is below `lowDurationRatio` times the mean.

Proposals may:

- Enlarge a slow user's RU.
- Shrink a fast user's RU.
- Exchange capacity by enlarging one slow user and shrinking one fast user.

Only proposals that fit and reduce duration variance are accepted.

This is a local-search heuristic. It is deterministic and inexpensive, but it
does not guarantee a globally optimal allocation.

### 6.4 MCS estimation

If a fresh path-loss estimate is available, scheduler SNR is estimated by:

1. Converting total transmission power to dBm.
2. Allocating power in proportion to RU bandwidth.
3. Subtracting path loss.
4. Computing thermal noise over the RU bandwidth.
5. Adding the configured receiver noise figure.

MCS 0 through 11 is selected using configurable SNR thresholds.

If the estimate is absent or stale, the implementation conservatively selects
MCS 0.

## 7. Standard RU representation

`Ieee80211HeRu` explicitly stores:

- Catalog index.
- Tone count.
- Tone offset.
- Number of data subcarriers.
- Number of pilot subcarriers.
- Center frequency.
- Bandwidth.

Supported RU sizes are:

- 26 tones.
- 52 tones.
- 106 tones.
- 242 tones.
- 484 tones.
- 996 tones.
- 1992 tones.

Supported channel layouts are:

| Channel | HE tones | Maximum 26-tone RU count |
|---|---:|---:|
| 20 MHz | 242 | 9 |
| 40 MHz | 484 | 18 |
| 80 MHz | 996 | 37 |
| 160 MHz | 1992 | 74 |

The allocation catalog is generated as a canonical tree:

- `1992 -> 996 + 996`
- `996 -> 484 + central 26 + 484`
- `484 -> 242 + 242`
- `242 -> 106 + central 26 + 106`
- `106 -> 52 + 52`
- `52 -> 26 + 26`

Small gaps represent DC or guard tones between standard sibling RUs.

Final layouts are validated for:

- Supported channel bandwidth.
- Catalog membership.
- Unique RU indices.
- Valid tone bounds.
- Non-overlap.

Using explicit tone offsets is a key improvement over an approximation that
merely divides channel bandwidth by user count.

## 8. Link estimates

The AP MIB records per-station:

- Advertised station transmit power.
- Most recently received power.
- Derived path loss.
- Update timestamp.
- Validity.

The transmit power is learned from association management information. Received
power is updated when the AP receives a two-address frame from the station.

An estimate is fresh when its age is less than `linkEstimateMaxAge`.

One configuration risk is that the scheduler's `totalTransmitPower` and
`receiverNoiseFigure` are independent `HeHcf` parameters. They are not
automatically read from the configured radio. For example, the OFDMA example
sets radio transmit power to 10 mW while `HeHcf` defaults to 20 mW. Unless the
parameters are synchronized, MCS and duration estimates may be biased.

## 9. MU PPDU construction

`HeDlMuTxOpFs` asks the scheduler for allocations and builds one packet named
`HE-MU-PPDU`.

### 9.1 Container representation

The outer MAC header is a broadcast QoS data header. The broadcast address acts
as the common container destination rather than identifying an individual
user.

The payload contains one section per selected station:

1. `Ieee80211HeMuRuPayloadHeader`
2. The station's PSDU or A-MPDU

The per-user header stores:

- RU index.
- RU tone size.
- RU tone offset.
- STA ID.
- MCS.
- PSDU length.

This is an internal simulation representation. It is not a bit-accurate
encoding of the standard HE-SIG fields.

### 9.2 Per-user aggregation

For each allocation, the frame builder selects packets that share:

- Destination STA.
- QoS data type.
- TID.
- Active Block Ack agreement.

Packing is limited by:

- `maxAmpduMpduCount`.
- Block Ack agreement buffer size.
- `maxHeMuPsduLength`.
- Estimated aligned user duration.
- `maxHeMuPpduDuration`.
- Remaining TXOP after reserving acknowledgment exchanges.

When several MPDUs are selected, the implementation adds A-MPDU delimiters and
four-byte alignment padding.

### 9.3 Safe queue mutation

Queue removal and sequence-number assignment happen only after:

- Initial eligibility validation.
- Scheduler allocation matching.
- Block Ack agreement validation.
- Packing-limit validation.
- Confirmation that at least two allocations remain.

For every selected MPDU:

- The packet is removed from its queue.
- A sequence number is assigned if this is not a retry.
- Ack policy is set to Block Ack.
- Duration is set to cover the sequential response exchange.
- The packet is added to the HCF in-progress set.

Deferring state mutation until final validation is one of the implementation's
strongest reliability decisions.

## 10. Sequential acknowledgment procedure

The current implementation does not model simultaneous uplink OFDMA Block Ack
responses. It uses a sequential exchange:

1. AP sends the HE-MU PPDU.
2. The first scheduled STA sends a Block Ack after SIFS.
3. The AP sends a BAR to the second STA.
4. The second STA responds with Block Ack after SIFS.
5. BAR and Block Ack repeat for every later STA.

The total reserved duration is:

- `SIFS + BlockAck` for the first allocation.
- `SIFS + BAR + SIFS + BlockAck` for every later allocation.

The total is written into the outer container and constituent data headers.

At the recipient:

- The first allocation responds immediately.
- Later allocations record the received MPDUs but wait for the AP's BAR.

`HeFrameSequenceHandler` treats a Block Ack timeout specially. It rejects the
current receive step and continues the sequence instead of aborting the entire
TXOP.

### 10.1 Per-user recovery

When an allocation times out:

- Its packets are individually passed to failure processing.
- Retry counters are updated.
- Rate control receives failure information.
- The packets are marked as retries.
- Retriable packets are returned to the corresponding per-STA queue.
- Packets exceeding the retry limit are dropped.

Other allocations remain independent. This provides useful failure isolation
despite all users sharing one PPDU.

## 11. PHY and radio-medium model

### 11.1 HE-MU PHY header construction

Before transmission, `Ieee80211Radio::encapsulate()` scans the internal MU
payload sections and creates an `Ieee80211HeMuPhyHeader`.

Each `Ieee80211HeMuUserInfo` contains:

- RU index.
- RU tone size and offset.
- STA ID.
- MCS.
- Number of spatial streams.
- DCM flag.

The normal MU construction currently sets only RU information, STA ID, and
MCS. Number of spatial streams remains one, DCM remains false, and BSS color
remains zero.

### 11.2 One-transmission design

The radio creates one `Ieee80211Transmission` for the complete HE-MU PPDU. It
does not create one independent transmission per user.

This preserves:

- One PPDU start and end time.
- Common channel occupancy.
- Common preamble and protection semantics.
- A single transmission identity in the radio medium.

This is arguably the implementation's most important architectural decision.

### 11.3 RU-specific reception

When the radio medium evaluates a selected receiver, it finds that receiver's
user entry and constructs an RU-specific analog reception:

- Center frequency becomes the RU center.
- Bandwidth becomes the RU bandwidth.
- Power is scaled by RU bandwidth divided by full-channel bandwidth.
- Preamble, header, and data timing remain those of the common PPDU.

As a result:

- Path loss can be evaluated at the RU center frequency.
- Noise is integrated over the RU bandwidth.
- Interference is evaluated over the RU's frequency range.
- Disjoint RUs do not act as mutually interfering independent transmissions.

### 11.4 Receiver filtering

The receiver:

- Looks up its STA ID in the HE-MU PHY header.
- Extracts only the matching RU payload from the container.
- Delivers only that user's PSDU to the MAC.

A non-selected station or incompatible receiver gets a synthetic
`HE-MU-Legacy-Preamble` indication instead. This indication carries the
transmission duration so the MAC can maintain legacy-visible medium occupancy
without receiving another user's data.

## 12. Important implementation decisions

### 12.1 One PPDU instead of synthetic per-user transmissions

This preserves common timing and channel occupancy and avoids treating
frequency-parallel users as unrelated transmitters.

### 12.2 RU-aware radio-medium evaluation

Frequency-domain isolation belongs in the physical-medium computation. Each
assigned receiver receives an analog model for its RU, enabling meaningful
attenuation, noise, and interference behavior.

### 12.3 Strict active-ADDBA gating

MU operation is limited to QoS traffic with an active Block Ack agreement. This
keeps aggregation, acknowledgment, and retry semantics well-defined.

### 12.4 Per-STA queues below EDCA

The implementation retains ordinary access-category contention while adding
destination-aware scheduling after channel acquisition. This minimizes
disruption to the existing HCF architecture.

### 12.5 Oldest-candidate anchor

The oldest eligible destination receives explicit protection during selection
and RU fitting, reducing starvation risk.

### 12.6 Rich pluggable scheduler contract

Schedulers are separate modules and can use queue, delay, path-loss, TXOP,
power, and noise information. This provides a sound research-extension point.

### 12.7 Canonical RU allocation tree

All RUs use standard sizes and placements, and layouts are validated. This is
more robust than arbitrary equal division.

### 12.8 Per-user, per-TID aggregation

Every user receives a separately packed PSDU constrained by its Block Ack
agreement and the common PPDU/TXOP limits.

### 12.9 Sequential Block Ack

Sequential response handling avoids requiring uplink OFDMA and trigger-frame
support. The tradeoff is additional BAR and SIFS overhead.

### 12.10 Additive fallback behavior

When MU conditions are not met, the implementation delegates to ordinary HCF
rather than replacing or duplicating it.

## 13. Current limitations and risks

### 13.1 Per-user MCS is not fully enforced

The scheduler selects an MCS and it is stored in the HE-MU user metadata.
However, the underlying transmission still carries one global
`IIeee80211Mode`.

Consequently, the selected per-user MCS influences:

- Scheduler duration estimates.
- Packing limits.
- Serialized metadata and visualization.

It does not necessarily determine:

- Per-user PHY error probability.
- Actual per-user symbol duration.
- The transmission's global mode.

### 13.2 Airtime is derived from aggregate container length

`Ieee80211Transmitter::createTransmission()` computes the transmission duration
from the HE PHY header's length field. That field is set from the complete
serialized container length.

A real OFDMA PPDU carries user PSDUs in parallel and is approximately governed
by the longest user allocation. Treating all user payload bytes as one
sequential data field can:

- Overestimate airtime.
- Underestimate throughput.
- Distort TXOP use.
- Distort collision and queueing behavior.

This is the most important fidelity limitation.

### 13.3 Channel center frequency is inferred

`HeDlMuTxOpFs` derives bandwidth from the first HE mode but substitutes:

- 2.412 GHz for the 2.4 GHz band.
- 5.18 GHz for the 5 GHz band.

It does not read the actual configured radio channel. Simulations on other
channels can therefore receive incorrect RU center frequencies at the
scheduler/container layer.

### 13.4 STA ID is not the association ID

`computeHeMuStaId()` uses the low 11 bits of the MAC address. The code marks
this as a fallback until association ID information is available at the PHY
boundary.

Two MAC addresses can therefore collide and appear to represent the same
HE-MU STA ID.

### 13.5 No uplink OFDMA procedure

The implementation currently lacks:

- Trigger frames.
- Uplink RU scheduling.
- Simultaneous uplink responses.
- Trigger-based Block Ack exchange.

### 13.6 Incomplete advanced HE features

Fields exist for some features, but the DL MU path does not meaningfully model:

- MU-MIMO.
- Multiple per-user spatial streams.
- DCM.
- BSS color.
- Preamble puncturing.
- Detailed HE-SIG-A and HE-SIG-B behavior.

### 13.7 Internal framing is not standards-bit-accurate

`Ieee80211HeMuPhyHeader` and `Ieee80211HeMuRuPayloadHeader` are useful
simulation formats, but they are not complete standard encodings.

### 13.8 Candidate suppression edge case

During candidate collection, a destination is added to `seenDestinations`
before its packet is checked for MU eligibility. An earlier ineligible packet
can suppress a later eligible packet for the same destination during that
scan.

### 13.9 Backlog accounting is destination-wide

`backlogBytes` sums all queued packets for the destination across the candidate
queues. These bytes may include other TIDs or packets that are not currently
eligible for the same Block Ack agreement. A backlog-based RU request can
therefore overstate the data that can actually be packed into the selected
PSDU.

### 13.10 Final MU assembly failure is fatal

If fewer than two users remain after final validation or packing, the frame
builder throws a runtime error. A production-quality simulation path would
preferably restore state and fall back to SU transmission.

### 13.11 Greedy RU placement

The allocator selects the first available canonical RU of the requested size.
Combined with local downgrade and duration-adjustment heuristics, it can miss a
feasible or superior global allocation.

## 14. Testing and validation

Focused tests cover:

- Equal-sized scheduling.
- Backlog- and HoL-aware scheduling.
- RU catalog generation and validation.
- Active ADDBA validation.
- Sequential Block Ack and BAR timing.
- Timeout recovery and individual retries.
- One-transmission radio-medium behavior.
- Assigned-RU receiver filtering.
- Legacy-preamble behavior.
- RU-specific attenuation.
- RU noise and SNIR isolation.
- HE-MU PHY header serialization.
- Recipient Block Ack gating.

The main tests are:

- `tests/unit/HeDlScheduler_1.test`
- `tests/unit/HeDlSchedulerQueueAware_1.test`
- `tests/unit/Ieee80211HeRu_1.test`
- `tests/unit/Ieee80211HeMuAddbaValidation_1.test`
- `tests/unit/Ieee80211HeMuSeqAck_1.test`
- `tests/unit/Ieee80211HeMuRadioMedium_1.test`
- `tests/unit/Ieee80211HeMuRx_1.test`
- `tests/unit/Ieee80211HeMuRuAttenuation_1.test`
- `tests/unit/Ieee80211HeMuRuNoiseIsolation_1.test`
- `tests/unit/Ieee80211HeMuPhyHeaderSerializer_1.test`
- `tests/unit/Ieee80211HeMuBlockAckGating_1.test`

The repository also contains an OFDMA example at:

- `examples/ieee80211/ofdma`

Its default configuration uses:

- A 20 MHz 5 GHz channel.
- Three stations.
- QoS and Block Ack support.
- `HeHcf`.
- `HeDlSchedulerEqualSizedRUs`.
- `fBW`.
- A maximum of three MU stations.

### Validation performed for this report

The focused HE/OFDMA unit-test command was run with ccache disabled and both
the OMNeT++ and INET environments loaded.

Result:

- 12 focused unit tests passed.
- No focused test failed.

The OFDMA example validation script was also run:

```sh
tests/validation/ieee80211/ofdma_example_validation.sh
```

Result:

- The sequential Block Ack timing test passed.
- The `General` OFDMA simulation successfully reached its configured
  2.0-second simulation limit.
- The validation script exited successfully.

## 15. SWOT analysis

### Strengths

- Clean separation among coordination, scheduling, queueing, frame sequencing,
  and PHY handling.
- Standards-shaped RU catalog with explicit non-overlap validation.
- One-transmission PPDU representation.
- RU-specific power, attenuation, noise, and interference handling.
- Multiple scheduler strategies.
- Fairness protection through the anchor.
- Strong ADDBA validation and delayed queue mutation.
- Per-user aggregation, acknowledgment, timeout, and retry handling.
- Transparent compatibility with existing HCF and SU operation.
- Broad focused regression coverage.

### Weaknesses

- Global PHY mode despite per-user MCS metadata.
- Aggregate-length airtime does not correctly model parallel OFDMA PSDUs.
- Hard-coded channel-center inference.
- Derived STA ID can collide.
- Sequential Block Ack adds substantial overhead.
- Scheduler power and noise parameters can diverge from radio configuration.
- Runtime aborts instead of SU fallback in some final-validation cases.
- Candidate and backlog accounting is destination-wide rather than rigorously
  per eligible TID.
- No detailed HE signaling or waveform model.

### Opportunities

- Make transmission duration the maximum per-user RU duration.
- Apply each user's MCS to error-rate and duration calculations.
- Read center frequency, bandwidth, and transmit power directly from the active
  radio.
- Use the actual association ID.
- Add a standards-oriented trigger-based or multi-STA Block Ack exchange.
- Add proportional-fair, deficit-round-robin, latency-budget, or
  channel-aware schedulers.
- Track available Block Ack window slots instead of only configured window
  size.
- Support puncturing, MU-MIMO, multiple spatial streams, DCM, and BSS color.
- Turn final MU assembly failure into safe SU fallback.
- Add performance validation for throughput, fairness, airtime, and delay, not
  only correctness.

### Threats

- Studies may overestimate airtime and underestimate OFDMA throughput because
  payloads are serialized for duration calculation.
- Per-user MCS experiments may appear realistic at the scheduler level while
  the PHY outcome still follows the global mode.
- MAC-derived STA ID collisions could deliver or evaluate the wrong RU in
  large scenarios.
- Hidden parameter mismatches can bias scheduler decisions.
- Larger or mixed-TID workloads may expose candidate-selection and
  backlog-accounting artifacts.
- Future changes to generic HCF, aggregation, or radio timing could silently
  invalidate assumptions made by the custom container.

## 16. Recommended development priorities

The highest-value next steps are:

1. Correct PPDU airtime so it is based on the maximum per-user transmission
   duration rather than aggregate container bytes.
2. Integrate per-user MCS into reception success and error-rate calculations.
3. Obtain channel center frequency, bandwidth, and transmit power from the
   active radio configuration.
4. Replace the MAC-derived STA ID with the actual association ID.
5. Gracefully revert to SU transmission when final MU construction retains
   fewer than two users.
6. Make candidate discovery and backlog accounting TID- and
   Block-Ack-agreement-aware.
7. Add throughput, airtime, fairness, and latency validation against expected
   OFDMA scaling behavior.

## Conclusion

The implementation has a well-structured and extensible MAC design. It
correctly addresses several difficult integration problems: destination-aware
queueing, standards-shaped RU allocation, safe aggregation, common-PPDU
representation, RU-specific propagation, receiver filtering, and per-user
recovery.

The code is also backed by a meaningful focused test suite and a working
end-to-end example.

The central remaining issue is PHY fidelity. Scheduling and metadata describe
parallel per-user RUs, but actual PPDU duration and reception mode are still
largely inherited from a single aggregate transmission. Resolving this
disconnect would move the implementation from a strong functional
packet-level OFDMA model toward a substantially more accurate 802.11ax
performance model.
