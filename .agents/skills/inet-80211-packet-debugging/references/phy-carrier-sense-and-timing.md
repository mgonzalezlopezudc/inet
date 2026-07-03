## Contents

* 5. 802.11 PHY Debugging
* 5.1 PHY investigation order
* 5.2 Match the radio and medium models
* 5.3 Radio state and half-duplex behavior
* 5.4 Frequency, channel, and bandwidth
* 5.5 Transmit power
* 5.6 Antenna model
* 5.7 Propagation and path loss
* 5.8 Energy detection and receiver sensitivity
* Energy detection
* Sensitivity
* 5.9 Background noise
* 5.10 Interference
* 5.11 Capture effect
* 5.12 Preamble, header, and payload
* 5.13 PHY mode and duration
* 5.14 Rate compatibility
* 5.15 Error model
* 5.16 Deterministic PHY isolation
* Replace only with valid parameters for the installed model.
* Use a high-link-margin or idealized radio configuration.
* 6. Physical and Virtual Carrier Sensing
* 6.1 Physical carrier sensing
* 6.2 Virtual carrier sensing
* 6.3 Diagnostic matrix
* 6.4 Common carrier-sense defects
* 7. Interframe Spaces and Timing
* 7.1 SIFS-separated responses
* 7.2 Backoff timing

---

# 5. 802.11 PHY Debugging

INET radios model transmission and reception using replaceable antenna, transmitter, receiver, error, propagation, path-loss, noise, and interference components. Reception normally requires the signal to be possible to receive, selected for attempted reception, and decoded successfully. INET provides scalar, dimensional, idealized, and more detailed alternatives, and its error model determines whether a reception succeeds.

## 5.1 PHY investigation order

For every missing or corrupted frame, answer:

1. Did the transmitter enter transmitting state?
2. Was a transmission object created?
3. Was the transmission placed on the radio medium?
4. Did the signal geometrically reach the receiver?
5. Was its frequency range relevant to the receiver?
6. Was its received power above energy detection?
7. Was its received power above sensitivity?
8. Did the receiver select that signal for reception?
9. Was the preamble detected?
10. Was synchronization maintained?
11. Was the PHY header decoded?
12. What interference overlapped the reception?
13. What was the signal-to-noise-plus-interference ratio over time and frequency?
14. What error model and PHY mode were used?
15. Was the data portion decoded successfully?
16. Was the resulting MPDU passed to the MAC?
17. Was it tagged as erroneous?
18. Did MAC filtering subsequently discard it?

## 5.2 Match the radio and medium models

Verify compatible types.

Typical model families include:

* Unit-disk radio and unit-disk medium
* Scalar radio and scalar radio medium
* Dimensional radio and dimensional radio medium
* Bit-level or layered OFDM alternatives where available

Do not combine incompatible analog representations.

Search instantiated types:

```sh
rg -n 'radioMedium.*typename|radio.*typename|signalAnalogRepresentation' \
  /path/to/project "$INET_ROOT/src"
```

A unit-disk model can be useful for topology and MAC validation but cannot establish that a realistic PHY configuration is correct.

A scalar model usually represents power without detailed frequency shape.

A dimensional model can represent time- and frequency-dependent power and is more suitable for overlapping channels, spectral masks, and cross-technology interference.

## 5.3 Radio state and half-duplex behavior

Track radio states such as:

* Off
* Sleep
* Receiver
* Transmitter
* Transceiver or switching state, if supported

Track reception states such as:

* Idle
* Busy
* Receiving
* Undefined or transitional states

Track transmission states such as:

* Idle
* Transmitting

Verify that:

* A station does not expect to receive while its half-duplex radio is transmitting.
* The transmitter has returned to receive mode before an expected response.
* Channel switching has completed before scanning or receiving.
* A power-state transition has not suppressed reception.
* Radio-state signals reach the MAC.

The INET 802.11 radio can expose radio-channel changes and can optionally simulate preamble, header, and data portions separately.

For detailed part-level analysis, inspect whether these parameters exist in the installed version:

```ini
*.host[*].wlan[*].radio.separateTransmissionParts = true
*.host[*].wlan[*].radio.separateReceptionParts = true
```

Do not add them without checking the instantiated NED type.

## 5.4 Frequency, channel, and bandwidth

Verify:

* Center frequency
* Channel number
* Channel bandwidth
* Primary channel
* Secondary-channel layout
* Channel switch state
* Supported mode set
* Regulatory-domain assumptions
* Whether AP and STA are actually tuned to overlapping frequency ranges

A matching channel number is insufficient if custom center frequencies or bandwidths are configured.

For wide-channel operation, determine whether the implementation models:

* 20, 40, 80, 160, or 320 MHz operation
* Primary and secondary subchannels
* Channel bonding
* Puncturing
* Dynamic bandwidth operation
* Per-subchannel interference

If the model does not implement these mechanisms, do not infer them from a configured nominal bitrate.

## 5.5 Transmit power

Verify:

* Configured transmit power
* Units
* Per-frame overrides
* Rate-dependent power, if any
* Antenna gain
* Cable or system loss
* Directionality
* Regulatory or algorithmic power changes

