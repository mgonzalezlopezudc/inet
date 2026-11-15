# IEEE 802.15.4 — O-QPSK PHY provider

> **Kind:** design · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [phy-services.md](phy-services.md), [energy-observation.md](energy-observation.md)

Pre-write implementation contract for the remaining step-4 work: `Ieee802154PhyService`
implements the paired PHY role over `Ieee802154StandardRadio`. The existing narrowband radio
remains separate. The new radio selects O-QPSK scalar reception, 250 kbit/s and 192 us total
SHR/PHR overhead. Provider-owned state readiness accounts for 192 us turnaround once; underlying
radio switching must be instantaneous. One timer owns readiness, scheduled transmission start
or measurement end. Actual radio transmission signals own start/completion callbacks; a
prediction never completes a transmit. Accepted packet ownership is consumed into a duplicated
radio representation before transmission completion. Reset/stop clear pending state and owned
packets before terminal callbacks, and request IDs survive restart. The provider binds a typed
client and radio, with no concrete MAC dependency.

The required radio-local extension is a synchronous `Ieee802154StandardRadio::transmit()`
method. It takes the radio representation and invokes the existing transmitter path immediately.
An ordinary gate send would leave an uncancellable queued packet between service acceptance and
the actual-start callback at the same instant. The synchronous entry closes that gap without
another event or a core edit. The PHY's receive path still uses the radio output gate. This is
an internal O-QPSK radio binding; substitutability remains at the public PHY service role.

Mode 1 uses peak observed energy or an attempted PPDU reception during the interval. ED averages
for `min(8 symbols, phyCcaDuration)` (2024, 11.2.6, physical PDF p. 596,
`ieee802154-2024@2372031:2373031`). ED maps -75..-35 dBm linearly to 0..255: the lower endpoint
is 10 dB above the selected PHY's specified -85-dBm sensitivity (13.3.4, physical p. 619),
and the range is 40 dB. LQI is a separately documented model power mapping, -100..-20 dBm,
with saturation; it characterizes received strength (11.2.7, physical p. 596), not measured PER.
Both mappings are packet-level abstractions and require boundary tests.

Table 12-2 names are exact. Channel descriptors select 11–26 at 2405+5*(channel-11) MHz.
Transmit powers are signed integer dBm with a selected model hardware range -100..20 dBm.
Broadcast/unicast are bounded by the current general power; peer overrides are bounded by
hardware power. Mutation is atomic and refused during active work/reception. Runtime CCA
duration mutation validates whole microseconds in 1..1000000 and refreshes the MAC's future
default-reset context through the delegated MLME path before its confirmation. The current
writable backoff period stays unchanged. MLME reset retains PHY configuration; direct PHY
reset can separately select PHY defaults. Startup duration has one interface parameter feeding
the PHY and MAC initialization context.
Default/retain reset and unsupported modes/attributes have explicit results.

Proposed `Ieee802154PhyServices_1.test` uses production radios/provider and test-local clients
to verify readiness, scheduled actual TX boundaries, reception metadata, energy/CCA/ED, channel
mutation, cancellation/reset/stop and deletion cleanup. This extends the feasibility fixture;
it must pass before step 4 is called complete. Radio abort currently cannot retract an already
propagating packet-level signal; this limitation must be resolved or explicitly bounded in the
operational lifecycle claim before M1 release, rather than hidden by a service abort result.

The executed `Ieee802154Measurements_1.test` checks quantitative mapping boundaries
through the real PHY, radio and scalar medium. Two colocated antennas use the
production zero-distance path-loss convention (unity gain) and a receiver antenna
gain of -10 dB. Receiver sensitivity is explicitly lowered for this measurement
fixture, with -180-dBm noise; this is controlled model evidence, not hardware
sensitivity validation. Eighteen independently tabulated receive powers span below,
within and above the ED/LQI maps. Each actual received packet must carry the
tabulated LQI and measured signal power; ED must include noise and produce its
tabulated level. At least eight distinct LQI values must be observed.

CCA during these correctly received PPDUs must report BUSY even with its energy
threshold set to 1 W. Quiet ED checks use CCA durations 1/17/128/1000 us and require
the exact min(128 us, CCA) interval. A final channel-26 transmitter/channel-11
receiver case requires quiet energy and no delivery, establishing channel isolation.
The fixture declares numerical tolerances before comparing powers and does not
reuse the provider's mapping function as its oracle.

The selected hardware maximum is 20 dBm. `Ieee802154StandardRadio` therefore sets
its underlying transmitter power to 100 mW so `getMaxPower()` and the medium's
optional range caches cover every admitted request. Actual transmission power
still comes from the PHY's per-packet request tag, default 0 dBm. Retaining the
legacy 2.24-mW transmitter maximum here would understate the advertised hardware
range when range filtering is enabled. The measurement fixture checks both the
underlying maximum and actual 20-dBm transmission; the legacy radio is unchanged.

The quantitative fixture exposed that generic `SignalPowerInd` is computed as
minimum aggregate signal-plus-noise power by `ReceiverBase::computeSignalPower()`.
That is not this service's promised mean desired-signal power. The PHY therefore
copies the rectangular scalar reception's constant power at `receptionEnded`,
keyed by the received packet ID supplied in the signal details, and consumes that
value when the same packet arrives at its input gate. No reception/packet pointer
is retained and no packet is modified while the radio owns it. The gate consumes
the copied value even when lifecycle/state checks discard the packet; reset/stop
clear queued quality copies. Completed-reception signals always schedule this
gate delivery in the supported radio binding, so abandoned receptions create no
orphan entries. Scalar constant power equals its PPDU mean; dimensional/time-varying
reception remains outside this binding. ED separately includes signal and noise.
