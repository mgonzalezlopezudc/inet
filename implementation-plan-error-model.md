# Implementation plan: table-based IEEE 802.11 EESM error model

## Outcome and clean-room boundary

Implement an opt-in `Ieee80211EesmErrorModel` for the packet-level IEEE 802.11 radio. The first supported domain is SISO HT with BCC, MCS 0–7, and 20 or 40 MHz. It uses future-supplied AWGN packet-error-rate curves and the paper's published BCC EESM calibration for TGn Model D or E. LDPC is outside this plan: the current INET HT mode layer represents convolutional coding only, and the paper contains no LDPC curves or calibration results.

The primary algorithm source is `standards/3067665.3067671.pdf`, Patidar et al., *Link-to-System Mapping for ns-3 Wi-Fi OFDM Error Models*, WNS3 2017, SHA-256 `a428fdcaa4423f331e3bd02044345ec4305ee54056c319404a897e713cdab16e`. The paper is an empirical workshop publication, not normative IEEE 802.11 behavior.

This must be a clean-room implementation:

- Do not inspect, copy, translate, adapt, or compare against ns-3 source code, tests, patches, table files, generated data, API layouts, or repository history.
- Do not download or use the code repository linked by the paper.
- Derive algorithms and published constants only from the paper and cited public specifications.
- Do not generate AWGN PER data in this work. The point set supplied directly by the project user may be used through an explicit `userAuthorizedLocal` acceptance mode for local evaluation and integration only, with its unavailable evidence and HT40=HT20 assumption recorded in a checked manifest. It does not satisfy the default reviewed-artifact gate or authorize redistribution or production use. Accept production tables only after their provenance, clean-room origin, inputs, and redistribution license have been independently reviewed and found INET-compatible.
- Do not digitize the paper's plots as production data; their resolution is insufficient.

The existing `implementation-plan.md` describes the TGn channel generator. Keep that concern separate: the channel/medium produces a per-frequency SNIR function, while this error model maps that function to a PER.

## Supported behavior and explicit exclusions

The MVP supports:

- HT MCS 0–7 with one spatial stream and BCC coding;
- 20 MHz and 40 MHz occupied-carrier plans;
- dimensional SNIR through EESM, including a flat dimensional function for AWGN;
- the four published BCC calibration sets D20, D40, E20, and E40, paired by this implementation contract with the 52/108 HT data-carrier EESM vectors;
- packet-level corruption only;
- declarative selection without changing the current NIST default.

Reject unsupported combinations during initialization when they are statically knowable, otherwise at the first attempted reception/PER lookup, with the mode and missing table key in the error message. Do not silently substitute an analytical model or a different calibration. Require `N_SS = N_STS = 1`, `STBC = 0`, no extension spatial streams, one transmit chain, one receive chain, and BCC. Specifically exclude LDPC, scalar SNIR, MIMO/NSS > 1, receiver diversity, per-stream SINR, time-varying-within-frame SNIR, and uncalibrated channel profiles.

Scalar SNIR is deliberately outside the MVP. INET's scalar background-noise power is currently normalized over the receiver listening bandwidth (22 MHz by default), whereas the paper uses 17.5/35.625 MHz occupied-carrier bandwidth. Add scalar support only after a separate contract converts signal and noise-density inputs to the same occupied-band definition and a scalar-versus-flat-dimensional test proves equality. A silent fixed offset is not acceptable.

The paper also does not validate interference, partial overlap, frame capture, or a general SINR mapping. The MVP may consume INET's SNIR value, but validation and documentation must label the result as noise-limited unless an independent interference campaign is added. Do not claim fidelity for collision scenarios.

## Freeze the mathematical contract before coding

### AWGN reference lookup and packet-length scaling

For a reference packet length `PL0`, apply paper Eq. 1:

$$
PER_{PL}=1-(1-PER_{PL_0})^{PL/PL_0}.
$$

Implement this as `-expm1((PL / PL0) * log1p(-PER0))` for numerical stability, with exact handling of `PER0 = 0` and `PER0 = 1`.

Use the 32-byte curve for `PL <= 400` and the 1458-byte curve for `PL > 400`. The paper calls `PL` a packet/frame/file size but does not define its exact 802.11 boundary, and it is also silent at exactly 400 bytes. Adopt this explicit standards-aligned INET contract:

- for an HT PPDU, `PL` is the PSDU length in octets, equal to the **HT Length** value in HT-SIG; IEEE Std 802.11-2024 Table 19-11 defines that field as the number of octets of data in the PSDU (`80211ax-2024:chunk:08094`, pp. 3431–3432);
- `PL = 400` selects the 32-byte reference curve as the deterministic boundary policy;
- do not use total PPDU duration, the legacy L-SIG `L_LENGTH`, the encoded Data-field length, SERVICE/tail/pad bits, or an application/MSDU payload length.

The PPDU is not the right length for Eq. 1: it is a timed PHY waveform containing the preamble, signaling fields, and encoded Data field, not the paper's byte-counted packet reference. IEEE Std 802.11-2024 Clause 19.3.11.1 separately constructs the BCC Data field from SERVICE, PSDU, tail, and pad processing (`80211ax-2024:chunk:08119`). Using PPDU duration or coded length would make `PL` depend on PHY encoding after coding has already been captured by the selected AWGN curve. The future table provider and the INET lookup must both interpret 32 B and 1458 B as PSDU octets and record that contract in the manifest. Reject HT Length 0 because it denotes an NDP rather than an ordinary zero-byte PSDU lookup.

The paper requires interpolation between discrete 0.2 dB SNR samples but does not define the interpolation scale or out-of-range behavior. Use these explicit INET policies and test them:

- SNR table keys are in dB and strictly increasing.
- Interpolate linearly in complementary-log-log probability versus SNR in dB. For `0 < p0,p1 < 1`, transform each endpoint with `z = log(-log1p(-p))`, linearly interpolate `z`, and recover `p = -expm1(-exp(z))`. This behaves like interpolation in `log(PER)` in the low-PER waterfall, behaves better near PER 1, and is algebraically consistent with Eq. 1 because packet-length scaling multiplies `-log(1-PER)`.
- Raw simulation points with zero errors or all errors are observations at finite sample resolution, not proof that the true PER is exactly 0 or 1. Retain raw `errors/packets`, but derive the finite interpolation value `p* = (errors + 0.5) / (packets + 1)` and record this Jeffreys-estimator policy in the manifest. The runtime table stores `p*`; raw counts remain available for audit.
- Require explicit low/high endpoint rows in the future-supplied tables; outside the grid clamp to those finite runtime endpoint estimates and never extrapolate a slope beyond the table.
- Require PER to be finite, in `[0,1]`, and non-increasing with SNR.

Treat this transform as an independently selected policy, not a claim about an unspecified detail of the paper. The code milestone may exercise it with synthetic numeric oracles, but production acceptance is deferred until the future table provider supplies directly simulated 0.1 dB midpoint points and intermediate PSDU lengths. Compare its absolute `log10(PER)` prediction error against raw-PER linear interpolation over the predeclared non-saturated PER range. Keep complementary-log-log interpolation only if that check is no worse overall and improves the low-PER region; otherwise revise the runtime interpolation policy before accepting the tables.

### EESM mapping

For data-carrier SNRs `gamma[i]`, use paper Eq. 5:

$$
\gamma_\mathrm{eff}=-\beta\ln\left(\frac{1}{N_d}\sum_{i=1}^{N_d} e^{-\gamma_i/\beta}\right).
$$

`gamma[i]`, `beta`, and `gammaEff` are linear, dimensionless ratios. Convert `gammaEff` to dB only before the AWGN lookup. Compute the expression with log-sum-exp; require finite nonnegative carrier SNRs, `beta > 0`, and a nonempty vector.

