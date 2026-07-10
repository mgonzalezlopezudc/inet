# MAC, retry, aggregation, and rate control

## Reconstruct one transmission attempt

For the disputed MPDU, build this state sequence:

1. Queue and access category receiving the frame.
2. Head-of-line time and association eligibility.
3. Physical carrier sense, NAV, required interframe space, and selected backoff.
4. Backoff freeze/resume and channel-access grant.
5. Selected protection and frame-exchange sequence.
6. PHY mode and expected ACK policy.
7. Response reception or timeout.
8. Retry counter, contention window, retransmission, or drop.

A missing ACK does not prove DATA loss. Use recipient evidence to distinguish DATA loss from successful DATA reception followed by ACK loss. In the latter case, the retry normally preserves MPDU identity, sets Retry, and may be removed as a duplicate at the recipient while still being acknowledged.

Distinguish short/long or other implementation-specific retry counters, RTS versus DATA failure, and per-frame versus per-recipient state. Query the installed results and source rather than assuming statistic or policy names.

## RTS/CTS and hidden stations

Determine whether the installed policy compares the RTS threshold with MSDU, MPDU, fragment, or aggregate size and whether group addressing, QoS, or protection mode changes the decision.

For a protected exchange, trace:

```text
backoff → RTS → SIFS → CTS → SIFS → DATA → SIFS → ACK/Block Ack
```

Identify whether RTS, CTS, DATA, or the final response failed, and at which receiver. A hidden-station diagnosis requires a power/detection/interference matrix showing that contenders cannot adequately sense one another while both affect the intended receiver. An exposed-station diagnosis likewise requires showing that deferral was unnecessary at the intended receivers.

## EDCA and TXOP

Confirm that QoS mode, classifier, packet tags, TID, access-category mapping, per-AC queues, and EDCA functions are active. A QoS header alone does not prove separate contention.

Reconstruct AIFSN, CWmin/CWmax, backoff, retry state, and TXOP independently for each access category. When local ACs finish together, verify one winner and the installed internal-collision update for losers.

For a TXOP, record the winning AC, limit, frame sequence, SIFS gaps, ACK policy, and termination reason. Check for premature end, overrun, ordinary contention inside the TXOP, or selecting a frame that cannot fit.

## Fragmentation and aggregation

For fragmentation, verify the threshold input, fragment count and sizes, shared sequence number, increasing fragment number, More Fragments bit, per-fragment retry/ACK behavior, and final reassembly. Correlate with TA, sequence, fragment, and Retry.

Distinguish aggregation models:

* A-MSDU combines subframes within one MPDU and typically shares its error outcome.
* A-MPDU combines MPDUs in a PHY transmission and may require per-MPDU sequence, error, Block Ack, and retry handling.

Confirm what the checked-out INET model represents: nested chunks, separate MPDUs, a wrapper transmission, independent MPDU decisions, or one aggregate-level decision. Do not infer A-MPDU support from generic aggregation support.

Check destination/TID compatibility, size and duration limits, TXOP fit, padding/length calculation, tag preservation, deaggregation, and whether sequence numbers and retries correspond to the modeled unit.

## Block Ack

Track both peers through agreement creation, TID/window parameters, starting sequence number, activity/expiry, Block Ack Request, bitmap interpretation, retransmission, reordering-window movement, and DELBA/timeout.

When a window stalls, identify the first missing sequence, whether it was reported in the bitmap, whether it was retried or exhausted its limit, and why the receiver window or delivery point did not advance.

## Rate selection and control

Separate frame-class rate selection from adaptive data-rate control. Determine the selected mode for DATA, management, RTS, CTS, ACK, and Block Ack, and verify peer/mode-set compatibility.

For adaptive control, reconstruct per-recipient algorithm state: attempted mode, success/failure feedback, retry, probe state, counters or thresholds, and rate transition. Lost ACKs and collisions may look like channel loss unless the controller has richer feedback.

Compare fixed/adaptive rate and single/multiple contender runs with the same RF conditions when separating link-quality response from contention response. Do not infer PHY capability from displayed nominal bitrate alone.
