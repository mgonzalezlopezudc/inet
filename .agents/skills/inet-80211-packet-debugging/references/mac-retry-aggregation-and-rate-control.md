## Contents

* 8. DCF Debugging
* 8.1 Expected unicast exchange
* 8.2 DCF state reconstruction
* 8.3 Retry counters and limits
* 9. RTS/CTS and Hidden Stations
* 9.1 Expected protected exchange
* 9.2 Determine whether RTS should be used
* 9.3 Hidden-station diagnosis
* 9.4 RTS/CTS failure modes
* 9.5 Exposed-station diagnosis
* 10. EDCA, QoS, and HCF Debugging
* 10.1 Confirm QoS mode
* 10.2 Traffic classification
* 10.3 Per-access-category parameters
* 10.4 Internal collisions
* 10.5 TXOP
* 11. ACK Policies and Retransmission
* 11.1 Normal ACK
* 11.2 No ACK
* 11.3 Lost ACK scenario
* 11.4 Incorrect ACK correlation
* 12. Fragmentation and Defragmentation
* 12.1 Fragmentation checks
* 12.2 Failure modes
* 12.3 Correlation
* 13. Aggregation
* 13.1 A-MSDU
* 13.2 A-MPDU
* 13.3 Aggregation failure modes
* 13.4 Model-fidelity check
* 14. Block Acknowledgment
* 14.1 Agreement lifecycle
* 14.2 Agreement parameters
* 14.3 Block Ack exchange
* 14.4 Failure modes
* 15. Rate Selection and Rate Control
* 15.1 Distinguish rate selection from rate control
* Rate selection
* Rate control
* 15.2 Reconstruct algorithm state
* 15.3 Rate-control failure modes
* 15.4 Separate collision loss from channel loss

---

# 8. DCF Debugging

The Distributed Coordination Function uses contention, carrier sensing, backoff, and acknowledgment.

## 8.1 Expected unicast exchange

A basic successful exchange is conceptually:

```text
medium idle
DIFS
backoff countdown
DATA
SIFS
ACK
```

## 8.2 DCF state reconstruction

For every failed attempt, determine:

1. When did the frame enter the MAC queue?
2. When did it become the head-of-line frame?
3. Was the medium busy?
4. Which interframe space was required?
5. What backoff was selected?
6. Was the countdown frozen?
7. When was channel access granted?
8. Which frame-exchange sequence was selected?
9. Was RTS/CTS required?
10. Which PHY mode was selected?
11. Was an ACK expected?
12. Was the response received before timeout?
13. Which retry counter changed?
14. Was the contention window updated?
15. Was the MPDU retried or dropped?

## 8.3 Retry counters and limits

Distinguish:

* Short retry counter
* Long retry counter
* Per-frame or per-station state
* RTS failure
* DATA failure
* ACK loss
* CTS loss
* Retry limit reached

A missing ACK does not prove that the data frame was lost. The data may have been received correctly while the ACK was lost.

Use recipient captures and duplicate-removal statistics to distinguish:

```text
DATA lost
```

from:

```text
DATA received, ACK lost
```

A retry-limit drop can be exposed in INET result statistics, but exact statistic names are version-dependent. Query rather than assume them.

---

# 9. RTS/CTS and Hidden Stations

INET supports configurable RTS/CTS policy in relevant 802.11 MAC configurations, and its hidden-node showcase demonstrates threshold-based use of RTS/CTS.

## 9.1 Expected protected exchange

```text
DIFS or AIFS
backoff
RTS
SIFS
CTS
SIFS
DATA
SIFS
ACK or Block Ack
```

## 9.2 Determine whether RTS should be used

Inspect:

* RTS threshold
* Frame-size quantity compared with the threshold
* Whether the comparison uses MSDU, MPDU, fragment, or aggregate size
* Broadcast or multicast status
* QoS policy
* Protection policy
* HT/VHT/HE protection mode if implemented