Keep the two carrier counts distinct under this project's calibration contract:

- `N` in paper Eq. 3 is the total number of data plus pilot subcarriers used to divide total transmit power: 56 for HT20 and 114 for HT40;
- `N_d` is the number of data subcarriers included in the EESM average: 52 for HT20 and 108 for HT40.

The physical per-data-subcarrier SNR is `gamma[i] = (Ptx / N) * |H[i]|^2 / sigma[i]^2`, and Eq. 5 is evaluated over only the `N_d` data-subcarrier SNRs. In the runtime path, the dimensional transmission spectrum has already allocated `Ptx / N` and `DimensionalSnir` has already formed the signal-to-noise ratio; consume that raw per-carrier SNIR directly and never divide it by `N`, `N_d`, or transmit power again. Pilot carriers affect the transmit-power allocation, but their SNIR values are not terms in the EESM sum. Exclude guard and DC carriers from both sets.

**Project-selected calibration convention:** pair the published Patidar 2017 BCC D20, D40, E20, and E40 `beta` values with an EESM vector containing exactly the 52 HT20 or 108 HT40 IEEE data subcarriers. Patidar's paper is internally inconsistent: Eq. 4 describes `N_d` as data carriers, later prose describes 56/114 data-plus-pilot carriers, and its figures label 52/108 data carriers. The 52/108 pairing is therefore a deliberate project contract, not a claim about the paper's unpublished implementation or universal transferability of its beta values. Record `eesmCarrierSet = "dataOnly"` and `eesmCarrierCount = 52|108` in calibration metadata and lock both with regression tests. Any future inclusion of pilots invalidates this pairing and requires a new calibration and validation decision.

The mode layer, not the error model, must own the exact carrier indices, spacing, and offsets. Use IEEE Std 802.11-2024 as the normative source:

- Clause 19.3.6, Table 19-6 defines `N_SD = 52`, `N_SP = 4`, and `N_ST = 56` for HT20, and `N_SD = 108`, `N_SP = 6`, and `N_ST = 114` for HT40 (processed chunks `80211ax-2024:chunk:08068` and `:08069`, pp. 3421–3422);
- Clause 19.3.7 defines occupied indices −28…−1 and 1…28 for HT20, and −58…−2 and 2…58 for HT40 (`80211ax-2024:chunk:08073`, pp. 3423–3424);
- Clauses 19.3.11.11.3 and 19.3.11.11.4, Equations (19-58) and (19-59), identify HT20 pilots at −21, −7, 7, and 21 and HT40 pilots at −53, −25, −11, 11, 25, and 53, and define the remaining data-carrier mapping (`80211ax-2024:chunk:08158` and `:08159`, pp. 3456–3458).

For a flat data-subcarrier vector, EESM must return the original SNR exactly within floating-point tolerance.

### SNR normalization

The paper's Eq. 2 divides total transmit power by the occupied data-plus-pilot bandwidth: 17.5 MHz for HT20 (`N = 56`, hence `56 × 312.5 kHz`) and 35.625 MHz for HT40 (`N = 114`, hence `114 × 312.5 kHz`). Eq. 3 likewise assumes equal division of total power among those `N` occupied carriers, while Eq. 5 averages only the `N_d = 52/108` data-carrier SNRs.

INET's current dimensional transmitter instead spreads power over the full nominal 20/40 MHz boxcar. That creates a systematic calibration offset if its raw SNIR is passed directly to this model. Resolve this before end-to-end validation by making the IEEE 802.11 dimensional transmit spectrum preserve total power over the authoritative occupied-carrier plan, using the source design in Production Design step 3. Do not hide a 20/17.5 or 40/35.625 correction factor inside the error model: it would double-correct a future subcarrier-aware transmitter and violate PHY ownership. Flat AWGN tests must therefore use a dimensional SNIR function with the same occupied-band normalization, not the current scalar analog representation.

### Published calibration data

Store these paper Tables 1–2 values as a versioned data artifact, not anonymous literals:

| MCS | D20 | D40 | E20 | E40 |
|---:|---:|---:|---:|---:|
| 0 | 1.01 | 0.96 | 0.97 | 0.98 |
| 1 | 2.10 | 1.97 | 2.06 | 2.02 |
| 2 | 1.99 | 1.81 | 1.82 | 1.68 |
| 3 | 7.72 | 6.96 | 7.31 | 6.90 |
| 4 | 9.09 | 8.39 | 8.75 | 7.96 |
| 5 | 32.56 | 31.18 | 32.12 | 29.07 |
| 6 | 33.17 | 32.62 | 31.93 | 30.91 |
| 7 | 37.41 | 36.09 | 35.82 | 33.58 |

These 32 published `beta` values are BCC-only and depend on the paper's receiver, channel, bandwidth, MCS, and the project-selected 52/108 data-only carrier convention above. The paper checks 500 versus 1000 bytes only for MCS 0 and 5 in D20; reuse at every length, MCS, channel, and bandwidth is therefore an extrapolation, not broadly validated packet-length independence. Name the calibration sets `patidar2017-bcc-d20`, `patidar2017-bcc-d40`, `patidar2017-bcc-e20`, and `patidar2017-bcc-e40` rather than treating them as universal TGn constants. Store `coding = BCC` in each set and reject any non-BCC runtime mode before lookup.

### Signal-part semantics

The AWGN table supplies one atomic calibrated probability, `pTable`, for failure of the reference packet after the receiver has admitted the reception. The paper does not publish separate preamble, HT-SIG, and PSDU error curves, and does not state whether its link-simulation packet failures include header decoding. Therefore do not combine `pTable` with `Ieee80211ErrorModelBase`'s unrelated analytical header-success formula and do not invent a signal-part decomposition.

Require `separateReceptionParts = false` for this error model. Return `pTable` only for `SIGNAL_PART_WHOLE`; fail explicitly if called for `PREAMBLE`, `HEADER`, or `DATA`, with a diagnostic explaining that split-part reception is outside the calibration. INET already carries the sender-selected mode in the immutable `Ieee80211Transmission`; using that mode is an intentional packet-level abstraction, not a claim that a physical receiver cannot fail to acquire the preamble or decode HT-SIG.

Atomic reception makes the probability contract unambiguous: `FlatReceiverBase` makes one Bernoulli success draw for `0 < pTable < 1`; exact endpoints need no draw; its later whole-signal query only copies deterministic rates into `ErrorRateInd`. A future split-part or header-error policy requires separate calibration and an explicit probability-composition rule.

Keep receiver admission separate from conditional decoding. The current path is:

1. **Reception possible/attempted:** the receiver checks that the transmission mode is supported, the listening and transmission bands match, integrated received power meets `sensitivity`, and the receiver is not already committed incompatibly to another reception. A failure here prevents decoding from being attempted.
2. **SNIR gate:** `SnirReceiverBase` compares either global minimum or global mean SNIR with `snirThreshold`. A failure returns unsuccessful before the error model is called.
3. **Conditional EESM decision:** only after those checks pass does `FlatReceiverBase` obtain `pTable` and, for `0 < pTable < 1`, perform one random success draw.

`energyDetection` is separate: it controls whether the radio regards the medium as detectable/busy. It is not a direct predicate in the desired reception's success call chain, but it can change carrier-sense/MAC behavior and therefore indirectly change a network experiment.

