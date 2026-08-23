# TGn wideband MIMO channel implementation plan

## Outcome and scope

Implement IEEE 802.11 TGn channel models A–F as an opt-in, time-varying, wideband MIMO channel in INET. The channel must produce an immutable $N_\mathrm{RX}\times N_\mathrm{TX}$ complex response as a function of absolute simulation time and baseband frequency. The first receiver integration supports SISO and one spatial stream sent through one configured transmit antenna and received with ideal maximum-ratio combining (MRC). It must retain the full matrix even when the receiver consumes only one column.

This document is self-contained. All required contracts, algorithms, TGn source constants, file changes, and tests are stated here. Implementation uses this repository and the source document identified below; it has no unlisted code dependency.

Time variation is part of the initial channel contract and completion gate. A static channel is allowed only as a diagnostic configuration and unit-test fixture. The base Bell process, the Model F moving-vehicle term, and the Models D/E fluorescent-light term are implemented and validated before this work is complete; they are not a later fidelity milestone.

The implementation is deliberately bounded:

- Supported channel matrices: 1–8 Rx antennas and 1–8 Tx antennas, with required coverage for 1×1, 2×1, 2×2, and 4×4. The upper bound matches the declared spatial-quadrature convergence domain.
- Supported decoding: SISO or one stream using one configured transmit column and ideal MRC across receive antennas.
- Supported arrays: horizontal, centered, equally spaced ULA; default spacing is $\lambda/2$. INET's current antenna contract exposes element count but not element locations, so spacing is an explicit channel-model parameter.
- Supported TGn conditions: explicit `nlos` or `los`; LOS is rejected beyond the profile breakpoint.
- Supported 802.11 integration: dimensional radios; one-stream HT20/HT40 data with no STBC; and the one-stream legacy DSSS, HR-DSSS, and OFDM control/management/data frames needed for normal mixed-format exchanges. The pure evaluator remains continuous in frequency. VHT and later PPDU formats are outside this plan.
- Runtime RF-channel switching is outside the initial integration. The first transmission for a radio pair fixes its reference carrier frequency; a later frequency change is rejected with both frequencies in the diagnostic. Switching back and forth becomes a separate feature only after frequency-keyed realization state and its continuity policy are designed.
- Model A is implemented and tested. The TGn report labels it optional and excludes it from performance comparisons; this plan still validates its profile, channel response, and reported Table III reference value.
- Spatial multiplexing, transmit beamforming/precoding, STBC, per-stream L-MMSE, arbitrary array geometry, elevation spread, antenna coupling, and polarization are outside this plan.
- Polarization is excluded because 03/940r4 gives 10 dB LOS and 3 dB NLOS XPD policies but no complete cross-polar correlation model.
- TGac 80/160 MHz tap expansion and TGax-specific variants are outside this plan.

TGn channel and MRC integration are validated independently; end-to-end smoke scenarios use the ordinary scalar error policy and are not presented as TGn PER validation. A future receiver-calibration change may consume the matrix-aware SNIR contract added here, but it is not part of this channel implementation.

## Source authority and fixed modeling decisions

The channel-model authority is IEEE 802.11-03/940r4, *TGn Channel Models*, May 10, 2004:

```text
standards/11-03-0940-04-000n-tgn-channel-models.pdf
SHA-256 d324baf8f3943ea4ee841cfc3bdf35cafff488e99e979cda290e945b1ea82463
```

This is an IEEE working-group contribution and evaluation model, not normative IEEE 802.11 behavior. Cite it as model provenance, not as a normative MAC/PHY requirement. Relevant locations are:

- §2 and Table I, submission pp. 6–7: A–F delay spreads, breakpoint path loss, and shadowing.
- §3, Equations (3)–(10), pp. 7–9: matrix dimensions, LOS/NLOS composition, Kronecker correlation, and ULA correlation integrals.
- §4.1 and Table II, pp. 11–12: cluster counts, first-tap K factors, and LOS addition without scaling back.
- §§4.2–4.6, pp. 12–17: Laplacian PAS, fixed Appendix C angles, and shared angular parameters within a cluster.
- §4.7.1, Equations (20)–(23), pp. 17–19: Bell spectrum and the 1.2 km/h environmental speed.
- §4.7.2, Equations (24)–(26), pp. 19–21: the Model F vehicle component.
- §4.7.3, Equations (27)–(30), pp. 21–24: fluorescent-light modulation.
- §7 and Table III, pp. 25–26: the 4×4 narrowband NLOS capacity oracle.
- Appendix C, pp. 34–41: the exact A–F component tables.

IEEE Std 802.11-2024 Clause 19.3.12.1 supplies the baseband interface $y_k=H_kx_k+n$, with $H_k$ having Rx rows and Tx columns. It treats the beamforming matrix separately and does not prescribe TGn stochastic generation. Therefore the following are explicit INET policies rather than claims about unspecified report behavior:

1. Normalize the complete NLOS component-power sum to one before adding path loss, shadowing, or LOS.
2. Use a centered horizontal ULA, positive phase convention, and an ordinary transpose on the transmit correlation factor.
3. Use the principal Hermitian PSD square root of each spatial correlation matrix.
4. Use the transmission center frequency as the complex-baseband reference frequency.
5. Make reciprocity optional and disabled by default; reciprocal reverse response is an ordinary transpose, not a conjugate transpose.
6. Use one link-level fluorescent modulation function shared by the three affected taps, and include added LOS energy when normalizing the requested fluorescent I/C ratio.
7. Treat the report's Model F “third tap” as Appendix C report tap index 3: the 20 ns component in Cluster 1.
8. Apply the report's D/E fluorescent tap numbers locally within the named cluster, resolving them to the report tap indices listed below.

Decisions 1, 2, 3, and 6 must be labeled `INET policy` in code comments and tests. They are not tuning knobs to be changed while debugging. If the reference MATLAB generator becomes available, compare these policies before medium integration; a discrepancy requires an explicit plan amendment, not an ad hoc code change.

## Exact TGn data owned by `TgnChannelProfile`

Do not flatten the profile into unique delays. A delay may contain independent components from several clusters with different spatial correlation. Store raw report data and derived normalized power separately.

```text
TgnProfile {
    model                         // A..F
    rmsDelaySpread
    breakpointDistance
    shadowSigmaLosDb
    shadowSigmaNlosDb
    firstTapKDb
    taps[]
    clusters[]
    components[]
}

Tap {
    reportTapIndex                // 1-based Appendix C index
    excessDelay
}

Cluster {
    reportClusterIndex            // 1-based Appendix C index
    meanAoA
    receiverAngularSpread
    meanAoD
    transmitterAngularSpread
}

Component {
    stableComponentIndex          // canonical (cluster, reportTapIndex) order
    reportClusterIndex
    reportTapIndex
    relativePowerDb
    rawLinearPower
    normalizedLinearPower
}
```

Profile summary and large-scale parameters:

| Model | RMS delay | Clusters | Unique taps | Components | Breakpoint | LOS/NLOS shadow σ | First-tap LOS K |
|---|---:|---:|---:|---:|---:|---:|---:|
| A | 0 ns | 1 | 1 | 1 | 5 m | 3/4 dB | 0 dB |
| B | 15 ns | 2 | 9 | 12 | 5 m | 3/4 dB | 0 dB |
| C | 30 ns | 2 | 14 | 18 | 5 m | 3/5 dB | 0 dB |
| D | 50 ns | 3 | 18 | 27 | 10 m | 3/5 dB | 3 dB |
| E | 100 ns | 4 | 18 | 38 | 20 m | 3/6 dB | 6 dB |
| F | 150 ns | 6 | 18 | 41 | 30 m | 3/6 dB | 6 dB |

Exact component delay/power data, written as `delay-ns/power-dB`:

