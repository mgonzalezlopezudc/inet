# UL MU-OFDMA Synchronization and AP Decoding Notes

## Context

The UL MU-OFDMA simulation initially appeared to show the STAs transmitting at
different times rather than simultaneously. This note records the findings from
tracing the MAC, PHY, radio-medium, and simulation-result paths.

## Observed transmission timing

In one `ScheduledOnly` UL exchange, the recorded STA radio transmission start
times were:

- STA 0: `0.207627565950 s`
- STA 2: `0.207627576670 s`
- STA 1: `0.207627618488 s`

The maximum departure-time skew was approximately `52.5 ns`. The transmissions
then overlapped for their common duration of approximately `1 ms`.

The STAs therefore did not have identical simulation timestamps, but they did
transmit concurrently.

## Why the start times differ

Each selected STA schedules its HE trigger-based response after one SIFS:

```cpp
tx->transmitFrame(responsePacket,
        responsePacket->peekAtFront<Ieee80211MacHeader>(),
        modeSet->getSifsTime(), this);
```

`Tx` implements this by scheduling a local timer relative to the instant at
which that STA processes the Trigger frame:

```cpp
scheduleAfter(ifs, endIfsTimer);
```

Consequently, differences in Trigger arrival or processing time become
differences in the absolute departure timestamps of the responses. Propagation
delay also means that perfectly aligned trigger-relative responses need not
have globally identical departure or arrival timestamps.

The configured example positions place all three STAs 5 m from the AP, but the
recorded skew still shows that the simulation should not assume exact timestamp
equality.

Relevant files:

- `src/inet/linklayer/ieee80211/mac/coordinationfunction/HeHcf.cc`
- `src/inet/linklayer/ieee80211/mac/Tx.cc`
- `examples/ieee80211ax/ul_ofdma/omnetpp.ini`

## Standards interpretation

IEEE 802.11ax trigger-based uplink operation does not require every STA to have
the same global transmitter timestamp. A STA responds to the Trigger after the
required interframe spacing, and the important PHY property is that the
responses arrive sufficiently aligned at the AP to form a decodable HE
trigger-based multi-user exchange.

Thus, the observed `52.5 ns` departure-time difference is not by itself a
standards violation. It is also much smaller than the minimum HE guard interval
of `0.8 us`.

However, exact standard compliance involves more than overlapping packet-level
transmissions. It includes HE-TB synchronization, preamble behavior, timing
accuracy, and receiver processing. The current implementation is a packet-level
approximation and should not be described as a complete waveform-level
standards implementation.

References:

- IEEE 802.11-2024:
  <https://standards.ieee.org/ieee/802.11/10548/>
- IEEE 802.11ax-2021:
  <https://standards.ieee.org/ieee/802.11ax/7180/>

## How each STA transmission is represented

Each STA response remains a separate radio-medium transmission object. It is
not merged with the other STA responses into one composite transmission
object.

This representation can still correctly model packet-level OFDMA if:

1. Every response is associated with the same Trigger exchange.
2. Every response occupies only its assigned RU.
3. The AP can attempt several matching HE-TB receptions concurrently.
4. Non-overlapping RUs do not contribute interference to one another.
5. Overlapping RUs do contribute interference and can collide.

The current implementation provides these mechanisms.

## Trigger correlation and parallel reception

The HE PHY header carries the PPDU format and Trigger ID. The AP receiver treats
an incoming transmission as another admissible parallel reception only when:

- it is an HE trigger-based uplink PPDU; and
- its Trigger ID matches the Trigger ID of the reception already in progress.

The relevant logic is in
`Ieee80211Receiver::computeIsReceptionAttempted()`:

```cpp
return currentHeader != nullptr &&
       currentHeader->getPpduFormat() == HE_TRIGGER_BASED_UPLINK &&
       currentHeader->getTriggerId() == heMuPhyHeader->getTriggerId();
```