For a calibration or paper-reproduction run, the generic receiver gates must be deliberately nonbinding so the measured loss is the EESM/table loss. Use `snirThresholdMode = "mean"` because the dimensional SNIR minimum is taken over the full nominal listening band and the intentional zero-power guard/DC regions would otherwise make the minimum zero. Set `snirThreshold` below the smallest campaign mean SNIR and `sensitivity` below the campaign's smallest integrated received power, each with a recorded margin. Choose `energyDetection` from the fixture link budget so the maximum noise-only power is below the threshold and the weakest intended received signal is above it. An arbitrarily low sentinel can make the medium continuously busy and is not permissive. If MAC/channel-access behavior is outside the test objective, use a direct PHY/link fixture instead. Derive and record the actual numeric receiver-gate values from the campaign; sketch values such as `-100 dB` and `-200 dBm` are not receiver recommendations. A focused low-MCS test must show the three success stages separately and prove that exactly one RNG draw occurs when the atomic conditional EESM decision is reached. If a normal scenario retains realistic hardware gates, report its observed frame-loss probability as the composition of receiver admission plus this conditional error model, not as a direct reproduction of the paper's PER curve.

## Production design

### 1. Add a dimensional SNIR contract

Add `src/inet/physicallayer/wireless/common/contract/packetlevel/IDimensionalSnir.h` exposing the immutable time/frequency SNIR function already available from `DimensionalSnir`. Make `DimensionalSnir` implement it.

This prevents the IEEE 802.11 error policy from depending on a concrete dimensional analog-model result class. Keep `ISnir` unchanged so scalar models do not acquire an irrelevant API.

### 2. Make HT carrier geometry authoritative

Add a small OFDM-data-mode carrier-plan contract/value type in `src/inet/physicallayer/wireless/ieee80211/mode/` and implement it for HT data modes. It must expose:

- occupied carrier indices, including pilot carriers;
- subcarrier spacing;
- each occupied carrier's frequency interval relative to center frequency;
- data/pilot counts and bandwidth.

Do not reconstruct the carrier grid from bandwidth or spread samples evenly over the nominal channel. Reuse the same carrier plan for future transmitter/channel work so the PHY has one source of truth. Cite IEEE Std 802.11-2024 Clause 19.3.6/Table 19-6, Clause 19.3.7, and Clauses 19.3.11.11.3–19.3.11.11.4 under `AR-WLAN-STD-TRACE`.

Transcribe the IEEE Std 802.11-2024 Clause 19 mapping above into a reviewable test oracle: the carrier plan contains all `N_ST` occupied carriers for power normalization and separately identifies the `N_SD` data carriers consumed by EESM.

### 3. Align the dimensional transmit spectrum

Use the occupied-carrier implementation, not a correction factor in the error model. Keep it opt-in so existing dimensional IEEE 802.11 trajectories retain their current nominal-band boxcar unless the new model is selected.

#### Prerequisite: correct the HT generic signal-part timing contract

Do not derive the PSDU interval from the current duration split as-is. `Ieee80211HtPreambleMode::getDuration()` currently covers every non-Data HT field, including HT-SIG, while `Ieee80211Transmitter::createTransmission()` separately assigns `getHeaderMode()->getDuration()` to the generic header and subtracts it from the total. For HT this starts the current generic Data part one HT-SIG duration late and shortens it by the same amount.

IEEE Std 802.11-2024 defines physical field chronology, not INET's three generic signal-part names. Preserve the standard's order and map it into three contiguous INET intervals:

- **HT-mixed:** `L-STF -> L-LTF -> L-SIG -> HT-SIG -> HT-STF -> HT-LTF1 -> HT-LTF2 ... HT-LTFN -> Data`;
- **HT-greenfield:** `HT-GF-STF -> HT-LTF1 -> HT-SIG -> HT-LTF2 ... HT-LTFN -> Data`.

This chronology and its field-start equations come from IEEE Std 802.11-2024 Clause 19.3.7, Figure 19-4, Equations (19-2) and (19-3) (`80211ax-2024:chunk:08073` and `:08074`, pp. 3423-3426). Field durations are defined in Table 19-6 (`:08069`, p. 3422), and Equation (19-22) defines the total number of HT-LTFs (`:08104`, pp. 3435-3436).

Use this contiguous partition:

| Format | `SIGNAL_PART_PREAMBLE` | `SIGNAL_PART_HEADER` | `SIGNAL_PART_DATA` |
|---|---|---|---|
| HT-mixed | `L-STF + L-LTF + L-SIG` | `HT-SIG + HT-STF + all HT-LTFs` | the IEEE BCC Data field |
| HT-greenfield | `HT-GF-STF + HT-LTF1` | `HT-SIG + HT-LTF2 ... HT-LTFN` | the IEEE BCC Data field |

`SIGNAL_PART_HEADER` is therefore an INET interval abstraction that aggregates HT-SIG with every training field that physically follows it before Data. Do not redefine `Ieee80211HtSignalMode::getDuration()`: that mode remains authoritative for HT-SIG alone.

Add an immutable `Ieee80211SignalPartDurations` value and a virtual `getSignalPartDurations(psduLength)` contract to `IIeee80211Mode`. Implement the compatibility default in `Ieee80211ModeBase` as the current generic split so every non-HT mode retains its existing behavior exactly:

```cpp
totalDuration = getDuration(psduLength)
preambleDuration = getPreambleMode()->getDuration()
headerDuration = getHeaderMode()->getDuration()
dataDuration = totalDuration - preambleDuration - headerDuration
require all durations >= 0
require preambleDuration + headerDuration + dataDuration == totalDuration
```

Add one immutable named-field breakdown to `Ieee80211HtPreambleMode` and make it the single production source of HT pre-Data timing. `Ieee80211HtPreambleMode::getDuration()` sums that breakdown, `Ieee80211HtMode::getSignalPartDurations()` groups the same values into the three generic parts, and the transmitter consumes only the resulting aggregate. The following equations define that grouping; do not implement a second independent set of duration formulas beside the breakdown:

```text
HT-mixed:
  preambleDuration = T_L-STF + T_L-LTF + T_L-SIG
  headerDuration   = T_HT-SIG + T_HT-STF
                   + T_HT-LTF1 + (N_HT-LTF - 1) * T_HT-LTFs

HT-greenfield:
  preambleDuration = T_HT-GF-STF + T_HT-LTF1
  headerDuration   = T_HT-SIG
                   + (N_HT-LTF - 1) * T_HT-LTFs

Both:
  dataDuration = dataMode->getDuration(psduLength)
```

`Ieee80211Transmitter::createTransmission()` must consume only `transmissionMode->getSignalPartDurations(psduLength)` and verify its sum against `transmissionMode->getDuration(psduLength)`. It must not reconstruct HT field timing or identify HT through a concrete type. This keeps PHY timing in the authoritative mode layer under `AR-WLAN-PHY-AUTHORITY` and `AR-WLAN-PHY-TIMING`.

The timing change surface is `IIeee80211Mode.h` for the immutable aggregate and virtual contract, `Ieee80211ModeBase.{h,cc}` for the legacy-preserving default, `Ieee80211HtMode.{h,cc}` for the single named-field breakdown and standard-derived grouping, and `Ieee80211Transmitter.cc` for consuming the aggregate. Do not change NED, MSG, or non-HT concrete mode classes for this timing correction.

The pre-Data field grouping and Data-start identities are traceable to Equations (19-2) and (19-3) and the HT PPDU field chronology. Full BCC TXTIME is specified by IEEE Std 802.11-2024 Clause 19.4.3, Equations (19-90)-(19-93) (`80211ax-2024:chunk:08244`, pp. 3492-3493), and the BCC Data symbol count by Equation (19-32), Clause 19.3.11.1 (`:08119`, pp. 3442-3443). This milestone preserves INET's current aggregate-duration behavior; it does not claim full Equations (19-90)-(19-93) conformance because `Ieee80211HtDataMode::getDuration()` currently uses a 4 us symbol interval and does not implement the mixed-format short-GI aggregate rounding rule. Correcting that rounding is a separate change. If INET later represents `SignalExtension` inside the HT transmission end time, include it in the generic Data duration exactly once; the current HT mode does not add such an extension.