```text
A1: 0/0

B1: 0/0 10/-5.4 20/-10.8 30/-16.2 40/-21.7
B2: 20/-3.2 30/-6.3 40/-9.4 50/-12.5 60/-15.6 70/-18.7 80/-21.8

C1: 0/0 10/-2.1 20/-4.3 30/-6.5 40/-8.6 50/-10.8 60/-13.0
    70/-15.2 80/-17.3 90/-19.5
C2: 60/-5.0 70/-7.2 80/-9.3 90/-11.5 110/-13.7 140/-15.8
    170/-18.0 200/-20.2

D1: 0/0 10/-0.9 20/-1.7 30/-2.6 40/-3.5 50/-4.3 60/-5.2 70/-6.1
    80/-6.9 90/-7.8 110/-9.0 140/-11.1 170/-13.7 200/-16.3
    240/-19.3 290/-23.2
D2: 110/-6.6 140/-9.5 170/-12.1 200/-14.7 240/-17.4 290/-21.9 340/-25.5
D3: 240/-18.8 290/-23.2 340/-25.2 390/-26.7

E1: 0/-2.6 10/-3.0 20/-3.5 30/-3.9 50/-4.5 80/-5.6 110/-6.9
    140/-8.2 180/-9.8 230/-11.7 280/-13.9 330/-16.1 380/-18.3
    430/-20.5 490/-22.9
E2: 50/-1.8 80/-3.2 110/-4.5 140/-5.8 180/-7.1 230/-9.9 280/-10.3
    330/-14.3 380/-14.7 430/-18.7 490/-19.9 560/-22.4
E3: 180/-7.9 230/-9.6 280/-14.2 330/-13.8 380/-18.6 430/-18.1 490/-22.8
E4: 490/-20.6 560/-20.5 640/-20.7 730/-24.6

F1: 0/-3.3 10/-3.6 20/-3.9 30/-4.2 50/-4.6 80/-5.3 110/-6.2
    140/-7.1 180/-8.2 230/-9.5 280/-11.0 330/-12.5 400/-14.3
    490/-16.7 600/-19.9
F2: 50/-1.8 80/-2.8 110/-3.5 140/-4.4 180/-5.3 230/-7.4 280/-7.0
    330/-10.3 400/-10.4 490/-13.8 600/-15.7 730/-19.9
F3: 180/-5.7 230/-6.7 280/-10.4 330/-9.6 400/-14.1 490/-12.7 600/-18.5
F4: 400/-8.8 490/-13.3 600/-18.7
F5: 600/-12.9 730/-14.2
F6: 880/-16.3 1050/-21.2
```

Exact cluster tuples `(AoA°, Rx AS°, AoD°, Tx AS°)`:

```text
A1: (45.0,40.0,45.0,40.0)
B1: (4.3,14.4,225.1,14.4)      B2: (118.4,25.2,106.5,25.4)
C1: (290.3,24.6,13.5,24.7)    C2: (332.3,22.4,56.4,22.5)
D1: (158.9,27.7,332.1,27.4)   D2: (320.2,31.4,49.3,32.1)
D3: (276.1,37.4,275.9,36.8)
E1: (163.7,35.8,105.6,36.1)   E2: (251.8,41.6,293.1,42.5)
E3: (80.0,37.4,61.9,38.0)     E4: (182.0,40.3,275.7,38.7)
F1: (315.1,48.0,56.2,41.6)    F2: (180.4,55.0,183.7,55.2)
F3: (74.7,42.0,153.0,47.4)    F4: (251.5,28.6,112.5,27.2)
F5: (68.5,30.7,291.0,33.0)    F6: (246.2,38.2,62.3,38.0)
```

Profile construction and validation pseudocode:

```text
createProfile(model):
    copy immutable raw taps, clusters, and components for model

    require tap indices are contiguous starting at 1
    require tap delays are finite, nonnegative, and strictly increasing
    require cluster indices are contiguous starting at 1
    require every component references an existing tap and cluster
    require every tap has at least one component
    require all powers, angles, and angular spreads are finite
    require angular spreads > 0
    require component iteration order is (cluster index, tap index)

    for component in components:
        component.rawLinearPower = 10^(component.relativePowerDb / 10)

    totalRawPower = sum(component.rawLinearPower)
    require totalRawPower is finite and > 0

    for component in components:
        component.normalizedLinearPower =
            component.rawLinearPower / totalRawPower

    require abs(sum(normalizedLinearPower) - 1) <= 1e-12
    powerAtDelay[tap] = sum(component.normalizedLinearPower
                            for every component at tap)
    meanDelay = sum(powerAtDelay[tap] * tap.excessDelay)
    rmsDelay = sqrt(sum(powerAtDelay[tap]
                        * (tap.excessDelay - meanDelay)^2))
    require rmsDelay equals the committed value derived from the exact rounded
            Appendix C table (0, 15.646634945, 33.439325332,
            50.162605460, 98.984243509, or 148.803723078 ns)
    retain 0/15/30/50/100/150 ns as the report's nominal profile metadata;
            the exact rounded powers do not reproduce those labels within 1 ns
    return immutable profile
```

Do not normalize individual taps, clusters, matrices, or realizations. Overlapping components remain separate terms in the normalizer and in channel generation.

## Architecture and source change surface

All planned physical-layer targets are currently unsealed. The sealed `src/inet/common/packet/` subtree is not touched. Before implementation, re-read the current sealing status because this statement is time-sensitive.

Use `Tgn`, not `TGn`, in C++/NED/file names. Packages are lowercase and singular. The proposed source surface is:

| Area | Artifact | Responsibility |
|---|---|---|
| Generic contract | `src/inet/physicallayer/wireless/common/contract/packetlevel/IWidebandChannelModel.{h,ned}` | Replaceable channel role; radio lifecycle hooks and immutable snapshot creation. |
| Generic contract | `src/inet/physicallayer/wireless/common/contract/packetlevel/IChannelMatrixSnapshot.h` | Rx-row/Tx-column dimensions and absolute-time/frequency response. SISO is a 1×1 matrix; do not create a parallel scalar snapshot hierarchy. |
| Generic value | `src/inet/physicallayer/wireless/common/analogmodel/common/ChannelMatrixSnapshot.{h,cc}` | Immutable snapshot metadata and response ownership. |
| Generic value | `src/inet/physicallayer/wireless/common/analogmodel/common/ChannelMatrixResponse.{h,cc}` | Rectangular complex matrix value and lazy coefficient evaluation. |
| Generic combiner | `src/inet/physicallayer/wireless/common/analogmodel/dimensional/ChannelMatrixCombiner.{h,cc}` | Selected-column SISO/MRC scalarization only. |
| Generic analog result | `src/inet/physicallayer/wireless/common/analogmodel/dimensional/ChannelMatrixReceptionAnalogModel.{h,cc}` | Derive from `DimensionalReceptionAnalogModel`; inherited power is decoded power, with snapshot, selected Tx column, and separate CCA power retained. |
| Generic noise/SNIR | `src/inet/physicallayer/wireless/common/analogmodel/dimensional/ChannelMatrixNoise.{h,cc}`, `src/inet/physicallayer/wireless/common/analogmodel/dimensional/ChannelMatrixSnir.{h,cc}` | Preserve background/interferer metadata, keep CCA operational, and reject unsupported MRC interference at the SNIR boundary. |
| Medium contract | `src/inet/physicallayer/wireless/common/contract/packetlevel/IRadioMedium.h` | Add nullable `getChannelModel()`. |
| Medium composition | `src/inet/physicallayer/wireless/common/medium/RadioMedium.{ned,h,cc}` | Add and resolve optional `channelModel`; forward radio add/remove lifecycle hooks. |
| Dimensional integration | `src/inet/physicallayer/wireless/common/analogmodel/dimensional/DimensionalMediumAnalogModel.{ned,h,cc}` | Use the optional snapshot path while preserving the exact existing path when the channel model is absent. |
| TGn data | `src/inet/physicallayer/wireless/ieee80211/channelmodel/TgnChannelProfile.{h,cc}` | Own all constants and validate/normalize profiles. |
| TGn math | `src/inet/physicallayer/wireless/ieee80211/channelmodel/TgnMimoChannel.{h,cc}` | Pure correlation, temporal-process, LOS, fluorescent, and frequency-response algorithms. No module lookup or RNG access. |
| TGn state owner | `src/inet/physicallayer/wireless/ieee80211/channelmodel/TgnChannelModel.{ned,h,cc}` | Stable link keys, derived substreams, shadowing, realization cache, reciprocity, and snapshot creation. |
| TGn path loss | `src/inet/physicallayer/wireless/ieee80211/pathloss/TgnIndoorPathLoss.{ned,h,cc}` | Deterministic breakpoint attenuation only. Link-cached shadowing stays in `TgnChannelModel`. |
| Declarative integration | `src/inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211TgnRadioMedium.ned` | Select dimensional analog model, channel model, path loss, and propagate the one authoritative profile/condition. |
| Documentation | `showcases/wireless/tgnchannel/` | Fixed-seed SISO, 2×1 MRC, and 2×2 matrix-response examples and plots. |

The generic contracts should have this effective shape; exact C++ ownership syntax may follow existing `Ptr` conventions:

```text
IChannelMatrixSnapshot:
    getNumReceiveAntennas() -> int
    getNumTransmitAntennas() -> int
    getReferenceFrequency() -> Hz
    getStartTime() -> simtime_t
    getEndTime() -> simtime_t
    getShadowingPowerGain() -> double
    getResponse(absoluteTime, frequency) -> immutable ComplexMatrix
    getActualMaximumTemporalFrequency() -> Hz
    getMaximumExcessDelay() -> s
    transpose() -> immutable IChannelMatrixSnapshot

IWidebandChannelModel:
    addRadio(radio)
    removeRadio(radio)
    computeChannel(receiver, transmission, arrival)
        -> immutable IChannelMatrixSnapshot

ComplexMatrix:
    rows, columns
    row-major complex<double> coefficients
    checked immutable access
    ordinary transpose()
```

Contract invariants:

- `rows == receiver->getAntenna()->getNumAntennas()`.
- `columns == transmission->getTransmitterRadio()->getAntenna()->getNumAntennas()`.
- All dimensions are positive and all coefficients are finite.
- A snapshot evaluation never consumes RNG, mutates link state, or depends on query order.
- Time arguments are absolute simulation times, not offsets from a packet start. Two packets on the same link therefore sample one continuous process.
- The reverse reciprocal snapshot transposes dimensions and coefficients without conjugation.
- `getResponse()` already includes the square root of `getShadowingPowerGain()`; the latter is observability metadata and must never be multiplied into received power a second time.
- Omitting `channelModel` leaves `RadioMedium` and `DimensionalMediumAnalogModel` on their existing code path byte-for-byte; do not emulate “disabled” with an identity TGn object.

## Core spatial and temporal channel algorithm

### ULA and spatial correlation

For antenna count $N$, wavelength $\lambda$, and configured spacing $s$ expressed in wavelengths:

```text
position[i] = (i - (N - 1)/2) * s * wavelength
```

For cluster mean angle $\mu$ and angular spread $\sigma$, convert degrees to radians and use the normalized circular/truncated Laplacian PAS:

$$
q(\phi)=\exp\!\left(-\frac{\sqrt 2\,|\operatorname{wrap}_{[-\pi,\pi)}(\phi-\mu)|}{\sigma}\right),
\qquad
p(\phi)=\frac{q(\phi)}{\int_{-\pi}^{\pi}q(\theta)d\theta}.
$$

For elements $p,q$:

$$
D_{pq}=\frac{2\pi(x_p-x_q)}{\lambda},\qquad
R_{pq}=\int_{-\pi}^{\pi}p(\phi)e^{jD_{pq}\sin\phi}d\phi.
$$

Use a deterministic fixed-order quadrature. Freeze its node count in the source, not as a user parameter, after a convergence test against twice as many nodes establishes `max(abs(R_N-R_2N)) <= 1e-10` for every Appendix C cluster at 1–8 antennas and spacings 0.25, 0.5, and 1.0 wavelengths.

```text
createCorrelation(cluster, antennaCount, spacing, wavelength):
    positions = centeredUlaPositions(...)
    normalization = integrateFixedOrder(q, -pi, pi)

    for row in 0..antennaCount-1:
        for column in row..antennaCount-1:
            delta = 2*pi*(positions[row]-positions[column])/wavelength
            rho = integrateFixedOrder(
                q(phi)/normalization * exp(j*delta*sin(phi)), -pi, pi)
            R[row,column] = rho
            R[column,row] = conjugate(rho)

    R = (R + conjugateTranspose(R)) / 2
    set diagonal to exactly 1 after requiring pre-fix error <= 1e-12
    require abs(R[i,j]) <= 1 + 1e-12
    return R
```

Compute a deterministic principal Hermitian PSD square root:

```text
principalSquareRoot(R):
    eigenvalues, eigenvectors = deterministicHermitianEigendecomposition(R)
    require residual norm(R*V - V*diag(eigenvalues)) <= tolerance
    require min(eigenvalues) >= -psdTolerance
    clipped = max(eigenvalues, 0)
    L = V * diag(sqrt(clipped)) * conjugateTranspose(V)
    require norm(L*conjugateTranspose(L) - R) <= reconstructionTolerance
    return L
```

The eigensolver must use this deterministic policy rather than delegating ordering and phase choices to a platform-default routine:

```text
deterministicHermitianEigendecomposition(R):
    A = exact Hermitian symmetrization of R
    V = identity

    for sweep in 0..(100 * rows(R)^2 - 1):
        changed = false
        for (p,q) in lexicographic order with p < q:
            if abs(A[p,q]) <= epsilon * rows(R) * frobeniusNorm(A):
                continue

            rotate q so A[p,q] is real and nonnegative
            apply the real symmetric Jacobi rotation to rows/columns p,q
            accumulate exactly the same unitary rotation into V
            changed = true

        if not changed:
            break

    require convergence before the sweep limit
    sort eigenpairs by decreasing eigenvalue, then by original column index
    for each eigenvector:
        rotate its largest-magnitude element, choosing the lowest tied index,
        so that element is real and nonnegative
    return real(diagonal(A)), V
```

Use `epsilon = numeric_limits<double>::epsilon()` in the stopping expression; freeze `psdTolerance` and reconstruction/residual tolerances with the quadrature convergence evidence. Repeated eigenvalues must not affect observable behavior: covariance reconstruction, not raw eigenvector identity, is the contract.

All components in a cluster reuse its Rx/Tx correlation matrices and square roots. For component $c$:

$$
X_c(t)=L_{\mathrm{rx},c}\,G_c(t)\,L_{\mathrm{tx},c}^{T}.
$$

The right factor is an ordinary transpose. Every entry of $G_c(t)$ is an independent proper $\mathcal{CN}(0,1)$ temporal process. Components remain independent even when their delays overlap.

### Continuous Bell process

The base TGn Doppler spectrum and normalized autocorrelation are:

$$
S_0(f)=\frac{1}{1+9(f/f_d)^2},\qquad
f_d=\frac{v_0}{\lambda},\qquad
v_0=1.2\ \mathrm{km/h},
$$

$$
\rho_0(\Delta t)=\exp\!\left(-\frac{2\pi f_d}{3}|\Delta t|\right).
$$

Represent it from the first implementation with a fixed oscillator bank. `TgnChannelModel` draws all coefficients while creating link state, then passes an immutable bank to the RNG-free `TgnMimoChannel` evaluator. Evaluation is a pure absolute-time function.

```text
TgnChannelModel.createLorentzianProcess(
        centerFrequency, halfWidth, oscillatorCount, derivedRng):
    for k in 0..oscillatorCount-1:
        u = (k + 0.5) / oscillatorCount
        oscillatorFrequency[k] =
            centerFrequency + halfWidth * tan(pi*(u - 0.5))
        realPart = normal(derivedRng, mean=0,
                          standardDeviation=1/sqrt(2))
        imagPart = normal(derivedRng, mean=0,
                          standardDeviation=1/sqrt(2))
        coefficient[k] = realPart + j*imagPart  // E|coefficient|^2 = 1
    return immutable LorentzianProcess(oscillatorFrequency, coefficient)

TgnMimoChannel.evaluate(process, absoluteTime):
        result = 0
        for k in process canonical increasing order:
            phase = remainder(2*pi*process.oscillatorFrequency[k]
                              *(absoluteTime - SIMTIME_ZERO), 2*pi)
            result += process.coefficient[k] * exp(j*phase)
        return result / sqrt(process.oscillatorCount)
```

For the base process, `centerFrequency = 0` and `halfWidth = fd/3`. This implements the untruncated Lorentzian spectrum; 03/940r4 permits but does not require an arbitrary cutoff near $5f_d$, so do not silently add one. If environmental speed is zero, return one fixed proper complex-normal coefficient instead of dividing by zero.

Start the convergence gate with `oscillatorCount = 128`. Before freezing that default, compare candidate counts 32/64/128/256 over a declared 10 s validation interval and lags from 0 through five coherence times. Select the smallest count for which all of these hold with fixed seeds:

- marginal complex variance differs from one by at most 2%;
- normalized autocorrelation differs from the analytic exponential by at most 0.03;
- no repeated sample sequence appears within the 10 s interval at 1 ms observation spacing;
- increasing to the next candidate changes the tested channel-capacity mean by less than 1%.

Record the selected count and evidence in the implementation change. Do not choose it during integration debugging.

### Model F vehicle component

For Model F report tap index 3 (20 ns, Cluster 1), use the complete §4.7.2 spectrum instead of the ordinary Bell-only process:

```text
environmentalSpeed = 1.2 km/h
vehicleSpeed = 40 km/h
A = 9
B = 0.5
vehicleRelativeBandwidth = 0.02
C = 36 / vehicleRelativeBandwidth^2 = 90000

fd = environmentalSpeed / wavelength
fSpike = vehicleSpeed / wavelength
bellHalfWidth = fd / sqrt(A)
spikeHalfWidth = fSpike / sqrt(C)

integratedBellPower = pi * bellHalfWidth
integratedSpikePower = pi * B * spikeHalfWidth

if integratedBellPower == 0 and integratedSpikePower == 0:
    vehicleTapProcess = one fixed proper CN(0,1) coefficient
else:
    bellWeight = integratedBellPower /
                 (integratedBellPower + integratedSpikePower)
    spikeWeight = 1 - bellWeight

    if bellWeight > 0:
        base = independent LorentzianProcess(0, bellHalfWidth)
    if spikeWeight > 0:
        spike = independent LorentzianProcess(fSpike, spikeHalfWidth)

    vehicleTapProcess(t) = (bellWeight > 0
                            ? sqrt(bellWeight)*base(t) : 0)
                         + (spikeWeight > 0
                            ? sqrt(spikeWeight)*spike(t) : 0)
```

The spike is positive-frequency only. The integrated-power normalization preserves unit marginal diffuse power while retaining the report's peak ratio. Thus `(environmentalSpeed, vehicleSpeed)` equal to `(0, positive)` selects only the spike, `(positive, 0)` selects only the Bell component, and `(0,0)` is the fixed diagnostic process. Negative speeds are invalid. Every other Model F component uses the base Bell process.

### Models D/E fluorescent-light component

Use §4.7.3 literally:

$$
g(t)=\sum_{l=0}^{2} A_l
e^{j(4\pi(2l+1)f_m t+\phi_l)},
$$

```text
A0 = 1
A1 = 10^(-15/20)
A2 = 10^(-20/20)
phi[l] ~ Uniform(0, 2*pi), drawn once per link
fm = configured mains frequency, default 50 Hz
```

This produces 100/300/500 Hz in 50 Hz regions and 120/360/600 Hz in 60 Hz regions. Resolve the report's cluster-local tap numbers as follows:

```text
Model D, Cluster 2, local taps 2,4,6
    -> report tap indices 12,14,16
    -> delays 140,200,290 ns

Model E, Cluster 1, local taps 3,5,7
    -> report tap indices 3,5,7
    -> delays 20,50,110 ns
```

Draw the target interference-to-carrier ratio and normalization once per link:

```text
x ~ Normal(mean=0.0203, standardDeviation=0.0107)
targetIc = x*x
meanGPower = A0^2 + A1^2 + A2^2
selectedPower = sum(normalized diffuse powers of selected components)
carrierPower = 1

if condition == los:
    carrierPower += firstTapKLinear * firstTapDiffusePower

fluorescentScale = sqrt(targetIc * carrierPower /
                        (selectedPower * meanGPower))

g(t) = sum(A[l] * exp(j*(4*pi*(2*l+1)*fm*t + phi[l])))

for each selected component c:
    componentMatrix[c](t) *= 1 + fluorescentScale*g(t)
```

One link-level `g(t)` is shared by all three selected components. This is the fixed INET policy for the ambiguity in the report. It preserves the unmodulated carrier and makes expected added modulation energy relative to the complete link carrier energy equal the drawn I/C.

### LOS and component composition

For LOS, only report tap index 1 receives a deterministic component. Let $p_0$ be that component's normalized diffuse power and $K=10^{K_{\mathrm{dB}}/10}$:

```text
diffuse first tap = sqrt(p0) * X0(t)
fixed LOS addition = sqrt(K*p0) * fixedLosMatrix
total first tap = diffuse first tap + fixed LOS addition
```

Do not replace the diffuse term with `sqrt(p0/(K+1))` and do not renormalize afterward. Expected total small-scale energy becomes `1 + K*p0`, as required by the report's instruction to add LOS on top of the NLOS PDP.

Use a fixed 45° AoA/AoD and this explicit centered-ULA convention:

```text
aRx[i] = exp(j*2*pi*rxPosition[i]/wavelength*sin(45 degrees))
aTx[j] = exp(j*2*pi*txPosition[j]/wavelength*sin(45 degrees))
fixedLosMatrix = outerProduct(aRx, aTx)  // ordinary transpose, no conjugation
```

The complete canonical response is:

```text
evaluateResponse(linkState, absoluteTime, frequency):
    H = zeroComplexMatrix(numRx, numTx)
    processTime = timeVariation ? absoluteTime : SIMTIME_ZERO

    for component in stableComponentIndex order:
        G = evaluateIndependentTemporalMatrix(component, processTime)
        X = component.rxSquareRoot
            * G
            * transpose(component.txSquareRoot)

        if component has fluorescent effect:
            X *= 1 + fluorescentScale*g(processTime)

        delay = profile.tap(component.reportTapIndex).excessDelay
        basebandPhase = exp(-j*2*pi
                            *(frequency - referenceFrequency)*delay)
        H += sqrt(component.normalizedLinearPower) * X * basebandPhase

    if condition == los:
        H += sqrt(firstTapKLinear*firstTapDiffusePower) * fixedLosMatrix

    H *= sqrt(shadowingPowerGain)
    require every coefficient is finite
    return H
```

Shadowing is included in the returned channel amplitude so it remains link-specific. Deterministic path loss, obstacle loss, and the current scalar element-pattern gains remain separate power factors in the dimensional analog model. This prevents double application and keeps each stage observable.

The snapshot computes its materialization limit from realized state, not just nominal report speeds:

```text
actualMaximumTemporalFrequency():
    if not timeVariation:
        return 0 Hz

    maximum = max(abs(frequency) for every realized diffuse oscillator)
    if fluorescent effect applies:
        maximum += 2 * 5 * fluorescentMainsFrequency
    return maximum
```

The fluorescent addition uses (2(2l+1)f_m), hence the largest offset is (10f_m). `maximumExcessDelay()` is the largest report-tap delay, or zero for Model A.

## Link state, reproducibility, and reciprocity

`TgnChannelModel` is the sole mutable owner of link realizations. A link state contains:

```text
LinkState {
    stableLinkKey
    canonical transmitter/receiver identities
    profile and condition
    reference carrier frequency
    Tx/Rx antenna counts and wavelength-normalized spacings
    one cached shadowing draw
    cluster correlation matrices and square roots
    all temporal oscillator frequencies and complex coefficients
    fixed LOS steering matrix
    fluorescent I/C draw and phases
}
```

Stable identity and derived substreams must not depend on pointers, allocation order, map iteration, or the first packet queried.

```text
initialize():
    read explicit OMNeT++ RNG indices for:
        linkSeedRng
        shadowingRng
        diffuseRng
        fluorescentRng
    for each configured cRNG in that order:
        highWord = rng.intRand()  // exactly one uint32 draw
        lowWord = rng.intRand()   // exactly one uint32 draw
        masterSeed = (uint64(highWord) << 32) | uint64(lowWord)
    name the four results:
        linkMasterSeed, shadowingMasterSeed,
        diffuseMasterSeed, fluorescentMasterSeed

seedFamily(effect):
    linkSalt = SplitMix64Finalizer(linkMasterSeed)
    if effect == shadowing:
        return SplitMix64Finalizer(linkSalt XOR shadowingMasterSeed)
    if effect in {diffuseBell, vehicleBell, vehicleSpike}:
        return SplitMix64Finalizer(linkSalt XOR diffuseMasterSeed)
    if effect in {fluorescentIc, fluorescentPhase}:
        return SplitMix64Finalizer(linkSalt XOR fluorescentMasterSeed)
    fail on an unregistered stochastic purpose

stableRadioId(radio):
    return canonical full module path plus radio/interface vector index

makeLinkKey(tx, rx, reciprocal):
    if reciprocal:
        return sorted(stableRadioId(tx), stableRadioId(rx))
    else:
        return ordered(stableRadioId(tx), stableRadioId(rx))

derivePurposeSeed(familySeed, linkKey, profile, condition, component, matrixRow,
                  matrixColumn, temporalEffect):
    bytes = ASCII("INET-TGN-SEED-V1")
    append every unsigned integer as 8-byte big-endian
    append every string as 8-byte big-endian length followed by UTF-8 bytes
    append the identity fields after familySeed in function-signature order;
           encode absent numeric fields as UINT64_MAX and absent strings as
           a UINT64_MAX length, never as an omitted byte sequence
    hash = FNV-1a-64(bytes, offsetBasis=14695981039346656037,
                     prime=1099511628211)
    return SplitMix64Finalizer(hash XOR familySeed)

createLinkState(key):
    for each stochastic purpose key in canonical order:
        purposeSeed = derivePurposeSeed(seedFamily(effect), ...)
        derivedRng = TgnDerivedMersenneTwister(purposeSeed)
        use OMNeT++ normal()/uniform() distributions with derivedRng
        draw the complete immutable value for that one purpose
    cache immutable coefficients and matrices
```