The generic radio is also modified so HE-TB reception timers can continue and
finish in parallel instead of only the first reception being processed.

Relevant files:

- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Receiver.cc`
- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.cc`
- `src/inet/physicallayer/wireless/common/radio/packetlevel/Radio.cc`

An earlier concern that Trigger-ID correlation was absent was too broad.
Correlation is present in the AP receiver. What is absent is aggregation of all
the responses into a single radio-medium transmission object.

## RU-specific transmission

For an HE trigger-based uplink response containing one user allocation, the
transmitter changes the signal from the full channel to the assigned RU:

- bandwidth becomes `ruToneSize * 78.125 kHz`;
- center frequency is shifted using the RU tone offset;
- duration becomes the Trigger-commanded common duration;
- requested transmit power is applied when present.

This means that the analog representation of each STA transmission is already
limited to that STA's RU.

Relevant file:

- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Transmitter.cc`

## RU-specific reception at the AP

For an HE trigger-based uplink transmission, the radio medium reads the RU
geometry from the HE MU PHY header and creates a reception with:

- the assigned RU center frequency;
- the assigned RU bandwidth;
- the received power of that STA signal;
- the common HE-TB timing.

The AP therefore evaluates a separate RU-band reception for each STA rather
than evaluating every STA as a full-channel signal.

Relevant file:

- `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211RadioMedium.cc`

## Interference treatment

This is the key distinction between OFDMA responses and unrelated independent
transmissions.

For each desired HE MU RU, the radio medium:

1. Collects all temporally interfering receptions.
2. Reads the center frequency and bandwidth of every interfering reception.
3. Retains only those whose frequency ranges overlap the desired RU.
4. Computes noise and SNIR using that filtered set.

The overlap condition is:

```cpp
if (std::min(desiredMax, interferingMax) >
        std::max(desiredMin, interferingMin))
    overlappingReceptions->push_back(interferingReception);