The focused timing regression must prove the exact IEEE field order, contiguous boundaries, HT-SIG as the first physical field inside the generic HEADER interval, every post-HT-SIG training field inside HEADER, `dataStart` equal to the standard's Data-field start, the three-part sum equal to INET's existing total PPDU duration, and the old/new aggregate duration unchanged. Cover HT-mixed and HT-greenfield, 20/40 MHz, long/short GI Data-start boundaries, MCS 0 and 7, valid `N_HT-LTF` cases, and minimum-nonzero plus multi-symbol PSDU sizes. Treat zero-length/NDP transmission as unsupported by EESM unless a separate NDP contract is implemented, and do not use a short-GI total as an Equation (19-90) oracle. The occupied spectrum may use `[start + preambleDuration + headerDuration, end)` only after this contract passes.

Do not implement the earlier `currentPreambleDuration - HT-SIG` rewrite. Although its arithmetic sum is correct, it moves the generic HEADER interval after training fields that physically follow HT-SIG and therefore contradicts the standard.

#### Source change surface

Add these generic, IEEE-independent contracts under `src/inet/physicallayer/wireless/common/contract/packetlevel/`:

- `ITransmissionSpectrum.h`: an immutable value describing disjoint frequency-offset bands and the fraction of total instantaneous power assigned to each band;
- `IPartitionedTransmissionSpectrum.h`: read-only access to immutable, owned preamble/header/Data spectrum descriptors retained by a transmission analog model. It exposes nullable `getPreambleSpectrum()`, `getHeaderSpectrum()`, and `getDataSpectrum()` results as `const ITransmissionSpectrum *`; the returned objects are owned by and live as long as the transmission analog model, and a null result means the legacy nominal-band spectrum;
- `IDimensionalTransmitterAnalogModel.h`: an optional C++ capability extending `ITransmitterAnalogModel` with a spectrum-aware overload. Keep the existing six-argument method unchanged for scalar, unit-disk, generic, APSK, IEEE 802.15.4, and all legacy callers.

Extend `DimensionalTransmitterAnalogModel.{h,cc}` to implement the optional creation capability, and extend `DimensionalTransmissionAnalogModel.{h,cc}` to implement `IPartitionedTransmissionSpectrum`. The returned transmission analog model must own immutable copies of the supplied descriptors; it must not retain caller-owned raw pointers. Do not put IEEE carrier knowledge in either common class.

Add the authoritative IEEE value/contract `IIeee80211OfdmSubcarrierPlan.h` under `src/inet/physicallayer/wireless/ieee80211/mode/`, and make `Ieee80211HtDataMode` implement it in `Ieee80211HtMode.{h,cc}`. It must expose the signed carrier index, role (`DATA` or `PILOT`), center offset, half-open frequency interval, and an equal-power `ITransmissionSpectrum`. The existing `getNumberOfDataSubcarriers()`, `getNumberOfPilotSubcarriers()`, and `getNumberOfTotalSubcarriers()` remain the authoritative counts; construction must cross-check the generated plan against them.

Add an opt-in parameter to `Ieee80211Transmitter.ned` and parse it in `Ieee80211Transmitter.{h,cc}`:

```ned
string dimensionalSpectrumMode
    @enum("nominalBand", "occupiedSubcarriers")
    = default("nominalBand");
```

The default is compatibility behavior. The EESM configuration selects `occupiedSubcarriers`. Do not change `ITransmitterAnalogModel`, `Ieee80211Transmission`, scalar/unit-disk analog models, or unrelated transmitter call sites.

#### Generic spectrum value

Pseudo-code for the immutable common value:

```cpp
struct PowerSpectralBand {
    Hz lowerFrequencyOffset;
    Hz upperFrequencyOffset;
    double powerFraction;  // fraction of total instantaneous power
};

class ITransmissionSpectrum : public virtual IPrintableObject {
  public:
    virtual const vector<PowerSpectralBand>& getBands() const = 0;
};

validateSpectrum(spectrum):
    require bands are ordered and do not overlap
    require every upperFrequencyOffset > lowerFrequencyOffset
    require every band lies within
            [-nominalBandwidth / 2, nominalBandwidth / 2]
    require every powerFraction is finite and > 0
    require abs(sum(powerFraction) - 1) <= 1e-12
```

Adjacent bands may touch. Coalesce only when `left.upper == right.lower` and `left.powerFraction / left.width` equals `right.powerFraction / right.width` within `1e-12` relative tolerance. The merged interval receives `left.powerFraction + right.powerFraction`; verify that coalescing preserves both the sum of fractions and the frequency integral. Otherwise use half-open intervals so a shared endpoint is never counted twice.

#### Authoritative HT carrier plan

Pseudo-code owned by the HT mode layer:

```cpp
enum class Ieee80211SubcarrierRole { DATA, PILOT };

struct Ieee80211OfdmSubcarrier {
    int index;
    Ieee80211SubcarrierRole role;
    Hz lowerFrequencyOffset;
    Hz centerFrequencyOffset;
    Hz upperFrequencyOffset;
};

createHtPlan(bandwidth, mcsIndex):
    require mcsIndex != 32  // excluded from the EESM MVP and has another map
    spacing = 312.5 kHz

    if bandwidth == 20 MHz:
        occupied = [-28 ... -1] U [1 ... 28]
        pilots = {-21, -7, 7, 21}
        expectedData = 52
        expectedTotal = 56
    else if bandwidth == 40 MHz:
        occupied = [-58 ... -2] U [2 ... 58]
        pilots = {-53, -25, -11, 11, 25, 53}
        expectedData = 108
        expectedTotal = 114
    else:
        throw unsupported bandwidth

    for k in occupied:
        center = k * spacing
        append subcarrier {
            index = k,
            role = pilots.contains(k) ? PILOT : DATA,
            lowerFrequencyOffset = center - spacing / 2,
            centerFrequencyOffset = center,
            upperFrequencyOffset = center + spacing / 2
        }
        append spectrum band {
            lowerFrequencyOffset,
            upperFrequencyOffset,
            powerFraction = 1.0 / expectedTotal
        }

    require subcarriers.size() == getNumberOfTotalSubcarriers()
    require count(DATA) == getNumberOfDataSubcarriers()
    require count(PILOT) == getNumberOfPilotSubcarriers()
```

All `N_ST = 56/114` data-plus-pilot carriers therefore carry `Ptx / N_ST`. Only the `N_SD = 52/108` entries marked `DATA` are later sampled by EESM.

#### Spectrum-aware dimensional analog model

Do not broaden the existing generic method. Add this optional capability:

```cpp
class IDimensionalTransmitterAnalogModel :
    public virtual ITransmitterAnalogModel
{
  public:
    virtual ITransmissionAnalogModel *createAnalogModel(
        simtime_t preambleDuration,
        simtime_t headerDuration,
        simtime_t dataDuration,
        Hz centerFrequency,
        Hz nominalBandwidth,
        W totalPower,
        const ITransmissionSpectrum *preambleSpectrum,
        const ITransmissionSpectrum *headerSpectrum,
        const ITransmissionSpectrum *dataSpectrum) const = 0;
};
```

A null part spectrum means “use the existing nominal-band boxcar.” The part-aware signature prevents this patch from falsely applying the HT data map to HT-mixed preamble fields that use other mappings.

Pseudo-code for `DimensionalTransmitterAnalogModel`:

```cpp
createPartPowerFunction(start, end, center, nominalBandwidth,
                        totalPower, spectrum):
    if start == end:
        return zeroFunction

    if spectrum == nullptr:
        return Boxcar2D(start, end,
                        center - nominalBandwidth / 2,
                        center + nominalBandwidth / 2,
                        totalPower / nominalBandwidth)

    require timeGains and frequencyGains are their flat defaults
    require timeGainsNormalization and frequencyGainsNormalization
            have their documented default values
        // Do not silently distort a calibrated equal-power allocation.

    parts = []
    for band in coalesceAdjacentEqualDensityBands(spectrum.getBands()):
        width = band.upperFrequencyOffset - band.lowerFrequencyOffset
        bandPower = totalPower * band.powerFraction
        parts += Boxcar2D(start, end,
                          center + band.lowerFrequencyOffset,
                          center + band.upperFrequencyOffset,
                          bandPower / width)

    result = SummedFunction(parts)
    require approximatelyEqual(
        integrateFrequency(result, midpoint(start, end)),
        totalPower,
        relativeTolerance = 1e-12)
    return result

createAnalogModel(..., preambleSpectrum, headerSpectrum, dataSpectrum):
    start = simTime()
    preambleEnd = start + preambleDuration
    headerEnd = preambleEnd + headerDuration
    dataEnd = headerEnd + dataDuration

    powerFunction = SummedFunction({
        createPartPowerFunction(start, preambleEnd, ..., preambleSpectrum),
        createPartPowerFunction(preambleEnd, headerEnd, ..., headerSpectrum),
        createPartPowerFunction(headerEnd, dataEnd, ..., dataSpectrum)
    })

    return new DimensionalTransmissionAnalogModel(
        preambleDuration, headerDuration, dataDuration,
        centerFrequency, nominalBandwidth,
        makeFirstQuadrantLimitedFunction(powerFunction),
        copySpectrum(preambleSpectrum),
        copySpectrum(headerSpectrum),
        copySpectrum(dataSpectrum))
```

`copySpectrum()` creates the owned immutable metadata exposed by `IPartitionedTransmissionSpectrum`; it is not a second PSD construction path. Preserve the original uncoalesced band/fraction descriptor so receiver-side structural validation compares the authoritative carrier plan rather than an implementation-dependent function simplification.

The initial spectrum-aware patch passes null preamble/header spectra and the HT equal-power spectrum for the PSDU data interval. At every interior data time the required invariants are:

```text
integral_over_frequency(PSD) = configured total transmit power
PSD on every data or pilot carrier = Ptx / (N_ST × 312.5 kHz)
PSD at DC and guard carriers = 0
HT20 occupied width = 56 × 312.5 kHz = 17.5 MHz
HT40 occupied width = 114 × 312.5 kHz = 35.625 MHz
```

The rectangular carrier bins are a discrete power-allocation representation for per-carrier SNIR/EESM. They are not an implementation of the continuous IEEE transmit spectral mask or the sinc-shaped waveform spectrum; document that limitation and do not reuse this profile for spectral-mask compliance claims.

Preamble/header remain on the existing nominal-band approximation. A future whole-PPDU spectrum requires authoritative per-field timing and carrier maps from the preamble/header modes; do not assign the HT data map to the whole PPDU.

#### Mode-aware transmitter call

Pseudo-code in `Ieee80211Transmitter::createTransmission()`:

```cpp
if dimensionalSpectrumMode == "nominalBand":
    analogModel = getAnalogModel()->createAnalogModel(
        durations, centerFrequency, bandwidth, totalPower)
else:
    dimensionalModel =
        dynamic_cast<IDimensionalTransmitterAnalogModel *>(getAnalogModel())
    if dimensionalModel == nullptr:
        throw cRuntimeError(
            "occupiedSubcarriers requires a dimensional transmitter analog model")

    carrierPlan =
        dynamic_cast<const IIeee80211OfdmSubcarrierPlan *>(
            transmissionMode->getDataMode())
    if carrierPlan == nullptr:
        throw cRuntimeError(
            "selected IEEE 802.11 data mode has no OFDM subcarrier plan")

    analogModel = dimensionalModel->createAnalogModel(
        durations, centerFrequency, bandwidth, totalPower,
        nullptr, nullptr, &carrierPlan->getEqualPowerSpectrum())
```

These are capability-contract casts, not concrete implementation casts. `SignalPowerReq` continues to select `totalPower`; the profile only distributes that power across frequency.

The spectrum-aware transmission analog model must retain the immutable per-part spectrum descriptors in `IPartitionedTransmissionSpectrum`, which is reachable from the source transmission through the reception/SNIR contract. Before calculating PER, EESM must compare the retained Data descriptor with the authoritative HT equal-power carrier plan, including every occupied band and power fraction. A missing descriptor (the nominal-band default), a structurally different descriptor, or an incompatible bandwidth must fail explicitly. This check must not inspect module paths, NED typenames, INI parameters, or a concrete analog-model class.

#### Receiver and medium interaction

Exact guard/DC zeros make the current full-nominal-band `DimensionalSnir::getMin()` equal zero. Because `SnirReceiverBase` uses a strict `minSnir > snirThreshold` test, even a threshold that converts to zero cannot pass. EESM configurations must therefore use:

```ini
**.wlan[*].radio.transmitter.dimensionalSpectrumMode = "occupiedSubcarriers"
**.wlan[*].radio.receiver.snirThresholdMode = "mean"
```

Keep the already specified permissive threshold and power gates. Here `mean` is only a pre-error-model gate; it is not the SNR passed to the AWGN table. EESM still evaluates the `N_SD` data-carrier centers.

Do not change `DimensionalSnir::getMin()` globally in this patch. A support-aware minimum would be a separate receiver/analog-model contract change with a much larger regression surface.

This selected transmitter change also does not by itself fix `DimensionalMediumAnalogModel`'s `bandwidth / 10` attenuation approximation. The TGn/dimensional-medium milestone must still supply or configure 312.5 kHz-aligned frequency resolution before an end-to-end D/E fidelity claim. The error model should consume meaningful per-carrier SNIR values, not infer transmit power allocation or channel gain from concrete upstream module types.

### 4. Add immutable table and EESM helpers

Under `src/inet/physicallayer/wireless/ieee80211/packetlevel/errormodel/`, add focused helpers such as:

- `Ieee80211PerTable.{h,cc}`: schema parsing, key validation, SNR interpolation, endpoint policy, and Eq. 1 length scaling;
- `Ieee80211Eesm.{h,cc}`: stable Eq. 5 evaluation and calibration lookup;
- `Ieee80211EesmErrorModel.{ned,h,cc}`: INET module integration only, deriving directly from `ErrorModelBase` rather than `Ieee80211ErrorModelBase` because this model has no independent header/data formulas.

Keep the helpers deterministic and free of RNG calls. `FlatReceiverBase` must retain ownership of the one packet-success random draw.

`Ieee80211EesmErrorModel` validates packet-level operation and implements the deterministic PER/BER/SER accessors required by the receiver contract. It returns `NaN` for BER/SER, rejects bit-level operation, and reports unsupported mode, bandwidth, coding, spectrum, or PSDU-length metadata during initialization when statically knowable, otherwise at the first attempted reception/PER lookup.

Use separate versioned provenance schemas for the two artifact classes:

- Future AWGN PER tables require provenance and artifact checksums; provider tool name/version/commit, executable and configuration checksums, and toolchain; campaign identifier and seed policy; raw trials/errors and confidence criteria; SNR definition and units; and curve keys with coding fixed to BCC, channel bandwidth, MCS, and reference length. Record the supported HT SISO scope in the manifest, and do not key these curves by TGn profile.
- Published beta metadata requires the Patidar paper citation and checksum, exact source table and row identity, beta-artifact checksum, calibration identity, coding, TGn profile, bandwidth, MCS, beta value, and the explicit project pairing `eesmCarrierSet = "dataOnly"` with `eesmCarrierCount = 52|108`. The publication provides no generator identity, campaign seed, raw observation counts, or confidence calculation for these rows; record unavailable publication fields as unavailable and never synthesize them.