`SplitMix64Finalizer(x)` is the fixed stateless transform `x ^= x >> 30; x *= 0xbf58476d1ce4e5b9; x ^= x >> 27; x *= 0x94d049bb133111eb; x ^= x >> 31`, with unsigned 64-bit wraparound. Check derived seeds for collisions among all purpose keys of each link and fail if one occurs.

`TgnDerivedMersenneTwister` is a private `omnetpp::cRNG` adapter in `TgnChannelModel.cc`. It owns the OMNeT++ `MTRand` implementation through a pointer so construction can use the two-word seed constructor directly and never invoke `MTRand`'s entropy-seeded default constructor:

```text
TgnDerivedMersenneTwister(seed64):
    words[0] = MTRand::uint32(seed64 >> 32)
    words[1] = MTRand::uint32(seed64 & 0xffffffff)
    generator = unique_ptr<MTRand>(new MTRand(words, seedLength=2))

intRand():
    increment cRNG.numDrawn once
    return uint32(generator.randInt())

intRand(n):
    require 0 < n < UINT32_MAX
    increment cRNG.numDrawn once
    return uint32(generator.randInt(n - 1))  // cRNG requires [0,n)

intRandMax(): return UINT32_MAX
doubleRand(): increment numDrawn; return generator.randExc()         // [0,1)
doubleRandNonz(): increment numDrawn; return generator.randDblExc()  // (0,1)
doubleRandIncl1(): increment numDrawn; return generator.rand()       // [0,1]
initialize(...): throw because this private adapter is constructor-seeded only
selfTest(): verify the committed fixed-seed golden vector
```

The adapter supplies the `cRNG` API but does not define a new random-number or distribution algorithm. All normal and uniform variates use OMNeT++ distribution functions against this adapter; do not use `std::random_device`, `std::hash`, global RNGs, or a home-grown distribution library. `TgnChannelModel` creates every coefficient, oscillator bank, shadow draw, I/C draw, and fluorescent phase. `TgnMimoChannel` receives immutable values and has no `cRNG` include, pointer, lookup, or draw. The stable seed derivation is a deterministic identity mapping, not a sequential lazy draw. This design requires architecture approval because it creates derived per-link generators; lazy allocation directly from one shared sequential RNG is forbidden.

Reciprocity policy:

```text
if reciprocal:
    canonical direction = lower stableRadioId -> higher stableRadioId
    build exactly one state in the canonical dimensions
    forward query returns Hcanonical(t,f)
    reverse query returns transpose(Hcanonical(t,f))
else:
    ordered directions use independent states
```

The reciprocal reverse query must not allocate new state or advance RNG. Shadowing, LOS, Bell/vehicle processes, and fluorescent modulation are shared in reciprocal mode.

Lifecycle rules:

- `RadioMedium::addRadio()` forwards registration to the channel model.
- Removal evicts medium/channel cache entries and retains no radio pointer. Already-returned immutable snapshots remain self-contained and valid for their declared interval.
- Re-adding the same stable identity reconstructs the same realization from derived seeds.
- Changing antenna count, profile, condition, or reference frequency for an existing state is an error; these are initialization-time inputs.
- Radio motion changes deterministic path loss through the existing arrival positions. The cached TGn realization remains a time process for the link; spatial decorrelation caused by endpoint motion is outside this plan and must be documented.

## Deterministic TGn path loss and shadowing

`TgnIndoorPathLoss` implements only the deterministic Table I law. For distance $d$, breakpoint $d_\mathrm{BP}$, propagation speed $c_p$, and RF frequency $f$:

```text
effectiveDistance = max(distance, referenceDistance)  // default 1 m INET policy
freeSpacePowerGain(d) = (c_p / (4*pi*f*d))^2

if effectiveDistance <= breakpointDistance:
    pathGain = freeSpacePowerGain(effectiveDistance)
else:
    pathGain = freeSpacePowerGain(breakpointDistance)
             * (effectiveDistance / breakpointDistance)^(-3.5)
```

This is continuous at the breakpoint, has 20 dB/decade and 35 dB/decade slopes, and is frequency-dependent. `computeRange()` must invert the same two branches and must not include fading or shadowing:

```text
computeRange(propagationSpeed, frequency, requestedGain):
    require 0 < requestedGain <= 1
    maximumModeledGain = computePathLoss(referenceDistance)
    breakpointGain = computePathLoss(breakpointDistance)

    if requestedGain > maximumModeledGain:
        return NaN  // no distance in the clamped model reaches this gain
    if requestedGain >= breakpointGain:
        distance = propagationSpeed /
                   (4*pi*frequency*sqrt(requestedGain))
    else:
        distance = breakpointDistance
                 * (breakpointGain/requestedGain)^(1/3.5)

    distance = max(distance, referenceDistance)
    require forward evaluation at distance agrees with requestedGain
    return distance
```

Shadowing belongs to `TgnChannelModel` because the current frequency/distance path-loss call has no Tx/Rx identity:

```text
sigmaDb = condition == los
        ? profile.shadowSigmaLosDb
        : profile.shadowSigmaNlosDb
shadowDb ~ Normal(0, sigmaDb), once per stable link
shadowingPowerGain = 10^(-shadowDb/10)
```

LOS is rejected when the current link distance exceeds the profile breakpoint. NLOS is allowed at any distance. The initial model has no spatially correlated or time-updated shadow field; caching one draw for the link lifetime is an explicit policy.

TGn configurations must set `rangeFilter = ""`. Gaussian fading and log-normal shadowing have no finite maximum gain, so `communicationRange` and `interferenceRange` filtering are not conservative. Initialization must throw if a TGn channel is combined with either range filter.

## Matrix-to-receiver integration

The medium and analog-model control flow is explicit:

```text
RadioMedium.initialize():
    channelModel = optional submodule implementing IWidebandChannelModel

RadioMedium.addRadio/removeRadio(radio):
    perform existing medium/cache lifecycle work
    if channelModel exists:
        channelModel.addRadio/removeRadio(radio)

DimensionalMediumAnalogModel.computeReception(receiver, transmission, arrival):
    if radioMedium.channelModel does not exist:
        return existingComputeReception(receiver, transmission, arrival)

    snapshot = radioMedium.channelModel.computeChannel(
                   receiver, transmission, arrival)
    largeScalePower = transmissionPower
                    * deterministicPathLoss
                    * obstacleLoss
                    * existingScalarTxAntennaGain
                    * existingScalarRxAntennaGain
    decodedPower = largeScalePower
                 * combineSelectedColumn(snapshot, selectedTransmitAntenna)
    ccaPower = largeScalePower
             * sumSelectedColumnBranchPowers(snapshot,
                                             selectedTransmitAntenna)
    return ChannelMatrixReceptionAnalogModel(snapshot, decodedPower, ccaPower)
```

`existingComputeReception` above means a direct call to the preserved pre-change implementation, not a reimplementation. Shadowing is absent from `largeScalePower` because it is already in the snapshot response.

### Selected-column MRC

The first release chooses one transmit antenna globally per medium, default column 0. It does not add per-packet precoder metadata. The selected column carries the complete transmission-power function; all unselected columns carry zero power, and there is no division by the physical Tx-antenna count. Validate the selected column against each transmitter's antenna count. The generic common analog model must not depend on `Ieee80211Transmission` or inspect IEEE modes.

The concrete IEEE-specific `TgnChannelModel::computeChannel()` validates the authoritative transmission mode before creating a snapshot:

```text
validateIeee80211Mode(transmission):
    mode = transmission.transmissionMode
    require mode.dataMode.numberOfSpatialStreams == 1

    if mode is Ieee80211HtMode:
        require mode.dataMode.bandwidth in {20 MHz, 40 MHz}
        stbc = mode.headerMode.getSTBC()
        numberOfSpaceTimeStreams =
            mode.dataMode.numberOfSpatialStreams + stbc
        require stbc == 0
        require numberOfSpaceTimeStreams == 1
    else if mode is legacy DSSS, HR-DSSS, or OFDM:
        require a dimensional transmission-power representation
        // These one-stream formats keep ACK/beacon/control exchanges usable.
    else:
        throw unsupported PPDU format
```

The two STBC checks are deliberately redundant: one diagnoses the unsupported feature, while the other seals the scalarizer's one-space-time-stream contract against future mode changes.

For selected column $k$, define $h(t,f)=H(t,f)[:,k]$. Ideal normalized MRC uses:

$$
w=\begin{cases}h/\|h\|,&\|h\|>0\\0,&\text{otherwise,}\end{cases}
\qquad
G_\mathrm{MRC}=|w^Hh|^2=\|h\|^2.
$$