Common failures include:

* Confusing watts and dBm
* Applying an override to the wrong module path
* Configuring only one node
* Changing the AP but not the STA
* Using a power value that is overwritten by a mode-specific transmitter
* Assuming equal uplink and downlink budgets

## 5.6 Antenna model

Inspect:

* Antenna type
* Gain
* Radiation pattern
* Orientation
* Polarization assumptions
* Mobility-driven rotation
* Number and placement of antennas
* Whether a directional antenna points toward the peer

For MIMO or beamforming scenarios, verify that the simulation actually models:

* Multiple spatial streams
* Antenna arrays
* Channel matrices
* Beamforming
* Training
* Stream-specific reception

Do not treat a configured number of spatial streams as proof that spatial processing is modeled.

## 5.7 Propagation and path loss

Inspect:

* Propagation-speed model
* Path-loss model
* Distance
* Positions in all three dimensions
* Coordinate units
* Mobility state
* Terrain and object geometry
* Obstacle-loss model
* Material properties
* Shadowing
* Fading
* Reflection or diffraction, if modeled
* Correlation and random-stream assignment

INET’s physical environment can represent physical objects and materials that affect propagation.

Check geometry:

```sh
rg -n 'mobility|initialX|initialY|initialZ|constraintArea|physicalEnvironment' \
  /path/to/project
```

When a link fails near a threshold, calculate a link-budget estimate independently and compare it with the simulator’s reception power.

At minimum, account for:

```text
received power
  = transmit power
  + transmitter antenna gain
  + receiver antenna gain
  - path loss
  - obstacle and system losses
```

Do not treat the estimate as authoritative when fading or directional models are active.

## 5.8 Energy detection and receiver sensitivity

These thresholds answer different questions.

### Energy detection

Below the energy-detection threshold, the receiver may behave as though the medium is idle.

Above it, the receiver may regard the channel as busy even if it cannot decode the frame.

### Sensitivity

Below receiver sensitivity, successful reception is not possible for the selected mode.

A signal can therefore:

* Be too weak to detect
* Be detectable but not decodable
* Be decodable without interference
* Become undecodable when interference is added

INET receiver models expose energy-detection and sensitivity concepts.

Debug asymmetric carrier sensing by comparing these thresholds and received powers at all participating stations.

## 5.9 Background noise

Verify:

* Noise model type
* Noise power or power spectral density
* Frequency range
* Bandwidth scaling
* Units
* Time variation
* Whether noise is disabled
* Whether each medium instance has its own noise model

For dimensional modeling, confirm that noise spectral density integrates to the intended noise power over the receiver bandwidth.

Do not use a noise value expressed per MHz as though it were total receiver noise.

## 5.10 Interference

For each failed reception, identify every overlapping transmission.

Classify interference as:

* Co-channel 802.11
* Partially overlapping 802.11 channel
* Adjacent-channel leakage
* Another PHY mode
* Another technology
* Background transmission
* Self-interference or simultaneous local transmission
* Hidden-station transmission

For each interferer, record:

* Transmitter
* Start and end times
* Frequency range
* Received power
* Overlap with preamble, header, and payload
* Whether it was decodable
* Whether its header could set the NAV
* Whether it triggered physical carrier sense

Cross-technology transmissions may be detectable as energy but undecodable as 802.11 frames, preventing NAV-based protection. INET’s dimensional analog model can represent frequency-dependent signal shapes and out-of-band interference when configured accordingly.

## 5.11 Capture effect

Do not assume that every temporal overlap causes both frames to fail.

Determine whether the selected receiver and error model support a capture-like effect in which a sufficiently strong signal is successfully decoded despite an overlapping weaker signal.

Inspect:

* Relative powers
* Which preamble arrived first
* Whether synchronization can switch to another signal
* Interference during each reception part
* Error-model behavior
* Receiver selection rules

If capture is not implemented, document that model limitation.

## 5.12 Preamble, header, and payload

PHY exchanges consist conceptually of:

* Preamble or training fields
* PHY header or signaling fields
* Encoded MAC payload

Rates and robustness may differ between these parts.

A receiver may:

* Fail to detect the preamble
* Detect the preamble but fail on the PHY header
* Decode the header but fail on the payload
* Decode the payload with an FCS error

Enable separate reception parts when supported and necessary.

## 5.13 PHY mode and duration

Determine:

* PHY family
* Modulation
* Coding rate
* Number of spatial streams
* Channel width
* Guard interval
* MCS
* Preamble type
* PHY header mode
* Data length
* Padding and tail behavior
* Aggregation
* Nominal and effective bitrate

Never calculate airtime using:

```text
payload bits / nominal bitrate
```

alone.

Include preamble, PHY header, MAC header, FCS, coding, symbol rounding, service fields, tail bits, padding, and aggregation delimiters as applicable.

Use the mode objects and duration calculators in the installed INET implementation where possible.

## 5.14 Rate compatibility

Verify that:

* The sender selects a mode supported by the receiver.
* Control and management responses use valid basic or mandatory rates.
* ACK and CTS response rates are selected correctly by the model.
* The advertised capability set is consistent with the configured mode set.
* AP and STA share at least one usable rate.
* A rate-control algorithm does not select a mode unsupported by the current channel width or receiver.

## 5.15 Error model

Identify the exact receiver error model.

INET provides replaceable 802.11 error models, including NIST-, YANS-, and table-oriented alternatives in applicable releases.

For a failed packet, record:

* Received signal power
* Noise power
* Interference power
* Minimum, mean, or time-varying SNIR used
* Modulation and coding
* Packet length
* BER, SER, or PER estimate
* Random variate
* Final reception decision

Do not compare results from different error models as though they represented the same PHY.

## 5.16 Deterministic PHY isolation

To distinguish MAC defects from RF-model effects, create temporary diagnostic configurations.

Examples:

```ini
[Config WifiDebugIdealChannel]
extends = WifiDebug

# Replace only with valid parameters for the installed model.
# Use a high-link-margin or idealized radio configuration.
```

Run three levels:

1. Ideal connectivity or no-error reception
2. Realistic path loss without competing traffic
3. Full interference, mobility, and noise

Interpretation:

* Failure under ideal conditions suggests MAC, management, addressing, or configuration problems.
* Success under ideal conditions but failure under realistic conditions suggests PHY or interference problems.
* Success without competing traffic but failure with contention suggests MAC access or collision problems.

Do not leave the idealized configuration as the final fix.

---

# 6. Physical and Virtual Carrier Sensing

802.11 medium access uses both physical and virtual carrier sensing.

## 6.1 Physical carrier sensing

A station can regard the medium as busy because:

* It is receiving a decodable 802.11 transmission.
* It detects sufficient undecodable RF energy.
* Its own radio is transmitting.
* The radio reports a switching or unavailable state.

Investigate the exact busy interval reported to the MAC.

## 6.2 Virtual carrier sensing

The NAV can remain active after a frame’s physical transmission ends.

A station may therefore see:

```text
PHY idle
NAV active
effective medium busy
```

or:

```text
PHY busy
NAV zero
effective medium busy
```

## 6.3 Diagnostic matrix

| PHY carrier sense |    NAV | Expected access state                                |
| ----------------- | -----: | ---------------------------------------------------- |
| Idle              |      0 | Eligible for interframe-space and backoff processing |
| Busy              |      0 | Defer and freeze backoff                             |
| Idle              | Active | Defer                                                |
| Busy              | Active | Defer                                                |

## 6.4 Common carrier-sense defects

Investigate:

* Incorrect energy-detection threshold
* Incorrect sensitivity
* Hidden station below detection threshold
* Exposed station detected although it cannot interfere at the receiver
* NAV set from an unrelated BSS
* NAV not reset
* Duration miscalculation
* Failure to decode RTS, CTS, or data header
* Backoff decrementing while busy
* Backoff not resuming after idle
* Channel-switch state reported as idle
* PHY busy event not delivered to the MAC

---

# 7. Interframe Spaces and Timing

Do not hardcode timing constants without first identifying the PHY mode set.

Relevant intervals include:

* SIFS
* Slot time
* PIFS
* DIFS
* AIFS per access category
* EIFS or equivalent extended deferral
* ACK timeout
* CTS timeout
* Probe dwell times
* Beacon interval
* Block Ack timeout
* Reordering timeout
* Channel-switch delay

Common relationships include:

```text
PIFS = SIFS + 1 × slot time
DIFS = SIFS + 2 × slot time
AIFS[AC] = SIFS + AIFSN[AC] × slot time
```

Verify these relationships against the implemented mode and standard revision.

## 7.1 SIFS-separated responses

Frames such as these are generally separated by SIFS when the relevant procedure applies:

```text
DATA → SIFS → ACK
RTS → SIFS → CTS
CTS → SIFS → DATA
DATA → SIFS → Block Ack
Block Ack Request → SIFS → Block Ack
```

Failures to inspect:

* Response scheduled after DIFS instead of SIFS
* Response suppressed because the radio is still transmitting
* Incorrect packet-duration calculation
* Timeout expiring before the response can finish
* Propagation and switching delay omitted from timeout assumptions
* Wrong response mode
* ACK or CTS sent to the wrong immediate transmitter
* Response colliding with another technology that does not honor the reservation

## 7.2 Backoff timing

For each contender, reconstruct:

* Initial contention window
* Random backoff value
* Number of idle slots observed
* Freeze times
* Resume times
* Transmission-attempt time
* Success or failure
* Updated contention window
* Retry counter

Backoff normally draws an integer number of slots from a contention-window-dependent range.

After failure, verify contention-window growth according to the implemented algorithm, commonly corresponding to:

```text
CWnew = min(2 × (CWold + 1) - 1, CWmax)
```

After success, verify reset to the appropriate minimum.

Do not infer a backoff error solely from two stations transmitting close together. Account for propagation, hidden stations, independently chosen equal counters, and internal EDCA collisions.

---

