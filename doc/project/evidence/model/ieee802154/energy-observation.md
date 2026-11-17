# IEEE 802.15.4 — scalar energy observation

> **Kind:** design · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [phy-services.md](phy-services.md), [frame-codec.md](frame-codec.md)

Step 4's feasibility gate uses existing `IRadioMedium::computeInterference`,
`IMediumAnalogModel::computeNoise` and scalar reception power. They expose energy independently
of reception decoding. Radio state alone is not the observation. A bounded PHY-local
`Ieee802154EnergyObservation` owns one requested observation interval, copied rectangular
scalar reception contributions and a listening descriptor. No live reception pointers survive
the initial query or arrival callback: medium cache purges must not erase already observed energy.

Before writing the helper, the traced contract is: the PHY owner starts observation at the
requested interval start, includes every matching medium `signalArrivalStarted` event during
the interval (including undecodable transmissions), and finishes exactly at its end. Existing
and already scheduled receptions are copied from the initial interference query. A transmission
ID deduplicates query/arrival observations. Complete scalar channel overlap is supported;
partial overlap needs a dimensional PHY and is explicitly rejected. Background energy is
evaluated over the same interval. Half-open intervals exclude a signal starting at the end.
The result contains peak power for Mode-1 threshold comparison and time-mean power for ED.
Neither metric claims waveform fidelity or calibrated hardware ED/LQI behavior.

Proposed `Ieee802154EnergyObservation_1.test` is a module fixture with a real scalar radio
medium, O-QPSK radio and configured noise, proving energy measurement without a decoded frame.
Additional arrival/history and threshold boundary cases precede advertising a complete PHY
provider. This helper alone does not close step 4 or implement CCA admission/readiness/timing.