Do not assume an application payload larger than the threshold necessarily produces RTS. The model may apply the threshold to a transformed MAC frame.

## 9.3 Hidden-station diagnosis

A hidden station exists when transmitters can interfere at a receiver while failing to sense each other adequately.

Demonstrate it with a power matrix:

| Transmitter | Observation station | Received power | Detectable? | Decodable? | Interferes at target? |
| ----------- | ------------------- | -------------: | ----------: | ---------: | --------------------: |
| STA A       | STA C               |              … |          No |         No |                     — |
| STA C       | STA A               |              … |          No |         No |                     — |
| STA A       | AP                  |              … |         Yes |        Yes |                   Yes |
| STA C       | AP                  |              … |         Yes |        Yes |                   Yes |

Inspect:

* Distances
* Obstacles
* Directional antennas
* Unequal power
* Asymmetric sensitivity
* Channel overlap

## 9.4 RTS/CTS failure modes

* RTS collides
* Receiver cannot decode RTS
* CTS collides
* Originator cannot decode CTS
* Hidden station cannot decode CTS
* CTS Duration does not reserve enough time
* DATA uses an unexpected duration
* CTS timeout is too short
* Response rate is incompatible
* RTS threshold is not applied
* Group-addressed traffic bypasses RTS/CTS
* Cross-technology interferer ignores NAV
* Exposed station unnecessarily defers

## 9.5 Exposed-station diagnosis

An exposed station detects a transmission and defers even though its own transmission would not disrupt the intended receiver.

Compare:

* Power at the deferring transmitter
* Power at both intended receivers
* Carrier-sense threshold
* Interference threshold or SNIR impact

Do not label ordinary contention as an exposed-station problem without receiver-side interference analysis.

---

# 10. EDCA, QoS, and HCF Debugging

INET’s MAC architecture includes DCF/HCF-related coordination components, EDCA channel access, per-access-category functions, and TXOP handling in applicable versions.

## 10.1 Confirm QoS mode

Check:

```ini
*.host[*].wlan[*].mac.qosStation
```

The exact path may differ.

Also inspect:

* QoS classifier
* Traffic-class mapping
* Queue structure
* Number of EDCA functions
* Access-category parameters

A QoS header alone does not prove that separate EDCA queues are active.

## 10.2 Traffic classification

Map:

```text
Application or network priority
        ↓
User Priority
        ↓
Traffic Identifier
        ↓
Access Category
```

Typical access categories are:

* Background: AC_BK
* Best effort: AC_BE
* Video: AC_VI
* Voice: AC_VO

Verify:

* Packet tags
* DSCP or PCP mapping
* Classifier rules
* TID
* Selected access category
* Queue receiving the frame

A packet can be starved because it was placed in the wrong access category even when EDCA itself works correctly.

## 10.3 Per-access-category parameters

For each access category, inspect:

* AIFSN
* CWmin
* CWmax
* TXOP limit
* Retry state
* Queue length
* Backoff counter
* Channel-access state

Reconstruct contention separately for every access category.

## 10.4 Internal collisions

Multiple access categories in one station may finish backoff simultaneously.

Expected conceptual behavior:

* The highest-priority eligible access category wins the internal contention.
* Losing local access categories behave as though they experienced a collision according to the implemented EDCA procedure.
* Only one transmission is emitted by the station.

Investigate:

* Incorrect winner
* Multiple simultaneous local transmissions
* Losing queue not updating contention state
* Starvation due to repeated internal collisions
* Shared instead of independent backoff state
* Incorrect AIFS handling

## 10.5 TXOP

During a transmission opportunity, a station may exchange multiple frames without returning to ordinary contention between every frame, subject to the configured TXOP limit and permitted frame sequences.

INET models TXOP behavior and can combine QoS data, aggregation, RTS/CTS, ACK, Block Ack Request, Block Ack, and Block Ack negotiation in applicable configurations.

