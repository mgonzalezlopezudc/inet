# PHY, carrier sense, and timing

## Locate the first failed PHY transition

For a missing or corrupted frame, determine in order:

1. Whether the transmitter entered transmit state and created a transmission.
2. Whether the signal reached the receiver in the relevant frequency range.
3. Whether received power crossed energy-detection and sensitivity thresholds.
4. Whether the receiver selected the signal, detected the preamble, and decoded the PHY header.
5. Which noise and transmissions overlapped each reception part.
6. Which mode and error model produced the final decision.
7. Whether the resulting MPDU reached the MAC or was marked erroneous.

Do not infer these stages from a MAC capture alone.

## Model compatibility and radio state

Confirm that radio and radio-medium analog representations are compatible: unit-disk, scalar, dimensional, or another installed family. A unit-disk model can isolate topology or MAC behavior but cannot validate realistic link budgets or interference. Dimensional models are required for frequency-dependent effects only when the configured components actually implement them.

Track transmit, receive, sleep/off, switching, and channel-change intervals. A half-duplex radio cannot ordinarily receive while transmitting; verify that it returns to receive state before SIFS responses and completes channel switching before scan observations.

## Frequency and link budget

Check center frequency, channel width, primary/secondary channel assumptions, supported modes, and actual spectral overlap. Matching channel labels do not prove matching frequency ranges.

For link-margin questions, identify transmit power, antenna gains and orientation, path loss, obstacle loss, fading, positions, and mobility at the event time. Compare an independent link-budget estimate with the simulator's received power, but treat the configured propagation and fading models as authoritative.

Avoid inferring MIMO, beamforming, puncturing, or per-subchannel behavior from a nominal mode or spatial-stream parameter unless the implementation models it.

## Detection, sensitivity, noise, and interference

Energy detection and sensitivity answer different questions:

* Below energy detection, the medium may appear idle.
* Above energy detection but below sensitivity, a signal may defer access without being decodable.
* Above sensitivity, decoding can still fail because of mode incompatibility, noise, or interference.

For each relevant interferer, record transmitter, start/end time, frequency overlap, received power, overlap with preamble/header/payload, and whether it was decodable or merely energy. Cross-technology energy may trigger physical carrier sense without setting NAV.

Do not assume every overlap destroys both frames. Inspect reception selection, preamble ordering, relative power, time-varying SNIR, and whether the receiver/error model implements capture-like behavior.

## Physical and virtual carrier sensing

Reconstruct both components of effective medium state:

| PHY sense | NAV | Access consequence |
| --- | --- | --- |
| idle | zero | eligible for interframe space/backoff |
| busy | zero | defer and freeze |
| idle | active | defer |
| busy | active | defer |

Investigate which signal caused PHY busy, which decodable frame set NAV, the reported busy interval, NAV expiration, and delivery of state changes to the MAC. Hidden- and exposed-station claims require received-power and interference evidence at both contenders and intended receivers.

## Mode, airtime, and response timing

Do not calculate airtime as payload bits divided by nominal bitrate. Use the installed mode and duration calculators so preamble, signaling, coding, symbol rounding, headers, FCS, padding, and aggregation are included.

Derive SIFS, slot time, DIFS/AIFS, ACK/CTS timeouts, and other timing from the active mode set. Check response mode compatibility and radio switching. For a failed SIFS exchange, distinguish late scheduling, incompatible response rate, response collision, receiver still transmitting, and timeout expiry.

For backoff, reconstruct the chosen counter, eligible idle slots, freeze/resume intervals, transmission time, success/failure, contention-window update, and retry counter. Close transmission times alone do not prove a backoff defect.

## Controlled isolation

When separating MAC from RF behavior, compare the same seed through progressively richer models:

1. High-link-margin or no-error reception.
2. Intended path loss without competing traffic.
3. Full noise, interference, and mobility.

Use the idealized case only as diagnostic evidence, never as the final repair unless it is the intended abstraction.
