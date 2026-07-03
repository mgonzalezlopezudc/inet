## Contents

* 16. Management, Scanning, and Association
* 16.1 Infrastructure state sequence
* 16.2 Beacon debugging
* 16.3 Active scanning
* 16.4 Authentication
* 16.5 Association
* 16.6 Deauthentication and disassociation
* 16.7 Handover
* 17. AP Forwarding and Distribution-System Behavior
* 17.1 Uplink
* 17.2 Downlink
* 17.3 Wireless-to-wireless through one AP
* 18. Broadcast and Multicast
* 19. Power-Save Behavior
* 19.1 Debugging sequence
* 19.2 Failure modes
* 20. Modern PHY and MAC Feature Gate
* 20.1 HT-oriented features
* 20.2 VHT-oriented features
* 20.3 HE-oriented features
* 20.4 EHT-oriented features
* 20.5 60 GHz DMG/EDMG
* 20.6 Sub-1 GHz and vehicular modes

---

# 16. Management, Scanning, and Association

INET management entities generate and process Beacon, Probe, Authentication, Association, and related management frames. Detailed and simplified management modules differ: simplified variants bypass realistic scanning, authentication, and association and are unsuitable for handover experiments.

## 16.1 Infrastructure state sequence

A typical state progression is:

```text
Unassociated
    ↓
Scanning
    ↓
Candidate AP selected
    ↓
Authentication
    ↓
Association
    ↓
Associated
    ↓
Data exchange
```

For reassociation or roaming:

```text
Associated with AP1
    ↓
Scan or handover trigger
    ↓
Candidate AP2 selected
    ↓
Authentication/reassociation
    ↓
Association state and forwarding updated
    ↓
Data exchange through AP2
```

## 16.2 Beacon debugging

Verify:

* AP beaconing enabled
* Beacon interval
* Correct channel
* SSID
* BSSID
* Capability information
* Supported rates or mode information
* QoS and amendment-specific capabilities
* Beacon transmission not blocked by radio state
* STA tuned to channel during beacon
* Received power and decode success
* STA management layer receives the beacon

Common defects:

* AP and STA on different channels
* Hidden SSID expectation mismatch
* Beacon lost during active scan dwell on another channel
* Simplified management bypassing expected behavior
* Beacon generated but not transmitted
* Beacon received at PHY but filtered by management
* Mobility places STA out of range

## 16.3 Active scanning

Expected conceptual behavior:

```text
STA tunes to channel
STA waits or sends Probe Request
AP receives Probe Request
AP sends Probe Response
STA records candidate
STA switches to next channel
STA selects AP
```

INET’s handover showcase demonstrates active scanning through Probe Request and Probe Response exchanges and distinguishes it from passive beacon-based discovery.

Inspect:

* Channel list
* Scan order
* Min and max channel time
* Channel-switch delay
* Probe Request SSID
* AP response conditions
* Candidate database
* Signal-strength measurement
* Selection policy
* Scan completion callback

## 16.4 Authentication

Determine the modeled authentication mechanism.

Inspect:

* Authentication frame sequence
* Algorithm number
* Transaction sequence
* Status code
* Peer addresses
* State transition
* Timeout and retry

Do not assume that real-world WPA/RSN authentication or key exchange is modeled merely because 802.11 Authentication frames are present.

## 16.5 Association

Inspect:

* Association Request
* Association Response
* Status code
* Association identifier
* SSID/BSSID
* Supported capabilities
* Data-rate compatibility
* QoS capability
* AP station table
* STA association state
* MAC forwarding state

A successful Association Response is insufficient if one side fails to update local state.

## 16.6 Deauthentication and disassociation

Determine:

* Originator
* Reason code
* Whether the frame was received
* Whether it is protected when protection is modeled
* Which state was cleared
* Whether queued frames were dropped
* Whether scanning or reassociation begins

## 16.7 Handover

Build a timeline containing:

* Last usable frame through old AP
* Signal metric or handover trigger
* Start of scanning
* Each channel dwell
* Probe or beacon results
* Candidate selection
* Authentication
* Reassociation or association
* New AP forwarding update
* First successful data through new AP
* Packet loss and interruption duration

Separate:

* Radio outage
* Scan latency
* Management exchange latency
* AP forwarding convergence
* IP-layer route or neighbor-cache effects
* Transport retransmission

---

# 17. AP Forwarding and Distribution-System Behavior

An AP is both:

* An 802.11 participant
* A forwarding or bridging point into a distribution system

For infrastructure data, trace both wireless and wired sides.

## 17.1 Uplink

```text
Wireless STA
    ↓ 802.11 frame with To DS
AP wireless interface
    ↓ decapsulation or bridging
AP bridging layer
    ↓ Ethernet or another link
Destination
```

## 17.2 Downlink

```text
Source
    ↓ distribution system
AP bridging layer
    ↓ 802.11 encapsulation with From DS
Wireless STA
```

## 17.3 Wireless-to-wireless through one AP

Depending on the model:

```text
STA A → AP → STA B
```