For each TXOP, record:

* Start time
* Winning access category
* TXOP limit
* Every frame start and end
* SIFS gaps
* Recipient
* ACK policy
* Remaining TXOP time
* Reason for ending the TXOP

Check:

* TXOP overrun
* Premature termination
* Frame selected although it cannot fit
* Wrong access category
* Incorrect SIFS spacing
* Unexpected contention inside the TXOP
* Failure to release the medium after the TXOP
* Starvation of other stations

---

# 11. ACK Policies and Retransmission

## 11.1 Normal ACK

Unicast data commonly expects a normal ACK unless another ACK policy applies.

Verify:

* Frame is individually addressed
* ACK policy requests normal ACK
* Recipient accepts the frame
* Recipient sends ACK after SIFS
* ACK is addressed to the immediate transmitter
* Originator receives ACK before timeout
* Retry state resets after success

## 11.2 No ACK

Group-addressed data is generally not acknowledged with a normal ACK.

Do not diagnose the lack of ACK as failure for:

* Broadcast
* Multicast
* Frames explicitly using no-ACK policy
* Exchanges covered by Block Ack or another mechanism

## 11.3 Lost ACK scenario

Evidence pattern:

```text
Recipient capture:
  DATA received successfully
  ACK transmitted

Originator capture:
  DATA transmitted
  ACK absent or erroneous
  DATA retransmitted with same sequence number and Retry=1
```

Recipient duplicate removal may then discard the second copy while still acknowledging it.

## 11.4 Incorrect ACK correlation

Correlate ACKs by:

* Immediate transmitter and receiver
* Timing
* Exchange context
* Sequence state where available
* PHY response rate

ACK frames do not carry all data-frame identifiers, so timing and MAC state are essential.

---

# 12. Fragmentation and Defragmentation

INET’s MAC data services include fragmentation, defragmentation, aggregation, reordering, sequence numbering, and duplicate-removal responsibilities in applicable versions.

## 12.1 Fragmentation checks

Verify:

* Fragmentation policy enabled
* Fragmentation threshold
* Quantity compared with threshold
* Number of fragments
* Shared sequence number
* Increasing fragment number
* More Fragments bit
* Per-fragment ACK behavior
* Retry behavior for an individual fragment
* Final reassembly

## 12.2 Failure modes

* Threshold applied before or after encapsulation incorrectly
* Fragment too large
* Fragment numbers skip or repeat
* Sequence number changes between fragments
* More Fragments bit wrong
* Recipient loses defragmentation state too early
* Retransmitted fragment treated as new
* Duplicate fragment delivered twice
* Timeout expires before remaining fragments
* Aggregation and fragmentation applied in an invalid order
* Group-addressed frame fragmented unexpectedly
* Reassembled packet length incorrect

## 12.3 Correlation

Use:

```text
TA + sequence number + fragment number + retry bit
```

to track fragments.

Do not use upper-layer packet names alone because fragmentation can duplicate or transform packet objects.

---

# 13. Aggregation

Distinguish A-MSDU and A-MPDU.

## 13.1 A-MSDU

Conceptually, multiple MSDUs are combined into one MPDU payload.

Inspect:

* Aggregation policy
* Destination compatibility
* TID compatibility
* Aggregate-size limit
* Lifetime and delay limit
* Subframe headers and padding
* Resulting MPDU length
* Deaggregation
* Error impact

A corrupted A-MSDU MPDU can lose all contained subframes.

## 13.2 A-MPDU

Conceptually, multiple MPDUs are combined into one PHY transmission.

Inspect:

* Aggregate construction
* MPDU delimiters
* MPDU sequence numbers
* Per-MPDU FCS outcome
* Block Ack bitmap
* Reordering window
* Selective retransmission
* Aggregate-size and duration limits

Do not assume that INET supports A-MPDU merely because it supports some form of aggregation. Confirm the source implementation.