The AWGN validator must fail on missing campaign metadata, duplicate keys, incomplete required reference lengths, nonmonotone axes/curves, invalid raw counts, NaN/infinity, checksum mismatches, or unsupported schema versions. The beta validator must accept complete publication provenance without generator-only fields and must fail on a missing source table/row, carrier convention, key, checksum, or finite positive beta value.

For a future AWGN manifest, keep “provenance” and “provider tool identity” distinct but linked. Provenance is the chain of origin and custody for the dataset: why it was produced, which source documents and campaign inputs it derives from, where the raw observations are, who reviewed it, what license permits redistribution, and confirmation that forbidden sources were not used. Provider tool identity names the executable that produced the supplied data: its name, version/commit, executable checksum, toolchain, and configuration checksum. Tool identity is one component of provenance, not a synonym for it. For example:

```json
{
  "provenance": {
    "method": "independent HT link simulation",
    "campaignId": "awgn-ht-siso-v1",
    "sourceDocuments": [
      {"id": "patidar2017", "sha256": "a428fdcaa442..."},
      {"id": "ieee80211-2024", "sha256": "c08beb0e16bf..."}
    ],
    "rawObservationsSha256": "...",
    "generatedDataLicense": "SPDX identifier or reviewed license text",
    "reviewedBy": "review record identifier",
    "forbiddenSourcesUsed": []
  },
  "providerTool": {
    "name": "future-bcc-ht-link-simulator",
    "version": "1.0.0",
    "gitCommit": "...",
    "executableSha256": "...",
    "toolchain": "compiler and dependency lock identifier",
    "configurationSha256": "..."
  }
}
```

### 5. Implement the receiver-side call flow

`Ieee80211EesmErrorModel::computePacketErrorRate()` should:

1. obtain `Ieee80211Transmission`, the HT data mode, bandwidth, NSS, MCS, BCC code, and PSDU length. Read the PSDU octet count from the front PHY header's HT Length field, which INET sets from the MAC packet length before PHY header/tail/pad insertion; do not use the post-encapsulation packet length, and reject HT Length 0/NDP;
2. require BCC and validate the mode against the selected bandwidth-specific PER curve and beta calibration set;
3. require `SIGNAL_PART_WHOLE`; reject split-part queries instead of decomposing the atomic table probability. The supported radio configuration sets `separateReceptionParts = false`, and the query check enforces the contract without looking up or downcasting the containing radio;
4. require `IDimensionalSnir`; reject scalar `ISnir` with a diagnostic that names the unsupported normalization contract;
5. obtain `dataStart`, `dataEnd`, and center frequency from the reception associated with `DimensionalSnir`, not from transmission-time coordinates. Convert the authoritative HT carrier offsets to absolute reception frequencies. Retrieve `snir->getReception()->getTransmission()->getAnalogModel()` as `const ITransmissionAnalogModel *` and capability-cast that analog model to `const IPartitionedTransmissionSpectrum *`; do not cast the IEEE transmission, reception, or containing modules. Require its immutable Data descriptor to match the authoritative HT equal-power occupied-carrier plan; reject a missing descriptor, the nominal-band default, or any incompatible descriptor;
6. restrict each carrier evaluation to the right-continuous reception Data interval `[dataStart, dataEnd)`. Accept the intentional header-to-Data spectrum discontinuity whose boundary is exactly `dataStart`, ignore a transition at `dataEnd`, and reject only variation strictly inside the interval, `dataStart < t < dataEnd`, when `abs(max - min) > max(1e-15, 1e-12 * max(abs(min), abs(max)))`. Use exact function-domain/range operations rather than a sampling grid, sample the interval midpoint after the check, and require exactly 52 or 108 Data-carrier values. Apply only the configured `snirOffset` to each raw dimensional SNIR value; do not divide by `N_ST`, `N_SD`, or transmit power again;
7. compute `gammaEff` with the selected beta, convert it to dB, query the AWGN reference curve, and apply Eq. 1;
8. return a finite PER in `[0,1]` without drawing random numbers.

Restrict `corruptionMode` to `packet`; return `NaN` for BER/SER because the model produces no defensible bit- or symbol-error distribution.

### 6. Keep configuration declarative and opt-in

Expose NED parameters with defaults and single meanings:

- `perTableFile = default("")` for the future-supplied runtime CSV;
- `object perTableManifest = default(nullptr)` for its reviewed provenance manifest, supplied from INI with `readJSON(...)`;
- `betaTableFile = default("")` for the versioned published-BCC calibration artifact;
- `calibrationSet = default("")` for the selected D20/D40/E20/E40 identity;
- the inherited `snirOffset`, default `0 dB`.

The empty-string defaults keep the module declarative without pretending that the future AWGN artifact already exists. Selecting `Ieee80211EesmErrorModel` with any required path or calibration identity empty must fail during initialization with a diagnostic that names the missing parameter. The default NIST error-model trajectory never instantiates this module and remains unaffected.

Time variation is unconditionally rejected in the MVP using the fixed comparison contract above, so do not add a one-value `timeVariationPolicy` parameter. Add a parameter only when a second, calibrated policy exists.

Example:

```ini
*.radioMedium.signalAnalogRepresentation = "dimensional"
**.wlan[*].radio.signalAnalogRepresentation = "dimensional"
**.wlan[*].radio.separateReceptionParts = false
**.wlan[*].radio.transmitter.dimensionalSpectrumMode = "occupiedSubcarriers"
**.wlan[*].radio.receiver.snirThresholdMode = "mean"
**.wlan[*].radio.receiver.snirThreshold = -100dB
**.wlan[*].radio.receiver.sensitivity = -200dBm
**.wlan[*].radio.receiver.errorModel.typename = "Ieee80211EesmErrorModel"
**.wlan[*].radio.receiver.errorModel.perTableFile = "path/to/future-awgn-per-table.csv"
**.wlan[*].radio.receiver.errorModel.perTableManifest = readJSON("path/to/future-awgn-per-manifest.json")
**.wlan[*].radio.receiver.errorModel.betaTableFile = "path/to/patidar2017-bcc-beta.csv"
**.wlan[*].radio.receiver.errorModel.calibrationSet = "patidar2017-bcc-d20"
```

This reviewed-mode configuration remains illustrative until future AWGN files pass the acceptance gate below. A runnable local showcase may instead select `artifactAcceptanceMode="userAuthorizedLocal"` and the exact user-supplied table, provided it exposes all limitations and makes no production-readiness or independent-validation claim.

The two placeholder threshold values above are showcase values, not universal defaults. Each validation campaign must prove that they are below its minimum received power/SNIR and record the proof. `energyDetection` is intentionally absent: derive it from the fixture link budget so noise-only listening is idle while the weakest intended signal crosses the threshold, or bypass MAC/channel access with a direct PHY/link fixture. Production scenarios may intentionally choose stricter front-end gates, but then their PER includes behavior outside this paper's model.

The error model must verify that the runtime bandwidth matches the calibration set. It must not inspect or downcast the configured channel generator to infer a TGn profile. The scenario author owns the explicit choice, and the configuration should state it once and propagate it.

Do not change `Ieee80211Radio`'s default error model in the first patch. Existing scalar/dimensional NIST trajectories must remain unchanged unless the new typename is selected.

## Accept future-supplied AWGN PER tables

AWGN curve generation is outside this plan. The default `reviewed` mode must continue to enforce this complete acceptance gate. An explicit generic `userAuthorizedLocal` mode may load artifacts only for local integration and must always require a direct authorization record, `deploymentScope="localEvaluationOnly"`, and exact checksums. The exact project-user table must additionally preserve its HT40-duplicates-HT20 assumption and truthful `unavailable`, `notVerified`, and `notGranted` evidence statuses. Repository-owned synthetic test fixtures may instead declare unavailable evidence `notApplicable` and their INET-compatible redistribution status `granted`; this does not relax or replace the reviewed production gate.