```

The resulting behavior is:

- Different, non-overlapping RUs are orthogonal and do not interfere with one
  another.
- Two STAs using the same RU remain in one another's interference sets.
- Partially overlapping allocations also interfere.
- Unrecognized or non-scalar interfering signals are conservatively retained
  as interference.

Therefore, the AP does not incorrectly treat all concurrent scheduled STA
responses as independent full-channel interferers.

## Packet decoding and delivery

Each HE-TB reception receives its own reception decision and SNIR evaluation.
The decoded packet is then delivered separately to the MAC.

The AP's UL MU frame sequence collects all responses until the collection
deadline. For every received data or QoS Null response it:

- determines the STA's AID from its transmitter address;
- marks that user's Multi-STA Block Ack record as received;
- records sequence information for non-null data;
- processes the triggered UL data and buffer-status information.

Relevant file:

- `src/inet/linklayer/ieee80211/mac/framesequence/HeUlMuTxOpFs.cc`

## Evidence from the scheduled example

The existing `ScheduledOnly` result contains:

- 52 Basic Trigger exchanges;
- 156 scheduled-user allocations;
- a maximum of 3 scheduled users per Trigger;
- 162 AP buffer-status updates;
- 237 unicast frames received by the AP HCF;
- 114 UDP packets delivered to the server.

The 156 allocations for 52 exchanges correspond to three scheduled STAs per
exchange. These results are consistent with the AP receiving and processing
multiple parallel responses rather than only decoding the first arriving STA.

The result data alone is not a formal PHY correctness proof, but it supports the
traced implementation behavior.

## What is correct in the current abstraction

For the current scheduled UL OFDMA example:

- Responses are correlated by Trigger ID at the receiver.
- The AP admits multiple concurrent HE-TB receptions from the same exchange.
- Each STA transmits on its assigned RU frequency and bandwidth.
- Each STA reception is evaluated on that RU.
- Non-overlapping scheduled RUs do not interfere.
- Same-RU random-access responses do interfere and can collide or capture.
- Successfully decoded responses are delivered separately to the AP response
  collection and acknowledged using a Multi-STA Block Ack.

The core packet-level interference behavior is therefore appropriate for UL
OFDMA.

## Modeling limitations

The current implementation is intentionally idealized:

- The complete UL MU PPDU is represented by several correlated transmissions,
  not one composite transmission object.
- RU orthogonality is ideal; adjacent-channel leakage is not modeled.
- Imperfect frequency synchronization is not modeled.
- Waveform-level timing synchronization and inter-symbol interference are not
  modeled.
- HE-TB preamble details are simplified. The entire STA signal is effectively
  represented with the RU bandwidth.
- Trigger-ID matching is used for admission, but there is no explicit
  standards-level arrival-time tolerance check.
- The current HE error path should be reviewed to ensure every per-user MCS and
  RU-specific PHY parameter is used with the desired fidelity.

These limitations do not make the current packet-level scheduled example
behave like ordinary independent transmissions. They do mean that the result
should be described as an idealized packet-level HE UL OFDMA model rather than
a complete standards-conformance PHY.

## Test coverage gap

The existing HE MU RU tests mostly exercise downlink-style MU container packets
or inspect RU metadata. Their names and descriptions imply stronger isolation
coverage than their implementations actually provide.

In particular, there is no strong end-to-end regression test that creates
multiple separate, simultaneous HE trigger-based uplink transmissions and
proves both of these cases:

1. Different RUs are simultaneously decoded by one AP.
2. The same RU produces interference, collision, or capture.

The simulation example exercises the behavior indirectly, but dedicated tests
would make the guarantee explicit and protect it from regressions.

## Recommended regression tests

### Different-RU simultaneous decoding

Create one AP and at least two STAs. Generate two HE-TB transmissions with:

- the same Trigger ID;
- the same common duration;
- overlapping arrival intervals;
- distinct non-overlapping RUs;
- sufficient received power.

Verify that:

- the AP attempts both receptions;
- each interference set excludes the other RU;
- both packets decode successfully;
- both packets reach the receive-collection step;
- the Multi-STA Block Ack marks both AIDs as received.

### Same-RU collision

Create two simultaneous HE-TB transmissions with:

- the same Trigger ID;
- the same RU geometry;
- comparable received powers.

Verify that:

- both are admitted as HE-TB receptions;
- each appears in the other's interference set;
- the expected decoding failure occurs;
- neither failed packet is acknowledged as received.

### Same-RU capture

Repeat the same-RU case with a large received-power difference and verify the
configured error model's capture behavior.

### Different Trigger IDs

Generate overlapping HE-TB transmissions with different Trigger IDs and verify
that they are not accepted as parallel members of the same exchange.

### Timing tolerance

Vary response arrival skew around the intended synchronization tolerance and
verify the selected packet-level policy. If no explicit standards tolerance is
implemented, document that the model accepts any temporally overlapping
same-Trigger responses that pass normal PHY checks.

### Result-level instrumentation

Add or record signals containing:

- Trigger ID;
- STA AID;
- RU index and geometry;
- transmission start and arrival start;
- reception-attempted status;
- reception success;
- minimum/mean SNIR;
- number of responses collected per Trigger.

This would make Qtenv and result analysis much less ambiguous.

## Overall conclusion

The STA responses do not begin at exactly the same simulation timestamp, but
they overlap and are treated as members of the same Trigger exchange.

Within the current packet-level abstraction, the AP correctly:

- admits their parallel reception;
- evaluates each transmission on its own RU;
- excludes disjoint RUs from interference;
- retains same-RU transmissions as interference;
- decodes and delivers successful responses separately.

The behavior is therefore materially different from unrelated independent
full-channel transmissions. The main remaining concerns are PHY fidelity,
explicit timing-tolerance modeling, and the absence of focused end-to-end
regression tests for different-RU success and same-RU collision.