## 13.3 Aggregation failure modes

* Incompatible destinations aggregated
* Different TIDs aggregated incorrectly
* Aggregate exceeds size or duration limit
* Aggregate cannot fit within TXOP
* Padding or length calculation wrong
* Deaggregation loses packet tags
* Sequence numbers assigned after aggregation incorrectly
* Block Ack bitmap does not correspond to MPDUs
* Successfully received MPDUs retransmitted
* Missing MPDUs never retransmitted
* Reordering window stalls
* Aggregate decoded as one all-or-nothing packet despite expected per-MPDU behavior
* Capture format hides aggregate boundaries

## 13.4 Model-fidelity check

Determine whether the model represents:

* Aggregate as one packet object
* Nested packet chunks
* Separate MPDUs with a transmission wrapper
* Independent MPDU error decisions
* Single aggregate-level error decision

Document the abstraction before interpreting results.

---

# 14. Block Acknowledgment

INET provides Block Ack agreement-policy modules and a block-ack showcase in applicable releases.

## 14.1 Agreement lifecycle

Track:

```text
No agreement
    ↓
ADDBA Request
    ↓
ACK
    ↓
ADDBA Response
    ↓
ACK
    ↓
Agreement active
    ↓
Block Ack exchanges
    ↓
DELBA or timeout
```

Exact exchange behavior depends on policy and implementation.

## 14.2 Agreement parameters

Inspect:

* Originator and recipient
* TID
* Buffer or window size
* Starting sequence number
* Immediate or delayed Block Ack policy
* Agreement timeout
* Inactivity timeout
* Reordering behavior
* Negotiation threshold

## 14.3 Block Ack exchange

Correlate:

* MPDU sequence numbers
* Block Ack Request starting sequence number
* Block Ack starting sequence number
* Bitmap bits
* Received and missing MPDUs
* Retransmitted MPDUs
* Reordering-window movement

## 14.4 Failure modes

* ADDBA Request not generated
* ADDBA Response rejected
* TID mismatch
* Agreement exists only on one peer
* Agreement expires unexpectedly
* Starting sequence numbers disagree
* Bitmap interpreted with wrong offset
* Window does not advance
* Missing MPDU blocks delivery indefinitely
* Normal ACK used unexpectedly
* Block Ack expected before negotiation completes
* Retransmission exceeds retry limit
* DELBA handling incomplete

---

# 15. Rate Selection and Rate Control

INET separates rate selection for frame classes from rate-control algorithms for data frames in relevant versions. Implementations include modules such as AARF- and Onoe-oriented controllers in documented releases.

## 15.1 Distinguish rate selection from rate control

### Rate selection

Chooses the PHY mode for:

* Data
* Management
* RTS
* CTS
* ACK
* Block Ack
* Other control frames

### Rate control

Adapts the data rate according to success and failure history.

## 15.2 Reconstruct algorithm state

For every data attempt, record:

* Selected mode or MCS
* Success or failure
* Retry number
* Consecutive-success count
* Consecutive-failure count
* Probe state
* Rate increase or decrease
* Current threshold
* Timer state
* Receiver support

## 15.3 Rate-control failure modes

* Data rate never increases
* Data rate never decreases
* Lost ACK interpreted as data decode failure
* Algorithm state shared incorrectly between recipients
* Broadcast uses an unsuitable high rate
* Control response rate incompatible with peer
* Retry chain absent or malformed
* Rate changes during an aggregate incorrectly
* MCS selected without required channel width or spatial streams
* PHY mode object and displayed bitrate disagree
* Rate controller reacts to congestion collisions as though they were path loss

## 15.4 Separate collision loss from channel loss

Run controlled experiments:

1. One sender, no interference
2. Multiple senders, same RF link
3. Fixed rate
4. Adaptive rate

If the rate controller collapses under contention, determine whether it lacks collision awareness or receives insufficient feedback.

---

