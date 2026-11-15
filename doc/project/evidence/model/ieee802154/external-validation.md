# External PHY abstraction validation

> **Kind:** design · **Status:** current · **Seal:** none · **Owns:** — · **Stands on:** [phy-provider.md](phy-provider.md), [results.md](results.md)

The selected model is packet-level 2.4-GHz DSSS O-QPSK, 250 kbit/s, with an
ideal 16-us symbol clock and scalar constant SNIR for this comparison. It does
not model chip waveforms, synchronization probability, oscillator error or
hardware calibration. The inherited IEEE 802.15.4 error abstraction uses the DSSS
O-QPSK BER curve, independent modeled bit errors and a hard sensitivity/SNIR
eligibility gate. A computed BER/PER comparison does not validate those gates
against a receiver or establish a quantitative IEEE 802.15.4-2024 PER claim.

The independent primary reference is ns-3.45's
[error-model regression](https://github.com/nsnam/ns-3-dev-git/blob/ns-3.45/src/lr-wpan/test/lr-wpan-error-model-test.cc)
and [model](https://github.com/nsnam/ns-3-dev-git/blob/ns-3.45/src/lr-wpan/model/lr-wpan-error-model.cc).
Its five published BER targets at SNIR 5, 2, -1, -4 and -7 dB are checked with
the reference's respective absolute tolerances 1e-16, 1e-9, 1e-5, 1e-4 and 1e-3.
This is a cross-model calculation check, not execution of ns-3 or independent
hardware evidence. No ns-3 implementation is incorporated into production code.

The production-transmission fixture fixes PSDU lengths
2, 5, 18, 20 and 127 bytes, bandwidth 2.8 MHz and the five constant SNIR values.
It observes actual selected PHY transmissions and compares their duration with
the 2024 SHR/PHR/PSDU oracle `(6 + length) * 32 us`, exactly in simulation time.
It invokes the radio's actual Ieee802154ErrorModel with those transmissions and a
constant-SNIR observation. DATA PER uses 8*length modeled bits; whole-frame PER
also includes the inherited 16 header bits. The four-byte preamble has no modeled
error probability, and SER is undefined. These are explicit abstraction limits.

For each signal part, compare PER with a long-double mathematical evaluation
of the referenced BER expression and independent-bit composition. Acceptance
uses absolute tolerance 1e-12 plus relative tolerance 1e-8. Emit all observed
values to a retained CSV-style log. The executed fixture passed all 25
length/SNIR points and actual encoded SHR/PHR/PSDU views. This checks production timing and the error
model called on production transmissions; it does not demonstrate a receiver's
stochastic loss frequency. Existing PHY reception and interference fixtures
separately establish the effective gate and delivery path.