The supplied native lookup domain must be BCC SISO HT MCS 0-7, 20 and 40 MHz, and 32-byte and 1458-byte PSDUs. Because the paper directly shows and validates only the 20 MHz AWGN case, channel bandwidth is part of the runtime curve key unless the provider supplies an independently reviewed flat-AWGN equivalence study that justifies aliasing the HT20 and HT40 curves. The conservative required domain is therefore:

| Coding | Bandwidths | MCS values | Reference PSDU lengths | Curve count |
|---|---|---|---|---:|
| BCC | 20 and 40 MHz | 0-7 | 32 B and 1458 B | 32 |

AWGN curves are selected by `(coding=BCC, bandwidth, MCS, referenceLength)`. TGn Model D/E is not an AWGN-curve key; it selects the separate `beta` calibration set. A missing bandwidth-specific curve must fail explicitly rather than aliasing another bandwidth or analytical model.

On 2026-08-23 the project user supplied parser-compatible BCC points for all 32 required curves in `tests/unit/data/Ieee80211PerTable.user-supplied-candidate.csv` (412 data rows plus the schema header), then explicitly requested execution of Milestones 4–5. The exact CSV is preserved unchanged as the ignored local-only `user-supplied-bcc-awgn-per-v1.csv`, SHA-256 `364c87c3e129876f7782b312fc7a7c5b69b23c96e542ecadf113be23129cd608`, with separate D20/D40/E20/E40 local manifests. This authorization is deliberately narrow: raw packet/error counts, confidence intervals and method, generator/provider identity and checksums, SNR normalization, license, clean-room verification, independent review, and production deployment approval remain unavailable or not granted. Exact 0 and 1 endpoints cannot be converted to the plan's Jeffreys estimates without raw counts.

Require these future artifacts:

1. `awgn-per-observations.csv`, retaining raw trials and errors for statistical review;
2. `awgn-per-table.csv`, the normalized deterministic runtime table;
3. `awgn-per-manifest.json`, containing provenance, provider-tool identity, configuration, licensing, checksums, and review approval.

Use normalized long-form CSV:

```text
# awgn-per-observations.csv
coding,bandwidth_mhz,mcs,psdu_bytes,snr_db,packets,decode_errors,observed_per,interpolation_per,ci95_low,ci95_high
BCC,20,0,32,-4.0,400,400,1.0,0.998753117,0.990488,1.0

# awgn-per-table.csv
coding,bandwidth_mhz,mcs,psdu_bytes,snr_db,per
BCC,20,0,32,-4.0,0.998753117
```

The acceptance validator must reject duplicate keys, missing bandwidth/MCS/length combinations, non-increasing SNR grids, invalid counts, inconsistent `observed_per`, an `interpolation_per` that differs from `(decode_errors + 0.5) / (packets + 1)` beyond the declared serialization tolerance, nonmonotone PER, unresolved endpoints, or a checksum mismatch. It must verify the named confidence-interval method, deterministic full-key ordering, and finite runtime values.

At minimum the manifest must contain:

- schema version and SHA-256 of both CSV files;
- provider repository/source identity, commit or immutable version, license, build toolchain, executable checksum, configuration checksum, and generated-data license;
- the Patidar and IEEE document checksums and exact source clauses/algorithms;
- SNR and PSDU-length definitions, explicit HT20/HT40 scope, BCC encoder, puncturing, interleaving, soft-input Viterbi decoder, traceback/metric configuration, and all ideal-receiver assumptions;
- SNR grid, stopping rule, seed derivation, campaign identifier, generation timestamp, and confidence information;
- independent review approval and an explicit statement that no forbidden ns-3 artifact was inspected or used.

The existing `BerParseFile` format is not the target representation because its schema and lookup are hard-wired around legacy PHY rates and packet lengths. Implement the new loader against the explicit tuple keys above. Use small synthetic CSV/manifest fixtures for parser, interpolation, length scaling, endpoint, missing-key, checksum, and unsupported-coding tests; clearly label them non-production data.

Before accepting future tables in `reviewed` mode, independently compare selected BCC points with reproducible, legally usable reference results and run direct flat-dimensional HT20 and HT40 holdouts. Confirm that the supplied curves use the same occupied-band power normalization and PSDU-octet contract as the runtime model. Until this gate passes, the default reviewed selection must fail at initialization for the local artifact; only the explicit user-authorized local mode may use it.

The paper calls Model D at 10 m “NLOS,” while TGn 03/940r4 defines 10 m as its inclusive LOS breakpoint. Preserve the published BCC beta values under the paper-specific calibration-set names, record the project-selected 52/108 data-only pairing, and do not claim that a generic TGn D realization reproduces them until the channel condition and receiver assumptions are independently locked.

## Focused verification plan

Add only directly mapped tests:

| Test | Required checks |
|---|---|
| `Ieee80211PerTable_1.test` | BCC-only schema and complete bandwidth/MCS/length keys using a non-production synthetic fixture; non-BCC rejection; raw-count and Jeffreys-estimate validation; checksum/manifest failures; monotonicity; complementary-log-log midpoint interpolation; direct midpoint holdouts versus raw-PER interpolation when future evidence arrives; endpoint clamping; Eq. 1 at 32/1458 and both sides of 400; stable mathematical PER 0/1 handling in the length-scaling helper; complete 32-curve domain and exact HT20/HT40 synthetic point equality. The ignored exact user table is checked separately by local artifact checks and showcase execution, not by the committed unit fixture. |
| `Ieee80211Eesm_1.test` | exactly 52/108-element data-carrier vectors and `dataOnly` calibration metadata; beta publication provenance without generator-only fields; missing source-row/convention rejection; flat-vector identity; permutation invariance; monotonicity; min/max bounds; deep fade; extreme-value stability; invalid beta/vector; all 32 published BCC beta values; carrier-count, pilot-inclusion, coding, and calibration-key mismatch rejection |
| `Ieee80211HtPartTiming_1.test` | exact HT-mixed/greenfield IEEE field chronology; one named-field timing source; contiguous PREAMBLE/HEADER/DATA boundaries; HT-SIG first inside HEADER; all post-HT-SIG training inside HEADER; Data start equals the standard boundary; HT-SIG semantic duration unchanged; data duration equals the data-mode duration; part sum and old/new aggregate equality; 20/40 MHz, long/short-GI Data starts without claiming full short-GI TXTIME conformance, valid `N_HT-LTF`, MCS 0/7, minimum-nonzero and multi-symbol PSDUs, and NDP rejection |
| `Ieee80211LegacyPartTiming_1.test` | representative DSSS, OFDM, ERP, and VHT modes retain the old transmitter split through `Ieee80211ModeBase`; ERP's residual 6 us placement is unchanged |
| `Ieee80211HtSubcarrierPlan_1.test` | `N_ST` = 56/114 occupied and `N_SD` = 52/108 data carriers; exact pilot indices; DC/guard exclusion; spacing, symmetry, and authoritative intervals from IEEE Std 802.11-2024 Clauses 19.3.6, 19.3.7, 19.3.11.11.3, and 19.3.11.11.4 |
| `Ieee80211OccupiedSpectrum_1.test` | HT20/HT40 PSD equals `Ptx / 17.5 MHz` or `Ptx / 35.625 MHz`; frequency integral equals requested `Ptx`; pre/post-coalescing integral and fraction equality; band containment; equal data/pilot allocation; DC/guards zero; corrected data interval boundaries; preamble/header retain the nominal boxcar; immutable Data-spectrum metadata survives transmission/reception; `SignalPowerReq` override; every nondefault gain or normalization setting is rejected |
| `Ieee80211EesmErrorModel_1.test` | direct `ErrorModelBase` packet-level integration; flat and frequency-selective dimensional SNIR; a flat one-carrier identity fixture proves no second carrier-count normalization; two equal-mean vectors produce the expected different EESM values; nominal-band metadata rejection and authoritative 52/108 occupied-spectrum acceptance; scalar SNIR rejection; atomic WHOLE-only semantics; split-part rejection; PSDU length extracted from HT Length before/after PHY encapsulation; `ErrorRateInd`; unsupported-mode failures |
| `Ieee80211EesmTimeVariation_1.test` | reception-domain half-open Data endpoints; the intended header-to-Data transition exactly at `dataStart` is accepted; a change exactly at `dataEnd` is ignored; strictly interior changes are rejected; absolute/relative differences exactly at and immediately above/below tolerance; continuous variation rejection |
| `Ieee80211EesmReceiverGates_1.test` | occupied-carrier guard/DC zeros make the current `min` gate reject; `mean` plus permissive gates lets MCS 0 below 4 dB reach the lookup; sensitivity reception boundary; a link-budget-derived energy-detection threshold leaves noise-only listening idle and detects the weakest desired signal; with atomic reception, exactly one receiver RNG draw for interior PER and none for exact endpoints |
| unaffected legacy selection | the existing NIST typename/configuration retains the same effective parameters and directly related trajectory when the EESM typename is not selected |
| future-table acceptance | after delivery, selected BCC bandwidth/MCS/SNR/reference-length points agree with the supplied raw observations and predeclared binomial confidence intervals; direct flat HT20/HT40 holdouts validate the keyed curves and occupied-band normalization |
| D/E validation campaign | only after the TGn channel, future AWGN tables, and calibration inputs match: start with BCC D20 using 100 channel realizations × 1000 packets × six SNRs per MCS; compare predicted/link PER MSE with the scale of paper Table 4 before expanding to D40/E20/E40 |

