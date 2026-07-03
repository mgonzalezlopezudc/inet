## Contents

* 4. IEEE 802.11 Frame Model
* 4.1 Frame classes
* Management frames
* Control frames
* Data frames
* 4.2 Address fields
* 4.3 Sequence and fragment numbers
* 4.4 Duration and NAV
* 4.5 FCS and bit errors

---

# 4. IEEE 802.11 Frame Model

Correctly identify the MAC frame before reasoning about its behavior.

## 4.1 Frame classes

### Management frames

Common management-frame purposes include:

* Beacon
* Probe Request
* Probe Response
* Authentication
* Deauthentication
* Association Request
* Association Response
* Reassociation Request
* Reassociation Response
* Disassociation
* Action and Action No Ack

Management frames establish and maintain WLAN membership and capabilities.

### Control frames

Common control-frame purposes include:

* RTS
* CTS
* ACK
* Block Ack Request
* Block Ack
* PS-Poll
* CF-End
* Control Wrapper
* Trigger and other amendment-specific control frames

### Data frames

Common data subtypes include:

* Data
* Null data
* QoS data
* QoS null
* Data carrying additional control-function semantics
* Four-address data for wireless distribution or mesh-related operation

## 4.2 Address fields

Do not assume Address 1 is always the IP destination’s MAC address.

Interpret the `To DS` and `From DS` bits.

| To DS | From DS | Address 1 | Address 2 | Address 3 | Address 4 |
| ----: | ------: | --------- | --------- | --------- | --------- |
|     0 |       0 | DA/RA     | SA/TA     | BSSID     | Absent    |
|     1 |       0 | BSSID/RA  | SA/TA     | DA        | Absent    |
|     0 |       1 | DA/RA     | BSSID/TA  | SA        | Absent    |
|     1 |       1 | RA        | TA        | DA        | SA        |

Where:

* `RA`: immediate wireless receiver
* `TA`: immediate wireless transmitter
* `DA`: final MAC destination
* `SA`: original MAC source
* `BSSID`: basic service set identifier

For an infrastructure uplink:

```text
STA → AP
RA = AP/BSSID
TA = STA
DA = final destination
SA = STA
```

For an infrastructure downlink:

```text
AP → STA
RA = STA
TA = AP/BSSID
DA = STA or final wireless destination
SA = original source behind the distribution system
```

A packet may therefore have been correctly transmitted over the air even when the address fields differ from an Ethernet frame carrying the same upper-layer packet.

## 4.3 Sequence and fragment numbers

Use the sequence-control information to correlate retries and fragments.

Expected behavior:

* Retransmissions of the same MPDU normally preserve its sequence number.
* The Retry bit is set on retransmission.
* Fragments of an MSDU share a sequence number and use increasing fragment numbers.
* Duplicate detection is performed using transmitter identity, sequence number, fragment number, and related state.
* Aggregated and QoS exchanges can introduce multiple sequence spaces or reordering contexts.

Do not match packets across capture points using frame number alone.

## 4.4 Duration and NAV

The Duration/ID field can reserve the medium for subsequent frames in an exchange.

Stations use it to update the Network Allocation Vector, implementing virtual carrier sensing.

Investigate:

* Which frame set the NAV
* The NAV expiration time
* Whether the Duration value covers the expected SIFS-separated response
* Whether a station incorrectly transmits while its NAV is active
* Whether a malformed or incorrectly modeled duration leaves the channel virtually busy
* Whether the receiving station could decode the frame carrying the reservation

A station that detects energy but cannot decode the MAC header may defer physically without being able to update its NAV.

## 4.5 FCS and bit errors

Distinguish:

* Frame not detected
* Preamble detected but synchronization failed
* Header decoding failed
* Payload decoding failed
* Frame marked with a bit error
* FCS failure
* MAC rejection after successful PHY reception
* Duplicate removal
* Address filtering
* Reordering or defragmentation delay

A frame absent from the recipient MAC capture does not prove that no RF energy reached the receiver.

---

