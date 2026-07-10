# IEEE 802.11 frame interpretation

Correctly identify the management, control, or data subtype before reasoning about the exchange. Management state depends on Beacon, Probe, Authentication, Association, Reassociation, Disassociation, Deauthentication, and Action frames; protection and acknowledgment use RTS, CTS, ACK, Block Ack Request, and Block Ack; QoS and null-data subtypes have different delivery and ACK semantics.

## Address fields

Do not assume Address 1 is the final destination. Interpret To DS and From DS first:

| To DS | From DS | Address 1 | Address 2 | Address 3 | Address 4 |
| ---: | ---: | --- | --- | --- | --- |
| 0 | 0 | DA/RA | SA/TA | BSSID | absent |
| 1 | 0 | BSSID/RA | SA/TA | DA | absent |
| 0 | 1 | DA/RA | BSSID/TA | SA | absent |
| 1 | 1 | RA | TA | DA | SA |

`RA` and `TA` identify the immediate wireless exchange; `DA` and `SA` identify the final MAC endpoints. AP forwarding may therefore transform the visible address layout without changing the upper-layer flow.

## Retry and fragment identity

Correlate retransmissions and fragments with transmitter, receiver, TID when applicable, sequence number, fragment number, and Retry bit.

* Retransmissions of one MPDU normally preserve sequence and fragment identity and set Retry.
* Fragments of one MSDU share a sequence number and use increasing fragment numbers.
* Aggregation and QoS may introduce several sequence or reordering contexts.
* Capture frame numbers are local observation indexes, not packet identities.

## Duration and carrier-sense evidence

Use the Duration/ID field to reconstruct the exchange reservation and NAV. Identify which decodable frame set the NAV, its expiration, and whether the duration covers the expected SIFS-separated responses. A receiver that detects energy but cannot decode the MAC header may defer physically without updating NAV.

## Distinguish failure stages

Do not collapse these outcomes into "packet loss":

* Signal not detected.
* Preamble/synchronization failure.
* PHY header or payload decoding failure.
* Bit error or FCS failure.
* Successful PHY reception followed by MAC address or state filtering.
* Duplicate removal, reordering, defragmentation, or deaggregation delay/drop.

A frame absent from a recipient MAC capture does not prove that no RF energy reached the receiver.