Useful deterministic numeric oracles include:

- Eq. 1: `PER32 = 0.2`, `PL = 200` → `0.752079511649`;
- Eq. 1: `PER1458 = 0.1`, `PL = 1000` → `0.069714479033`;
- Eq. 5: `beta = 1.01`, `gamma = [1,4]` → `1.6495623414`;
- Eq. 5: `beta = 32.56`, `gamma = [1,20]` → `9.1333253576`.

Run in debug mode from the repository root, using only explicit filters mapped to the changed paths:

```sh
make MODE=debug -j$(nproc)

inet_run_unit_tests -m debug -f \
  'Ieee80211(PerTable|Eesm|HtPartTiming|LegacyPartTiming|HtSubcarrierPlan|OccupiedSpectrum|EesmTimeVariation)_1\.test'
```

Run the two receiver-integrated module fixtures separately:

```sh
inet_run_module_tests -m debug -f \
  'Ieee80211(EesmErrorModel|EesmReceiverGates)_1\.test'
```

Run the existing dimensional IEEE 802.11 example fingerprint rows only if a common path used by those configurations changes. The new model is opt-in, so existing trajectories should not move. These rows cover default/common compatibility only; they do not cover the new HT timing split. No existing focused HT timing fingerprint was identified, so the deterministic HT-mixed and HT-greenfield timing/module tests are the primary coverage. Never update or add fingerprint expectations without explicit user approval and first-divergence trajectory evidence.

Because this design extends `DimensionalTransmitterAnalogModel` and corrects HT signal-part boundaries, run these directly mapped legacy rows in debug mode and require the stored fingerprints to remain unchanged:

```sh
(cd tests/fingerprint && ./fingerprinttest -d \
  -m 'Ieee80211RadioWithDimensionalAnalogModel' \
  -f 'tplx' -f '~tNl' -f '~tND')

(cd tests/fingerprint && ./fingerprinttest -d \
  -m 'Ieee80211MacWithIeee80211DimensionalRadio' \
  -f 'tplx' -f '~tNl' -f '~tND')
```

If either compatibility fingerprint diverges, stop at the first event and present it for review. Do not update either CSV without explicit user approval.

## Milestones and completion gates

1. **Source/data contract:** record the clean-room boundary, BCC-only scope, 52/108 data-only beta pairing, mathematical decisions, future CSV-plus-manifest acceptance schema, unsupported-domain behavior, paper/TGn source checksums, and IEEE Std 802.11-2024 Clause 19 citations. Keep the publication-derived beta artifact in the ignored local evaluation area until an explicit redistribution decision; commit no AWGN PER or paper-derived beta data.
2. **Pure deterministic core:** correct and verify the standard-authoritative HT signal-part contract, then implement the carrier plan, PER-table loader, EESM helper, and synthetic non-production dimensional-data unit tests. Use `N_ST` only when allocating transmit power in the spectrum and use the resulting raw SNIR values for the `N_SD`-element EESM sum without a second normalization.
3. **Dimensional BCC SISO integration:** add the dimensional SNIR and optional spectrum-aware transmitter contracts, authoritative HT carrier plan, opt-in occupied-carrier PSDU spectrum, and EESM module; pass power-conservation, carrier-allocation, receiver-gate, synthetic per-carrier, missing-table, and effective-configuration tests. The module must fail initialization without explicit reviewed table paths.
4. **AWGN artifact execution:** the 2026-08-23 user-supplied 32-curve, 412-row table is accepted only for explicit local evaluation, unchanged and checksum-pinned. Its manifests record unavailable raw counts/SNR normalization/provider identity, unverified clean-room status, no redistribution grant, and the user-directed HT40=HT20 assumption. The separate `reviewed` production gate remains pending. Scalar integration remains deferred until an occupied-band scalar-normalization contract exists.
5. **TGn D/E integration:** integrate true per-carrier TGn output with static SISO D20/D40/E20/E40 local showcase configurations. Disable large-scale shadowing and temporal/vehicle/fluorescent effects; select D NLOS and E LOS; apply the paper-calibration ensemble normalization only for E. Treat the runs as wiring/integration evidence. Independent reproduction of Table 4 remains pending because the paper does not publish the decoder outcomes/raw validation points needed to avoid comparison against Bernoulli samples generated from the same EESM prediction.
6. **Review and documentation:** document the API, both artifact acceptance modes, explicit unavailable-data behavior, and the local showcase limitations. Run focused architecture checks and the complete general/WLAN semantic checklists on the production diff.

Architecture/review gates for the final patch:

```sh
bash .agents/skills/inet-architectural-requirements/references/enforcement/check-architecture.sh
```

Run this as a repository-wide check and reconcile every finding against the current architecture/naming ledgers and a recorded baseline. Do not pass the IEEE 802.11 subtree as the script's domain argument: that changes the dependency root and creates false internal-layer violations. Then perform the complete general and WLAN semantic review on the actual diff.

Map the final diff to at least `AR-ORG-CONTRACTS`, `AR-MOD-PLUGGABLE`, `AR-MOD-FIDELITY`, `AR-PKT-ERRORS`, `AR-PKT-SIGNAL`, `AR-OBS-NED-TRUTH`, `AR-CFG-PARAMS`, `AR-QUAL-TESTS`, `AR-QUAL-DETERMINISM`, `AR-QUAL-NAMING`, `AR-QUAL-TRACEABILITY`, `AR-WLAN-STD-TRACE`, `AR-WLAN-STD-GATING`, `AR-WLAN-ARCH-BOUNDARIES`, `AR-WLAN-PHY-AUTHORITY`, and `AR-WLAN-QUAL-TESTS`.

All likely source targets are currently unsealed; the only recursive source seal is `src/inet/common/packet/`, which this plan does not touch. No production source files were changed while preparing this plan.