```text
combineSelectedColumn(snapshot, selectedColumn, time, frequency):
    H = snapshot.getResponse(time, frequency)
    require selectedColumn in [0, H.columns)
    gain = 0
    for rx in 0..H.rows-1:
        gain += squaredMagnitude(H[rx, selectedColumn])
    return gain
```

For a 1×1 matrix this is exactly $|H_{00}|^2$. For the fixture `{3+4i, 12i}`, the gain is `25 + 144 = 169`. Never substitute Frobenius norm across all columns; unselected columns carry no transmitted power in this receiver policy.

### Desired power, CCA power, and interference

The current medium reduces each reception to one power function and later sums those functions as noise. That is insufficient for MRC because an interferer must be projected through the desired signal's weights, while CCA must remain available even when decoding is unsupported. Preserve these meanings explicitly:

```text
ChannelMatrixReceptionAnalogModel extends DimensionalReceptionAnalogModel {
    snapshot
    selectedTransmitColumn
    deterministicLargeScalePowerFunction
    decodedPowerFunction       // selected-column MRC gain
    ccaPowerFunction           // sum of physical selected-column branch powers
    constructor passes decodedPowerFunction to the base-class power argument
    inherited getPower() therefore returns decodedPowerFunction
}

ChannelMatrixNoise extends DimensionalNoise {
    ccaBackgroundPowerFunction
    postCombinerBackgroundPowerFunction
    interferingMatrixReceptions[]
    ccaAggregatePowerFunction
    constructor passes ccaAggregatePowerFunction to the base-class power argument
    inherited getPower() therefore returns ccaAggregatePowerFunction
}

ChannelMatrixSnir implements IDimensionalSnir {
    desiredMatrixReception
    matrixNoise
    postCombinerSnirFunction
    combinerKind = selectedColumnMrc
}
```

For a one-stream desired signal, decoded power and total selected-column branch energy have the same numeric gain but remain separate named functions because their consumers and future extensions differ. Derivation from `DimensionalReceptionAnalogModel` preserves all existing receiver casts. Its inherited `getPower()` returns decoded power, so sensitivity checks and `ReceiverBase` observe the MRC-decoded signal rather than a raw branch or Frobenius sum.

`ChannelMatrixNoise::getPower()` exposes the CCA aggregate so `FlatReceiverBase` can continue energy detection. The existing configured scalar background-noise function is interpreted as the equivalent noise of one complete radio front end, not a per-antenna branch. Store it in two separately named fields—`ccaBackgroundPowerFunction` and `postCombinerBackgroundPowerFunction`—whose numeric functions are initially identical. CCA includes it once, and does not multiply it by the Rx count. Unit-norm MRC also includes it once. This is a bounded receiver policy forced by the current scalar background-noise contract; the separate names prevent a future per-branch covariance model from silently changing both consumers together.

Implement all three matrix-path analog-model operations; the two `computeNoise()` overloads have different consumers and must not be conflated:

```text
computeNoise(listening, interference):
    background = dimensional background noise, or zero function
    matrixInterferers = checked matrix receptions from interference
    ccaAggregate = background
                 + sum(interferer.ccaPowerFunction for matrixInterferers)
    do not throw merely because CCA sees several transmissions
    return ChannelMatrixNoise(
        ccaBackgroundPowerFunction = background,
        postCombinerBackgroundPowerFunction = background,
        interferingMatrixReceptions = matrixInterferers,
        inheritedPower = ccaAggregate)

postCombinerNoise(desiredReception, matrixNoise):
    if desired reception has more than one receive antenna and
       any interfering reception overlaps the decoded time/frequency region:
        throw unsupported MRC interference error

    if desired reception is 1x1:
        return matrixNoise.postCombinerBackgroundPowerFunction
             + sum(interferer.getPower() for overlapping interferers)
    else:
        return matrixNoise.postCombinerBackgroundPowerFunction

computeNoise(reception, noise):
    // ReceiverBase::computeSignalPower() uses this for SignalPowerInd.
    desired = checked ChannelMatrixReceptionAnalogModel
    matrixNoise = checked ChannelMatrixNoise
    signalPlusNoise = desired.getPower()
                    + postCombinerNoise(desired, matrixNoise)
    return ordinary DimensionalNoise over the reception interval/band
           with power = signalPlusNoise

computeSNIR(reception, noise):
    desired = checked ChannelMatrixReceptionAnalogModel
    matrixNoise = checked ChannelMatrixNoise
    denominator = postCombinerNoise(desired, matrixNoise)
    numerator = desired.getPower()
    return ChannelMatrixSnir(desired, matrixNoise,
                             numerator / denominator)
```

When the optional channel model is absent, both overloads and `computeSNIR()` execute their unchanged existing implementations. The matrix path provides correct noise-limited SISO/MRC, correct SISO interference, correct CCA, receiver-sensitivity compatibility, and `SignalPowerInd` compatibility without pretending that independently MRC-combined interferer powers are valid. Covariance-aware interference, $|w^Hg|^2$, and multiple interfering streams belong to a future receiver policy that has access to every interferer's selected column or precoder.

### Dimensional function materialization

`ChannelMatrixResponse` evaluates the exact continuous process at any point. The scalar dimensional function must also implement partition/min/max/mean operations without drawing randomness. Build a deterministic piecewise-bilinear view over each reception:

```text
materializePowerFunction(transmission, receptionInterval, snapshot):
    timeNodes = all signal-part boundaries
              + all OFDM symbol boundaries and midpoints from the
                authoritative transmission mode

    if snapshot.actualMaximumTemporalFrequency > 0:
        add extra time nodes so adjacent nodes are no farther apart than
            1/(20 * snapshot.actualMaximumTemporalFrequency)

    frequencyNodes = nominal band edges
                   + every occupied subcarrier interval edge and center from
                     retained transmission-spectrum metadata

    if the transmission has no partitioned spectrum:
        if snapshot.maximumExcessDelay == 0:
            add the nominal band center; edges and center are sufficient
            because the Model A response is frequency-flat
        else:
            add uniform frequency nodes no farther apart than
                1/(20 * snapshot.maximumExcessDelay)

    for each (timeNode, frequencyNode) in canonical order:
        value = selectedColumnGain(snapshot.getResponse(node))

    return immutable piecewise-bilinear power function
```

When maximum temporal frequency is zero, signal-part boundaries suffice. Every exact EESM or diagnostic carrier query is evaluated at a stored carrier center; interpolation is only between those nodes. Grid creation is a representation/detail knob, not a second stochastic channel, and must be tested against direct response evaluation.

Do not change `Ieee80211EesmErrorModel` in this work. Its current one-Tx/one-Rx and no-interior-time-variation checks remain intentional calibration gates. The TGn showcase must state which error model is used and must not present NIST/mean-SNIR packet results as validation of TGn PER.

## Declarative configuration

Add `Ieee80211TgnRadioMedium.ned`, extending `Ieee80211DimensionalRadioMedium`. It owns the authoritative user parameters and propagates them to the channel/path-loss submodules rather than requiring duplicate INI assignments.

The generic `RadioMedium.ned` gains exactly one optional role:

```text
channelModel: <default("")> like IWidebandChannelModel if typename != "" {
    @display("p=300,300");
}
```

Proposed parameter contract:

```text
string profile @enum("A","B","C","D","E","F") = default("B")
string condition @enum("nlos","los") = default("nlos")
bool reciprocal = default(false)
bool timeVariation = default(true)
bool vehicleEffect = default(true)
bool fluorescentEffect = default(true)
double fluorescentMainsFrequency @unit(Hz) = default(50Hz)
double environmentalSpeed @unit(mps) = default(0.333333333333mps)  // 1.2 km/h
double vehicleSpeed @unit(mps) = default(11.111111111111mps)       // 40 km/h
double antennaSpacing = default(0.5)                               // wavelengths
int selectedTransmitAntenna = default(0)
int oscillatorCount = default(256)                                // frozen by convergence gate
double referenceDistance @unit(m) = default(1m)
int linkSeedRng = default(0)
int shadowingRng = default(0)
int diffuseRng = default(0)
int fluorescentRng = default(0)
```

The four names allow explicit OMNeT++ RNG mapping when desired, but all default to the always-available local RNG 0. When indices alias, master extraction still follows the fixed link/shadowing/diffuse/fluorescent order and consumes exactly eight 32-bit draws.

Parameters with standard-fixed values (`A=9`, `B=0.5`, vehicle relative bandwidth `0.02`, fluorescent harmonic amplitudes, affected taps, K factors, breakpoints, and shadow sigmas) are constants in `TgnChannelProfile`; do not expose them as duplicated user inputs.