may involve receiving and retransmitting two separate over-the-air frames.

Inspect:

* AP association table
* MAC learning or bridging table
* Source and destination address transformation
* BSSID
* To DS and From DS bits
* Queue selection on the AP
* Wireless isolation policy
* Broadcast and multicast replication
* VLAN or bridge configuration
* Packet tags and interface identifiers

Do not expect a direct STA-to-STA frame in infrastructure mode unless the scenario explicitly models direct-link behavior.

---

# 18. Broadcast and Multicast

Broadcast and multicast differ materially from unicast.

Common properties:

* No normal ACK
* No ordinary MAC retransmission based on missing ACK
* Often a basic or robust rate
* No RTS/CTS protection in ordinary operation
* Greater susceptibility to loss
* AP may retransmit group traffic onto the wireless medium
* Power-save buffering can affect delivery

Debug:

* Group address recognized
* Correct BSSID and DS bits
* Rate selection
* Queueing
* AP forwarding
* Recipient filtering
* Duplicate delivery
* Lack of ACK incorrectly interpreted as failure
* Upper-layer expectations of reliability

Do not compare broadcast packet delivery directly with unicast without accounting for these differences.

---

# 19. Power-Save Behavior

Only apply this section after confirming the installed model implements the required mechanism.

Potential mechanisms include:

* Legacy power-save mode
* TIM and DTIM
* PS-Poll
* Buffered unicast delivery
* Buffered group delivery
* U-APSD
* Target Wake Time
* Wake-up radio
* Amendment-specific power-save procedures

## 19.1 Debugging sequence

Track:

* Radio sleep transition
* AP awareness of STA sleep state
* Frame buffering
* Beacon TIM or DTIM information
* STA wake time
* Poll or trigger
* Buffered-frame release
* More Data indication
* Return to sleep

## 19.2 Failure modes

* STA sleeps before ACK
* AP does not buffer
* TIM bit missing
* STA misses beacon
* Poll sent on wrong association
* AP responds after STA sleeps
* Buffered frames expire
* Group traffic released on wrong DTIM
* Wake interval or clock drift mismatch
* Power state changes not propagated to radio

Do not invent power-save behavior if the model merely changes radio state without implementing AP buffering.

---

# 20. Modern PHY and MAC Feature Gate

The standard includes multiple PHY generations and specialized amendments. An INET scenario may implement only legacy OFDM and a subset of QoS or aggregation.

For every modern feature, inspect source and configuration before using its expected standard behavior as a diagnostic invariant.

## 20.1 HT-oriented features

Potential features:

* MCS index
* Multiple spatial streams
* 20/40 MHz operation
* Short guard interval
* A-MSDU
* A-MPDU
* Block Ack
* HT protection
* Greenfield or mixed-mode preambles

Debug implications:

* Capability compatibility
* Aggregate construction
* MCS/NSS selection
* Width mismatch
* Protection exchanges
* Per-MPDU acknowledgment and reordering

## 20.2 VHT-oriented features

Potential features:

* Wider channels
* Higher-order modulation
* Additional spatial-stream handling
* MU-MIMO
* VHT control information

Verify whether the model implements actual VHT signal behavior or only corresponding nominal rates.

## 20.3 HE-oriented features

Potential features:

* OFDMA
* Resource units
* Trigger-based uplink
* BSS coloring
* Spatial reuse
* Target Wake Time
* Multi-user transmissions
* HE-specific preambles and signaling

Debug:

* RU allocation
* Trigger timing
* User assignment
* Simultaneous uplink alignment
* Per-user MCS
* BSS color and OBSS decisions
* Spatial-reuse thresholds
* Multi-user ACK behavior

If the simulator represents a multi-user transmission as independent ordinary frames, document that abstraction.

## 20.4 EHT-oriented features

Potential features:

* 320 MHz operation
* Multi-link operation
* Puncturing
* Additional multi-user and multi-RU behavior
* Enhanced aggregation and acknowledgment
* EHT PHY signaling

Debug:

* Link identity
* Per-link channel and radio state
* Link selection
* Cross-link sequence and reordering
* Multi-link addressing
* Link-specific interference
* Simultaneous transmit/receive assumptions
* Aggregate and Block Ack context

Do not label an ordinary multi-radio configuration as standards-compliant multi-link operation without explicit implementation.

## 20.5 60 GHz DMG/EDMG

Potential concerns:

* Directional links
* Beamforming training
* Sector sweep
* Beam refinement
* Blockage
* Very wide channels
* Rapid link loss due to orientation or obstacles

A conventional omnidirectional scalar 802.11 radio is not an adequate proxy for this behavior.

## 20.6 Sub-1 GHz and vehicular modes

For S1G, OCB, 802.11p/bd, or other specialized operation, inspect:

* Channelization
* Timing
* Data rates
* Association requirements or absence
* Mobility and Doppler support
* Specialized frame formats
* Regulatory assumptions

Do not reuse ordinary infrastructure association expectations for modes that intentionally operate differently.

---