Effective NED composition:

```text
module Ieee80211TgnRadioMedium extends Ieee80211DimensionalRadioMedium
{
    parameters:
        // parameters above
        rangeFilter = default("");
        channelModel.typename = default("TgnChannelModel");
        pathLoss.typename = default("TgnIndoorPathLoss");

        channelModel.profile = profile;
        channelModel.condition = condition;
        channelModel.reciprocal = reciprocal;
        channelModel.timeVariation = timeVariation;
        channelModel.vehicleEffect = vehicleEffect;
        channelModel.fluorescentEffect = fluorescentEffect;
        channelModel.fluorescentMainsFrequency = fluorescentMainsFrequency;
        channelModel.environmentalSpeed = environmentalSpeed;
        channelModel.vehicleSpeed = vehicleSpeed;
        channelModel.antennaSpacing = antennaSpacing;
        channelModel.oscillatorCount = oscillatorCount;
        channelModel.linkSeedRng = linkSeedRng;
        channelModel.shadowingRng = shadowingRng;
        channelModel.diffuseRng = diffuseRng;
        channelModel.fluorescentRng = fluorescentRng;

        analogModel.selectedTransmitAntenna = selectedTransmitAntenna;

        pathLoss.profile = profile;
        pathLoss.referenceDistance = referenceDistance;
}
```

Example configuration:

```ini
*.radioMedium.typename = "Ieee80211TgnRadioMedium"
*.radioMedium.profile = "D"
*.radioMedium.condition = "nlos"
*.radioMedium.timeVariation = true
*.radioMedium.reciprocal = false
*.radioMedium.fluorescentMainsFrequency = 50Hz
*.radioMedium.selectedTransmitAntenna = 0

**.wlan[*].radio.typename = "Ieee80211DimensionalRadio"
**.wlan[*].radio.antenna.numAntennas = 2
**.wlan[*].radio.transmitter.dimensionalSpectrumMode = "occupiedSubcarriers"
**.wlan[*].opMode = "n(mixed-2.4Ghz)"
```

Initialization, link-state creation, or reception use must reject at the earliest boundary that has all required information:

- unknown profile or condition;
- antenna count outside 1–8, or nonpositive spacing, oscillator count, or reference distance;
- selected transmit antenna outside the actual Tx dimension;
- LOS at a distance beyond the profile breakpoint;
- an HT bandwidth other than 20/40 MHz, a VHT-or-later PPDU, more than one spatial stream, nonzero STBC, or more than one space-time stream; legacy DSSS/HR-DSSS/OFDM one-stream frames are accepted;
- missing dimensional analog representation;
- matrix dimensions inconsistent with antenna counts;
- nonfinite generated coefficients or invalid covariance;
- range filtering enabled with TGn;
- negative environmental or vehicle speed;
- fluorescent effect with a nonpositive mains frequency when it applies to Model D/E;
- a reference carrier frequency change after link-state creation.

`vehicleEffect` is accepted for every profile so one configuration can sweep A–F; it is an explicitly documented no-op for A–E and affects only Model F report tap 3. Likewise, `fluorescentEffect` is a no-op outside Models D/E. These are not invalid configurations.

## Implementation phases and pseudocode gates

### Phase 0 — lock evidence and architecture

Before production code:

1. Reconfirm the TGn PDF checksum and exact Appendix C transcription.
2. Recheck sealing for every proposed source target.
3. Freeze the spatial quadrature node count with the stated convergence test.
4. Freeze the oscillator count with the stated 32/64/128/256 convergence campaign.
5. Add fixed golden vectors for 64-bit master extraction, the `INET-TGN-SEED-V1` serialization/FNV-1a/SplitMix64 derivation, the two-word `MTRand` seed adapter, and the first normal/uniform variates.
6. Confirm the selected normalization policy against the Table III capacity oracle; if the reference MATLAB program is legally available, compare it as an additional oracle.
7. Obtain architecture approval for the generic contract placement and derived per-link OMNeT++ RNG scheme.

No medium integration begins until these gates are recorded. These are bounded design-verification tasks, not opportunities to alter algorithms during later debugging.

### Phase 1 — immutable contracts and deterministic value objects

Implement `IWidebandChannelModel`, `IChannelMatrixSnapshot`, `ChannelMatrixSnapshot`, `ChannelMatrixResponse`, and `ChannelMatrixCombiner` first.

Acceptance:

- 1×1 and rectangular matrices retain dimensions and finite values.
- Ordinary transpose swaps dimensions without conjugation.
- Checked indexing rejects invalid rows/columns.
- Snapshots own all response state and remain valid independently of temporary builders.
- A two-tap synthetic response proves the sign of `exp(-j*2*pi*deltaF*delay)` through exact cancellation/reinforcement.

### Phase 2 — profiles, spatial generation, and time variation

Implement `TgnChannelProfile` and pure `TgnMimoChannel` together. Time processes are not postponed.

Acceptance:

- Exact A–F table equality and component counts 1/12/18/27/38/41.
- RMS delay spreads 0/15/30/50/100/150 ns.
- Complete NLOS expected diffuse energy equals one.
- Correlation matrices are Hermitian PSD with unit diagonal and reconstruct from their square roots.
- Empirical Kronecker covariance agrees with the target.
- Overlapping-delay components remain independent until coherent response summation.
- LOS ratio and total-power increase are exact.
- Bell marginal power/autocorrelation, Model F positive-frequency spike, and D/E harmonic/I-C behavior pass their initial statistical gates.
- Model F speed pairs `(0,0)`, `(0,positive)`, and `(positive,0)` follow the declared fixed/spike/Bell branches; either negative speed fails.
- Querying the same time/frequency in any order is bit-identical and consumes no RNG.

### Phase 3 — link state, path loss, and reciprocity

Implement `TgnChannelModel` and `TgnIndoorPathLoss`.

Acceptance:

- Stable identity/configuration gives an identical realization across runs.
- Link creation/query order does not change results.
- Different ordered links use independent realizations.
- Reciprocal reverse links are exact ordinary transposes at every tested time/frequency.
- Shadowing is drawn once per link and uses the profile/condition sigma.
- Path loss is continuous at each breakpoint with correct frequency dependence and slopes.
- LOS beyond the breakpoint and unsafe range filtering fail at initialization/use boundary.
- Remove/re-add of a stable radio identity reconstructs without dangling state.
- Reuse at the same carrier frequency succeeds; runtime carrier-frequency change fails with a precise diagnostic.

### Phase 4 — medium, CCA, and bounded receiver integration

Add the optional medium slot and channel-aware reception/noise/SNIR values. Preserve the no-channel path exactly.

Acceptance:

- Channel omitted: existing dimensional behavior and directly related fingerprints are unchanged.
- SISO: matrix path equals a hand-computed scalar channel.
- 2×1: integrated selected-column MRC ratio is exactly 169 for the deterministic fixture.
- A two-column fixture proves that the full transmit power is assigned only to the selected column and is not divided by antenna count.
- CCA still observes multiple matrix receptions and never fails merely because decoding would be unsupported.
- Scalar background noise is counted once for both CCA and post-combiner SNIR, independent of Rx antenna count.
- Noise-only 1×1 and 2×1 fixtures therefore report the same configured CCA floor and post-combiner noise floor.
- Noise-limited MRC forms a valid dimensional SNIR.
- `ChannelMatrixReceptionAnalogModel` passes the existing dimensional receiver cast, reception-possibility/sensitivity check, and `ReceiverBase` signal-power indication path.
- Both `computeNoise()` overloads return the declared CCA or signal-plus-post-combiner function, respectively.
- Overlapping MRC interference fails only at the matrix-aware SNIR/decoding boundary with a precise diagnostic.
- More than one spatial stream, any HT STBC, or more than one space-time stream fails before scalarization; no Frobenius fallback exists.
- One-stream HT data followed by its ordinary legacy ACK completes, proving that the format gate does not reject required mixed-format control traffic.
- Model A without partitioned spectrum materializes from band edges/center without a zero-delay division and equals direct evaluation.
- A response varies continuously across packet boundaries because it uses absolute time.

### Phase 5 — showcase, validation, and review

Add `showcases/wireless/tgnchannel/` with:

- an abstract base configuration with fixed seeds and time variation enabled;
- one configuration per A–F profile;
- 1×1, 2×1 selected-column MRC, and 2×2 matrix-response cases;
- fixed-time/frequency response plots and absolute-time evolution plots;
- capacity CDF/mean generation for the Table III setup;
- explicit text separating the TGn channel, the one-stream MRC policy, and the selected error model;
- no claim that NIST packet loss or the existing SISO EESM is a TGn MRC PER oracle.

## Focused verification matrix

Add these tests and keep source-to-test mapping explicit:

| Test | Direct source/contract |
|---|---|
| `tests/unit/TgnChannelProfile_1.test` | Exact profile tables, normalization, RMS delays, invalid data. |
| `tests/unit/ChannelMatrixSnapshot_1.test` | Immutable dimensions, checked lookup, transpose, response metadata. |
| `tests/unit/ChannelMatrixCombiner_1.test` | 1×1, selected columns, exact MRC gain, invalid metadata. |
| `tests/unit/TgnMimoChannel_1.test` | Spatial correlation, coherent delays, phase sign, LOS, time processes. |
| `tests/unit/TgnChannelModel_1.test` | Stable keys/seeds, cache, lifecycle, shadowing, reciprocity, query order. |
| `tests/unit/TgnIndoorPathLoss_1.test` | Breakpoint continuity, slopes, inverse range, frequency, reference distance. |
| `tests/unit/TgnMimoChannelStatistics_1.test` | Covariance, Doppler/fluorescent spectra, Table III capacity means. |
| `tests/module/TgnDimensionalRadioMedium_1.test` | Optional fallback, SISO/MRC integration, receiver casts/tags, both noise overloads, CCA, mixed-format ACK, time continuity. |
| `tests/module/TgnInvalidConfiguration_1.test` | NED/runtime failures for dimension, PPDU/NSS/STBC/space-time-stream, speed, range-filter, LOS, and carrier-switch boundaries. |

Unit-test invariants must include:

- raw and normalized profile power are both inspectable;
- two components at one delay retain distinct cluster identity;
- response phase uses baseband frequency offset and the negative sign;
- all generated values are finite;
- same seed/link/time is bit-reproducible and different identities differ;
- base Bell samples are correlated, never redrawn independently per packet;
- only F report tap 3 has the vehicle spike;
- only D Cluster 2 local taps 2/4/6 and E Cluster 1 local taps 3/5/7 have fluorescent components;
- `timeVariation=false` produces a stable diagnostic response, while the main acceptance fixture has it enabled;
- a Model A non-partitioned response uses the explicit zero-delay materialization branch;
- Model F covers all nonnegative zero/positive speed pairs and rejects negative speeds;
- LOS changes only report tap 1 and increases total expected power by `K*p0`;
- 1×1 matrix generation reduces exactly to SISO;
- reciprocal rectangular matrices swap dimensions correctly.

Table III statistical validation uses:

```text
4 Tx × 4 Rx
half-wavelength ULA
isotropic scalar element pattern
no coupling
same polarization
NLOS
timeVariation = false
vehicleEffect = false
fluorescentEffect = false
10 dB average SNR
fixed Appendix C angles
2000 realizations per batch
C = log2(det(I + (rho/Ntx) * H * H^H))
```

This test deliberately isolates the report's spatial/correlation capacity oracle. It does not defer time variation: Bell, vehicle, and fluorescent behavior have separate mandatory statistical tests in the same initial implementation gate, and the module acceptance fixture runs with time variation enabled.

Expected mean capacities in bit/s/Hz:

```text
A 9.1   B 8.9   C 8.6   D 10.0   E 9.3   F 10.4   iid 10.9
```

Expected percentages of the iid mean, rounded as in the report:

```text
A 83%   B 82%   C 79%   D 92%   E 85%   F 95%   iid 100%
```

Use 20 independently seeded batches of 2000 realizations. For each model, require the report value to lie within the measured 95% confidence interval expanded by an absolute 0.15 bit/s/Hz numerical/model-policy allowance. Also require the percent-of-iid ordering to agree within 3 percentage points. Store seeds and batch order in the test diagnostics. Do not tune normalization or correlation to force a pass; a miss blocks medium integration and triggers a source/policy review.

Focused debug commands from the repository root:

```sh
make MODE=debug -j$(nproc)

inet_run_unit_tests -m debug -f \
'(TgnChannelProfile|ChannelMatrixSnapshot|ChannelMatrixCombiner|TgnMimoChannel|TgnChannelModel|TgnIndoorPathLoss|TgnMimoChannelStatistics)_1\.test'

inet_run_module_tests -m debug -f \
'Tgn(DimensionalRadioMedium|InvalidConfiguration)_1\.test'
```

The integration fixture belongs under `tests/module`, not `tests/unit`, because it contains NED/INI radios and a medium.

For channel-disabled legacy coverage, run only the directly mapped dimensional fingerprints from `tests/fingerprint`:

```sh
./fingerprinttest -d \
  -m '.*(GenericRadioWithDimensionalAnalogModel|Ieee80211RadioWithDimensionalAnalogModel).*' \
  -f 'tplx' -f '~tNl' -f '~tND' \
  wireless-combo.csv
```

Explain the first mismatch before considering any expectation change. Do not update fingerprint CSV files without explicit user approval. Do not add a stochastic TGn fingerprint unless a fixed-seed end-to-end trajectory is deliberately made part of the contract.

Because common wireless contracts and IEEE 802.11 sources both change, also verify in debug mode:

- the normal enabled-feature build;
- a `PhysicalLayerWirelessCommon` build with IEEE 802.11 disabled;
- an IEEE 802.11 build with TGn unselected in configuration;
- focused existing dimensional module tests whose source paths overlap the changed common files;
- focused existing EESM tests only if a shared dimensional SNIR/spectrum contract is actually edited.

Never broaden to an unfiltered test suite merely because no mapped test exists; report the coverage gap instead.

## Architecture, review, and completion gates

The implementation must map and review at least:

- `R-SCOPE-WIRELESS`, `R-SCOPE-FIDELITY`, `R-RUN-REPRO`, `R-COMPOSE-NOCODE`;
- `AR-ORG-DOMAINS`, `AR-ORG-CONTRACTS`;
- `AR-MOD-COMPOSITION`, `AR-MOD-PLUGGABLE`, `AR-MOD-FIDELITY`;
- `AR-PKT-SIGNAL`;
- `AR-CFG-INFER`, `AR-CFG-PARAMS`;
- `AR-EXT-FEATURES`;
- `AR-QUAL-TESTS`, `AR-QUAL-DETERMINISM`, `AR-QUAL-NAMING`, `AR-QUAL-TRACEABILITY`;
- `AR-WLAN-STD-TRACE`, with 03/940r4 identified as nonnormative model provenance;
- `AR-WLAN-STD-GATING`, so merely compiling TGn never changes legacy modes;
- `AR-WLAN-ARCH-BOUNDARIES`, `AR-WLAN-ARCH-OWNERSHIP`, `AR-WLAN-ARCH-VARIANTS`;
- `AR-WLAN-PHY-AUTHORITY`, `AR-WLAN-PHY-TIMING`, and `AR-WLAN-QUAL-TESTS`.

Run focused architecture checks after the diff stabilizes:

```sh
bash /home/user/omnetpp_ws/inet-skills/.agents/skills/inet-architectural-requirements/references/enforcement/check-architecture.sh \
  src/inet/physicallayer/wireless/common

bash /home/user/omnetpp_ws/inet-skills/.agents/skills/inet-architectural-requirements/references/enforcement/check-architecture.sh \
  src/inet/physicallayer/wireless/ieee80211
```

Then perform the complete general semantic checklist and the complete IEEE 802.11 semantic checklist. Review must explicitly answer:

- Is the generic contract necessary and free of IEEE-specific constants?
- Does `TgnChannelModel` alone own mutable link/RNG state?
- Can observation, logging, and plot generation be removed without changing channel values?
- Does evaluation consume zero RNG and remain query-order independent?
- Are desired, CCA, background noise, and unsupported interference kept distinct?
- Does legacy behavior take the original code path when `channelModel` is omitted?
- Are all physical NED parameters unit-annotated, defaulted, and single-purpose?
- Do all new modules have appropriate display icons?
- Does the feature-off build prove the common layer has no dependency on IEEE 802.11 code?

The work is complete only when:

1. All Phase 0 design constants and policies are frozen with evidence.
2. Exact profile, deterministic math, statistical channel, link-state, path-loss, and integration tests pass in debug mode with explicit filters.
3. Time-enabled Bell, vehicle, and fluorescent behavior passes as part of the main channel gate.
4. Table III capacity validation passes without tuning to the oracle.
5. Channel-disabled dimensional fingerprints are unchanged, or any first divergence is explained and explicitly approved before a baseline change.
6. Architecture checks and both semantic review checklists pass or have explicit, recorded dispositions.
7. The showcase is reproducible from fixed configuration, seed, and documented artifact paths.
8. The final report records working directory, debug build command/status, each filtered test command/status, configuration/run/seed, and generated result/plot paths.
